#pragma once

#include <glm/glm.hpp>

// Camera position. Shared with main so the world can stream chunks around the player.
extern glm::vec3 position;

// How far the camera can see, in world units. A chunk is one unit wide, so this
// is a radius in chunks. The projection's far plane and the fog band are both
// derived from it, which is what makes the world fade into the sky exactly
// where chunks stop being loaded rather than ending against a hard black edge.
// The far plane used to sit at 100 units regardless -- three times past the
// furthest loaded chunk -- which spent all the depth buffer's precision on
// empty space.
void setViewDistance(float chunks);

float getFogStart();
float getFogEnd();

glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();

void computeMatricesFromInputs();

float getNormalRotation(float angle);
void printPositions(const char* message = "cls");
