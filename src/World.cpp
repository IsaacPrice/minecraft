#include "headers/World.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

// Definitions for the globals declared extern in World.hpp / ChunkCoord.hpp.
// These must live in exactly one translation unit.
std::unordered_map<ChunkCoord, Chunk> chunks;
std::mutex chunkMutex;
std::condition_variable chunkCondition;
Chunk blankChunk;


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


// Builds the starting world up front. This one blocks, because there is nothing
// worth rendering until it finishes.
void World::GenerateChunks()
{
    std::unique_lock<std::mutex> lock(chunkMutex);

    std::set<ChunkCoord> wanted = ChunksInRangeOf(0, 0);
    std::vector<ChunkCoord> coords(wanted.begin(), wanted.end());

    std::vector<Chunk*> pending;
    pending.reserve(coords.size());

    for (const ChunkCoord& coord : coords)
    {
        Chunk& chunk = chunks[coord];
        chunk.chunkPos = {static_cast<float>(coord.x), static_cast<float>(coord.z)};
        pending.push_back(&chunk);
    }

    parallelFor(pending.size(), [&](size_t index)
    {
        pending[index]->Generate(_heightMapNoise, _gravelNoise, _dirtNoise);
    });

    // Meshing only reads neighbours, so it parallelises too now that every
    // block map is complete.
    parallelFor(coords.size(), [&](size_t index)
    {
        const ChunkCoord& coord = coords[index];

        auto neighbour = [&](int dx, int dz) -> Chunk&
        {
            auto it = chunks.find({coord.x + dx, coord.z + dz});
            return (it == chunks.end()) ? blankChunk : it->second;
        };

        chunks[coord].MakeVertexObject(
            neighbour(-1, 0),
            neighbour(1, 0),
            neighbour(0, -1),
            neighbour(0, 1)
        );
    });

    // CreateObject talks to OpenGL, so it stays on this thread.
    for (const ChunkCoord& coord : coords)
    {
        chunks[coord].CreateObject();
        chunks[coord].Cleanup();
    }

    lock.unlock();
    chunkCondition.notify_one();
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
    // Each worker samples from its own copies. FastNoise reads are const, but
    // separate copies keep the workers off the same cache lines.
    FastNoise heightMapNoise = _heightMapNoise;
    FastNoise gravelNoise = _gravelNoise;
    FastNoise dirtNoise = _dirtNoise;

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

        Chunk chunk;
        chunk.chunkPos = {static_cast<float>(coord.x), static_cast<float>(coord.z)};
        chunk.Generate(heightMapNoise, gravelNoise, dirtNoise);
        chunk.MakeVertexObject(blankChunk, blankChunk, blankChunk, blankChunk);

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

    _heightMapNoise.SetSeed(seed);
    _heightMapNoise.SetNoiseType(FastNoise::PerlinFractal);
    _heightMapNoise.SetFrequency(0.00153f);
    _heightMapNoise.SetFractalOctaves(16);

    _gravelNoise.SetSeed(seed);
    _gravelNoise.SetNoiseType(FastNoise::Cellular);
    _gravelNoise.SetFrequency(0.03f);
    _gravelNoise.SetFractalOctaves(6);
    _gravelNoise.SetFractalLacunarity(1.86f);
    _gravelNoise.SetFractalGain(3.0f);
    _gravelNoise.SetCellularReturnType(FastNoise::Distance2Add);

    _dirtNoise.SetSeed(seed + 1);
    _dirtNoise.SetNoiseType(FastNoise::Cellular);
    _dirtNoise.SetFrequency(0.03f);
    _dirtNoise.SetFractalOctaves(6);
    _dirtNoise.SetFractalLacunarity(1.86f);
    _dirtNoise.SetFractalGain(3.0f);
    _dirtNoise.SetCellularReturnType(FastNoise::Distance2Add);

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

    _renderDistance = newRenderDistance;
    GenerateChunks();
    StartWorkers();
}
