#include "headers/Screens.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "headers/Controls.hpp"
#include "headers/Display.hpp"
#include "headers/Input.hpp"
#include "headers/Object.hpp"
#include "headers/Settings.hpp"

namespace
{
    const float TITLE_Y = 46.0f;
    const float FIRST_ROW_Y = 110.0f;

    std::string formatInt(const char* label, float value, const char* suffix = "")
    {
        char text[96];
        snprintf(text, sizeof(text), "%s: %d%s", label, (int)(value + 0.5f), suffix);
        return text;
    }

    // Anisotropy is offered as the powers of two the driver will honour, so a
    // machine that stops at four never shows a sixteen that quietly does four.
    std::vector<std::string> anisotropyOptions(std::vector<int>& levels)
    {
        std::vector<std::string> options;

        levels.clear();
        levels.push_back(1);
        options.push_back("Off");

        const int limit = std::max(1, MaxTerrainAnisotropy());
        for (int level = 2; level <= limit; level *= 2)
        {
            levels.push_back(level);

            char text[16];
            snprintf(text, sizeof(text), "%dx", level);
            options.push_back(text);
        }

        return options;
    }

    // The frame cap slider runs in tens, with one step past the top standing for
    // no limit at all. A slider rather than a toggle because twenty-two options
    // is a long thing to click through.
    const float FRAME_CAP_MIN = 30.0f;
    const float FRAME_CAP_MAX = 240.0f;
    const float FRAME_CAP_UNLIMITED = 250.0f;

    float frameCapToSlider(int cap)
    {
        return (cap <= 0) ? FRAME_CAP_UNLIMITED : (float)cap;
    }

    int sliderToFrameCap(float value)
    {
        return (value >= FRAME_CAP_UNLIMITED) ? 0 : (int)(value + 0.5f);
    }

    std::vector<std::string> resolutionOptions(std::vector<int>& widths, std::vector<int>& heights)
    {
        std::vector<std::string> options;

        widths.clear();
        heights.clear();

        for (int i = 0; i < Display::ResolutionCount(); i++)
        {
            int width = 0, height = 0;
            Display::ResolutionAt(i, width, height);

            widths.push_back(width);
            heights.push_back(height);

            char text[32];
            snprintf(text, sizeof(text), "%dx%d", width, height);
            options.push_back(text);
        }

        // A monitor that reports nothing usable still needs one entry, or the
        // toggle has nothing to show and nothing to cycle.
        if (options.empty())
        {
            widths.push_back(Display::Width());
            heights.push_back(Display::Height());

            char text[32];
            snprintf(text, sizeof(text), "%dx%d", Display::Width(), Display::Height());
            options.push_back(text);
        }

        return options;
    }

    int indexOfLevel(const std::vector<int>& levels, int level)
    {
        for (size_t i = 0; i < levels.size(); i++)
        {
            if (levels[i] == level)
                return (int)i;
        }

        return 0;
    }

    // One row of the controls list: the action on the left, the key it is bound
    // to in a box on the right. Its own widget rather than a Label beside a
    // Button, because the two halves have to agree about the row they share and
    // only the box is clickable.
    class BindingRow : public Widget
    {
    public:
        BindingRow(Input::Action action, float boxWidth)
            : _action(action), _boxWidth(boxWidth)
        {
        }

        void Update(const UIContext& context) override
        {
            _hovered = _rebindable && BoxContains(context.mouseX, context.mouseY);

            if (_hovered && context.mousePressed)
                Input::BeginCapture(_action);
        }

        void Render(UIRenderer& ui) const override
        {
            const float textY = _y + (_h - ui.LineHeight(UI::TEXT_SCALE)) * 0.5f;

            ui.DrawTextShadowed(_x, textY, UI::TEXT_SCALE,
                                _rebindable ? UI::TEXT : UI::TEXT_DIM,
                                Input::ActionName(_action));

            const float boxX = _x + _w - _boxWidth;

            const bool capturing = Input::Capturing() && Input::CapturingAction() == _action;
            const Input::Binding binding = Input::GetBinding(_action);

            ui.DrawRect(boxX, _y, _boxWidth, _h, _hovered ? UI::FILL_HOVER : UI::FILL);
            ui.DrawRectOutline(boxX, _y, _boxWidth, _h, UI::BORDER_WIDTH,
                               _hovered || capturing ? UI::BORDER_HOVER : UI::BORDER);

            std::string text;
            Colour colour = UI::TEXT;

            if (capturing)
            {
                text = "> ? <";
            }
            else if (!binding.Bound())
            {
                // Left behind when its key was taken by something else. Shown
                // rather than silently blank, because an action that does
                // nothing and looks normal is the worst of both.
                text = "---";
                colour = UI::TEXT_WARNING;
            }
            else
            {
                text = Input::BindingName(binding);
                if (!_rebindable)
                    colour = UI::TEXT_DIM;
            }

            const float boxTextY = _y + (_h - ui.LineHeight(UI::TEXT_SCALE)) * 0.5f;
            ui.DrawTextCentredShadowed(boxX + _boxWidth * 0.5f, boxTextY, UI::TEXT_SCALE, colour, text);
        }

