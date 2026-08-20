#pragma once

#include <cstdint>

// Stateless, coordinate-addressed randomness for the terrain generator.
//
// Generation used to reach for rand(), which does not survive the move to
// worker threads: the workers share its state, so the world came out different
// every run and the same chunk could generate differently depending on which
// thread got to it. Hashing the world coordinate instead makes every decision a
// pure function of (seed, x, z), which buys two things. Runs are reproducible
// from the seed, and two chunks asking about the same world column always get
// the same answer -- that is what lets a tree rooted just outside a chunk grow
// its leaves into it without the two chunks ever talking to each other.

// splitmix64's finalizer. Avalanches well enough that neighbouring coordinates
// give uncorrelated results, which matters because we hash adjacent columns
// constantly.
inline uint64_t mix64(uint64_t z)
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}


// Salts keep unrelated decisions at the same column from lining up. Without
// them, every feature that rolled "is the hash below 0.1" would place itself in
// exactly the same places.
namespace salt
{
    const uint64_t DIRT_DEPTH   = 0x01;
    const uint64_t TREE         = 0x02;
    const uint64_t TREE_SHAPE   = 0x03;
    const uint64_t TALL_GRASS   = 0x04;
    const uint64_t FLOWER       = 0x05;
    const uint64_t MUSHROOM     = 0x06;
    const uint64_t DEAD_SHRUB   = 0x07;
    const uint64_t SUGAR_CANE   = 0x08;
    const uint64_t PUMPKIN      = 0x09;
    const uint64_t CACTUS       = 0x0A;
    const uint64_t CANE_PATCH   = 0x0B;
}


// A short stream of numbers for one decision at one column. Cheap enough to
// build inside a per-column loop; it is three multiplies and some shifts.
class WorldRandom
{
public:
    WorldRandom(uint64_t seed, int worldX, int worldZ, uint64_t saltValue)
    {
        _state = mix64(seed ^ (saltValue * 0x9E3779B97F4A7C15ULL));
        _state = mix64(_state ^ static_cast<uint64_t>(static_cast<int64_t>(worldX)));
        _state = mix64(_state ^ static_cast<uint64_t>(static_cast<int64_t>(worldZ)));
    }

    uint64_t next()
    {
        _state = mix64(_state);
        return _state;
    }

    // Uniform in [0, 1). Takes the top 24 bits, which are the best mixed.
    float nextFloat()
    {
        return static_cast<float>(next() >> 40) * (1.0f / 16777216.0f);
    }

    // Uniform in [low, high]. The modulo bias is irrelevant at the range sizes
    // used here (a handful of values against a 64 bit draw).
    int nextInt(int low, int high)
    {
        if (high <= low)
            return low;

        return low + static_cast<int>(next() % static_cast<uint64_t>(high - low + 1));
    }

    bool chance(float probability)
    {
        return nextFloat() < probability;
    }

private:
    uint64_t _state;
};
