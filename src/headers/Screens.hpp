#pragma once

#include <memory>
#include <vector>

#include "UIRenderer.hpp"
#include "Widgets.hpp"

struct GLFWwindow;

class MenuSystem;

// One page of the menu. Screens are stacked rather than swapped, so Escape backs
// out one level at a time and the screen underneath is still there to return to.
class Screen
{
public:
    explicit Screen(MenuSystem& menu) : _menu(menu) {}
    virtual ~Screen() = default;

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    virtual const char* Title() const = 0;

    // Called every frame before Update, so the layout follows the window rather
    // than being fixed at the size it happened to be built at.
    virtual void Layout(float width, float height) = 0;

    virtual void Update(const UIContext& context);
    virtual void Render(UIRenderer& ui) const;

    // Run just before the screen is destroyed. Where a setting is expensive
    // enough that it should be applied on leaving rather than on every change,
    // this is where that happens.
    virtual void OnClose() {}

protected:
    // The usual arrangement: a title, then a centred column of full width rows.
    // Returns the y of the first row.
    float LayoutColumn(float width, float height, int rows) const;

    MenuSystem& _menu;
    std::vector<std::unique_ptr<Widget>> _widgets;
};

// Owns the screen stack, the renderer, and the pause state.
//
// Stack changes are deferred to the end of the frame rather than applied where
// they are asked for. A screen's button is clicked from inside that screen's own
// Update, so popping it there would destroy the object whose method is running;
// and a screen pushed mid-frame would be handed the very click that opened it
// and act on whatever sits at the same place on the new page.
class MenuSystem
{
public:
    // The renderer is main's, not the menu's. The debug overlay draws into the
    // same batch, and a HUD is not a modal screen -- so whoever owns the frame
    // owns the batch, and both of these are guests in it.
    void Create(UIRenderer& ui) { _ui = &ui; }

    bool Paused() const { return !_screens.empty(); }

    // Handles the pause key, routes the mouse to the top screen, and applies any
    // stack change afterwards. Also owns the cursor mode, because releasing and
    // recapturing the pointer is exactly the pause transition. Call after the
    // batch has been begun, because hit testing is done in its canvas units.
    void Update(GLFWwindow* menuWindow);

    // Queues the current screen. Draws nothing when nothing is open, and does
    // not flush -- the caller flushes once, after the HUD has had its turn.
    void Render();

    void Push(std::unique_ptr<Screen> screen);
    void Pop();
    void CloseAll();

    // The graphics screen asks for this on the way out rather than as the slider
    // moves: applying it stops the chunk worker pool, throws away every queued
    // and cached chunk and starts again, which is not something to do on each of
    // the thirty frames a drag lasts.
    void RequestRenderDistance(int chunks);

    // True once, when a request is outstanding. main owns the world, so main is
    // what acts on it.
    bool TakeRenderDistanceRequest(int& chunks);

    // Switching between fast and fancy leaves changes which faces exist, so it
    // means remeshing the world. Same reasoning as the render distance: asked
    // for on the way out of the graphics screen, not as the toggle is clicked.
    void RequestMeshRebuild();
    bool TakeMeshRebuildRequest();

private:
    void ApplyPendingTransition(GLFWwindow* menuWindow);

    UIRenderer* _ui = NULL;
    std::vector<std::unique_ptr<Screen>> _screens;

    enum class Transition
    {
        None,
        Push,
        Pop,
        CloseAll
    };

    Transition _pending = Transition::None;
    std::unique_ptr<Screen> _pendingScreen;

    int _requestedRenderDistance = 0;
    bool _hasRenderDistanceRequest = false;

    bool _hasMeshRebuildRequest = false;

    bool _cursorReleased = false;
};

// The four pages. Declared here so main can open the first one and so each can
// push the next.
class PauseScreen : public Screen
{
public:
    explicit PauseScreen(MenuSystem& menu);
    const char* Title() const override { return "Paused"; }
    void Layout(float width, float height) override;
};

class SettingsScreen : public Screen
{
public:
    explicit SettingsScreen(MenuSystem& menu);
    const char* Title() const override { return "Settings"; }
    void Layout(float width, float height) override;
};

class ControlsScreen : public Screen
{
public:
    explicit ControlsScreen(MenuSystem& menu);
    const char* Title() const override { return "Controls"; }
    void Layout(float width, float height) override;
    void OnClose() override;
};

class GraphicsScreen : public Screen
{
public:
    explicit GraphicsScreen(MenuSystem& menu);
    const char* Title() const override { return "Graphics"; }
    void Layout(float width, float height) override;
    void OnClose() override;

private:
    // Recorded on entry and compared on the way out, so nothing expensive is
    // rebuilt for a control that was cycled back to where it started.
    int _renderDistanceOnEntry = 0;
    int _renderDistance = 0;

    int _windowModeOnEntry = 0;
    int _windowMode = 0;

    bool _fancyOnEntry = true;

    int _resolutionWidthOnEntry = 0;
    int _resolutionHeightOnEntry = 0;
    int _resolutionIndex = 0;
};
