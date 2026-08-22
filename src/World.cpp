#include "headers/World.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

// Definitions for the globals declared extern in World.hpp / ChunkCoord.hpp.
// These must live in exactly one translation unit.
std::unordered_map<ChunkCoord, Chunk> chunks;
std::mutex chunkMutex;


namespace
{
    // Uploading a mesh blocks the driver, so uploads are capped per frame. This
    // used to be a flat two chunks, which is where most of the holes in the
    // world came from: two a frame is 120 a second, and flying at the default
    // speed asks for closer to two hundred, so the leading edge could never
    // catch up and the gap in front of the player stayed empty.
    //
    // A time budget instead lets a frame upload as many small meshes as it can
    // afford and still stop before it misses vsync. The count is a backstop for
    // the case where the clock is coarser than the work.
    const double UPLOAD_BUDGET_MS = 2.0;
    const size_t MAX_UPLOADS_PER_FRAME = 64;

    // Whether a chunk lies inside the square of the given radius about a centre.
    bool inSquareAround(const ChunkCoord& coord, int centreX, int centreZ, int radius)
    {
        return coord.x >= centreX - radius && coord.x <= centreX + radius &&
               coord.z >= centreZ - radius && coord.z <= centreZ + radius;
    }

    // Squared distance in chunks, which is all the ordering needs.
    long long chunkDistanceSquared(const ChunkCoord& coord, int centreX, int centreZ)
    {
        long long dx = coord.x - centreX;
        long long dz = coord.z - centreZ;
        return dx * dx + dz * dz;
    }

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


// How much world is built before the first frame is drawn. Only enough to stand
// on and look at: the rest streams in from the workers, nearest first, and the
// fog hides the edge while it does.
//
// This used to be the entire render distance. At 64 that is 4225 chunks built
// on the main thread before the window drew anything, which is most of a minute
// of CPU work and, with every block map held at once, hundreds of megabytes.
const int World::STARTING_RADIUS;


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

    // Nothing reads a chunk's blocks once it is meshed, and holding the
    // reference kept every loaded chunk's block map alive for as long as the
    // chunk was loaded. At a render distance of 64 that is 4225 maps at 128 KB
    // each, half a gigabyte pinned to answer questions nobody asks. The
    // generator's own cache still holds the map for as long as a neighbour
    // might need it to mesh against.
    chunk.blocks.reset();

    return chunk;
}


// Builds the ground around the spawn point up front. This one blocks, because
// there is nothing worth rendering until it finishes -- but it only covers
// STARTING_RADIUS, and the streaming path fills in the rest.
void World::GenerateChunks()
{
    int radius = std::min<int>(STARTING_RADIUS, LoadRadius());

    std::vector<ChunkCoord> coords;
    for (int x = -radius; x <= radius; x++)
    {
        for (int z = -radius; z <= radius; z++)
        {
            coords.push_back({x, z});
        }
    }

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

            coord = _pending.back();
            _pending.pop_back();
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


// Works out which chunks are missing around the player and queues them nearest
// first. Only called when the answer can have changed -- when the player crosses
// into a new chunk, or when the workers have run dry -- because the walk itself
// is over the whole loaded square.
void World::QueueMissingChunks(int centreX, int centreZ)
{
    int radius = LoadRadius();

    std::vector<ChunkCoord> missing;
    for (int x = -radius; x <= radius; x++)
    {
        for (int z = -radius; z <= radius; z++)
        {
            ChunkCoord coord{x + centreX, z + centreZ};
            if (chunks.find(coord) == chunks.end())
                missing.push_back(coord);
        }
    }

    std::lock_guard<std::mutex> lock(_queueMutex);

    // Everything still only queued is dropped and rebuilt against the new
    // centre, so the ordering below reflects where the player is now. Requests
    // a worker has already picked up, or has finished and left in _ready, stay
    // in _inFlight and are not queued twice.
    for (const ChunkCoord& coord : _pending)
        _inFlight.erase(coord);
    _pending.clear();

    for (const ChunkCoord& coord : missing)
    {
        if (_inFlight.find(coord) != _inFlight.end())
            continue;

        _inFlight.insert(coord);
        _pending.push_back(coord);
    }

    // Farthest first, so the back of the vector -- which is where workers take
    // from -- is the chunk nearest the player.
    std::sort(_pending.begin(), _pending.end(),
        [&](const ChunkCoord& a, const ChunkCoord& b)
        {
            return chunkDistanceSquared(a, centreX, centreZ) >
                   chunkDistanceSquared(b, centreX, centreZ);
        });
}


// Takes the finished meshes off the ready queue, nearest to the player first.
// How many of them actually reach OpenGL this frame is decided by the caller's
// time budget; the rest are handed back.
std::vector<std::pair<ChunkCoord, Chunk>> World::TakeChunksToUpload(int centreX, int centreZ)
{
    std::lock_guard<std::mutex> lock(_queueMutex);

    if (_ready.empty())
        return std::vector<std::pair<ChunkCoord, Chunk>>();

    std::sort(_ready.begin(), _ready.end(),
        [&](const std::pair<ChunkCoord, Chunk>& a, const std::pair<ChunkCoord, Chunk>& b)
        {
            return chunkDistanceSquared(a.first, centreX, centreZ) <
                   chunkDistanceSquared(b.first, centreX, centreZ);
        });

    size_t count = std::min(MAX_UPLOADS_PER_FRAME, _ready.size());

    std::vector<std::pair<ChunkCoord, Chunk>> taken;
    taken.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        taken.push_back(std::move(_ready[i]));
        _inFlight.erase(taken.back().first);
    }
    _ready.erase(_ready.begin(), _ready.begin() + count);

    return taken;
}