        void SetRebindable(bool rebindable) { _rebindable = rebindable; }

    private:
        bool BoxContains(float x, float y) const
        {
            const float boxX = _x + _w - _boxWidth;
            return x >= boxX && x <= _x + _w && y >= _y && y <= _y + _h;
        }

        Input::Action _action;
        float _boxWidth;
        bool _rebindable = true;
    };
}

// --- Screen ---

void Screen::Update(const UIContext& context)
{
    for (size_t i = 0; i < _widgets.size(); i++)
        _widgets[i]->Update(context);
}

void Screen::Render(UIRenderer& ui) const
{
    ui.DrawTextCentredShadowed(ui.Width() * 0.5f, TITLE_Y, UI::TITLE_SCALE, UI::TEXT, Title());

    for (size_t i = 0; i < _widgets.size(); i++)
        _widgets[i]->Render(ui);
}

float Screen::LayoutColumn(float, float height, int rows) const
{
    const float span = rows * UI::ROW_HEIGHT + (rows - 1) * UI::ROW_GAP;

    // Centred in what is left under the title where the column is short, and
    // pinned under it where the column is tall enough to need the room.
    const float centred = (height - span) * 0.5f;
    return std::max(FIRST_ROW_Y, centred);
}

// --- MenuSystem ---

void MenuSystem::Push(std::unique_ptr<Screen> screen)
{
    _pending = Transition::Push;
    _pendingScreen = std::move(screen);
}

void MenuSystem::Pop()
{
    _pending = Transition::Pop;
}

void MenuSystem::CloseAll()
{
    _pending = Transition::CloseAll;
}

void MenuSystem::RequestRenderDistance(int chunks)
{
    _requestedRenderDistance = chunks;
    _hasRenderDistanceRequest = true;
}

bool MenuSystem::TakeRenderDistanceRequest(int& chunks)
{
    if (!_hasRenderDistanceRequest)
        return false;

    chunks = _requestedRenderDistance;
    _hasRenderDistanceRequest = false;
    return true;
}

void MenuSystem::RequestMeshRebuild()
{
    _hasMeshRebuildRequest = true;
}

bool MenuSystem::TakeMeshRebuildRequest()
{
    const bool requested = _hasMeshRebuildRequest;
    _hasMeshRebuildRequest = false;
    return requested;
}

