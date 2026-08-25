#include "headers/Input.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Input
{
namespace
{
    GLFWwindow* inputWindow = nullptr;

    Frame frame;

    bool capturing = false;
    Action captureAction = Action::Forward;

    Binding bindings[ACTION_COUNT];

    struct ActionInfo
    {
        const char* name; // shown in the menu
        const char* key;  // written to options.txt
        Device device;
        int code;
        bool rebindable;
    };

    // The single place an action is described. Adding one -- Jump, Attack,
    // Open Inventory -- is a line here and an enumerator, and the controls
    // screen picks it up without being touched.
    const ActionInfo ACTIONS[ACTION_COUNT] = {
        { "Move Forward", "forward", Device::Keyboard, GLFW_KEY_W,          true  },
        { "Move Back",    "back",    Device::Keyboard, GLFW_KEY_S,          true  },
        { "Strafe Left",  "left",    Device::Keyboard, GLFW_KEY_A,          true  },
        { "Strafe Right", "right",   Device::Keyboard, GLFW_KEY_D,          true  },
        { "Fly Up",       "up",      Device::Keyboard, GLFW_KEY_SPACE,      true  },
        { "Fly Down",     "down",    Device::Keyboard, GLFW_KEY_LEFT_SHIFT, true  },
        { "Pause",        "pause",   Device::Keyboard, GLFW_KEY_ESCAPE,     false },
        { "Debug Overlay","debug",   Device::Keyboard, GLFW_KEY_F3,         true  },
    };

    struct NamedKey
    {
        int key;
        const char* name;
    };

    // glfwGetKeyName gives the layout-correct name for keys that produce a
    // character, and NULL for everything else -- which is every key anyone
    // actually rebinds movement to. Those are named here.
    const NamedKey NAMED_KEYS[] = {
        { GLFW_KEY_SPACE,         "Space"        },
        { GLFW_KEY_ESCAPE,        "Escape"       },
        { GLFW_KEY_ENTER,         "Enter"        },
        { GLFW_KEY_TAB,           "Tab"          },
        { GLFW_KEY_BACKSPACE,     "Backspace"    },
        { GLFW_KEY_INSERT,        "Insert"       },
        { GLFW_KEY_DELETE,        "Delete"       },
        { GLFW_KEY_RIGHT,         "Right"        },
        { GLFW_KEY_LEFT,          "Left"         },
        { GLFW_KEY_DOWN,          "Down"         },
        { GLFW_KEY_UP,            "Up"           },
        { GLFW_KEY_PAGE_UP,       "Page Up"      },
        { GLFW_KEY_PAGE_DOWN,     "Page Down"    },
        { GLFW_KEY_HOME,          "Home"         },
        { GLFW_KEY_END,           "End"          },
        { GLFW_KEY_CAPS_LOCK,     "Caps Lock"    },
        { GLFW_KEY_SCROLL_LOCK,   "Scroll Lock"  },
        { GLFW_KEY_NUM_LOCK,      "Num Lock"     },
        { GLFW_KEY_PRINT_SCREEN,  "Print Screen" },
        { GLFW_KEY_PAUSE,         "Pause Key"    },
        { GLFW_KEY_LEFT_SHIFT,    "Left Shift"   },
        { GLFW_KEY_LEFT_CONTROL,  "Left Ctrl"    },
        { GLFW_KEY_LEFT_ALT,      "Left Alt"     },
        { GLFW_KEY_LEFT_SUPER,    "Left Super"   },
        { GLFW_KEY_RIGHT_SHIFT,   "Right Shift"  },
        { GLFW_KEY_RIGHT_CONTROL, "Right Ctrl"   },
        { GLFW_KEY_RIGHT_ALT,     "Right Alt"    },
        { GLFW_KEY_RIGHT_SUPER,   "Right Super"  },
        { GLFW_KEY_MENU,          "Menu"         },
        { GLFW_KEY_KP_DECIMAL,    "Keypad ."     },
        { GLFW_KEY_KP_DIVIDE,     "Keypad /"     },
        { GLFW_KEY_KP_MULTIPLY,   "Keypad *"     },
        { GLFW_KEY_KP_SUBTRACT,   "Keypad -"     },
        { GLFW_KEY_KP_ADD,        "Keypad +"     },
        { GLFW_KEY_KP_ENTER,      "Keypad Enter" },
        { GLFW_KEY_KP_EQUAL,      "Keypad ="     },
    };

    const int NAMED_KEY_COUNT = static_cast<int>(sizeof(NAMED_KEYS) / sizeof(NAMED_KEYS[0]));

    // F1-F25 and Keypad 0-9 are runs rather than one-offs, so they are formatted
    // rather than listed.
    bool formatRunKey(int key, char* out, size_t size)
    {
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
        {
            snprintf(out, size, "F%d", key - GLFW_KEY_F1 + 1);
            return true;
        }

        if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
        {
            snprintf(out, size, "Keypad %d", key - GLFW_KEY_KP_0);
            return true;
        }

        return false;
    }

    void OnKey(GLFWwindow*, int key, int, int action, int mods)
    {
        if (action != GLFW_PRESS)
            return;

        if (capturing)
        {
            // Escape is the way out of a rebind rather than something to bind,
            // which is also why the pause action cannot be lost by rebinding it
            // to nothing and then having no way back to the menu.
            if (key == GLFW_KEY_ESCAPE)
                CancelCapture();
            else
                SetBinding(captureAction, Binding{ Device::Keyboard, key });

            capturing = false;
            return;
        }

        KeyPress press;
        press.key = key;
        press.mods = mods;
        frame.keyPresses.push_back(press);
    }

    void OnMouseButton(GLFWwindow* window, int button, int action, int)
    {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(window, &x, &y);

        if (action == GLFW_PRESS && capturing)
        {
            SetBinding(captureAction, Binding{ Device::Mouse, button });
            capturing = false;
            return;
        }

        MousePress press;
        press.button = button;
        press.x = x;
        press.y = y;

        if (action == GLFW_PRESS)
            frame.mousePresses.push_back(press);
        else if (action == GLFW_RELEASE)
            frame.mouseReleases.push_back(press);
    }

    void OnScroll(GLFWwindow*, double, double y)
    {
        frame.scrollY += y;
    }

    void OnChar(GLFWwindow*, unsigned int codepoint)
    {
        frame.characters.push_back(codepoint);
    }
}

void Init(GLFWwindow* window)
{
    inputWindow = window;

    ResetBindings();

    glfwSetKeyCallback(window, OnKey);
    glfwSetMouseButtonCallback(window, OnMouseButton);
    glfwSetScrollCallback(window, OnScroll);
    glfwSetCharCallback(window, OnChar);
}

void BeginFrame()
{
    frame.keyPresses.clear();
    frame.mousePresses.clear();
    frame.mouseReleases.clear();
    frame.characters.clear();
    frame.scrollY = 0.0;
}

void CursorPosition(double& x, double& y)
{
    x = 0.0;
    y = 0.0;

    if (inputWindow)
        glfwGetCursorPos(inputWindow, &x, &y);
}

bool MouseHeld()
{
    return inputWindow &&
           glfwGetMouseButton(inputWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

const Frame& CurrentFrame()
{
    return frame;
}

bool IsActionDown(Action action)
{
    if (!inputWindow)
        return false;

    const Binding binding = bindings[static_cast<int>(action)];
    if (!binding.Bound())
        return false;

    if (binding.device == Device::Mouse)
        return glfwGetMouseButton(inputWindow, binding.code) == GLFW_PRESS;

    return glfwGetKey(inputWindow, binding.code) == GLFW_PRESS;
}

bool ActionPressed(Action action)
{
    const Binding binding = bindings[static_cast<int>(action)];
    if (!binding.Bound())
        return false;

    if (binding.device == Device::Mouse)
    {
        for (size_t i = 0; i < frame.mousePresses.size(); i++)
        {
            if (frame.mousePresses[i].button == binding.code)
                return true;
        }

        return false;
    }

    for (size_t i = 0; i < frame.keyPresses.size(); i++)
    {
        if (frame.keyPresses[i].key == binding.code)
            return true;
    }

    return false;
}

Binding GetBinding(Action action)
{
    return bindings[static_cast<int>(action)];
}

void SetBinding(Action action, Binding binding)
{
    // One key, one action. Leaving the previous holder bound would make a
    // single press both walk forwards and open the menu, and nothing in the
    // menu would show why.
    if (binding.Bound())
    {
        for (int i = 0; i < ACTION_COUNT; i++)
        {
            if (i == static_cast<int>(action) || !(bindings[i] == binding))
                continue;

            // A fixed action keeps its key and the new binding is refused, so
            // nothing can take Escape away from the menu.
            if (!ACTIONS[i].rebindable)
                return;

            bindings[i] = Binding{ Device::Keyboard, -1 };
        }
    }

    bindings[static_cast<int>(action)] = binding;
}

void ResetBindings()
{
    for (int i = 0; i < ACTION_COUNT; i++)
        bindings[i] = Binding{ ACTIONS[i].device, ACTIONS[i].code };
}

Binding DefaultBinding(Action action)
{
    const ActionInfo& info = ACTIONS[static_cast<int>(action)];
    return Binding{ info.device, info.code };
}

const char* ActionName(Action action)
{
    return ACTIONS[static_cast<int>(action)].name;
}

const char* ActionKey(Action action)
{
    return ACTIONS[static_cast<int>(action)].key;
}

bool Rebindable(Action action)
{
    return ACTIONS[static_cast<int>(action)].rebindable;
}

std::string BindingName(Binding binding)
{
    if (!binding.Bound())
        return "---";

    if (binding.device == Device::Mouse)
    {
        char text[32];
        snprintf(text, sizeof(text), "Mouse %d", binding.code + 1);
        return text;
    }

    for (int i = 0; i < NAMED_KEY_COUNT; i++)
    {
        if (NAMED_KEYS[i].key == binding.code)
            return NAMED_KEYS[i].name;
    }

    char run[32];
    if (formatRunKey(binding.code, run, sizeof(run)))
        return run;

    // Layout-aware, so a French keyboard shows A where a US one shows Q.
    const char* name = glfwGetKeyName(binding.code, 0);
    if (name && name[0])
    {
        std::string upper(name);
        for (size_t i = 0; i < upper.size(); i++)
            upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(upper[i])));
        return upper;
    }

    char fallback[32];
    snprintf(fallback, sizeof(fallback), "Key %d", binding.code);
    return fallback;
}

bool ParseBinding(const std::string& text, Binding& out)
{
    if (text.empty() || text == "---")
    {
        out = Binding{ Device::Keyboard, -1 };
        return true;
    }

    int button = 0;
    if (sscanf(text.c_str(), "Mouse %d", &button) == 1)
    {
        out = Binding{ Device::Mouse, button - 1 };
        return true;
    }

    int raw = 0;
    if (sscanf(text.c_str(), "Key %d", &raw) == 1)
    {
        out = Binding{ Device::Keyboard, raw };
        return true;
    }

    for (int i = 0; i < NAMED_KEY_COUNT; i++)
    {
        if (text == NAMED_KEYS[i].name)
        {
            out = Binding{ Device::Keyboard, NAMED_KEYS[i].key };
            return true;
        }
    }

    // No reverse of glfwGetKeyName exists, so the printable range is walked and
    // named until one matches. It runs once per binding at startup.
    char run[32];
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++)
    {
        if (formatRunKey(key, run, sizeof(run)) && text == run)
        {
            out = Binding{ Device::Keyboard, key };
            return true;
        }

        const char* name = glfwGetKeyName(key, 0);
        if (!name || !name[0])
            continue;

        std::string upper(name);
        for (size_t i = 0; i < upper.size(); i++)
            upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(upper[i])));

        if (text == upper)
        {
            out = Binding{ Device::Keyboard, key };
            return true;
        }
    }

    return false;
}

void BeginCapture(Action action)
{
    capturing = true;
    captureAction = action;
}

void CancelCapture()
{
    capturing = false;
}

bool Capturing()
{
    return capturing;
}

Action CapturingAction()
{
    return captureAction;
}
}
