#include "chunk_coord.hpp"

#include <typeinfo>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <thread>
#include <set>

using namespace std;

mutex chunkMutex;
condition_variable chunkCondition;

Chunk blankChunk;

int totalDeleted = 0;

class World {
public:
    World(unsigned long seed, unsigned short renderDistance) {
        this->seed = seed;
        this->renderDistance = renderDistance;

        // Set the noise settings
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
        

        // Generate the chunks
        GenerateChunks();
    }

    void GenerateChunks() {
        unique_lock<mutex> lock(chunkMutex);
        int halfRenderDistance = renderDistance / 2;
        int rendistsquare = renderDistance * renderDistance;

        vector<thread> threads(rendistsquare);

        // First pass: Generate chunk data
        for (int i = 0, k = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                ChunkCoord chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
                Chunk& newChunk = chunks[chunkPos];
                newChunk.chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
                //newChunk.Generate(heightMap, gravel, dirt);
                threads[k] = thread([&]{newChunk.Generate(heightMap, gravel, dirt);});
                chunks[chunkPos] = std::move(newChunk);
                k++;
            }
        }

        for(int i = 0; i < threads.size(); i++) {
            threads[i].join();
            cout << "Chunk done\n";
        }
        
        cout << "Finished Generating Terrain" << endl;

        // Second pass: Generate VBOs
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                ChunkCoord chunkPos = {i - halfRenderDistance, j - halfRenderDistance};
                // Correct the neighbor references by correctly adjusting indices
                chunks[chunkPos].MakeVertexObject(
                    (i == 0) ? blankChunk : chunks[{chunkPos.x - 1, chunkPos.z}],
                    (i == renderDistance - 1) ? blankChunk : chunks[{chunkPos.x + 1, chunkPos.z}],
                    (j == 0) ? blankChunk : chunks[{chunkPos.x, chunkPos.z - 1}],
                    (j == renderDistance - 1) ? blankChunk : chunks[{chunkPos.x, chunkPos.z + 1}]
                );
                chunks[chunkPos].CreateObject();
                chunks[chunkPos].Cleanup();
            }
        }

        cout << "Finished Meshing Terrain" << endl;
        lock.unlock();
        chunkCondition.notify_one();
    }

    void UpdateChunks(vec3 playerPos) {
        unique_lock<mutex> lock(chunkMutex);

        // Calculate player's chunk position
        int playerChunkX = static_cast<int>(playerPos.x);
        int playerChunkZ = static_cast<int>(playerPos.z);
        int halfRenderDistance = renderDistance / 2;

        std::set<ChunkCoord> existingChunks;
        for (auto& pair : chunks) {
            existingChunks.insert(pair.first);
        }

        std::vector<ChunkCoord> chunksToRemove;
        std::vector<ChunkCoord> chunksToCreate;

        // Determine new chunks and chunks to remove
        for (int i = -halfRenderDistance; i <= halfRenderDistance; i++) {
            for (int j = -halfRenderDistance; j <= halfRenderDistance; j++) {
                ChunkCoord searchCoord = {i + playerChunkX, j + playerChunkZ};

                if (existingChunks.find(searchCoord) == existingChunks.end()) {
                    // If not found, it needs to be created
                    chunksToCreate.push_back(searchCoord);
                }
                existingChunks.erase(searchCoord); // Mark this chunk as active
            }
        }

        // Any remaining coordinates in existingChunks are now too far and should be removed
        chunksToRemove.assign(existingChunks.begin(), existingChunks.end());

        // Remove old chunks
        for (auto& coord : chunksToRemove) {
            chunks.erase(coord);
        }

        vector<thread> threads(chunksToCreate.size());

        // Create new chunks
        int i = 0;
        for (auto& coord : chunksToCreate) {
            Chunk& newChunk = chunks[coord];
            newChunk.chunkPos = {coord.x, coord.z};
            //newChunk.Generate(heightMap, gravel, dirt);
            threads[i] = thread([&]{newChunk.Generate(heightMap, gravel, dirt);});
            printf("Creating chunk at %d, %d\n", coord.x, coord.z);
            chunks[coord] = std::move(newChunk);
            i++;
        }

        for(int i = 0; i < threads.size(); i++) {
            threads[i].join();
            cout << "Chunk done\n";
        }

        // Update the VBOs
        for (auto& coord : chunksToCreate) {
            // Ensure neighbors exist before this call or handle cases where they don't
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
        //chunkCondition.notify_one();
    }
    

    void changeRenderDistance(unsigned short newRenderDistance) {
        renderDistance = newRenderDistance;
        GenerateChunks();
    }

private:
    // World Settings
    uint64_t seed;

    // User Settings
    unsigned short renderDistance;

    // Noise
    FastNoise heightMap;
    FastNoise gravel;
    FastNoise dirt;

    // Temporary chunks
    vector<Chunk> tempChunks;
};
