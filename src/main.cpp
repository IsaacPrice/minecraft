#define GLM_ENABLE_EXPERIMENTAL

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "headers/Chunk.hpp"
#include "headers/DebugOverlay.hpp"
#include "headers/Display.hpp"
#include "headers/Frustum.hpp"
#include "headers/Input.hpp"
#include "headers/PostProcess.hpp"
#include "headers/Screens.hpp"
#include "headers/Settings.hpp"
#include "headers/World.hpp"
#include "headers/Shader.hpp"
#include "headers/Controls.hpp"

using namespace std;
using namespace glm;

GLuint programID;

// The window and its size used to live here as a global and a pair of consts,
// which is why nothing could change either. Both belong to Display now; see
// Display.hpp for what that made possible.

// How far out chunks are kept loaded, and how far out the small plants are drawn
// -- grass, ferns, flowers, mushrooms, dead bushes, sugar cane, saplings -- are
// both settings now, so the graphics menu can reach them. They were consts here,
// which meant changing either was an edit and a rebuild; the defaults are
// unchanged and live in Settings.hpp.
//
// Past the foliage distance the small plants are dropped and tree canopies
// switch from the see-through leaf tile to the solid one, which is what they
// read as at that range anyway. The fog has already faded everything to sky by
// about a chunk inside the loaded radius, so the default sits just outside that
// and changes nothing on screen. Lower it to trade foliage in the middle
// distance for fill rate.
//
// World counts render distance as the width of the loaded square while the menu
// counts it as a radius, so it is handed twice the setting. Without the
// doubling, asking for 32 chunks loaded 16.
static unsigned short worldRenderDistance(int chunks)
{
    return (unsigned short)(chunks * 2);
}

// The sky, and so also the colour the fog fades terrain into. Setting this at
// all is new: the clear colour was left at its default, which is black, so
// anywhere the world had not been built yet read as a hole rather than as sky.
const vec3 SKY_COLOUR = vec3(0.55f, 0.75f, 0.94f);


