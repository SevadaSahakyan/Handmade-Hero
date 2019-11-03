internal sim_entity_hash *
GetHashFromStorageIndex(sim_region *simRegion, ui32 storageIndex)
{
    Assert(storageIndex);
    sim_entity_hash *result = 0;
    
    ui32 hashValue = storageIndex;
    for(ui32 offset = 0;
        offset < ArrayCount(simRegion->hash);
        offset++)
    {
        ui32 hashMask = (ArrayCount(simRegion->hash) - 1);
        ui32 hashIndex = ((hashValue + offset) & hashMask);
        sim_entity_hash *entry = simRegion->hash + hashIndex;

        if(entry->index == 0 || entry->index == storageIndex)
        {
            result = entry;
            break;
        }
    }

    return result;
}

inline sim_entity *
GetEntityByStorageIndex
(
    sim_region *simRegion, ui32 storageIndex
)
{
    sim_entity_hash *entry = GetHashFromStorageIndex
        (
            simRegion, storageIndex
        );
    sim_entity *result = entry->ptr;
    return result;
}

inline v2
GetSimSpacePos(sim_region *simRegion, low_entity *stored)
{
    v2 result = InvalidPos;
    if(!IsSet(&stored->sim, EntityFlag_Nonspatial))
    {
        world_difference diff = Subtract
            (
                simRegion->_world,
                &stored->pos,
                &simRegion->origin
            );
        result = diff.dXY;
    }
    
    return result;
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex,
    low_entity *source, v2 *simPos
);
inline void
LoadEntityReference
(
    game_state *gameState,
    sim_region *simRegion, entity_reference *ref
)
{
    if(ref->index)
    {
        sim_entity_hash *entry = GetHashFromStorageIndex
            (
                simRegion, ref->index
            );
        if(entry->ptr == 0)
        {
            entry->index = ref->index;
            low_entity *lowEntity = GetLowEntity(gameState, ref->index);
            v2 pos = GetSimSpacePos(simRegion, lowEntity);
            entry->ptr = AddEntity
                (
                    gameState,
                    simRegion, ref->index,
                    lowEntity,
                    &pos
                );
        }
    
        ref->ptr = entry->ptr;
    }
}

inline void
StoreEntityReference(entity_reference *ref)
{
    if(ref->ptr != 0)
    {
        ref->index = ref->ptr->storageIndex;
    }
}

internal sim_entity *
AddEntityRaw
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex, low_entity *source
)
{
    Assert(storageIndex);
    sim_entity *_entity = 0;

    sim_entity_hash *entry = GetHashFromStorageIndex
            (
                simRegion, storageIndex
            );
    if(entry->ptr == 0)
    {
        if(simRegion->entityCount < simRegion->maxEntityCount)
        {
            _entity = simRegion->entities + simRegion->entityCount++;
            entry->index = storageIndex;
            entry->ptr = _entity;
            
            if(source)
            {
                *_entity = source->sim;
                LoadEntityReference
                    (
                        gameState, simRegion, &_entity->sword
                    );

                Assert(!IsSet(&source->sim, EntityFlag_Simming));
                AddFlag(&source->sim, EntityFlag_Simming);
            }
        
            _entity->storageIndex = storageIndex;
            _entity->isUpdatable = false;
        }
        else
        {
            InvalidCodePath;
        }
    }
    
    return _entity;
}

internal sim_entity *
AddEntity
(
    game_state *gameState,
    sim_region *simRegion, ui32 storageIndex,
    low_entity *source, v2 *simPos
)
{
    sim_entity *dest = AddEntityRaw
        (
            gameState,
            simRegion, storageIndex, source
        );
    if(dest)
    {
        if(simPos)
        {
            dest->pos = *simPos;
            dest->isUpdatable = IsInRectangle
                (
                    simRegion->updatableBounds,
                    dest->pos
                );
        }
        else
        {
            dest->pos = GetSimSpacePos
                (
                    simRegion, source
                );
        }
    }

    return dest;
}

