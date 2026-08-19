#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ChunkCoord.hpp"

extern std::mutex chunkMutex;

// Stand-in neighbour used when meshing a chunk at the edge of the loaded area.
extern Chunk blankChunk;

class World {
private:
    uint64_t _seed;
    unsigned short _renderDistance;

    FastNoise _heightMapNoise;
    FastNoise _gravelNoise;
    FastNoise _dirtNoise;

    // Chunks are generated and meshed on these workers. Only the OpenGL upload
    // happens on the render thread, so crossing a chunk border no longer stalls
    // the frame while a strip of terrain is built.
    std::vector<std::thread> _workers;
    std::mutex _queueMutex;
    std::condition_variable _queueCondition;

    std::deque<ChunkCoord> _pending;                  // requested, not yet built
    std::set<ChunkCoord> _inFlight;                   // requested or being built
    std::vector<std::pair<ChunkCoord, Chunk>> _ready; // built, awaiting GL upload
    bool _stopWorkers = false;

    void StartWorkers();
    void StopWorkers();
    void WorkerLoop();

    void GenerateChunks();
    std::set<ChunkCoord> ChunksInRangeOf(int centreX, int centreZ) const;

public:
    World(unsigned long seed, unsigned short renderDistance);
    ~World();

    // Owns threads and GL-backed chunks; copying it makes no sense.
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void UpdateChunks(glm::vec3 playerPos);
    void changeRenderDistance(unsigned short newRenderDistance);
};
