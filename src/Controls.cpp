#include "headers/Controls.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "headers/Display.hpp"
#include "headers/Input.hpp"

glm::vec3 position = glm::vec3(0, 4, 0);

namespace
{
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;

    float horizontalAngle = 3.14f;
    float verticalAngle = 0.0f;
    float fieldOfView = 60.0f;

    float speed = 3.0f;

    // The rate a sensitivity of 1.0 means, in radians of turn per pixel of
    // mouse travel. The menu scales this rather than replacing it, so the
    // number it shows keeps its meaning if this is ever retuned.
    const float BASE_MOUSE_SPEED = 0.001f;
    float mouseSensitivity = 1.0f;
    bool invertMouseY = false;

    // Measured across frames, so it has to survive between calls rather than
    // living as a function static -- notifyResumed has to be able to reset it.
    double lastTime = 0.0;
    bool hasLastTime = false;

    // Set when the cursor is recaptured. The frame after that reads a position
    // that has nothing to do with where the player was looking, so the delta it
    // implies is thrown away and only the recentring is kept.
    bool discardMouseDelta = false;

    // Overwritten by setViewDistance before the first frame. The default keeps
    // the projection sane if it never is.
    float viewDistance = 32.0f;

    const float radian = 180.f / 3.14159265359f;
}

void setViewDistance(float chunks)
{
    viewDistance = chunks;
}

void setFieldOfView(float degrees)
{
    fieldOfView = degrees;
}

void setMouseSensitivity(float multiplier)
{
    mouseSensitivity = multiplier;
}

void setInvertMouseY(bool invert)
{
    invertMouseY = invert;
}

float getFieldOfView()
{
    return fieldOfView;
}

void getLookAngles(float& yawDegrees, float& pitchDegrees)
{
    yawDegrees = getNormalRotation(horizontalAngle * radian);
    pitchDegrees = getNormalRotation(verticalAngle * radian);
}

void notifyResumed()
{
    hasLastTime = false;
    discardMouseDelta = true;
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
    double currentTime = glfwGetTime();

    if (!hasLastTime)
    {
        lastTime = currentTime;
        hasLastTime = true;
    }

    float deltaTime = float(currentTime - lastTime);

    // Read from Display rather than from a pair of constants: the window can
    // change size now, and centring on a stale half-width would walk the view
    // sideways a little more every frame.
    GLFWwindow* window = Display::Window();
    const int width = Display::Width();
    const int height = Display::Height();

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    glfwSetCursorPos(window, width / 2, height / 2);

    if (discardMouseDelta)
    {
        // The recentring above still happened, so the next frame measures from
        // the middle of the screen as usual. Only this one sample is dropped.
        discardMouseDelta = false;
    }
    else
    {
        const float lookSpeed = BASE_MOUSE_SPEED * mouseSensitivity;

        horizontalAngle += lookSpeed * float(width / 2 - xpos);
        verticalAngle   += lookSpeed * float(height / 2 - ypos) * (invertMouseY ? -1.0f : 1.0f);

        // Stopped just short of straight up and straight down. Past vertical the
        // up vector flips and the view rolls over, which the original had no
        // guard against.
        const float limit = 1.5533f; // 89 degrees
        verticalAngle = std::max(-limit, std::min(limit, verticalAngle));
    }

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

    // Asked for by action rather than by key, so a rebound key moves the player
    // without this code knowing which key it now is.
    if (Input::IsActionDown(Input::Action::Forward))
    {
        position += direction * deltaTime * speed;
    }
    if (Input::IsActionDown(Input::Action::Back))
    {
        position -= direction * deltaTime * speed;
    }
    if (Input::IsActionDown(Input::Action::Right))
    {
        position += right * deltaTime * speed;
    }
    if (Input::IsActionDown(Input::Action::Left))
    {
        position -= right * deltaTime * speed;
    }
    if (Input::IsActionDown(Input::Action::Up))
    {
        position.y += deltaTime * speed;
    }
    if (Input::IsActionDown(Input::Action::Down))
    {
        position.y -= deltaTime * speed;
    }

    ViewMatrix = glm::lookAt(position, position + direction, up);

    lastTime = currentTime;
}

void updateProjectionMatrix()
{
    // Computed per frame rather than cached: the window can be resized or sent
    // fullscreen from the graphics menu, and a stale aspect ratio stretches the
    // whole world.
    float aspectRatio = (float)Display::Width() / (float)Display::Height();

    // The far plane only has to reach the corner of the loaded square, which is
    // the radius times root two. The extra half unit keeps a chunk that is just
    // inside that corner from being clipped by it.
    float farPlane = viewDistance * 1.5f;

    ProjectionMatrix = glm::perspective(glm::radians(fieldOfView), aspectRatio, 0.1f, farPlane);
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
