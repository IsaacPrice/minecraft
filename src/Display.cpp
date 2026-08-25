#include "headers/Display.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "headers/Input.hpp"

namespace Display
{
namespace
{
    GLFWwindow* displayWindow = nullptr;

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    bool sizeChanged = false;

    Mode currentMode = Mode::Windowed;

    // The size to go back to when leaving fullscreen or borderless. Without it,
    // returning to windowed would come back at whatever the monitor happened to
    // be, which after a fullscreen switch is the whole screen.
    int windowedWidth = 1920;
    int windowedHeight = 1080;

    double frameBudget = 0.0; // seconds a frame may take, 0 for no limit
    double nextFrameTime = 0.0;

    bool vsyncEnabled = true;

    struct Resolution
    {
        int width;
        int height;
    };

    std::vector<Resolution> resolutions;

    void NoteSize(int width, int height)
    {
        // A minimised window reports zero, which would divide by zero in the
        // aspect ratio and make an offscreen buffer with no pixels in it.
        if (width <= 0 || height <= 0)
            return;

        if (width == framebufferWidth && height == framebufferHeight)
            return;

        framebufferWidth = width;
        framebufferHeight = height;
        sizeChanged = true;
    }

    void OnFramebufferSize(GLFWwindow*, int width, int height)
    {
        NoteSize(width, height);
    }

    void BuildResolutionList()
    {
        resolutions.clear();

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor)
            return;

        int count = 0;
        const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);

        for (int i = 0; i < count; i++)
        {
            // Below this there is not enough room to lay the menus out, and no
            // one is choosing to play at 640x480 on a monitor that offers more.
            if (modes[i].width < 1024 || modes[i].height < 576)
                continue;

            bool seen = false;
            for (size_t j = 0; j < resolutions.size(); j++)
            {
                if (resolutions[j].width == modes[i].width &&
                    resolutions[j].height == modes[i].height)
                {
                    seen = true;
                    break;
                }
            }

            // The same size is listed once per refresh rate and colour depth, so
            // the list is deduplicated down to distinct sizes.
            if (!seen)
            {
                Resolution resolution = { modes[i].width, modes[i].height };
                resolutions.push_back(resolution);
            }
        }

        std::sort(resolutions.begin(), resolutions.end(),
                  [](const Resolution& a, const Resolution& b)
                  {
                      if (a.width != b.width)
                          return a.width < b.width;
                      return a.height < b.height;
                  });
    }

    // The video mode nearest a requested size, for going fullscreen. Asking for a
    // size the display cannot show gives a black screen on some drivers rather
    // than an error.
    const GLFWvidmode* NearestVideoMode(GLFWmonitor* monitor, int width, int height)
    {
        int count = 0;
        const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);
        if (count <= 0)
            return glfwGetVideoMode(monitor);

        const GLFWvidmode* best = &modes[0];
        long bestError = -1;

        for (int i = 0; i < count; i++)
        {
            const long dw = modes[i].width - width;
            const long dh = modes[i].height - height;
            const long error = dw * dw + dh * dh;

            // Ties are broken by refresh rate, so a 60Hz and a 144Hz entry for
            // the same size resolves to the faster one.
            if (bestError < 0 || error < bestError ||
                (error == bestError && modes[i].refreshRate > best->refreshRate))
            {
                bestError = error;
                best = &modes[i];
            }
        }

        return best;
    }
}

bool InitLibrary()
{
    // glfwInit returns straight away if the library is already up, so this is
    // safe to call from both main and Create.
    return glfwInit() == GLFW_TRUE;
}

bool Create(int width, int height, Mode mode, bool vsync)
{
    if (!InitLibrary())
        return false;

    // No multisampling: it costs a sample per pixel for the silhouettes alone
    // and does nothing for the aliasing inside a texture, which in a world made
    // of textured cubes is nearly all of what is visible. Mipmaps deal with
    // that, and FXAA picks up the silhouettes afterwards for far less.
    glfwWindowHint(GLFW_SAMPLES, 0);

    // Still not resizable by dragging the frame -- the size is a setting rather
    // than something to fiddle with -- but the size itself is no longer fixed,
    // and a framebuffer callback keeps everything that depends on it in step.
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    displayWindow = glfwCreateWindow(width, height, "Minecraft", NULL, NULL);
    if (displayWindow == NULL)
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(displayWindow);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        return false;
    }

    glfwGetFramebufferSize(displayWindow, &framebufferWidth, &framebufferHeight);
    glfwSetFramebufferSizeCallback(displayWindow, OnFramebufferSize);

    windowedWidth = width;
    windowedHeight = height;

    BuildResolutionList();

    // Registers the GLFW callbacks, of which this project previously had none.
    // Until this runs there is no way to know that a key went down as opposed to
    // being down, which is the whole difference between opening a menu and
    // flickering one.
    Input::Init(displayWindow);

    SetVSync(vsync);
    Apply(mode, width, height);

    return true;
}