internal sim_region *
BeginSim
(
    memory_arena *simArena, game_state *gameState, world *_world,
    world_position origin, rectangle2 bounds
)
{
    sim_region *simRegion = PushStruct(simArena, sim_region);
    ZeroStruct(simRegion->hash);

    // TODO: IMPORTANT: Calculate this from the max value of all
    // entities radius + their speed
    r32 updateSafetyMargin = 1.0f;
    
    simRegion->_world = _world;
    simRegion->origin = origin;
    simRegion->updatableBounds = bounds;
    simRegion->bounds = AddRadiusTo
        (
            simRegion->updatableBounds,
            updateSafetyMargin,
            updateSafetyMargin
        );

    simRegion->maxEntityCount = 4096;
    simRegion->entityCount = 0;
    simRegion->entities = PushArray
        (
            simArena,
            simRegion->maxEntityCount, sim_entity
        );
    
    world_position minChunkPos = MapIntoChunkSpace
        (
            _world,
            simRegion->origin, GetMinCorner(simRegion->bounds)
        );
    world_position maxChunkPos = MapIntoChunkSpace
        (
            _world,
            simRegion->origin, GetMaxCorner(simRegion->bounds)
        );
    
    for(i32 chunkY = minChunkPos.chunkY;
        chunkY <= maxChunkPos.chunkY;
        chunkY++)
    {
        for(i32 chunkX = minChunkPos.chunkX;
            chunkX <= maxChunkPos.chunkX;
            chunkX++)
        {
            world_chunk *chunk = GetWorldChunk
                (
                    _world, chunkX, chunkY, simRegion->origin.chunkZ
                );

            if(chunk)
            {
                for(world_entity_block *block = &chunk->firstBlock;
                    block;
                    block = block->next)
                {
                    for(ui32 entityIndexIndex = 0;
                        entityIndexIndex < block->entityCount;
                        entityIndexIndex++)
                    {
                        ui32 lowEntityIndex = block->lowEntityIndex[entityIndexIndex];
                        low_entity *low = gameState->lowEntities + lowEntityIndex;
                        if(!IsSet(&low->sim, EntityFlag_Nonspatial))
                        {
                            v2 simSpacePos = GetSimSpacePos
                                (
                                    simRegion, low
                                );
                            if(IsInRectangle(simRegion->bounds,
                                             simSpacePos))
                            {
                                AddEntity
                                    (
                                        gameState, simRegion,
                                        lowEntityIndex, low, &simSpacePos
                                    );
                            }
                        }
                    }
                }
            }
        }
    }

    return simRegion;
}

internal void
EndSim(sim_region *region, game_state *gameState)
{
    sim_entity *_entity = region->entities;
    for(ui32 entityIndex = 0;
        entityIndex < region->entityCount;
        entityIndex++, _entity++)
    {
        low_entity *stored = gameState->lowEntities + _entity->storageIndex;
        
        Assert(IsSet(&stored->sim, EntityFlag_Simming));    
        stored->sim = *_entity;
        Assert(!IsSet(&stored->sim, EntityFlag_Simming));
        
        StoreEntityReference(&stored->sim.sword);
        
        world_position newPos = IsSet(_entity, EntityFlag_Nonspatial) ?
            NullPosition() :
            MapIntoChunkSpace
            (
                gameState->_world,
                region->origin, _entity->pos
            );
        
        ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            _entity->storageIndex, stored,
            newPos
        );
    
        if(_entity->storageIndex == gameState->cameraFollowingEntityIndex)
        {
            world_position newCameraPos = gameState->cameraPos;
			newCameraPos.chunkZ = stored->pos.chunkZ;

#if 0
            if(cameraFollowingEntity.high->pos.x >
               9.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileX += 17;
            }

            if(cameraFollowingEntity.high->pos.x <
               -9.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileX -= 17;
            }

            if(cameraFollowingEntity.high->pos.y >
               5.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileY += 9;
            }

            if(cameraFollowingEntity.high->pos.y <
               -5.0f * _world->tileSideInMeters)
            {
                newCameraPos.absTileY -= 9;
            }
#else
            newCameraPos = stored->pos;
#endif

            gameState->cameraPos = newCameraPos;
        }
    }
}

internal b32
TestWall
(
    r32 wallX, r32 relX, r32 relY,
    r32 deltaPlayerX, r32 deltaPlayerY,
    r32 *tMin, r32 minY, r32 maxY
)
{
    b32 isHit = false;
    
    r32 tEpsilon = 0.001f;
    if(deltaPlayerX != 0.0f)
    {
        r32 tResult = (wallX - relX) / deltaPlayerX;
        r32 y = relY + tResult * deltaPlayerY;
                
        if(tResult >= 0.0f && *tMin > tResult)
        {
            if(y >= minY && y <= maxY)
            {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                isHit = true;
            }
        }
    }

    return isHit;
}

internal void
HandleCollision(sim_entity *a, sim_entity *b)
{
    if(a->type == EntityType_Monster &&
       b->type == EntityType_Sword)
    {
        a->hitPointMax--;
        MakeEntityNonSpatial(b);
    }
}

