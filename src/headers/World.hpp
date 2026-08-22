#pragma once

#include <condition_variable>
#include <cstdint>
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

    // Nearest last, so a worker takes from the back and always builds the chunk
    // the player is closest to. This was a plain FIFO filled in set order, which
    // is sorted by x then z: chunks were built in scanline order across the
    // loaded square, so flying forwards left a hole directly ahead while the
    // workers filled in ground off to one side.
    std::vector<ChunkCoord> _pending;                 // requested, not yet built
    std::set<ChunkCoord> _inFlight;                   // requested or being built
    std::vector<std::pair<ChunkCoord, Chunk>> _ready; // built, awaiting GL upload
    bool _stopWorkers = false;

    // The chunk the player was in when the wanted set was last worked out.
    // Rebuilding that set costs a walk over the whole loaded square, and at a
    // render distance of 64 that is 4225 coordinates; doing it every frame,
    // through a std::set, was a measurable slice of the frame on its own.
    int _centreX = 0;
    int _centreZ = 0;
    bool _hasCentre = false;

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

    // Chunks built before the first frame is drawn, as a radius about spawn.
    // Everything past this streams in while the player is already flying.
    static const int STARTING_RADIUS = 4;

    void GenerateChunks();

    // Chunks load out to this radius and are not dropped until they pass the
    // unload one. The gap is deliberate: with a single radius, flying forwards
    // unloaded the trailing edge on the very frame it went out of range while
    // the leading edge still had a queue of chunks to build, so the loaded
    // region was always smaller than it should be in the direction of travel.
    int LoadRadius() const { return _renderDistance / 2; }
    int UnloadRadius() const { return _renderDistance / 2 + 2; }

    void QueueMissingChunks(int centreX, int centreZ);
    std::vector<std::pair<ChunkCoord, Chunk>> TakeChunksToUpload(int centreX, int centreZ);

public:
    World(unsigned long seed, unsigned short renderDistance);
    ~World();

    // Owns threads and GL-backed chunks; copying it makes no sense.
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void UpdateChunks(glm::vec3 playerPos);
    void changeRenderDistance(unsigned short newRenderDistance);

    // The loaded square runs this many chunks out from the player in each
    // direction. Rendering derives its far plane and fog band from it.
    float LoadedRadius() const { return _renderDistance / 2.0f; }
};
