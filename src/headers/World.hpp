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
#include "TerrainGen.hpp"

extern std::mutex chunkMutex;

class World {
private:
    uint64_t _seed;
    unsigned short _renderDistance;

    TerrainGen _terrain;

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

    // Block maps for chunks that have been generated but not necessarily
    // meshed. Meshing a chunk reads the four chunks around it, so those have to
    // exist as block maps first; caching them here means each one is generated
    // once and then reused, both by its own mesh and by its neighbours.
    //
    // Guarded by its own mutex rather than _queueMutex, because generation runs
    // while the entry is held and would otherwise block the queue for as long
    // as it takes.
    struct BlockMapSlot
    {
        std::once_flag generated;
        BlockMapPtr map;
    };

    std::mutex _blockMapMutex;
    std::unordered_map<ChunkCoord, std::shared_ptr<BlockMapSlot>> _blockMaps;

    BlockMapPtr EnsureBlockMap(const ChunkCoord& coord, const TerrainGen& terrain);
    void EvictBlockMaps(int centreX, int centreZ, int radius);

    void StartWorkers();
    void StopWorkers();
    void WorkerLoop();

    Chunk BuildChunk(const ChunkCoord& coord, const TerrainGen& terrain);

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