internal void
MoveEntity
(
    sim_region *simRegion,
    sim_entity *_entity, r32 deltaTime,
    move_spec *moveSpec, v2 ddPos
)
{
    Assert(!IsSet(_entity, EntityFlag_Nonspatial));
        
    world *_world = simRegion->_world;

    if(moveSpec->isUnitMaxAccelVector)
    {
        r32 ddPLength = LengthSq(ddPos);
        if(ddPLength > 1.0f)
        {
            ddPos *= 1.0f / SqRt(ddPLength);
        }
    }
    
    ddPos *= moveSpec->speed;

    // TODO: ODE!!!
    ddPos += -moveSpec->drag * _entity->dPos;

    v2 oldPlayerPos = _entity->pos;
    v2 deltaPlayerPos = 0.5f * ddPos *
        Square(deltaTime) + _entity->dPos * deltaTime;
    _entity->dPos = ddPos * deltaTime +
        _entity->dPos;
    
    v2 newPlayerPos = oldPlayerPos + deltaPlayerPos;
    r32 ddZ = -9.8f;
    _entity->z = 0.5f*ddZ*Square(deltaTime) + _entity->dZ * deltaTime + _entity->z;
    _entity->dZ = ddZ * deltaTime + _entity->dZ;
    if(_entity->z < 0)
    {
        _entity->z = 0;
    }

    r32 distanceRemaining = _entity->distanceLimit;
    if(distanceRemaining == 0.0f)
    {
        // TODO: Maybe formalize this number?
        distanceRemaining = 10000.0f;
    }
    
    for(ui32 i = 0; i < 4; i++)
    {
        r32 tMin = 1.0f;
        r32 playerDeltaLength = Length(deltaPlayerPos);
        // TODO: Epsilons??
        if(playerDeltaLength > 0.0f)
        {
            if(playerDeltaLength > distanceRemaining)
            {
                tMin = distanceRemaining / playerDeltaLength;
            }
        
            v2 wallNormal = {};
            sim_entity *hitEntity = 0;

            v2 desiredPos = _entity->pos + deltaPlayerPos;

            b32 stopsOnCollision = IsSet(_entity, EntityFlag_Collides);
            
            if(!IsSet(_entity, EntityFlag_Nonspatial))
            {
                // TODO: Spatial partition here
                for(ui32 highEntityIndex = 0;
                    highEntityIndex < simRegion->entityCount;
                    highEntityIndex++)
                {
                    sim_entity *testEntity = simRegion->entities + highEntityIndex;
                    if(_entity != testEntity)
                    {
                        if(IsSet(testEntity, EntityFlag_Collides) &&
                           !IsSet(testEntity, EntityFlag_Nonspatial))
                        {
                            r32 diameterW = testEntity->width + _entity->width;
                            r32 diameterH = testEntity->height + _entity->height;
                
                            v2 minCorner = -0.5f * v2{diameterW, diameterH};                        
                            v2 maxCorner = 0.5f * v2{diameterW, diameterH};

                            v2 rel = _entity->pos - testEntity->pos;
                
                            if(TestWall(minCorner.x, rel.x, rel.y,
                                        deltaPlayerPos.x, deltaPlayerPos.y,
                                        &tMin, minCorner.y, maxCorner.y))
                            {
                                wallNormal = v2{-1, 0};
                                hitEntity = testEntity;
                            }                    
                
                            if(TestWall(maxCorner.x, rel.x, rel.y,
                                        deltaPlayerPos.x, deltaPlayerPos.y,
                                        &tMin, minCorner.y, maxCorner.y))
                            {
                                wallNormal = v2{1, 0};
                                hitEntity = testEntity;
                            }
                
                            if(TestWall(minCorner.y, rel.y, rel.x,
                                        deltaPlayerPos.y, deltaPlayerPos.x,
                                        &tMin, minCorner.x, maxCorner.x))
                            {
                                wallNormal = v2{0, -1};
                                hitEntity = testEntity;
                            }
                
                            if(TestWall(maxCorner.y, rel.y, rel.x,
                                        deltaPlayerPos.y, deltaPlayerPos.x,
                                        &tMin, minCorner.x, maxCorner.x))
                            {
                                wallNormal = v2{0, 1};
                                hitEntity = testEntity;
                            }
                        }
                    }
                }
            }
        
            _entity->pos += tMin * deltaPlayerPos;
            distanceRemaining -= tMin * playerDeltaLength;
            if(hitEntity)
            {
                deltaPlayerPos = desiredPos - _entity->pos;
                if(stopsOnCollision)
                {
                    _entity->dPos = _entity->dPos - 1 *
                        Inner(_entity->dPos, wallNormal) * wallNormal;

                    deltaPlayerPos = deltaPlayerPos - 1 *
                        Inner(deltaPlayerPos, wallNormal) * wallNormal;
                }
                // TODO: IMPORTANT: Collision table!
                
                sim_entity *a = _entity;
                sim_entity *b = hitEntity;
                if(a->type > b->type)
                {
                    sim_entity *temp = a;
                    a = b;
                    b = temp;
                }
                
                HandleCollision(a, b);
                
                // TODO: Stairs
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    if(_entity->distanceLimit != 0.0f)
    {
        _entity->distanceLimit = distanceRemaining;
    }

    if(_entity->dPos.x == 0.0f && _entity->dPos.y == 0.0f)
    {
        // NOTE: Leave facing direction whatever it was
    }
    else if(AbsoluteValue(_entity->dPos.x) >
            AbsoluteValue(_entity->dPos.y))
    {
        if(_entity->dPos.x > 0)
        {
            _entity->facingDirection = 0;
        }
        else
        {
            _entity->facingDirection = 2;
        }
    }
    else if(AbsoluteValue(_entity->dPos.x) <
            AbsoluteValue(_entity->dPos.y))
    {
        if(_entity->dPos.y > 0)
        {
            _entity->facingDirection = 1;
        }
        else
        {
            _entity->facingDirection = 3;
        }
    }
}