void MenuSystem::ApplyPendingTransition(GLFWwindow* menuWindow)
{
    switch (_pending)
    {
    case Transition::None:
        break;

    case Transition::Push:
        if (_pendingScreen)
            _screens.push_back(std::move(_pendingScreen));
        break;

    case Transition::Pop:
        if (!_screens.empty())
        {
            _screens.back()->OnClose();
            _screens.pop_back();
        }
        break;

    case Transition::CloseAll:
        while (!_screens.empty())
        {
            _screens.back()->OnClose();
            _screens.pop_back();
        }
        break;
    }

    _pending = Transition::None;
    _pendingScreen.reset();

    const bool shouldRelease = !_screens.empty();
    if (shouldRelease == _cursorReleased)
        return;

    _cursorReleased = shouldRelease;

    if (shouldRelease)
    {
        glfwSetInputMode(menuWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Put the pointer somewhere the player can see it. Coming out of mouse
        // look it is wherever the last recentring left it, which under a
        // disabled cursor is not a position that means anything on screen.
        int windowWidth = 0, windowHeight = 0;
        glfwGetWindowSize(menuWindow, &windowWidth, &windowHeight);
        glfwSetCursorPos(menuWindow, windowWidth / 2.0, windowHeight / 2.0);
    }
    else
    {
        glfwSetInputMode(menuWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // The camera has to be told the clock and the cursor both jumped, or the
        // first frame back moves the player by the whole time spent in the menu
        // and snaps the view to wherever the mouse was left.
        notifyResumed();

        SaveSettings();
    }
}

void MenuSystem::Update(GLFWwindow* menuWindow)
{
    if (!_ui)
        return;

    const Input::Frame& input = Input::CurrentFrame();

    // A rebind swallows the key it captures, so nothing here sees it and the
    // menu does not act on the press that was meant for the binding.
    if (!Input::Capturing() && Input::ActionPressed(Input::Action::Pause))
    {
        if (_screens.empty())
            Push(std::unique_ptr<Screen>(new PauseScreen(*this)));
        else
            Pop();
    }

    if (!_screens.empty() && _pending == Transition::None)
    {
        UIContext context;

        // Straight from GLFW, which this is downstream of glfwPollEvents and so
        // sees the current values for. Both were cached on the Frame once and
        // both went stale in ways that were hard to see: see the note on
        // Input::Frame.
        double cursorX = 0.0, cursorY = 0.0;
        Input::CursorPosition(cursorX, cursorY);

        _ui->CursorToUI(cursorX, cursorY, context.mouseX, context.mouseY);
        context.mouseDown = Input::MouseHeld();

        for (size_t i = 0; i < input.mousePresses.size(); i++)
            context.mousePressed |= input.mousePresses[i].button == GLFW_MOUSE_BUTTON_LEFT;

        for (size_t i = 0; i < input.mouseReleases.size(); i++)
            context.mouseReleased |= input.mouseReleases[i].button == GLFW_MOUSE_BUTTON_LEFT;

        // Laid out every frame rather than once at construction, so the column
        // stays centred if the window ever stops being a fixed size.
        Screen& top = *_screens.back();
        top.Layout(_ui->Width(), _ui->Height());

        // Only the top screen takes input. The ones underneath are covered by it.
        top.Update(context);
    }

    ApplyPendingTransition(menuWindow);
}

void MenuSystem::Render()
{
    if (!_ui || _screens.empty())
        return;

    // A screen pushed during this frame's Update has not been laid out yet, so
    // it is done here rather than only in Update.
    _screens.back()->Layout(_ui->Width(), _ui->Height());

    // The world stays visible behind the menu, dimmed so the text reads over it.
    _ui->DrawRect(0.0f, 0.0f, _ui->Width(), _ui->Height(), UI::PANEL);

    _screens.back()->Render(*_ui);
}

// --- PauseScreen ---

PauseScreen::PauseScreen(MenuSystem& menu)
    : Screen(menu)
{
    // By pointer, and by value into the lambda. Capturing a reference to a local
    // alias of _menu would leave every button holding a reference to a variable
    // that died with this constructor.
    MenuSystem* owner = &_menu;

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Back to Game",
        [owner]() { owner->CloseAll(); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Settings",
        [owner]() { owner->Push(std::unique_ptr<Screen>(new SettingsScreen(*owner))); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Quit to Desktop",
        []() { glfwSetWindowShouldClose(Display::Window(), GLFW_TRUE); })));
}

void PauseScreen::Layout(float width, float height)
{
    const float x = (width - UI::WIDE_WIDTH) * 0.5f;
    float y = LayoutColumn(width, height, (int)_widgets.size());

    for (size_t i = 0; i < _widgets.size(); i++)
    {
        _widgets[i]->SetBounds(x, y, UI::WIDE_WIDTH, UI::ROW_HEIGHT);
        y += UI::ROW_HEIGHT + UI::ROW_GAP;
    }
}

// --- SettingsScreen ---

SettingsScreen::SettingsScreen(MenuSystem& menu)
    : Screen(menu)
{
    MenuSystem* owner = &_menu;
    Settings& options = settings();

    // The field of view lives here rather than under Graphics. It is how the
    // world is looked at rather than how it is drawn, and it is the one setting
    // people reach for often enough to want at the top level.
    _widgets.push_back(std::unique_ptr<Widget>(new Slider(
        options.fov, 30.0f, 110.0f, 1.0f,
        [](float value) { return formatInt("Field of View", value); },
        [](float value) { settings().fov = value; ApplyGeneralSettings(); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Slider(
        options.mouseSensitivity, 0.1f, 3.0f, 0.05f,
        [](float value) { return formatInt("Sensitivity", value * 100.0f, "%"); },
        [](float value) { settings().mouseSensitivity = value; ApplyGeneralSettings(); })));

    std::vector<std::string> onOff;
    onOff.push_back("Off");
    onOff.push_back("On");

    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "Invert Mouse Y", onOff, options.invertMouseY ? 1 : 0,
        [](int index) { settings().invertMouseY = index == 1; ApplyGeneralSettings(); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Controls...",
        [owner]() { owner->Push(std::unique_ptr<Screen>(new ControlsScreen(*owner))); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Graphics...",
        [owner]() { owner->Push(std::unique_ptr<Screen>(new GraphicsScreen(*owner))); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Done",
        [owner]() { owner->Pop(); })));
}

void SettingsScreen::Layout(float width, float height)
{
    const float x = (width - UI::WIDE_WIDTH) * 0.5f;
    float y = LayoutColumn(width, height, (int)_widgets.size());

    for (size_t i = 0; i < _widgets.size(); i++)
    {
        _widgets[i]->SetBounds(x, y, UI::WIDE_WIDTH, UI::ROW_HEIGHT);
        y += UI::ROW_HEIGHT + UI::ROW_GAP;
    }
}

// --- ControlsScreen ---

ControlsScreen::ControlsScreen(MenuSystem& menu)
    : Screen(menu)
{
    MenuSystem* owner = &_menu;

    // Built from the action table rather than listed here, so an action added
    // to Input shows up in this list without this file being touched.
    for (int i = 0; i < Input::ACTION_COUNT; i++)
    {
        Input::Action action = static_cast<Input::Action>(i);

        BindingRow* row = new BindingRow(action, UI::HALF_WIDTH);
        row->SetRebindable(Input::Rebindable(action));
        _widgets.push_back(std::unique_ptr<Widget>(row));
    }

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Reset to Defaults",
        []() { Input::ResetBindings(); })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Done",
        [owner]() { owner->Pop(); })));
}

