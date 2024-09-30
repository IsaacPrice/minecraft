#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <glm/glm.hpp>
#include "BlockData.hpp"

using namespace std;
using namespace glm;

float blockWidth = 0.0625f;


vector<vec3> getSideVertex(float x, float y, float z, SIDE part) 
{
    if (part == TOP) 
    {
        return 
        {
            {x, y + blockWidth, z},
            {x, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z},
            {x, y + blockWidth, z}
        };;
    }
    else if (part == BOTTOM) 
    {
        return 
        {
            {x, y, z},
            {x + blockWidth, y, z},
            {x + blockWidth, y, z + blockWidth},
            {x + blockWidth, y, z + blockWidth},
            {x, y, z + blockWidth},
            {x, y, z}
        };
    }
    else if (part == NORTH) 
    {
        return 
        {
            {x, y, z},
            {x, y + blockWidth, z},
            {x, y + blockWidth, z + blockWidth},
            {x, y + blockWidth, z + blockWidth},
            {x, y, z + blockWidth},
            {x, y, z}
        };
    }
    else if (part == EAST) 
    {
        return 
        {
            {x, y, z + blockWidth},
            {x + blockWidth, y, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x, y + blockWidth, z + blockWidth},
            {x, y, z + blockWidth}
        };
    }
    else if (part == SOUTH) 
    {
        return 
        {
            {x + blockWidth, y, z},
            {x + blockWidth, y + blockWidth, z},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y, z + blockWidth},
            {x + blockWidth, y, z}
        };
    }
    else 
    {
        return 
        {
            {x, y, z},
            {x + blockWidth, y, z},
            {x + blockWidth, y + blockWidth, z},
            {x + blockWidth, y + blockWidth, z},
            {x, y + blockWidth, z},
            {x, y, z}
        };
    }
}


vector<vec2> getTextureCoords(BLOCK blockID, SIDE side) 
{
    bool altCoords = false;

    if (blockID == GRASS && side == BOTTOM)
    {
        blockID = DIRT;
    }
    else if (blockID == GRASS && side != TOP) 
    {
        blockID = GRASS_SIDE;
        if (side == NORTH || side == SOUTH) {
            altCoords = true;
        }
    }

    float startX = ((blockID - 1) % 16) * 0.0625;
    float startY = (int((blockID - 1) / 16)) * 0.0625;

    if (altCoords) 
    {
        return 
        {
            {startX, startY + 0.0625},
            {startX, startY},
            {startX + 0.0625, startY},
            {startX + 0.0625, startY},
            {startX + 0.0625, startY + 0.0625},
            {startX, startY + 0.0625},
        };
    }

    return 
    {
        {startX + 0.0625, startY + 0.0625},
        {startX, startY + 0.0625},
        {startX, startY},
        {startX, startY},
        {startX + 0.0625, startY},
        {startX + 0.0625, startY + 0.0625},
    };
}
