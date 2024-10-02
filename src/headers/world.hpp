#include <set>
#include <thread>
#include <utility>
#include <typeinfo>
#include <iostream>
#include <unordered_map>
#include "ChunkCoord.hpp"

using namespace std;

mutex chunkMutex;
condition_variable chunkCondition;
Chunk blankChunk;

class World {
private:
    uint64_t _seed;
    unsigned short _renderDistance;

    FastNoise _heightMapNoise;
    FastNoise _gravelNoise;
    FastNoise _dirtNoise;

    vector<Chunk> tempChunks;

    void GenerateChunks();

public:
    World(unsigned long seed, unsigned short renderDistance);

    void UpdateChunks(vec3 playerPos);
    void changeRenderDistance(unsigned short newRenderDistance);
};
