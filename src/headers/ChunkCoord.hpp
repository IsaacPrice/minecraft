#include <unordered_map>
#include <utility>
#include <mutex>
#include <condition_variable>

#include "Chunk.hpp"

struct ChunkCoord 
{
    int x, z;

    bool operator==(const ChunkCoord& other) const 
    {
        return x == other.x && z == other.z;
    }

    bool operator<(const ChunkCoord& other) const 
    {
        if (x == other.x) 
        {
            return z < other.z;  
        }
        return x < other.x; 
    }
};


namespace std 
{
    template <>
    struct hash<ChunkCoord> 
    {
        size_t operator()(const ChunkCoord& coord) const 
        {
            return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.z) << 1);
        }
    };
}

std::unordered_map<ChunkCoord, Chunk> chunks;
