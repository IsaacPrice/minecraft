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

// Vertical field of view in degrees, the look speed as a multiplier on the base
// rate, and whether moving the mouse down looks up. All three were file statics
// with no way to reach them; the options menu writes them through here.
void setFieldOfView(float degrees);
void setMouseSensitivity(float multiplier);
void setInvertMouseY(bool invert);

float getFieldOfView();

// Yaw and pitch in degrees, wrapped to -180..180. The debug overlay reports
// them, and works a compass direction out of the yaw.
void getLookAngles(float& yawDegrees, float& pitchDegrees);

// Movement, mouse look and the view matrix. Only while playing -- with the pause
// menu open this must not run, because it recentres the cursor every frame and
// would drag the pointer back to the middle of the screen under the menu.
void computeMatricesFromInputs();

// The projection on its own, rebuilt from the field of view, the aspect ratio
// and the view distance. Separate from the above and called every frame,
// including while paused, so the field of view slider is seen as it is dragged
// rather than only taking effect on resume.
void updateProjectionMatrix();

// Called as the pause menu closes. Two things go wrong without it: the frame
// time is measured from before the menu opened, so the first frame back moves
// the player by however long they spent in the menu times their speed; and the
// cursor has been sitting wherever it was clicked rather than at the centre, so
// the first mouse sample is a delta of half a screen and the view snaps.
void notifyResumed();

float getNormalRotation(float angle);
void printPositions(const char* message = "cls");
