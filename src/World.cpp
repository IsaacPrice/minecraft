#include "headers/World.h"


World::World(unsigned long seed, unsigned short renderDistance) {
    this->seed = seed;
    this->renderDistance = renderDistance;

    heightMap.SetSeed(seed);
    heightMap.SetNoiseType(FastNoise::PerlinFractal);
    heightMap.SetFrequency(0.00153f);
    heightMap.SetFractalOctaves(16);

    gravel.SetSeed(seed);
    gravel.SetNoiseType(FastNoise::Cellular);
    gravel.SetFrequency(0.03f);
    gravel.SetFractalOctaves(6);
    gravel.SetFractalLacunarity(1.86f);
    gravel.SetFractalGain(3.0f);
    gravel.SetCellularReturnType(FastNoise::Distance2Add);

    dirt.SetSeed(seed + 1);
    dirt.SetNoiseType(FastNoise::Cellular);
    dirt.SetFrequency(0.03f);
    dirt.SetFractalOctaves(6);
    dirt.SetFractalLacunarity(1.86f);
    dirt.SetFractalGain(3.0f);
    dirt.SetCellularReturnType(FastNoise::Distance2Add);
    
    GenerateChunks();
}


void World::GenerateChunks() 
{
    int halfRenderDistance = renderDistance / 2;
    int rendistsquare = renderDistance * renderDistance;

    for (int i = 0; i < renderDistance; i++) 
    {
        for (int j = 0; j < renderDistance; j++) 
        {
            ChunkCoord chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
            Chunk* newChunk = new Chunk();

            newChunk->chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
            newChunk->Generate(heightMap, gravel, dirt);

            newChunk->MakeVertexObject(blankChunk, blankChunk, blankChunk, blankChunk);
            newChunk->CreateObject();
            newChunk->CleanupPrimatives();
            
            chunksToAdd[chunkPos] = newChunk;
        }
    }
}


// void World::UpdateChunks(vec3 playerPos) 
// {
//     int playerChunkX = static_cast<int>(playerPos.x);
//     int playerChunkZ = static_cast<int>(playerPos.z);
//     int halfRenderDistance = renderDistance / 2;

//     std::set<ChunkCoord> existingChunks;
//     for (auto& pair : chunks) 
//     {
//         existingChunks.insert(pair.first);
//     }

//     std::vector<ChunkCoord> chunksToRemove;
//     std::vector<ChunkCoord> chunksToCreate;

//     for (int i = -halfRenderDistance; i <= halfRenderDistance; i++) 
//     {
//         for (int j = -halfRenderDistance; j <= halfRenderDistance; j++) 
//         {
//             ChunkCoord searchCoord = {i + playerChunkX, j + playerChunkZ};

//             if (existingChunks.find(searchCoord) == existingChunks.end()) 
//             {
//                 chunksToCreate.push_back(searchCoord);
//             }
//             existingChunks.erase(searchCoord); 
//         }
//     }

//     chunksToRemove.assign(existingChunks.begin(), existingChunks.end());

//     for (auto& coord : chunksToRemove) {
//         chunks.erase(coord);
//     }


//     for (auto& coord : chunksToCreate) {
//         Chunk& newChunk = chunks[coord];
//         newChunk.chunkPos = {coord.x, coord.z};
//         newChunk.Generate(heightMap, gravel, dirt);
//         printf("Creating chunk at %d, %d\n", coord.x, coord.z);
//         chunks[coord] = std::move(newChunk);
//     }

//     for (auto& coord : chunksToCreate) {
//         chunks[coord].MakeVertexObject(
//             blankChunk,
//             blankChunk,
//             blankChunk,
//             blankChunk
//         );
//         chunks[coord].CreateObject();
//         chunks[coord].CleanupPrimatives();
//     }
// }


void World::ChangeRenderDistance(unsigned short newRenderDistance)
{
    renderDistance = newRenderDistance;
    GenerateChunks();
}