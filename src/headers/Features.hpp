#pragma once

#include <cstdint>

#include "BlockMap.hpp"
#include "ColumnCache.hpp"
#include "TerrainGen.hpp"

// Puts trees, plants and pumpkins on top of a chunk of finished terrain.
//
// A tree rooted near a chunk border has to put leaves in the next chunk over,
// and chunks are generated independently and in any order, so there is no way
// to reach across and write into a neighbour. Instead every chunk scans a
// margin of columns around itself, works out which of them a tree is rooted in,
// and builds each of those trees in full while discarding the parts that fall
// outside its own bounds. The neighbour scans the same margin, finds the same
// tree, and keeps the half this one dropped. Both agree because placement is a
// pure function of the seed and the world coordinate.
// The column cache is built by the caller and shared with the fill, rather than
// being worked out again here.
void DecorateChunk(BlockMap& map, const ColumnCache& cache, const TerrainGen& terrain,
                   uint64_t seed, int chunkX, int chunkZ);
