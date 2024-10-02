#include "headers/World.hpp"


void World::GenerateChunks() 
{
    unique_lock<mutex> lock(chunkMutex);
    int halfRenderDistance = _renderDistance / 2;
    int rendistsquare = _renderDistance * _renderDistance;

    vector<thread> threads(rendistsquare);

    for (int i = 0, k = 0; i < _renderDistance; i++) 
    {
        for (int j = 0; j < _renderDistance; j++) 
        {
            ChunkCoord chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
            Chunk& newChunk = chunks[chunkPos];
            newChunk.chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
            threads[k] = thread([&]{newChunk.Generate(_heightMapNoise, _gravelNoise, _dirtNoise);});
            chunks[chunkPos] = std::move(newChunk);
            k++;
        }
    }

    for(int i = 0; i < threads.size(); i++) 
    {
        threads[i].join();
        cout << "Chunk done\n";
    }
    
    cout << "Finished Generating Terrain" << endl;

    for (int i = 0; i < _renderDistance; i++) 
    {
        for (int j = 0; j < _renderDistance; j++) 
        {
            ChunkCoord chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
            chunks[chunkPos].MakeVertexObject(
                (i == 0) ? blankChunk : chunks[{chunkPos.x - 1, chunkPos.z}],
                (i == _renderDistance - 1) ? blankChunk : chunks[{chunkPos.x + 1, chunkPos.z}],
                (j == 0) ? blankChunk : chunks[{chunkPos.x, chunkPos.z - 1}],
                (j == _renderDistance - 1) ? blankChunk : chunks[{chunkPos.x, chunkPos.z + 1}]
            );
            chunks[chunkPos].CreateObject();
            chunks[chunkPos].Cleanup();
        }
    }

    cout << "Finished Meshing Terrain" << endl;
    lock.unlock();
    chunkCondition.notify_one();
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
}


void World::UpdateChunks(vec3 playerPos) 
{
    unique_lock<mutex> lock(chunkMutex);

    int playerChunkX = static_cast<int>(playerPos.x);
    int playerChunkZ = static_cast<int>(playerPos.z);
    int halfRenderDistance = _renderDistance / 2;

    std::set<ChunkCoord> existingChunks;
    for (auto& pair : chunks) 
    {
        existingChunks.insert(pair.first);
    }

    std::vector<ChunkCoord> chunksToRemove;
    std::vector<ChunkCoord> chunksToCreate;

    for (int i = -halfRenderDistance; i <= halfRenderDistance; i++) 
    {
        for (int j = -halfRenderDistance; j <= halfRenderDistance; j++) 
        {
            ChunkCoord searchCoord = {i + playerChunkX, j + playerChunkZ};

            if (existingChunks.find(searchCoord) == existingChunks.end()) 
            {
                chunksToCreate.push_back(searchCoord);
            }
            existingChunks.erase(searchCoord);
        }
    }

    chunksToRemove.assign(existingChunks.begin(), existingChunks.end());

    for (auto& coord : chunksToRemove) 
    {
        chunks.erase(coord);
    }

    vector<thread> threads(chunksToCreate.size());

    int i = 0;
    for (auto& coord : chunksToCreate) 
    {
        Chunk& newChunk = chunks[coord];
        newChunk.chunkPos = {coord.x, coord.z};
        threads[i] = thread([&]{newChunk.Generate(_heightMapNoise, _gravelNoise, _dirtNoise);});
        printf("Creating chunk at %d, %d\n", coord.x, coord.z);
        chunks[coord] = std::move(newChunk);
        i++;
    }

    for(int i = 0; i < threads.size(); i++) 
    {
        threads[i].join();
        cout << "Chunk done\n";
    }

    for (auto& coord : chunksToCreate) 
    {
        chunks[coord].MakeVertexObject(
            blankChunk,
            blankChunk,
            blankChunk,
            blankChunk
        );
        chunks[coord].CreateObject();
        chunks[coord].Cleanup();
    }

    lock.unlock();
}


void World::changeRenderDistance(unsigned short newRenderDistance) 
{
    _renderDistance = newRenderDistance;
    GenerateChunks();
}