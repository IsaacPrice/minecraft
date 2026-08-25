#pragma once

struct GLFWwindow;

// The window, and everything about how the world is presented in it: the mode it
// is shown in, the resolution it is drawn at, whether frames wait for the
// display, and how many of them a second there may be.
//
// This used to be a pair of constants -- `extern const int width, height` in
// main.cpp -- and a non-resizable window, because the offscreen buffer, the
// viewport and the projection's aspect ratio were each built once from those two
// numbers and nothing was wired up to rebuild them. Everything that could change
// the window's size therefore had to be forbidden. Gathering the size here, with
// a framebuffer callback behind it, is what lets it change at all.
namespace Display
{
    enum class Mode
    {
        Windowed = 0,
        Borderless = 1,  // no frame, always at the monitor's own resolution
        Fullscreen = 2   // takes the display, and may change its video mode
    };

    // Brings GLFW up without opening anything. Needed on its own because the
    // settings have to be read before the window can be created -- otherwise it
    // opens at a default size and visibly jumps to the saved one -- and reading
    // a keybind back means naming it through glfwGetKeyName, which needs the
    // library running. Calling it twice is harmless.
    bool InitLibrary();

    // Creates the window and the GL context, loads the function pointers, and
    // registers the input callbacks. False if any of that failed.
    bool Create(int width, int height, Mode mode, bool vsync);

    void Destroy();

    GLFWwindow* Window();

    // The framebuffer, which is what the viewport, the offscreen buffer and the
    // aspect ratio are all measured in. On Windows this matches the window's
    // client area; keeping the distinction means a scaled display would not
    // silently render at the wrong size.
    int Width();
    int Height();

    // True once per change. main uses it to rebuild the offscreen buffer, which
    // is the one thing sized in pixels that nothing else owns.
    bool TakeSizeChange();

    // Applies a mode and a size together, because the two interact: borderless
    // ignores the size and takes the monitor's, and fullscreen has to pick the
    // video mode nearest what was asked for.
    void Apply(Mode mode, int width, int height);

    void SetVSync(bool on);

    // Frames a second, or 0 for no limit. Separate from vsync: vsync pins the
    // rate to the display and stops tearing, a cap holds it below that to leave
    // the machine alone.
    void SetFrameCap(int fps);

    // Waits out whatever is left of this frame's budget. Called after the swap,
    // where the frame has actually finished.
    void LimitFrameRate();

    // The distinct sizes the monitor reports, largest last, filtered to those big
    // enough to lay a menu out in. Offered as a list rather than free numbers so
    // the setting cannot ask for a size the display cannot show.
    int ResolutionCount();
    void ResolutionAt(int index, int& width, int& height);
    int ClosestResolution(int width, int height);
}
