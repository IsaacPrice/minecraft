#ifndef CHUNK_H
#define CHUNK_H

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <cstdlib>

#include "object.hpp"
#include "block_data.hpp"
#include "fast_noise.hpp"
#include "chunk_helper.hpp"


extern GLuint programID;


using namespace std;
using namespace glm;


class Chunk {
private:
    Object _chunk;

    vector<vec3> _vertices;
    vector<vec2> _uvCoords;


public:
    Chunk() {};
    Chunk(int start_x, int start_y);

    void Generate(FastNoise &heightGen, FastNoise &gravel, FastNoise &dirt);
    void CreateObject();
    void CleanupPrimatives();
    bool isChunkSaved();
    void MakeVertexObject(Chunk &negativeX, Chunk &positiveX, Chunk &negativeZ, Chunk &positiveZ);
    void Draw();

    bool operator==(const Chunk &other) {
        return chunkPos == other.chunkPos;
    }

    vec2 chunkPos;

    unsigned short blockMap[16][255][16] = { AIR };
};


#endif