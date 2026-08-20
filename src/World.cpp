#include "headers/World.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

// Definitions for the globals declared extern in World.hpp / ChunkCoord.hpp.
// These must live in exactly one translation unit.
std::unordered_map<ChunkCoord, Chunk> chunks;
std::mutex chunkMutex;


namespace
{
    // Uploading a mesh blocks the driver, so only a few chunks are handed to
    // OpenGL per frame. The rest wait in the ready queue for later frames.
    const size_t MAX_UPLOADS_PER_FRAME = 2;

    // Chunk coordinates must round towards negative infinity. Truncating with a
    // cast makes chunk 0 twice as wide as the others and shifts every chunk on
    // the negative side of the origin by one.
    int chunkCoordFor(float worldPos)
    {
        return static_cast<int>(std::floor(worldPos));
    }

    // Runs work(i) for every i in [0, count) across a bounded set of workers.
    template <typename Fn>
    void parallelFor(size_t count, Fn work)
    {
        if (count == 0)
            return;

        unsigned hardwareThreads = std::thread::hardware_concurrency();
        if (hardwareThreads == 0)
            hardwareThreads = 4;

        size_t workerCount = std::min(static_cast<size_t>(hardwareThreads), count);

        std::atomic<size_t> nextIndex(0);
        std::vector<std::thread> workers;
        workers.reserve(workerCount);

        for (size_t i = 0; i < workerCount; i++)
        {
            workers.emplace_back([&]
            {
                for (size_t index = nextIndex++; index < count; index = nextIndex++)
                {
                    work(index);
                }
            });
        }

        for (std::thread& worker : workers)
        {
            worker.join();
        }
    }
}


std::set<ChunkCoord> World::ChunksInRangeOf(int centreX, int centreZ) const
{
    int halfRenderDistance = _renderDistance / 2;

    std::set<ChunkCoord> wanted;
    for (int i = -halfRenderDistance; i <= halfRenderDistance; i++)
    {
        for (int j = -halfRenderDistance; j <= halfRenderDistance; j++)
        {
            wanted.insert({i + centreX, j + centreZ});
        }
    }
    return wanted;
}


// Hands back the block map for a chunk, generating it the first time it is
// asked for. Several workers ask for the same coordinate constantly, because
// every chunk needs the four around it in order to mesh, so the flag makes sure
// exactly one of them does the work and the rest wait for it rather than
// generating their own copy.
BlockMapPtr World::EnsureBlockMap(const ChunkCoord& coord, const TerrainGen& terrain)
{
    std::shared_ptr<BlockMapSlot> slot;
    {
        std::lock_guard<std::mutex> lock(_blockMapMutex);

        std::shared_ptr<BlockMapSlot>& entry = _blockMaps[coord];
        if (!entry)
            entry = std::make_shared<BlockMapSlot>();

        slot = entry;
    }

    // Deliberately outside the lock. Generation is slow, and holding the map
    // mutex across it would serialise every worker behind whichever one happens
    // to be generating.
    std::call_once(slot->generated, [&]
    {
        BlockMapPtr map = std::make_shared<BlockMap>();
        terrain.GenerateChunk(*map, coord.x, coord.z);
        slot->map = map;
    });

    return slot->map;
}


// Drops cached block maps that no chunk still needs. Workers hold shared_ptrs
// to the maps they are meshing against, so erasing an entry here only releases
// this cache's reference and never pulls a map out from under a worker.
//
// The kept region is the square of loaded chunks grown by one ring, because
// meshing a chunk at the edge of the render distance reads the chunk just
// outside it. It is passed as bounds rather than as a set of coordinates since
// this runs every frame and building that set would cost more than the walk.
void World::EvictBlockMaps(int centreX, int centreZ, int radius)
{
    std::lock_guard<std::mutex> lock(_blockMapMutex);

    for (auto it = _blockMaps.begin(); it != _blockMaps.end(); )
    {
        const ChunkCoord& coord = it->first;

        bool keep = coord.x >= centreX - radius && coord.x <= centreX + radius &&
                    coord.z >= centreZ - radius && coord.z <= centreZ + radius;

        if (keep)
            ++it;
        else
            it = _blockMaps.erase(it);
    }
}


Chunk World::BuildChunk(const ChunkCoord& coord, const TerrainGen& terrain)
{
    BlockMapPtr self = EnsureBlockMap(coord, terrain);
    BlockMapPtr negativeX = EnsureBlockMap({coord.x - 1, coord.z}, terrain);
    BlockMapPtr positiveX = EnsureBlockMap({coord.x + 1, coord.z}, terrain);
    BlockMapPtr negativeZ = EnsureBlockMap({coord.x, coord.z - 1}, terrain);
    BlockMapPtr positiveZ = EnsureBlockMap({coord.x, coord.z + 1}, terrain);

    Chunk chunk;
    chunk.chunkPos = {static_cast<float>(coord.x), static_cast<float>(coord.z)};
    chunk.blocks = self;
    chunk.MakeVertexObject(*negativeX, *positiveX, *negativeZ, *positiveZ);

    return chunk;
}


