#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <vector>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "headers/world.hpp"
#include "headers/shader.hpp"
#include "headers/controls.hpp"

GLFWwindow* window;
GLuint programID;
const int width = 1920, height = 1080;

using namespace std;
using namespace glm;

int setupWindow(bool vsync, bool fullscreen);

int main() {

    // Create the window
    setupWindow(true, false);

    // Create the shader
    programID = LoadShaders( "src/shaders/shader.vert", "src/shaders/shader.frag" );
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

    // Setup the input modes
	//glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwPollEvents();
	glfwSetCursorPos(window, width/2, height/2);

    // Set the opengl settings
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CW);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // Set light data
    vec3 lightDirection = vec3(0.f, 0.f, 1.f);

    // Get the uniform locations
    GLint lightDirUniformLocation = glGetUniformLocation(programID, "lightDirection");

    // Generate the seed
    uint64_t seed = time(NULL);

    cout << "Seed: " << seed << "\n";

    // Create the world
    World world(seed, 32);

    // Run the program
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);

		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

        // print out the directions
        // printPositions("clear");

        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        glUniform3fv(lightDirUniformLocation, 1, value_ptr(lightDirection));

        // Update the world
        world.UpdateChunks(position);

        // Render the world
        unique_lock<mutex> lock(chunkMutex);
        chunkCondition.wait(lock, [] { return !chunks.empty(); });
        for (auto& chunk : chunks) {
            chunk.second.Draw();
        }
        lock.unlock();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

        // Update the window
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Close the program
    glDeleteProgram(programID);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


int setupWindow(bool vsync, bool fullscreen) {
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    if (fullscreen) {
        window = glfwCreateWindow(width, height, "Minecraft", glfwGetPrimaryMonitor(), NULL);
    }
    else {
        window = glfwCreateWindow(width, height, "Minecraft", NULL, NULL);
    }
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (vsync) {
	    glfwSwapInterval(1);
    } else {
        glfwSwapInterval(0);
    }

    // Check for errors
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return -1;
    }

    return 0;
}