#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern GLFWwindow* window;
extern const int width, height;

using namespace glm;

glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;
glm::vec3 position = glm::vec3( 0, 4, 0 ); 
float horizontalAngle = 3.14f;
float verticalAngle = 0.0f;
float initialFoV = 60.0f;

glm::mat4 getViewMatrix(){
	return ViewMatrix;
}
glm::mat4 getProjectionMatrix(){
	return ProjectionMatrix;
}
//glm::vec3 getPosition(){
//	return position;
//}

float speed = 3.0f; // 3 units / second
float mouseSpeed = 0.001f;

float aspectRatio = (float) width / (float) height;

float radian = 180.f / 3.14159265359f;

void computeMatricesFromInputs(){

	// glfwGetTime is called only once, the first time this function is called
	static double lastTime = glfwGetTime();

	// Compute time difference between current and last frame
	double currentTime = glfwGetTime();
	float deltaTime = float(currentTime - lastTime);

	// Get mouse position
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// Reset mouse position for next frame
	glfwSetCursorPos(window, width/2, height/2);

	// Compute new orientation
	horizontalAngle += mouseSpeed * float( width/2 - xpos );
	verticalAngle   += mouseSpeed * float( height/2 - ypos );

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	glm::vec3 direction(
		cos(verticalAngle) * sin(horizontalAngle), 
		sin(verticalAngle),
		cos(verticalAngle) * cos(horizontalAngle)
	);
	
	// Right vector
	glm::vec3 right = glm::vec3(
		sin(horizontalAngle - 3.14f/2.0f), 
		0,
		cos(horizontalAngle - 3.14f/2.0f)
	);
	
	// Up vector
	glm::vec3 up = glm::cross( right, direction );

	// Move forward
	if (glfwGetKey( window, GLFW_KEY_W ) == GLFW_PRESS){
		position += direction * deltaTime * speed;
	}
	// Move backward
	if (glfwGetKey( window, GLFW_KEY_S ) == GLFW_PRESS){
		position -= direction * deltaTime * speed;
	}
	// Strafe right
	if (glfwGetKey( window, GLFW_KEY_D ) == GLFW_PRESS){
		position += right * deltaTime * speed;
	}
	// Strafe left
	if (glfwGetKey( window, GLFW_KEY_A ) == GLFW_PRESS){
		position -= right * deltaTime * speed;
	}
	// Up
	if (glfwGetKey( window, GLFW_KEY_SPACE ) == GLFW_PRESS ){
		position.y +=  deltaTime * speed;
	}
	// Down
	if (glfwGetKey( window, GLFW_KEY_LEFT_SHIFT ) == GLFW_PRESS ){
		position.y -=  deltaTime * speed;
	}

	float FoV = initialFoV;// - 5 * glfwGetMouseWheel(); // Now GLFW 3 requires setting up a callback for this. It's a bit too complicated for this beginner's tutorial, so it's disabled instead.

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	ProjectionMatrix = glm::perspective(glm::radians(FoV), aspectRatio, 0.1f, 100.0f);
	// Camera matrix
	ViewMatrix       = glm::lookAt(
								position,           // Camera is here
								position+direction, // and looks here : at the same position, plus "direction"
								up                  // Head is up (set to 0,-1,0 to look upside-down)
						   );

	// For the next frame, the "last time" will be "now"
	lastTime = currentTime;
}

float getNormalRotation(float angle) {
	while (!(angle >= -180 && angle <= 180)) {
		if (angle < -180) {
			angle += 360;
		} else if (angle > 180) {
			angle -= 360;
		}
	}
	return angle;
}

void printPositions(const char* message = "cls") {
    system(message);
	printf("Position: (%f, %f, %f)\n", position.x, position.y, position.z);
	float horizontalDegrees = getNormalRotation(horizontalAngle * radian);
	float verticalDegrees = getNormalRotation(verticalAngle * radian);
    printf("Direction: (%f, %f)\n", horizontalDegrees, verticalDegrees);
	// Print which direction they are facing
	if (horizontalDegrees >= -45 && horizontalDegrees <= 45) {
		printf("Facing: East\n");
	} else if (horizontalDegrees >= 45 && horizontalDegrees <= 135) {
		printf("Facing: South\n");
	} else if (horizontalDegrees >= 135 || horizontalDegrees <= -135) {
		printf("Facing: West\n");
	} else if (horizontalDegrees >= -135 && horizontalDegrees <= -45) {
		printf("Facing: North\n");
	}

}
