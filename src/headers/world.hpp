#include "chunk.hpp"

#include <typeinfo>

using namespace std;

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

    // Renders the world
    void Render(vec3 playerPos) {
        // Update the current chunks
        //UpdateCurrentChunks(playerPos);

        // Render the chunks
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                currentChunks[i][j].Draw();
            }
        }
    }

    void GenerateChunks() {
        // Resize the outer vector and initialize all chunks to the default value
        currentChunks = std::vector<std::vector<Chunk>>(renderDistance, std::vector<Chunk>(renderDistance, Chunk()));

        int halfRenderDistance = renderDistance / 2;

        // Part one of generation
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                currentChunks[i][j].chunkPos = { i - halfRenderDistance, j - halfRenderDistance };
                currentChunks[i][j].Generate(heightMap, gravel, dirt);
            }
        }

        // Part two of generation
        for (int i = 0; i < renderDistance; i++) {
            for (int j = 0; j < renderDistance; j++) {
                currentChunks[i][j].MakeVertexObject(
                    (i == 0) ? blankChunk : currentChunks[i-1][j],
                    (i == renderDistance - 1) ? blankChunk : currentChunks[i+1][j],
                    (j == 0) ? blankChunk : currentChunks[i][j-1],
                    (j == renderDistance - 1) ? blankChunk : currentChunks[i][j+1]
                );
                currentChunks[i][j].CreateObject();
                currentChunks[i][j].Cleanup();
            }
        }

        cout << "Finished Generating Terrain" << endl;
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

    // The chunks
    vector<vector<Chunk>> currentChunks;
};
