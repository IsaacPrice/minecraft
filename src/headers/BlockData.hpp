#pragma once

enum SIDE {
    TOP,
    BOTTOM, 
    NORTH,
    EAST,
    SOUTH,
    WEST,
    NO_SIDE
};

// Block ids index the 16x16 atlas in content/terrain.png as `tile = id - 1`,
// so every id below is pinned to the tile it actually samples. They used to be
// implicit, which had drifted: the atlas has two tiles for each half of a double
// chest but the list only named one of each, so everything from the crafting
// table onwards sampled the tile before the one it was named after. LEAVES drew
// redstone ore and FERN drew a dead shrub. Explicit values keep an insertion in
// the middle from silently repainting every block after it.
enum BLOCK {
    AIR = 0,

    // Atlas row 0
    GRASS = 1,
    STONE = 2,
    DIRT = 3,
    GRASS_SIDE = 4,
    OAK_PLANKS = 5,
    SMOOTH_STONE_SLAB = 6,
    SMOOTH_STONE = 7,
    BRICKS = 8,
    TNT_SIDE = 9,
    TNT_TOP = 10,
    TNT_BOTTOM = 11,
    COBWEB = 12,
    ROSE = 13,
    DANDELION = 14,
    WATER = 15,
    OAK_SAPLING = 16,

    // Atlas row 1
    COBBLESTONE = 17,
    BEDROCK = 18,
    SAND = 19,
    GRAVEL = 20,
    OAK_LOG_SIDE = 21,
    OAK_LOG_TOP = 22,
    IRON_BLOCK = 23,
    GOLD_BLOCK = 24,
    DIAMOND_BLOCK = 25,
    CHEST_TOP = 26,
    CHEST_SIDE = 27,
    CHEST_FRONT = 28,
    RED_MUSHROOM = 29,
    BROWN_MUSHROOM = 30,
    SPRUCE_SAPLING = 31,
    UNUSED_ROW1_END = 32,

    // Atlas row 2
    GOLD_ORE = 33,
    IRON_ORE = 34,
    COAL_ORE = 35,
    BOOKSHELF = 36,
    MOSSY_COBBLESTONE = 37,
    OBSIDIAN = 38,
    GRASS_SIDE_OVERLAY = 39,
    LONG_GRASS = 40,
    GRASS_TOP_ALT = 41,
    DOUBLE_CHEST_FRONT_LEFT = 42,
    DOUBLE_CHEST_FRONT_RIGHT = 43,
    CRAFTING_TABLE_TOP = 44,
    FURNACE_FRONT = 45,
    FURNACE_SIDE = 46,
    DISPENSER_FRONT = 47,
    UNUSED_ROW2_END = 48,

    // Atlas row 3
    SPONGE = 49,
    GLASS = 50,
    DIAMOND_ORE = 51,
    REDSTONE_ORE = 52,
    LEAVES = 53,
    LEAVES_OPAQUE = 54,
    STONE_BRICK = 55,
    DEAD_SHRUB = 56,
    FERN = 57,
    DOUBLE_CHEST_BACK_LEFT = 58,
    DOUBLE_CHEST_BACK_RIGHT = 59,
    CRAFTING_TABLE_SIDE = 60,
    CRAFTING_TABLE_FRONT = 61,
    FURNACE_LIT_FRONT = 62,
    STONE_ALT = 63,
    SPRUCE_LEAVES = 64,

    // Atlas rows 4 and beyond, added for terrain features.
    CACTUS_TOP = 70,
    CACTUS_SIDE = 71,
    CACTUS_BOTTOM = 72,
    CLAY = 73,
    SUGAR_CANE = 74,
    PUMPKIN_TOP = 103,
    PUMPKIN_SIDE = 119,
    PUMPKIN_FACE = 120,
    SANDSTONE_TOP = 177,
    SANDSTONE_SIDE = 193,
    SANDSTONE_BOTTOM = 209,

    // Blocks whose faces do not all share one tile are stored under the id of
    // their side tile; getTextureCoords swaps in the top and bottom tiles when
    // it meshes them. These aliases just let placement code say what it means.
    OAK_LOG = OAK_LOG_SIDE,
    PUMPKIN = PUMPKIN_SIDE,
    SANDSTONE = SANDSTONE_SIDE,
    CACTUS = CACTUS_SIDE,
};


// Block ids are stored one to a byte in BlockMap, so the atlas cannot be indexed
// past its 256th tile without widening that back out.
static_assert(SANDSTONE_BOTTOM <= 255, "block ids must fit in the byte BlockMap stores them in");


// Cross blocks are the flat plants: instead of a cube they mesh as two quads
// standing on the block's diagonals, so they are see-through from every angle.
inline bool isCross(unsigned short block)
{
    switch (block)
    {
        case LONG_GRASS:
        case FERN:
        case DEAD_SHRUB:
        case ROSE:
        case DANDELION:
        case RED_MUSHROOM:
        case BROWN_MUSHROOM:
        case SUGAR_CANE:
        case OAK_SAPLING:
        case SPRUCE_SAPLING:
            return true;
        default:
            return false;
    }
}


// Only opaque blocks hide the face of the block next to them. Meshing used to
// treat everything except air as opaque, which is fine while the world is all
// stone but would seal leaves, water and plants off from the faces behind them.
inline bool isOpaque(unsigned short block)
{
    if (block == AIR || block == WATER || block == LEAVES || block == GLASS)
        return false;

    // A cactus is a cube, but a narrower one than a block, so the ground and
    // the air around it keep the faces a full block would have hidden.
    if (block == CACTUS_SIDE)
        return false;

    return !isCross(block);
}


// Whether a plant can be rooted on top of this block.
inline bool canSupportPlant(unsigned short block)
{
    return block == GRASS || block == DIRT || block == SAND;
}
