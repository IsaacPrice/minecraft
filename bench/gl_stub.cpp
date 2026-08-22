// The benchmark links Chunk.cpp, which owns an Object, which is a GL handle.
// None of the timed paths draw anything, so the GL side is stubbed out rather
// than dragging a window and a context into a command line tool.
#include "../src/headers/Object.hpp"

GLuint programID = 0;

Object::~Object() {}
void Object::release() {}
void Object::Create(const std::vector<Vertex>&) {}
void Object::Draw() const {}
void InitRenderResources() {}
void BindTerrainTexture() {}
