#include "chunk_coord.hpp"

#include <iostream>
#include <unordered_map>
#include <utility>
#include <set>


using namespace std;


Chunk blankChunk;


class World {
private:
    uint64_t seed;
    unsigned short renderDistance;

    FastNoise heightMap;
    FastNoise gravel;
    FastNoise dirt;

    unordered_map<ChunkCoord, Chunk*> chunksToAdd;

public:
    World(unsigned long seed, unsigned short renderDistance); 

    void GenerateChunks();
    void UpdateChunks(vec3 playerPos); 
    void ChangeRenderDistance(unsigned short newRenderDistance);
};
