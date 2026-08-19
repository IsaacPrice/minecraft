#pragma once

#include <glm/glm.hpp>

// Camera position. Shared with main so the world can stream chunks around the player.
extern glm::vec3 position;

glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();

void computeMatricesFromInputs();

float getNormalRotation(float angle);
void printPositions(const char* message = "cls");