void ControlsScreen::Layout(float width, float height)
{
    // The rows are wider than a button column, because each carries a name on
    // one side and a key box on the other.
    const float rowWidth = UI::WIDE_WIDTH + UI::HALF_WIDTH;
    const float x = (width - rowWidth) * 0.5f;

    float y = LayoutColumn(width, height, (int)_widgets.size());

    for (size_t i = 0; i < _widgets.size(); i++)
    {
        const bool isButton = i >= _widgets.size() - 2;

        if (isButton)
            _widgets[i]->SetBounds((width - UI::WIDE_WIDTH) * 0.5f, y, UI::WIDE_WIDTH, UI::ROW_HEIGHT);
        else
            _widgets[i]->SetBounds(x, y, rowWidth, UI::ROW_HEIGHT);

        y += UI::ROW_HEIGHT + UI::ROW_GAP;
    }
}

void ControlsScreen::OnClose()
{
    // Leaving the page mid-rebind would otherwise leave the next key press
    // captured by a row that is no longer on screen.
    Input::CancelCapture();
}

// --- GraphicsScreen ---

GraphicsScreen::GraphicsScreen(MenuSystem& menu)
    : Screen(menu)
{
    MenuSystem* owner = &_menu;
    Settings& options = settings();

    _renderDistanceOnEntry = options.renderDistance;
    _renderDistance = options.renderDistance;

    _windowModeOnEntry = options.windowMode;
    _resolutionWidthOnEntry = options.resolutionWidth;
    _resolutionHeightOnEntry = options.resolutionHeight;
    _fancyOnEntry = options.fancyGraphics;

    // Written into the settings as it moves so the label and the file agree, but
    // not handed to the world until the page is left. See OnClose.
    int* pending = &_renderDistance;
    _widgets.push_back(std::unique_ptr<Widget>(new Slider(
        (float)options.renderDistance, 4.0f, 96.0f, 1.0f,
        [](float value) { return formatInt("Render Distance", value, " chunks"); },
        [pending](float value)
        {
            *pending = (int)(value + 0.5f);
            settings().renderDistance = *pending;
        })));

    _widgets.push_back(std::unique_ptr<Widget>(new Slider(
        options.foliageDistance, 0.0f, 96.0f, 1.0f,
        [](float value) { return formatInt("Foliage Distance", value, " chunks"); },
        [](float value) { settings().foliageDistance = value; })));

    std::vector<std::string> onOff;
    onOff.push_back("Off");
    onOff.push_back("On");

    std::vector<std::string> fastFancy;
    fastFancy.push_back("Fast");
    fastFancy.push_back("Fancy");

    // Recorded now, acted on in OnClose. Fast and fancy differ in which faces
    // are meshed at all, so switching means rebuilding every chunk -- not
    // something to do on each click while cycling the toggle.
    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "Graphics", fastFancy, options.fancyGraphics ? 1 : 0,
        [](int index) { settings().fancyGraphics = index == 1; })));

    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "Antialiasing", onOff, options.fxaa ? 1 : 0,
        [](int index) { settings().fxaa = index == 1; })));

    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "VSync", onOff, options.vsync ? 1 : 0,
        [](int index) { settings().vsync = index == 1; ApplyGraphicsSettings(); })));

    // The levels are captured by value into the callback so the list the labels
    // were built from is the same list the choice is read back out of.
    std::vector<int> levels;
    std::vector<std::string> anisotropy = anisotropyOptions(levels);

    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "Anisotropic Filtering", anisotropy, indexOfLevel(levels, options.anisotropy),
        [levels](int index)
        {
            settings().anisotropy = levels[(size_t)index];
            ApplyGraphicsSettings();
        })));

    _widgets.push_back(std::unique_ptr<Widget>(new Slider(
        frameCapToSlider(options.frameCap), FRAME_CAP_MIN, FRAME_CAP_UNLIMITED, 10.0f,
        [](float value)
        {
            if (value >= FRAME_CAP_UNLIMITED)
                return std::string("Max Framerate: Unlimited");

            return formatInt("Max Framerate", value, " fps");
        },
        [](float value)
        {
            settings().frameCap = sliderToFrameCap(value);
            ApplyGraphicsSettings();
        })));

    std::vector<std::string> windowModes;
    windowModes.push_back("Windowed");
    windowModes.push_back("Borderless");
    windowModes.push_back("Fullscreen");

    // The window mode and the resolution are recorded here and applied together
    // in OnClose. Cycling a toggle resizes the window on every click otherwise,
    // and getting from the first entry to the one you want means watching the
    // whole list happen to your desktop on the way.
    int* pendingMode = &_windowMode;
    _windowMode = options.windowMode;

    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "Window Mode", windowModes, options.windowMode,
        [pendingMode](int index)
        {
            *pendingMode = index;
            settings().windowMode = index;
        })));

    std::vector<int> widths, heights;
    std::vector<std::string> sizes = resolutionOptions(widths, heights);

    _resolutionIndex = Display::ClosestResolution(options.resolutionWidth,
                                                  options.resolutionHeight);
    int* pendingResolution = &_resolutionIndex;

    _widgets.push_back(std::unique_ptr<Widget>(new Toggle(
        "Resolution", sizes, _resolutionIndex,
        [pendingResolution, widths, heights](int index)
        {
            *pendingResolution = index;
            settings().resolutionWidth = widths[(size_t)index];
            settings().resolutionHeight = heights[(size_t)index];
        })));

    _widgets.push_back(std::unique_ptr<Widget>(new Button("Done",
        [owner]() { owner->Pop(); })));
}

void GraphicsScreen::Layout(float width, float height)
{
    const float x = (width - UI::WIDE_WIDTH) * 0.5f;
    float y = LayoutColumn(width, height, (int)_widgets.size());

    for (size_t i = 0; i < _widgets.size(); i++)
    {
        _widgets[i]->SetBounds(x, y, UI::WIDE_WIDTH, UI::ROW_HEIGHT);
        y += UI::ROW_HEIGHT + UI::ROW_GAP;
    }
}

void GraphicsScreen::OnClose()
{
    if (_renderDistance != _renderDistanceOnEntry)
        _menu.RequestRenderDistance(_renderDistance);

    if (settings().fancyGraphics != _fancyOnEntry)
        _menu.RequestMeshRebuild();

    const bool displayChanged =
        settings().windowMode != _windowModeOnEntry ||
        settings().resolutionWidth != _resolutionWidthOnEntry ||
        settings().resolutionHeight != _resolutionHeightOnEntry;

    // Resizes the window and, in fullscreen, may change the display's video
    // mode. main notices the new framebuffer size on the next frame and rebuilds
    // the offscreen buffer to match.
    if (displayChanged)
        ApplyDisplaySettings();
}
