#pragma once

#include <string>
#include <vector>

struct GLFWwindow;

// Input used to be read in one way only: glfwGetKey, against a literal key
// constant, in the middle of the movement code. That works for a fixed set of
// keys and nothing else. A menu needs two things that cannot give:
//
//   - the key that is bound to an action, rather than the key itself, so a
//     binding can be changed without touching the code that moves the player;
//   - the *moment* a key went down, rather than whether it is down now. Escape
//     polled as a level opens and closes the pause menu on every frame it is
//     held.
//
// So actions and bindings live here, and the GLFW callbacks -- of which this
// project previously registered none -- turn presses into a per-frame event
// list the UI reads. Gameplay keeps polling, because for held movement that is
// still the right shape.
namespace Input
{
    enum class Action
    {
        Forward,
        Back,
        Left,
        Right,
        Up,
        Down,
        Pause,
        ToggleDebug,
        Count
    };

    const int ACTION_COUNT = static_cast<int>(Action::Count);

    enum class Device
    {
        Keyboard,
        Mouse
    };

    // A mouse binding costs almost nothing to carry and is what breaking and
    // placing blocks will want, so the distinction is here from the start
    // rather than retrofitted through every binding site later.
    struct Binding
    {
        Device device = Device::Keyboard;
        int code = -1; // GLFW_KEY_UNKNOWN, or a GLFW mouse button

        bool Bound() const { return code >= 0; }
        bool operator==(const Binding& other) const
        {
            return device == other.device && code == other.code;
        }
    };

    struct KeyPress
    {
        int key = 0;
        int mods = 0;
    };

    struct MousePress
    {
        int button = 0;
        double x = 0.0;
        double y = 0.0;
    };

    // Everything that *happened* between one glfwPollEvents and the next. Cleared
    // by BeginFrame, filled by the callbacks.
    //
    // Deliberately events only. Where the pointer is and whether a button is
    // held are state, not events, and both are read straight from GLFW at the
    // point of use -- after glfwPollEvents, which is when GLFW's own copy is
    // current. Caching them here got both wrong in turn: sampled before the
    // poll, a press was still reading as "up" on the frame it arrived, so a
    // slider began and cancelled its drag in the same frame; left to the
    // callbacks, the position simply stopped updating in cases where no motion
    // event was delivered, and every button went dead.
    struct Frame
    {
        std::vector<KeyPress> keyPresses;
        std::vector<MousePress> mousePresses;
        std::vector<MousePress> mouseReleases;
        std::vector<unsigned int> characters;

        double scrollY = 0.0;
    };

    void Init(GLFWwindow* window);

    // Call immediately before glfwPollEvents, so the callbacks fill an empty list.
    void BeginFrame();
    const Frame& CurrentFrame();

    // Held state, for movement. Resolves the binding and polls it.
    bool IsActionDown(Action action);

    // Went down this frame. Opening a menu needs this and not the above: Escape
    // polled as a level opens and closes the pause screen on every frame it is
    // held down, which reads as the menu flickering.
    bool ActionPressed(Action action);

    // The pointer, in window pixels, and whether the left button is held. Read
    // from GLFW rather than from a cached copy; see the note on Frame. Only
    // meaningful once the cursor has been released, which is to say while a
    // menu is open.
    void CursorPosition(double& x, double& y);
    bool MouseHeld();

    Binding GetBinding(Action action);

    // Any other action holding this binding is unbound, so one key can never
    // drive two things at once.
    void SetBinding(Action action, Binding binding);

    void ResetBindings();
    Binding DefaultBinding(Action action);

    const char* ActionName(Action action); // "Move Forward", for the menu
    const char* ActionKey(Action action);  // "forward", for options.txt

    // False for Pause. Rebinding it is the one change that can lock the player
    // out of the menu that would let them change it back, so Escape is fixed --
    // which is also what Minecraft does with it.
    bool Rebindable(Action action);

    // "W", "Left Shift", "Mouse 1". glfwGetKeyName covers the printable keys
    // and returns nothing for the rest, so the rest are named by a table.
    std::string BindingName(Binding binding);

    // Parses what BindingName produced, for reading options.txt back.
    bool ParseBinding(const std::string& text, Binding& out);

    // While capturing, the next key or mouse button pressed is bound to the
    // action instead of being dispatched. Escape cancels.
    void BeginCapture(Action action);
    void CancelCapture();
    bool Capturing();
    Action CapturingAction();
}