// Builds the starting world up front. This one blocks, because there is nothing
// worth rendering until it finishes.
void World::GenerateChunks()
{
    std::set<ChunkCoord> wanted = ChunksInRangeOf(0, 0);
    std::vector<ChunkCoord> coords(wanted.begin(), wanted.end());
    std::vector<Chunk> built(coords.size());

    parallelFor(coords.size(), [&](size_t index)
    {
        built[index] = BuildChunk(coords[index], _terrain);
    });

    // CreateObject talks to OpenGL, so it stays on this thread.
    std::lock_guard<std::mutex> lock(chunkMutex);
    for (size_t index = 0; index < coords.size(); index++)
    {
        Chunk& chunk = chunks[coords[index]] = std::move(built[index]);
        chunk.CreateObject();
        chunk.Cleanup();
    }
}


void World::StartWorkers()
{
    unsigned hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads == 0)
        hardwareThreads = 4;

    // Leave a core for the render thread.
    unsigned workerCount = (hardwareThreads > 1) ? hardwareThreads - 1 : 1;

    _stopWorkers = false;
    _workers.reserve(workerCount);
    for (unsigned i = 0; i < workerCount; i++)
    {
        _workers.emplace_back([this] { WorkerLoop(); });
    }
}


void World::StopWorkers()
{
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _stopWorkers = true;
    }
    _queueCondition.notify_all();

    for (std::thread& worker : _workers)
    {
        if (worker.joinable())
            worker.join();
    }
    _workers.clear();
}


void World::WorkerLoop()
{
    for (;;)
    {
        ChunkCoord coord;
        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _queueCondition.wait(lock, [this] { return _stopWorkers || !_pending.empty(); });

            if (_stopWorkers)
                return;

            coord = _pending.front();
            _pending.pop_front();
        }

        Chunk chunk = BuildChunk(coord, _terrain);

        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            _ready.emplace_back(coord, std::move(chunk));
        }
    }
}


World::World(unsigned long seed, unsigned short renderDistance)
{
    this->_seed = seed;
    this->_renderDistance = renderDistance;

    _terrain = TerrainGen(seed);

    GenerateChunks();
    StartWorkers();
}


World::~World()
{
    StopWorkers();
}


void World::UpdateChunks(glm::vec3 playerPos)
{
    int playerChunkX = chunkCoordFor(playerPos.x);
    int playerChunkZ = chunkCoordFor(playerPos.z);

    std::set<ChunkCoord> wanted = ChunksInRangeOf(playerChunkX, playerChunkZ);

    // Drop anything that has moved outside the render distance.
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        for (auto it = chunks.begin(); it != chunks.end(); )
        {
            if (wanted.find(it->first) == wanted.end())
                it = chunks.erase(it);
            else
                ++it;
        }
    }

    EvictBlockMaps(playerChunkX, playerChunkZ, _renderDistance / 2 + 1);

    // Which of the wanted chunks are not loaded yet. Only this thread touches
    // the chunk map, so no lock is needed to read it here.
    std::vector<ChunkCoord> missing;
    for (const ChunkCoord& coord : wanted)
    {
        if (chunks.find(coord) == chunks.end())
            missing.push_back(coord);
    }

    std::vector<std::pair<ChunkCoord, Chunk>> toUpload;
    bool queuedWork = false;

    {
        std::lock_guard<std::mutex> lock(_queueMutex);

        // Requests the player has already outrun are not worth building.
        _pending.erase(
            std::remove_if(_pending.begin(), _pending.end(),
                [&](const ChunkCoord& coord)
                {
                    if (wanted.find(coord) != wanted.end())
                        return false;
                    _inFlight.erase(coord);
                    return true;
                }),
            _pending.end());

        for (const ChunkCoord& coord : missing)
        {
            if (_inFlight.find(coord) != _inFlight.end())
                continue;

            _inFlight.insert(coord);
            _pending.push_back(coord);
            queuedWork = true;
        }

        size_t uploadCount = std::min(MAX_UPLOADS_PER_FRAME, _ready.size());
        for (size_t i = 0; i < uploadCount; i++)
        {
            toUpload.push_back(std::move(_ready[i]));
            _inFlight.erase(toUpload.back().first);
        }
        _ready.erase(_ready.begin(), _ready.begin() + uploadCount);
    }

    if (queuedWork)
        _queueCondition.notify_all();

    // The only part of chunk streaming that has to run on the render thread.
    for (auto& entry : toUpload)
    {
        if (wanted.find(entry.first) == wanted.end())
            continue;

        std::lock_guard<std::mutex> lock(chunkMutex);
        auto inserted = chunks.emplace(entry.first, std::move(entry.second));
        if (inserted.second)
        {
            inserted.first->second.CreateObject();
            inserted.first->second.Cleanup();
        }
    }
}


void World::changeRenderDistance(unsigned short newRenderDistance)
{
    StopWorkers();

    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _pending.clear();
        _inFlight.clear();
        _ready.clear();
    }

    {
        std::lock_guard<std::mutex> lock(_blockMapMutex);
        _blockMaps.clear();
    }

    _renderDistance = newRenderDistance;
    GenerateChunks();
    StartWorkers();
}