void World::UpdateChunks(glm::vec3 playerPos)
{
    int playerChunkX = chunkCoordFor(playerPos.x);
    int playerChunkZ = chunkCoordFor(playerPos.z);

    int unloadRadius = UnloadRadius();

    // Drop anything that has moved past the unload radius. Tested as bounds
    // rather than against a set of wanted coordinates: this runs every frame,
    // and building that set was thousands of tree insertions a frame for an
    // answer that four comparisons give.
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        for (auto it = chunks.begin(); it != chunks.end(); )
        {
            if (inSquareAround(it->first, playerChunkX, playerChunkZ, unloadRadius))
                ++it;
            else
                it = chunks.erase(it);
        }
    }

    EvictBlockMaps(playerChunkX, playerChunkZ, unloadRadius + 1);

    // Requeue when the player crosses a chunk border, and also whenever the
    // workers have nothing left to do -- which catches the case where a request
    // was dropped for being out of range and then came back into it.
    bool centreMoved = !_hasCentre || playerChunkX != _centreX || playerChunkZ != _centreZ;

    bool workersIdle;
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        workersIdle = _pending.empty() && _inFlight.empty();
    }

    if (centreMoved || workersIdle)
    {
        _centreX = playerChunkX;
        _centreZ = playerChunkZ;
        _hasCentre = true;

        QueueMissingChunks(playerChunkX, playerChunkZ);
        _queueCondition.notify_all();
    }

    // The only part of chunk streaming that has to run on the render thread.
    std::vector<std::pair<ChunkCoord, Chunk>> toUpload = TakeChunksToUpload(playerChunkX, playerChunkZ);

    std::chrono::steady_clock::time_point uploadStart = std::chrono::steady_clock::now();

    size_t uploaded = 0;
    for (auto& entry : toUpload)
    {
        uploaded++;

        if (!inSquareAround(entry.first, playerChunkX, playerChunkZ, unloadRadius))
            continue;

        {
            std::lock_guard<std::mutex> lock(chunkMutex);
            auto inserted = chunks.emplace(entry.first, std::move(entry.second));
            if (inserted.second)
            {
                inserted.first->second.CreateObject();
                inserted.first->second.Cleanup();
            }
        }

        double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - uploadStart).count();
        if (elapsedMs >= UPLOAD_BUDGET_MS)
            break;
    }

    // Whatever the budget did not reach goes back on the ready queue for the
    // next frame, rather than being dropped and generated again from scratch.
    if (uploaded < toUpload.size())
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        for (size_t i = uploaded; i < toUpload.size(); i++)
        {
            _inFlight.insert(toUpload[i].first);
            _ready.push_back(std::move(toUpload[i]));
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
