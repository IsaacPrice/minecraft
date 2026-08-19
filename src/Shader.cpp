#include "headers/Shader.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // Returns false if the file could not be opened.
    bool readFile(const char* path, std::string& out)
    {
        std::ifstream stream(path, std::ios::in);
        if (!stream.is_open())
        {
            printf("Could not open %s. Is the working directory the project root?\n", path);
            return false;
        }

        std::stringstream sstr;
        sstr << stream.rdbuf();
        out = sstr.str();
        return true;
    }

    void printShaderLog(GLuint shaderID)
    {
        GLint logLength = 0;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);

        if (logLength > 0)
        {
            std::vector<char> message(logLength + 1);
            glGetShaderInfoLog(shaderID, logLength, NULL, &message[0]);
            printf("%s\n", &message[0]);
        }
    }
}

GLuint LoadShaders(const char* vertex_file_path, const char* fragment_file_path)
{
    std::string vertexShaderCode;
    std::string fragmentShaderCode;

    if (!readFile(vertex_file_path, vertexShaderCode))
        return 0;

    if (!readFile(fragment_file_path, fragmentShaderCode))
        return 0;

    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    printf("Compiling shader : %s\n", vertex_file_path);
    char const* VertexSourcePointer = vertexShaderCode.c_str();
    glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
    glCompileShader(VertexShaderID);
    printShaderLog(VertexShaderID);

    printf("Compiling shader : %s\n", fragment_file_path);
    char const* FragmentSourcePointer = fragmentShaderCode.c_str();
    glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
    glCompileShader(FragmentShaderID);
    printShaderLog(FragmentShaderID);

    printf("Linking program\n");
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);

    GLint logLength = 0;
    glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        std::vector<char> message(logLength + 1);
        glGetProgramInfoLog(ProgramID, logLength, NULL, &message[0]);
        printf("%s\n", &message[0]);
    }

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);

    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

    return ProgramID;
}
