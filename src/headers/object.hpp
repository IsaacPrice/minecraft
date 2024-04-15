#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std;

// Function Prototype
GLuint loadBMP_custom(const char *imagepath);

class Object {
public:
    // Create the Object Class
    Object(const GLfloat vertex_data[], const GLfloat uv_data[], size_t vertex_size, size_t uv_size, GLuint programID, const char *image) {
        glGenVertexArrays(1, &VertexArrayID);
        glBindVertexArray(VertexArrayID);

        glGenBuffers(1, &vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertex_size, vertex_data, GL_STATIC_DRAW);

        glGenBuffers(1, &uvBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
        glBufferData(GL_ARRAY_BUFFER, uv_size, uv_data, GL_STATIC_DRAW);

        Texture = loadBMP_custom(image);
        TextureID = glGetUniformLocation(programID, "myTextureSampler");
        cout << Texture << " " << TextureID << endl;
    };  

    ~Object() {
        glDeleteBuffers(1, &vertexBuffer);
        glDeleteBuffers(1, &uvBuffer);
        glDeleteVertexArrays(1, &VertexArrayID);
        glDeleteTextures(1, &Texture);
    };

    void Draw() {
        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		glUniform1i(TextureID, 0);

        // Bind the wireframe
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // bind the uv
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
        glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // Draw the object
        glDrawArrays(GL_TRIANGLES, 0, 12*3); 

        // Close the stuff
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }

private:
    GLuint Texture;
    GLuint TextureID;
    GLuint VertexArrayID;
    GLuint vertexBuffer;
    GLuint uvBuffer;

    int vertices;
};

// TODO: Likely that this will be changed for minecraft
GLuint loadBMP_custom(const char * imagepath){

	printf("Reading image %s\n", imagepath);

	// Data read from the header of the BMP file
	unsigned char header[54];
	unsigned int dataPos;
	unsigned int imageSize;
	unsigned int width, height;
	// Actual RGB data
	unsigned char * data;

	// Open the file
	FILE * file = fopen(imagepath,"rb");
	if (!file){
		printf("%s could not be opened. Are you in the right directory ? \n", imagepath);
		getchar();
		return 0;
	}

	// Read the header, i.e. the 54 first bytes

	// If less than 54 bytes are read, problem
	if ( fread(header, 1, 54, file)!=54 ){ 
		printf("Not a correct BMP file\n");
		fclose(file);
		return 0;
	}
	// A BMP files always begins with "BM"
	if ( header[0]!='B' || header[1]!='M' ){
		printf("Not a correct BMP file\n");
		fclose(file);
		return 0;
	}
	// Make sure this is a 24bpp file
	if ( *(int*)&(header[0x1E])!=0  )         {printf("Not a correct BMP file\n");    fclose(file); return 0;}
	if ( *(int*)&(header[0x1C])!=24 )         {printf("Not a correct BMP file\n");    fclose(file); return 0;}

	// Read the information about the image
	dataPos    = *(int*)&(header[0x0A]);
	imageSize  = *(int*)&(header[0x22]);
	width      = *(int*)&(header[0x12]);
	height     = *(int*)&(header[0x16]);

	// Some BMP files are misformatted, guess missing information
	if (imageSize==0)    imageSize=width*height*3; // 3 : one byte for each Red, Green and Blue component
	if (dataPos==0)      dataPos=54; // The BMP header is done that way

	// Create a buffer
	data = new unsigned char [imageSize];

	// Read the actual data from the file into the buffer
	fread(data,1,imageSize,file);

	// Everything is in memory now, the file can be closed.
	fclose (file);

	// Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);
	
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);

	delete [] data;


	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);

	// Return the ID of the texture we just created
	return textureID;
}