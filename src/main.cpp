#define GLM_ENABLE_EXPERIMENTAL

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <thread>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "headers/Frustum.hpp"
#include "headers/PostProcess.hpp"
#include "headers/World.hpp"
#include "headers/Shader.hpp"
#include "headers/Controls.hpp"

using namespace std;
using namespace glm;

GLFWwindow* window;
GLuint programID;
// extern: const has internal linkage by default, and Controls.cpp needs these.
extern const int width = 1920, height = 1080;

// How far out chunks are kept loaded, as a square this many chunks on a side.
const unsigned short RENDER_DISTANCE = 64;

// How far out, in chunks, the small plants are drawn: grass, ferns, flowers,
// mushrooms, dead bushes, sugar cane, saplings. Past this they are dropped, and
// tree canopies switch from the see-through leaf tile to the solid one, which
// is what they read as at a distance anyway.
//
// Note that the fog has already faded everything to sky by about a chunk inside
// the loaded radius, which at the render distance above is 31 chunks -- so the
// default here is deliberately just outside that and changes nothing on screen.
// Lower it to trade foliage in the middle distance for fill rate.
const float FOLIAGE_DISTANCE = 24.0f;

// The sky, and so also the colour the fog fades terrain into. Setting this at
// all is new: the clear colour was left at its default, which is black, so
// anywhere the world had not been built yet read as a hole rather than as sky.
const vec3 SKY_COLOUR = vec3(0.55f, 0.75f, 0.94f);


int setupWindow(bool vsync, bool fullscreen);


int main()
{
    setupWindow(true, false);

    programID = LoadShaders( "src/shaders/shader.vert", "src/shaders/shader.frag" );
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

    // The atlas and its sampler uniform are shared by every chunk, so they are
    // set up once here. They used to be established lazily inside Object and
    // then rebound by every single chunk on every single draw.
    InitRenderResources();

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwPollEvents();
	glfwSetCursorPos(window, width/2, height/2);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CW);
    glClearColor(SKY_COLOUR.r, SKY_COLOUR.g, SKY_COLOUR.b, 1.0f);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // The world is drawn into an offscreen buffer so the finished image can be
    // run through FXAA on the way to the window. If that buffer cannot be made,
    // rendering falls back to drawing straight to the window without it rather
    // than failing to start.
    PostProcess postProcess;
    bool antialias = postProcess.Create(width, height);

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
    World world(seed, RENDER_DISTANCE);

    // Ties the projection's far plane and the fog band to how much world is
    // actually loaded, so terrain fades into the sky exactly where it runs out.
    setViewDistance(world.LoadedRadius());

    // Reused across frames so the per-frame culling pass does not allocate.
    std::vector<std::pair<float, const Chunk*>> visible;

    while (!glfwWindowShouldClose(window))
    {
        if (antialias)
            postProcess.BeginScene(SKY_COLOUR.r, SKY_COLOUR.g, SKY_COLOUR.b);
        else
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(programID);

		computeMatricesFromInputs();
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
        glUniform1f(foliageDistanceUniformLocation, FOLIAGE_DISTANCE);

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
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glUniform1f(alphaScaleUniformLocation, 0.65f);
        for (auto it = visible.rbegin(); it != visible.rend(); ++it)
        {
            it->second->DrawWater();
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        lock.unlock();

        if (antialias)
            postProcess.Resolve();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(programID);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


int setupWindow(bool vsync, bool fullscreen)
{
    glfwInit();
    // No multisampling: it costs a sample per pixel for the silhouettes alone
    // and does nothing for the aliasing inside a texture, which in a world made
    // of textured cubes is nearly all of what is visible. Mipmaps deal with
    // that, and FXAA picks up the silhouettes afterwards for far less.
    glfwWindowHint(GLFW_SAMPLES, 0);

    // The offscreen buffer, the viewport and the projection's aspect ratio are
    // all built once from the size below. Resizing would need all three rebuilt,
    // and none of that was ever wired up, so the window is fixed rather than
    // resizable into a state that renders wrongly.
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Minecraft", fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);

    if (window == NULL)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(vsync);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        return -1;
    }

    return 0;
}




