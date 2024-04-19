#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "headers/shader.hpp"
//#include "headers/chunk.hpp"
#include "headers/controls.hpp"
#include "headers/object.hpp"

GLFWwindow* window;
GLuint programID;
const int width = 1920, height = 1080;

using namespace std;

vector<GLfloat> vertices = {
	-0.5f, -0.5f,  0.5f, 
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  -0.5f,
     0.5f, -0.5f,  -0.5f,
     0.5f,  0.5f,  -0.5f,
    -0.5f,  0.5f,  -0.5f,
};

vector<GLfloat> uvs = {
    // Front face
    0.125f, 0.0f,
    0.1875f, 0.0f,
    0.1875f, 0.0625f,
    0.125f, 0.0625f,

    // Back face
    0.125f, 0.0f,
    0.1875f, 0.0f,
    0.1875f, 0.0625f,
    0.125f, 0.0625f,

    // Top face
    0.125f, 0.0f,
    0.1875f, 0.0f,
    0.1875f, 0.0625f,
    0.125f, 0.0625f,

    // Bottom face
    0.125f, 0.0f,
    0.1875f, 0.0f,
    0.1875f, 0.0625f,
    0.125f, 0.0625f,

    // Left face
    0.125f, 0.0f,
    0.1875f, 0.0f,
    0.1875f, 0.0625f,
    0.125f, 0.0625f,

    // Right face
    0.125f, 0.0f,
    0.1875f, 0.0f,
    0.1875f, 0.0625f,
    0.125f, 0.0625f,
};

vector<unsigned short> indices = {
    // front
	0, 1, 2,
	2, 3, 0,

	// right
	1, 5, 6,
	6, 2, 1,

	// back
	7, 6, 5,
	5, 4, 7,

	// left
	4, 0, 3,
	3, 7, 4,

	// bottom
	4, 5, 1,
	1, 0, 4,

	// top
	3, 2, 6,
	6, 7, 3
};

int main() {
	
    // Create and initialize the program
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Minecraft", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

    // Check for errors
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return -1;
    }

    // Create the shader
    programID = LoadShaders( "src/shaders/shader.vert", "src/shaders/shader.frag" );
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

    // Create the object
    Object triangle1(vertices, uvs, indices);

    // Create the camera
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwPollEvents();
	glfwSetCursorPos(window, width/2, height/2);

    glEnable(GL_DEPTH_TEST);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    glDepthFunc(GL_LESS);

	//Chunk chunky;

    // Run the program
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);

		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

        triangle1.Draw();
        //chunky.DrawChunk();

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
