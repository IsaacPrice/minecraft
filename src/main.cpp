#define GLM_ENABLE_EXPERIMENTAL

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <vector>
#include <thread>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "headers/World.hpp"
#include "headers/Shader.hpp"
#include "headers/Controls.hpp"


using namespace std;
using namespace glm;

GLFWwindow* window;
GLuint programID;
const int width = 1280, height = 720;


int setupWindow(bool vsync, bool fullscreen);


int main() 
{
    setupWindow(true, false);

    programID = LoadShaders( "src/shaders/shader.vert", "src/shaders/shader.frag" );
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwPollEvents();
	glfwSetCursorPos(window, width/2, height/2);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CW);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    vec3 lightDirection = vec3(0.f, 0.f, 1.f);
    GLint lightDirUniformLocation = glGetUniformLocation(programID, "lightDirection");

    uint64_t seed = time(NULL);
    cout << "Seed: " << seed << "\n";
    World world(seed, 6);

    while (!glfwWindowShouldClose(window)) 
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);

		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        glUniform3fv(lightDirUniformLocation, 1, value_ptr(lightDirection));

        world.UpdateChunks(position);

        unique_lock<mutex> lock(chunkMutex);
        for (auto& chunk : chunks) 
        {
            chunk.second.Draw();
        }

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
    glfwWindowHint(GLFW_SAMPLES, 0);
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