int main()
{
    // GLFW first, but no window yet. The settings decide the size and mode the
    // window is created at, and reading a keybind back out of the file names it
    // through glfwGetKeyName, so the library has to be up before the file is
    // read and the file has to be read before the window is made.
    if (!Display::InitLibrary())
    {
        printf("Could not initialise GLFW.\n");
        return -1;
    }

    LoadSettings();

    Settings& options = settings();

    if (!Display::Create(options.resolutionWidth, options.resolutionHeight,
                         (Display::Mode)options.windowMode, options.vsync))
    {
        printf("Could not open a window.\n");
        return -1;
    }

    GLFWwindow* window = Display::Window();

    programID = LoadShaders( "src/shaders/shader.vert", "src/shaders/shader.frag" );
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

    // The atlas and its sampler uniform are shared by every chunk, so they are
    // set up once here. They used to be established lazily inside Object and
    // then rebound by every single chunk on every single draw.
    InitRenderResources();

    // After the atlas exists, because anisotropy is a parameter on the texture
    // InitRenderResources has just uploaded, and the level asked for is clamped
    // to what this driver reports.
    ApplyGeneralSettings();
    ApplyGraphicsSettings();

    // One batch for everything drawn over the world, menu and overlay alike, so
    // the whole interface is a single draw call however much of it is showing.
    UIRenderer ui;
    const bool interfaceReady = ui.Create();
    if (!interfaceReady)
    {
        // A missing font or UI shader costs the interface, not the game. Better
        // to fly around without it than to refuse to start.
        printf("The user interface could not be loaded; running without it.\n");
    }

    MenuSystem menu;
    menu.Create(ui);

    DebugOverlay debug;

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwPollEvents();
	glfwSetCursorPos(window, Display::Width()/2, Display::Height()/2);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Back faces are dropped now. Fancy leaves mesh every face of every leaf
    // block rather than a hollow shell, which is a great deal more geometry, and
    // throwing away the half of it that points away from the camera is what pays
    // for that. It needed the box faces wound consistently first; see appendQuad
    // in Chunk.cpp. Plants are emitted both ways round so they survive it.
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(SKY_COLOUR.r, SKY_COLOUR.g, SKY_COLOUR.b, 1.0f);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // The world is drawn into an offscreen buffer so the finished image can be
    // run through FXAA on the way to the window. If that buffer cannot be made,
    // rendering falls back to drawing straight to the window without it rather
    // than failing to start.
    PostProcess postProcess;
    bool antialiasAvailable = postProcess.Create(Display::Width(), Display::Height());

    vec3 lightDirection = vec3(0.f, 0.f, 1.f);
    GLint lightDirUniformLocation = glGetUniformLocation(programID, "lightDirection");
    GLint alphaScaleUniformLocation = glGetUniformLocation(programID, "alphaScale");
    GLint cameraPositionUniformLocation = glGetUniformLocation(programID, "cameraPosition");
    GLint fogColorUniformLocation = glGetUniformLocation(programID, "fogColor");
    GLint fogStartUniformLocation = glGetUniformLocation(programID, "fogStart");
    GLint fogEndUniformLocation = glGetUniformLocation(programID, "fogEnd");
    GLint foliageDistanceUniformLocation = glGetUniformLocation(programID, "foliageDistance");

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uint64_t seed = time(NULL);
    cout << "Seed: " << seed << "\n";
    // Set before the first chunk is meshed. Afterwards only World::RebuildMeshes
    // may touch it, because the mesher reads it from the worker threads.
    SetFancyLeaves(options.fancyGraphics);

    World world(seed, worldRenderDistance(options.renderDistance));

    // Ties the projection's far plane and the fog band to how much world is
    // actually loaded, so terrain fades into the sky exactly where it runs out.
    setViewDistance(world.LoadedRadius());

    // Reused across frames so the per-frame culling pass does not allocate.
    std::vector<std::pair<float, const Chunk*>> visible;

    while (!glfwWindowShouldClose(window))
    {
        // Events first, into a list the menu reads. Polling used to sit at the
        // bottom of the loop, which meant each frame acted on input gathered
        // before the previous one was drawn.
        Input::BeginFrame();
        glfwPollEvents();

        // The window can change size now, so everything sized in pixels is
        // rebuilt when it does. The offscreen buffer is the only one nothing
        // else owns; the viewport and the aspect ratio are worked out per frame
        // from the same numbers.
        if (Display::TakeSizeChange())
        {
            antialiasAvailable = postProcess.Create(Display::Width(), Display::Height());
            glViewport(0, 0, Display::Width(), Display::Height());
        }

        // Timed every frame, shown only when asked for, so opening the overlay
        // reports the rate the game has been running at rather than starting to
        // measure from the moment it appeared.
        debug.NewFrame();

        if (Input::ActionPressed(Input::Action::ToggleDebug))
            debug.Toggle();

        // Before anything reads or draws into it: this is what establishes the
        // canvas that hit testing and layout are both measured in.
        ui.BeginFrame(Display::Width(), Display::Height());

        menu.Update(window);

        // Applied out here rather than inside the menu: the world is main's, and
        // changing the distance stops the chunk workers and restarts them, which
        // has to happen on the thread that holds the GL context.
        int requestedRenderDistance = 0;
        if (menu.TakeRenderDistanceRequest(requestedRenderDistance))
        {
            world.changeRenderDistance(worldRenderDistance(requestedRenderDistance));

            // Without this the fog band and the far plane stay where the old
            // distance put them, and terrain ends on a hard edge instead of
            // fading into the sky.
            setViewDistance(world.LoadedRadius());
        }

        if (menu.TakeMeshRebuildRequest())
            world.RebuildMeshes(options.fancyGraphics);

        const bool paused = menu.Paused();
        const bool antialias = antialiasAvailable && options.fxaa;

        if (antialias)
            postProcess.BeginScene(SKY_COLOUR.r, SKY_COLOUR.g, SKY_COLOUR.b);
        else
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(programID);

        // Frozen while the menu is up. The camera keeps the view it had and the
        // world carries on being drawn behind the menu, rather than the screen
        // holding a stale frame.
        if (!paused)
            computeMatricesFromInputs();

        // Rebuilt every frame either way, so dragging the field of view slider
        // is seen as it moves rather than only once the menu is closed.
        updateProjectionMatrix();

		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        glUniform3fv(lightDirUniformLocation, 1, value_ptr(lightDirection));
        glUniform3fv(cameraPositionUniformLocation, 1, value_ptr(position));
        glUniform3fv(fogColorUniformLocation, 1, value_ptr(SKY_COLOUR));
        glUniform1f(fogStartUniformLocation, getFogStart());
        glUniform1f(fogEndUniformLocation, getFogEnd());
        glUniform1f(foliageDistanceUniformLocation, options.foliageDistance);

        world.UpdateChunks(position);

        // Every chunk samples the same atlas, so it is bound once for the frame
        // rather than once per chunk per pass.
        BindTerrainTexture();

        unique_lock<mutex> lock(chunkMutex);

        // Cull to the view, then order by distance. Both passes are over the
        // loaded chunks, which is thousands, but each test is a handful of dot
        // products and it saves submitting most of them to the driver.
        Frustum frustum(MVP);

        visible.clear();
        visible.reserve(chunks.size());

        for (const auto& entry : chunks)
        {
            const Chunk& chunk = entry.second;
            if (chunk.Empty())
                continue;

            if (!frustum.Intersects(chunk.BoundsLow(), chunk.BoundsHigh()))
                continue;

            glm::vec3 centre = (chunk.BoundsLow() + chunk.BoundsHigh()) * 0.5f;
            visible.push_back({ glm::dot(centre - position, centre - position), &chunk });
        }

        // Nearest first for the solid pass. The depth test then throws away most
        // of the fragments behind what is already drawn, instead of shading a
        // distant hillside and painting over it.
        std::sort(visible.begin(), visible.end(),
                  [](const std::pair<float, const Chunk*>& a,
                     const std::pair<float, const Chunk*>& b) { return a.first < b.first; });

        // Solid terrain and the cut-out plants first, with depth writes on.
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glUniform1f(alphaScaleUniformLocation, 1.0f);
        for (const auto& entry : visible)
        {
            entry.second->Draw();
        }

        // Then water, blended over the top, and furthest first so that where two
        // water surfaces overlap the nearer one is blended over the further one.
        // Depth writes stay off: the chunks are sorted but the faces inside them
        // are not, so a chunk that wrote depth would hide its own far surfaces.
        //
        // Culling comes off for this pass. A lake is meshed as a lid with no
        // underside, so culling it would leave nothing overhead once the camera
        // drops below the surface.
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glUniform1f(alphaScaleUniformLocation, 0.65f);
        for (auto it = visible.rbegin(); it != visible.rend(); ++it)
        {
            it->second->DrawWater();
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);

        DebugStats stats;
        if (debug.Visible())
        {
            // Only gathered when the overlay is up. Counting triangles means
            // walking the visible list again, which is thousands of chunks at a
            // large render distance and pure waste when nothing reads it.
            stats.chunksVisible = visible.size();
            stats.chunksLoaded = chunks.size();

            for (const auto& entry : visible)
                stats.triangles += entry.second->TriangleCount();
        }

        lock.unlock();

        if (antialias)
            postProcess.Resolve();

        // After the resolve, so the menu goes straight into the window. FXAA is
        // a luminance edge filter and would soften every glyph edge, and
        // anything drawn before the resolve is inside the world pass, where it
        // would be fogged and buried by the nearest hillside.
        //
        // Escape used to be read here as a level, which released the cursor and
        // had no way of ever taking it back. It is an action with a pause screen
        // behind it now; see MenuSystem::Update.
        if (debug.Visible())
        {
            stats.chunksPending = world.PendingChunkCount();
            stats.blockMaps = world.BlockMapCount();

            // A world unit is one chunk, which is sixteen blocks. The camera
            // works in the former and a coordinate is only useful in the latter.
            stats.blockX = position.x * (float)CHUNK_WIDTH;
            stats.blockY = position.y * (float)CHUNK_WIDTH;
            stats.blockZ = position.z * (float)CHUNK_WIDTH;

            stats.chunkX = (int)std::floor(position.x);
            stats.chunkZ = (int)std::floor(position.z);

            stats.seed = seed;
            stats.renderDistance = options.renderDistance;
            stats.fancyGraphics = options.fancyGraphics;
            stats.fxaa = antialias;
            stats.vsync = options.vsync;
            stats.frameCap = options.frameCap;
        }

        menu.Render();
        debug.Render(ui, stats);

        // One flush for the menu and the overlay together.
        ui.EndFrame();

        glfwSwapBuffers(window);

        // After the swap, where the frame has actually finished. With vsync off
        // this is the only thing between the renderer and running the machine
        // at whatever rate it can manage.
        Display::LimitFrameRate();
    }

    SaveSettings();

    glDeleteProgram(programID);
    Display::Destroy();
    return 0;
}
