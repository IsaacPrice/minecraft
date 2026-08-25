#pragma once

// Everything the options menu can change, in one place.
//
// These were compile-time constants scattered across main.cpp and file statics
// in an anonymous namespace in Controls.cpp, which meant changing the field of
// view was an edit and a rebuild. Gathering them here is what lets a menu reach
// them at all; keeping them a plain struct of values, with no knowledge of the
// systems that consume them, is what stops the menu having to know how each one
// is stored.
//
// Persisted to options.txt beside the executable as plain key=value lines. A
// missing file means defaults, an unknown key is ignored, and a value out of
// range is clamped -- so an options.txt from an older build still loads.
struct Settings
{
    // --- General ---

    // Degrees, vertical. The menu offers 30 to 110.
    float fov = 60.0f;

    // Multiplier on the base look speed rather than the speed itself, so the
    // number the menu shows means the same thing whatever the base is tuned to.
    float mouseSensitivity = 1.0f;

    bool invertMouseY = false;

    // --- Graphics ---

    // How far the world is loaded, as a radius in chunks -- so this is the
    // number of chunks between the player and the edge, which is what everything
    // else in the menu is measured in and what the fog band and the far plane
    // are derived from.
    //
    // World counts in the width of the loaded square rather than its radius, so
    // it is handed twice this. That mismatch is why a render distance that read
    // as 64 only reached 32 chunks out, and why the foliage cut-off at 24 looked
    // like it was arriving far too early: it was three quarters of the way out,
    // not three eighths.
    int renderDistance = 32;

    // How far out, in chunks, small plants are still drawn and canopies keep
    // their see-through tile.
    float foliageDistance = 24.0f;

    // Fancy draws leaves with the see-through tile up close and lets you see
    // through a canopy. Fast uses the solid tile everywhere and drops the faces
    // inside a canopy, which are invisible once the outside is opaque.
    bool fancyGraphics = true;

    bool fxaa = true;
    bool vsync = true;

    // 1 is off; otherwise 2, 4, 8 or 16, clamped to what the driver reports.
    int anisotropy = 16;

    // Frames a second, or 0 for no limit. The menu offers 30 to 240 in tens and
    // then unlimited.
    int frameCap = 0;

    // 0 windowed, 1 borderless, 2 fullscreen; matches Display::Mode.
    int windowMode = 0;

    // The size the world is drawn at. Borderless ignores it and takes the
    // monitor's own, because that is the point of borderless.
    int resolutionWidth = 1920;
    int resolutionHeight = 1080;
};

Settings& settings();

// Reading needs GLFW initialised, because a binding is named through
// glfwGetKeyName. Call after glfwInit.
void LoadSettings(const char* path = "options.txt");
void SaveSettings(const char* path = "options.txt");

// Pushes the general values into Controls. Cheap, so the menu calls it on every
// change and the slider is felt as it moves.
void ApplyGeneralSettings();

// Pushes vsync, the frame cap and anisotropy into GLFW and the block atlas. Also
// cheap. Render distance, the window mode and the resolution are deliberately
// not here: each one tears something down and rebuilds it -- the chunk worker
// pool, or the window and the offscreen buffer behind it -- so they are applied
// when the graphics screen is left rather than as a control is dragged through
// twenty values on the way to the one that was wanted.
void ApplyGraphicsSettings();

// The window mode and resolution, applied together because they interact.
void ApplyDisplaySettings();
