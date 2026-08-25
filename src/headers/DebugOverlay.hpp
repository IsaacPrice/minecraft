#pragma once

#include <cstddef>
#include <cstdint>

class UIRenderer;

// Everything the overlay reports that it cannot work out for itself. Gathered by
// main once a frame, because main is what holds the world and the renderer; the
// overlay only knows how to lay numbers out.
struct DebugStats
{
    // Chunks
    size_t chunksLoaded = 0;
    size_t chunksVisible = 0;
    size_t chunksPending = 0;
    size_t blockMaps = 0;
    size_t triangles = 0;

    // Where the player is, in blocks rather than the chunk units the camera
    // works in -- a coordinate is only useful if it counts the same thing the
    // world is built out of.
    float blockX = 0.0f;
    float blockY = 0.0f;
    float blockZ = 0.0f;

    int chunkX = 0;
    int chunkZ = 0;

    uint64_t seed = 0;

    // Settings worth seeing without opening the menu, because they are the ones
    // a frame rate question usually turns on.
    int renderDistance = 0;
    bool fancyGraphics = true;
    bool fxaa = true;
    bool vsync = true;
    int frameCap = 0;
};

// The F3 overlay: frame timing and what the renderer is actually doing, drawn
// over the world without pausing it.
//
// This is a HUD rather than a screen. It takes no input beyond its own toggle,
// does not stack, and is drawn while play continues -- so it deliberately sits
// outside MenuSystem, which is for modal pages that release the cursor and stop
// the camera. The hotbar will belong here too.
class DebugOverlay
{
public:
    // Called every frame whether or not the overlay is showing, so that opening
    // it reports the rate the game has been running at rather than starting its
    // measurement from the moment it appeared.
    void NewFrame();

    void Toggle() { _visible = !_visible; }
    bool Visible() const { return _visible; }

    // Queues its quads into the batch the caller is already building. Does
    // nothing when hidden.
    void Render(UIRenderer& ui, const DebugStats& stats) const;

private:
    bool _visible = false;

    double _lastFrameTime = 0.0;
    bool _hasLastFrameTime = false;

    // Averaged over a window rather than shown per frame: a number that changes
    // sixty times a second cannot be read, and the instantaneous value is mostly
    // scheduler noise.
    double _windowStart = 0.0;
    int _windowFrames = 0;
    double _windowWorst = 0.0;

    double _fps = 0.0;
    double _averageMs = 0.0;

    // The slowest frame in the last window. A stutter is what actually gets
    // noticed, and an average hides it completely.
    double _worstMs = 0.0;
};
