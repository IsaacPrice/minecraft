#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <glm/glm.hpp>
#include "Object.hpp"
#include "FastNoise.hpp"
#include "BlockData.hpp"

using namespace std;
using namespace glm;

class Chunk {
public:
    Chunk();
    Chunk(int start_x, int start_y);

    void Generate(FastNoise &heightGen, FastNoise &gravel, FastNoise &dirt);
    void CreateObject();
    void Cleanup();
    bool isChunkSaved();
    void MakeVertexObject(Chunk &negativeX, Chunk &positiveX, Chunk &negativeZ, Chunk &positiveZ);
    void Draw();

    bool operator==(const Chunk &other);

    vec2 chunkPos;
    unsigned short blockMap[16][255][16];

private:
    Object chunk;
    vector<vec3> vertices;
    vector<vec2> uvCoords;
};

// Declare these functions in the header, but implement them in the .cpp file
vector<vec3> getSideVertex(float x, float y, float z, SIDE side);
vector<vec2> getTextureCoords(BLOCK block, SIDE side);

// Declare these variables as extern in the header
extern unsigned int Texture;
extern unsigned int TextureID;
extern float blockWidth;

#endif // CHUNK_HPP