#include "headers/DebugOverlay.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "headers/BlockMap.hpp"
#include "headers/Controls.hpp"
#include "headers/UIRenderer.hpp"

namespace
{
    // How often the displayed numbers are refreshed. Long enough to read, short
    // enough that a change is felt as a response to whatever was just changed.
    const double SAMPLE_WINDOW = 0.5;

    const float TEXT_SCALE = 1.0f;
    const float MARGIN = 4.0f;
    const float PADDING = 2.0f;
    const float LINE_GAP = 2.0f;

    const Colour BACKDROP = { 0, 0, 0, 130 };
    const Colour TEXT = { 255, 255, 255, 255 };

    std::string format(const char* pattern, ...)
    {
        char text[192];

        va_list args;
        va_start(args, pattern);
        vsnprintf(text, sizeof(text), pattern, args);
        va_end(args);

        return text;
    }

    // The same mapping printPositions uses, so the two never disagree about
    // which way the player is looking.
    const char* compass(float yawDegrees)
    {
        if (yawDegrees >= -45.0f && yawDegrees <= 45.0f)
            return "east  (+X)";
        if (yawDegrees > 45.0f && yawDegrees < 135.0f)
            return "south (+Z)";
        if (yawDegrees >= 135.0f || yawDegrees <= -135.0f)
            return "west  (-X)";
        return "north (-Z)";
    }
}

void DebugOverlay::NewFrame()
{
    const double now = glfwGetTime();

    if (!_hasLastFrameTime)
    {
        _lastFrameTime = now;
        _windowStart = now;
        _hasLastFrameTime = true;
        return;
    }

    const double frame = now - _lastFrameTime;
    _lastFrameTime = now;

    _windowFrames++;
    if (frame > _windowWorst)
        _windowWorst = frame;

    const double elapsed = now - _windowStart;
    if (elapsed < SAMPLE_WINDOW)
        return;

    _fps = _windowFrames / elapsed;
    _averageMs = elapsed * 1000.0 / _windowFrames;
    _worstMs = _windowWorst * 1000.0;

    _windowStart = now;
    _windowFrames = 0;
    _windowWorst = 0.0;
}

void DebugOverlay::Render(UIRenderer& ui, const DebugStats& stats) const
{
    if (!_visible)
        return;

    float yaw = 0.0f, pitch = 0.0f;
    getLookAngles(yaw, pitch);

    // A block map is the terrain of one chunk kept in memory so that meshing it,
    // and meshing the four chunks around it, does not have to generate it again.
    // At a large render distance it is by far the biggest thing the process
    // holds, which is why it is worth a line of its own.
    const double blockMapMegabytes =
        stats.blockMaps * (double)sizeof(BlockMap) / (1024.0 * 1024.0);

    std::vector<std::string> lines;

    lines.push_back(format("%.0f fps   %.2f ms avg   %.2f ms worst",
                           _fps, _averageMs, _worstMs));

    if (stats.vsync && stats.frameCap)
        lines.push_back(format("vsync on    cap %d fps", stats.frameCap));
    else if (stats.vsync)
        lines.push_back("vsync on");
    else if (stats.frameCap)
        lines.push_back(format("vsync off   cap %d fps", stats.frameCap));
    else
        lines.push_back("vsync off   uncapped");

    lines.push_back("");

    lines.push_back(format("chunks %d/%d drawn   %d queued",
                           (int)stats.chunksVisible, (int)stats.chunksLoaded,
                           (int)stats.chunksPending));
    lines.push_back(format("tris   %d", (int)stats.triangles));
    lines.push_back(format("terrain cached %d chunks (%.0f MB)",
                           (int)stats.blockMaps, blockMapMegabytes));

    lines.push_back("");

    lines.push_back(format("xyz    %.1f / %.1f / %.1f",
                           stats.blockX, stats.blockY, stats.blockZ));
    lines.push_back(format("chunk  %d, %d", stats.chunkX, stats.chunkZ));
    lines.push_back(format("facing %s  (%.1f / %.1f)", compass(yaw), yaw, pitch));

    lines.push_back("");

    lines.push_back(format("render distance %d chunks", stats.renderDistance));
    lines.push_back(format("graphics %s   fxaa %s",
                           stats.fancyGraphics ? "fancy" : "fast",
                           stats.fxaa ? "on" : "off"));
    lines.push_back(format("seed   %llu", (unsigned long long)stats.seed));

    const float lineHeight = ui.LineHeight(TEXT_SCALE);
    float y = MARGIN;

    for (size_t i = 0; i < lines.size(); i++)
    {
        // A blank line is a gap between groups and wants no backdrop behind it.
        if (lines[i].empty())
        {
            y += lineHeight * 0.5f;
            continue;
        }

        const float width = ui.MeasureText(lines[i], TEXT_SCALE);

        // A panel behind each line rather than one behind the block, so the text
        // stays readable over bright sky and dark stone alike without a slab of
        // grey sitting over the corner of the view where there is nothing to
        // read.
        ui.DrawRect(MARGIN - PADDING, y - PADDING,
                    width + PADDING * 2.0f, lineHeight + PADDING * 2.0f, BACKDROP);

        ui.DrawText(MARGIN, y, TEXT_SCALE, TEXT, lines[i]);

        y += lineHeight + LINE_GAP;
    }
}
