#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <cstdlib>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std;

class Chunk {
public:
    Chunk(long unsigned &seed);
    ~Chunk();

    // Game Functions
    void Generate(long unsigned &seed);
    int RemoveBlock(int xPosition, int yPosition, int zPosition);
    bool AddBlock(int xPosition, int yPosition, int zPosition, int blockID);

    // Engine Functions
    void MakeVertexObject();

    // Coding Functions
    void DoubleLoop(void (*function)(int x, int y));

private:
    int blockMap[16][64][16] = 0; // 16 wide, 64 tall, and 16 long

}

// Initializer 
Chunk::Chunk(long unsigned &seed) {
    this->Generate(seed);
    this->MakeVertexObject();
    this->MakeUVObject();
    this->MakeTexture();
}

// Takes the seed of the world and randomly generates the 
void Chunk::Generate(long unsigned &seed) {
    // Set the random seed
    srand(seed);

    // Generate the height for each (x, z)
    int height[16][16];
    DoubleLoop([&](int i, int j) { height[i][j] = (rand() % 16) + 32; });

    // Add grass to the top height
    DoubleLoop([&](int i, int j) { blockMap[i][height[i][j]][j] = 1; });

    // Add dirt for random height (3,4) below grass
    DoubleLoop([&](int i, int j) {
        unsigned dirt = (rand() % 2) + 3;
        for (unsigned k = 1; k <= dirt; k++) {
            blockMap[i][height[i][j] - k][j] = 2; // 2 is the ID for dirt
        }
    });

    // Below that, add in stone.
    DoubleLoop([&](int i, int j) {
        unsigned y = 0;
        while (blockMap[i][y][j] != 0) {
            blockMap[i][y][j] = 3; // 3 is the ID for stone 
            y++;
        }
    });
}

// Template to save repetitive coding
void Chunk::DoubleLoop(void (*func)(int x, int y)) {
    for (unsigned i = 0; i < 16; i++) {
        for(unsigned j = 0; j < 16; j++) {
            func(i, j);
        }
    }
}