void Destroy()
{
    if (displayWindow)
        glfwDestroyWindow(displayWindow);

    displayWindow = nullptr;
    glfwTerminate();
}

GLFWwindow* Window()
{
    return displayWindow;
}

int Width()
{
    return framebufferWidth > 0 ? framebufferWidth : 1;
}

int Height()
{
    return framebufferHeight > 0 ? framebufferHeight : 1;
}

bool TakeSizeChange()
{
    // Asked for rather than waited for. The framebuffer callback is registered
    // and does fire for an ordinary resize, but switching to a borderless window
    // resized the window without one arriving in time -- so the viewport and the
    // offscreen buffer stayed at the old size and the world was drawn into the
    // bottom-left corner of a larger black window.
    //
    // Two integers a frame is nothing next to being wrong, and this cannot miss
    // a change however the size came about.
    if (displayWindow)
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(displayWindow, &width, &height);
        NoteSize(width, height);
    }

    const bool changed = sizeChanged;
    sizeChanged = false;
    return changed;
}

void Apply(Mode mode, int width, int height)
{
    if (!displayWindow)
        return;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* desktop = monitor ? glfwGetVideoMode(monitor) : NULL;

    // Remembered before the switch, so leaving fullscreen comes back to the size
    // the window had rather than to the size of the display.
    if (currentMode == Mode::Windowed && mode != Mode::Windowed)
    {
        windowedWidth = framebufferWidth;
        windowedHeight = framebufferHeight;
    }

    currentMode = mode;

    switch (mode)
    {
    case Mode::Fullscreen:
    {
        if (!monitor)
            break;

        const GLFWvidmode* target = NearestVideoMode(monitor, width, height);
        glfwSetWindowAttrib(displayWindow, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(displayWindow, monitor, 0, 0,
                             target->width, target->height, target->refreshRate);
        break;
    }

    case Mode::Borderless:
    {
        if (!desktop)
            break;

        // A frameless window the size of the desktop, rather than a real
        // fullscreen one. It never changes the display's video mode, so alt-
        // tabbing away is instant, which is the whole reason to prefer it -- and
        // it is why the resolution setting has no say here.
        glfwSetWindowAttrib(displayWindow, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(displayWindow, NULL, 0, 0,
                             desktop->width, desktop->height, 0);
        break;
    }

    case Mode::Windowed:
    default:
    {
        int x = 60, y = 60;
        if (desktop)
        {
            // Centred, and never with its title bar off the top of the screen.
            x = std::max(0, (desktop->width - width) / 2);
            y = std::max(30, (desktop->height - height) / 2);
        }

        glfwSetWindowAttrib(displayWindow, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(displayWindow, NULL, x, y, width, height, 0);
        break;
    }
    }

    // glfwSetWindowMonitor can rebind the context to a different surface, and
    // some drivers drop the swap interval when it does. Set again rather than
    // assumed to have survived.
    glfwSwapInterval(vsyncEnabled ? 1 : 0);
}

void SetVSync(bool on)
{
    vsyncEnabled = on;
    glfwSwapInterval(on ? 1 : 0);
}

void SetFrameCap(int fps)
{
    frameBudget = (fps > 0) ? (1.0 / (double)fps) : 0.0;
    nextFrameTime = 0.0;
}

void LimitFrameRate()
{
    if (frameBudget <= 0.0)
        return;

    const double now = glfwGetTime();

    // A first call, or a long stall such as sitting in the menu, restarts the
    // schedule rather than trying to catch up on frames that were never owed.
    if (nextFrameTime <= 0.0 || now > nextFrameTime + frameBudget)
        nextFrameTime = now;

    nextFrameTime += frameBudget;

    double remaining = nextFrameTime - glfwGetTime();
    if (remaining <= 0.0)
        return;

    // Slept for the bulk and spun for the last millisecond. Sleep on Windows is
    // only accurate to the scheduler's tick, which at 240 frames a second is
    // most of the budget; spinning the whole wait instead would burn a core.
    const double spin = 0.001;
    if (remaining > spin)
    {
        std::this_thread::sleep_for(
            std::chrono::duration<double>(remaining - spin));
    }

    while (glfwGetTime() < nextFrameTime)
        std::this_thread::yield();
}

int ResolutionCount()
{
    return (int)resolutions.size();
}

void ResolutionAt(int index, int& width, int& height)
{
    if (index < 0 || index >= (int)resolutions.size())
    {
        width = Width();
        height = Height();
        return;
    }

    width = resolutions[index].width;
    height = resolutions[index].height;
}

int ClosestResolution(int width, int height)
{
    int best = 0;
    long bestError = -1;

    for (size_t i = 0; i < resolutions.size(); i++)
    {
        const long dw = resolutions[i].width - width;
        const long dh = resolutions[i].height - height;
        const long error = dw * dw + dh * dh;

        if (bestError < 0 || error < bestError)
        {
            bestError = error;
            best = (int)i;
        }
    }

    return best;
}
}
