#include "chunk.hpp"

#include <typeinfo>

using namespace std;

vector<Chunk> currentChunks;
mutex chunkMutex;
condition_variable chunkCondition;

Chunk blankChunk;

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
        // Take control of the mutex and generate the chunks
        unique_lock<mutex> lock(chunkMutex);
        currentChunks = std::vector<Chunk>(renderDistance * renderDistance, Chunk());

        int halfRenderDistance = renderDistance / 2;

        // Part one of generation
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                currentChunks[i * renderDistance + j].chunkPos = { i - halfRenderDistance, j - halfRenderDistance };
                currentChunks[i * renderDistance + j].Generate(heightMap, gravel, dirt);
            }
        }

        // Part two of generation
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                currentChunks[i * renderDistance + j].MakeVertexObject(
                    (i == 0) ? blankChunk : currentChunks[(i-1) * renderDistance + j],
                    (i == renderDistance - 1) ? blankChunk : currentChunks[(i+1) * renderDistance + j],
                    (j == 0) ? blankChunk : currentChunks[i * renderDistance + j-1],
                    (j == renderDistance - 1) ? blankChunk : currentChunks[i * renderDistance + j+1]
                );
                currentChunks[i * renderDistance + j].CreateObject();
                currentChunks[i * renderDistance + j].Cleanup();
            }
        }

        //tempChunks = currentChunks;

        cout << "Finished Generating Terrain" << endl;
        lock.unlock();
        chunkCondition.notify_one();
    }

    void UpdateChunks(vec3 playerPos) {

        unique_lock<mutex> lock(chunkMutex);

        // Get the player's chunk position
        int playerChunkX = (int)playerPos.x;
        int playerChunkZ = (int)playerPos.z;

        // Go through the chunks and remove the ones that are too far away and add the ones that are inside
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                int chunkX = currentChunks[i * renderDistance + j].chunkPos.x;
                int chunkZ = currentChunks[i * renderDistance + j].chunkPos.y;

                // Check if the chunk is too far away
                if (abs(playerChunkX - chunkX) > renderDistance / 2 || abs(playerChunkZ - chunkZ) > renderDistance / 2) {
                    

                }
                
            }
        }



        // resize the vector to the render distance ^2 currentChunks
        // currentChunks.resize(renderDistance * renderDistance);


        // Take control of the mutex and update the chunks
        // Set the current chunks to the temp chunks
        
        lock.unlock();
        chunkCondition.notify_one();

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
