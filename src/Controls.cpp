#include "headers/Controls.hpp"

#include <cstdio>
#include <cstdlib>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

extern GLFWwindow* window;
extern const int width, height;

glm::vec3 position = glm::vec3(0, 4, 0);

namespace
{
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;

    float horizontalAngle = 3.14f;
    float verticalAngle = 0.0f;
    float initialFoV = 60.0f;

    float speed = 3.0f;
    float mouseSpeed = 0.001f;

    // Overwritten by setViewDistance before the first frame. The default keeps
    // the projection sane if it never is.
    float viewDistance = 32.0f;

    const float radian = 180.f / 3.14159265359f;
}

void setViewDistance(float chunks)
{
    viewDistance = chunks;
}

// The fog band ends just inside the loaded region, so the last ring of chunks
// is already sky-coloured by the time it is reached and popping in at the edge
// is invisible. It starts three quarters of the way out, which is far enough
// that fog reads as haze rather than as a wall.
float getFogStart()
{
    return (viewDistance - 1.0f) * 0.75f;
}

float getFogEnd()
{
    return viewDistance - 1.0f;
}

glm::mat4 getViewMatrix()
{
    return ViewMatrix;
}

glm::mat4 getProjectionMatrix()
{
    return ProjectionMatrix;
}

void computeMatricesFromInputs()
{
    static double lastTime = glfwGetTime();

    double currentTime = glfwGetTime();
    float deltaTime = float(currentTime - lastTime);

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    glfwSetCursorPos(window, width / 2, height / 2);

    horizontalAngle += mouseSpeed * float(width / 2 - xpos);
    verticalAngle   += mouseSpeed * float(height / 2 - ypos);

    glm::vec3 direction(
        cos(verticalAngle) * sin(horizontalAngle),
        sin(verticalAngle),
        cos(verticalAngle) * cos(horizontalAngle)
    );

    glm::vec3 right = glm::vec3(
        sin(horizontalAngle - 3.14f / 2.0f),
        0,
        cos(horizontalAngle - 3.14f / 2.0f)
    );

    glm::vec3 up = glm::cross(right, direction);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        position += direction * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        position -= direction * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        position += right * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        position -= right * deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        position.y += deltaTime * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        position.y -= deltaTime * speed;
    }

    // Computed per frame rather than at namespace scope: width/height live in another
    // translation unit, so a static initialiser here would depend on init order.
    float aspectRatio = (float)width / (float)height;

    // The far plane only has to reach the corner of the loaded square, which is
    // the radius times root two. The extra half unit keeps a chunk that is just
    // inside that corner from being clipped by it.
    float farPlane = viewDistance * 1.5f;

    ProjectionMatrix = glm::perspective(glm::radians(initialFoV), aspectRatio, 0.1f, farPlane);
    ViewMatrix = glm::lookAt(position, position + direction, up);

    lastTime = currentTime;
}

float getNormalRotation(float angle)
{
    while (!(angle >= -180 && angle <= 180))
    {
        if (angle < -180)
        {
            angle += 360;
        }
        else if (angle > 180)
        {
            angle -= 360;
        }
    }
    return angle;
}

void printPositions(const char* message)
{
    system(message);
    printf("Position: (%f, %f, %f)\n", position.x, position.y, position.z);
    float horizontalDegrees = getNormalRotation(horizontalAngle * radian);
    float verticalDegrees = getNormalRotation(verticalAngle * radian);
    printf("Direction: (%f, %f)\n", horizontalDegrees, verticalDegrees);

    if (horizontalDegrees >= -45 && horizontalDegrees <= 45)
    {
        printf("Facing: East\n");
    }
    else if (horizontalDegrees >= 45 && horizontalDegrees <= 135)
    {
        printf("Facing: South\n");
    }
    else if (horizontalDegrees >= 135 || horizontalDegrees <= -135)
    {
        printf("Facing: West\n");
    }
    else if (horizontalDegrees >= -135 && horizontalDegrees <= -45)
    {
        printf("Facing: North\n");
    }
}
