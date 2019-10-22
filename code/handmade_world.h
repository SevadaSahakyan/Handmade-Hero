struct world_difference
{
    v2 dXY;
    r32 dZ;
};

struct world_position
{
    i32 chunkX;
    i32 chunkY;
    i32 chunkZ;

    // NOTE: These are the offsets from the chunk center
    v2 _offset;
};

struct world_entity_block
{
    ui32 entityCount;
    ui32 lowEntityIndex[16];
    world_entity_block *next;
};

struct world_chunk
{
    i32 chunkX;
    i32 chunkY;
    i32 chunkZ;
    
    world_entity_block firstBlock;
    
    world_chunk *nextInHash;
};

struct world
{    
    r32 tileSideInMeters;
    r32 chunkSideInMeters;    

    world_entity_block *firstFree;
    
    world_chunk chunkHash[4096];
};
