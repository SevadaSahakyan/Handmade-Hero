#include "handmade.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
    int16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / 400;
    int16 *sampleOut = soundBuffer->samples;
    
    for(int sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {
#if 0
        real32 sineValue = sinf(gameState->tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
#else
        int16 sampleValue = 0;
#endif
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
#if 0
        gameState->tSine += 2.0f * Pi32 / (real32)wavePeriod;
        if(gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

internal void
RenderWeirdGradient(game_offscreen_buffer *buffer, int blueOffset, int greenOffset)
{
    uint8 *row = (uint8 *)buffer->memory;

    for(int y = 0; y < buffer->height; y++)
    {
        uint32 *pixel = (uint32 *)row;
        
        for(int x = 0; x < buffer->width; x++)
        {
            uint8 blue = (uint8)(x + blueOffset);
            uint8 green = (uint8)(y + greenOffset);
            
            *pixel++ = ((green << 8) | blue);
        }

        row += buffer->pitch;
    }
}

internal void
DrawRectangle
(
    game_offscreen_buffer *buffer,
    real32 realMinX, real32 realMinY,
    real32 realMaxX, real32 realMaxY,
    real32 r, real32 g, real32 b
)
{
    int32 minX = RoundReal32ToInt32(realMinX);
    int32 minY = RoundReal32ToInt32(realMinY);
    int32 maxX = RoundReal32ToInt32(realMaxX);
    int32 maxY = RoundReal32ToInt32(realMaxY);

    if(minX < 0)
    {
        minX = 0;
    }

    if(minY < 0)
    {
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    uint8 *endOfBuffer = (uint8 *)buffer->memory + buffer->pitch * buffer->height;
    
    uint32 color =
        (
            RoundReal32ToUInt32(r * 255.0f) << 16 |
            RoundReal32ToUInt32(g * 255.0f) << 8 |
            RoundReal32ToUInt32(b * 255.0f) << 0
        );

    uint8 *row = (uint8 *)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;
    for(int y = minY; y < maxY; y++)
    {
        uint32 *pixel = (uint32 *)row;
        for(int x = minX; x < maxX; x++)
        {            
            *pixel++ = color;            
        }
        
        row += buffer->pitch;
    }
}

#pragma pack(push, 1)
struct bitmap_header
{
    uint16 fileType;
	uint32 fileSize;
	uint16 reserved1;
	uint16 reserved2;
	uint32 bitmapOffset;
    uint32 size;
    int32 width;
	int32 height;
	uint16 planes;
	uint16 bitsPerPixel; 
};
#pragma pack(pop)

internal uint32 *
DEBUGLoadBMP
(
    thread_context *thread,
    debug_platform_read_entire_file *readEntireFile,
    char *fileName
)
{
    uint32 *result = 0;
    
    debug_read_file_result readResult;
    readResult = readEntireFile(thread, fileName);

    if(readResult.contentsSize != 0)
    {
        bitmap_header *header = (bitmap_header *)readResult.contents;
        uint32 *pixels = (uint32 *)((uint8 *)readResult.contents + header->bitmapOffset);
        result = pixels;
    }

    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
    
    real32 playerHeight = 1.4f;
    real32 playerWidth = 0.75f * playerHeight;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        gameState->pixelPointer = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_background.bmp"
            );
        
        gameState->playerPos.absTileX = 1;
        gameState->playerPos.absTileY = 3;
        gameState->playerPos.tileRelX = 5.0f;
        gameState->playerPos.tileRelY = 5.0f;
        
        InitializeArena
            (
                &gameState->worldArena,
                memory->permanentStorageSize - sizeof(game_state),
                (uint8 *)memory->permanentStorage + sizeof(game_state)
            );

        gameState->worldMap = PushStruct
            (
                &gameState->worldArena,
                world
            );
        world *worldMap = gameState->worldMap;
        worldMap->tileMap = PushStruct
            (
                &gameState->worldArena,
                tile_map
            );

        tile_map *tileMap = worldMap->tileMap;

        tileMap->chunkShift = 4;
        tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
        tileMap->chunkDim = 1 << tileMap->chunkShift;
        
        tileMap->tileChunkCountX = 128;
        tileMap->tileChunkCountY = 128;
        tileMap->tileChunkCountZ = 2;

        tileMap->tileChunks = PushArray
            (
                &gameState->worldArena,
                tileMap->tileChunkCountX *
                tileMap->tileChunkCountY *
                tileMap->tileChunkCountZ,
                tile_chunk
            );
    
        tileMap->tileSideInMeters = 1.4f;

        uint32 randomNumberIndex = 0;
        
        uint32 tilesPerWidth = 17;
        uint32 tilesPerHeight = 9;

        uint32 screenX = 0;
        uint32 screenY = 0;
        
        uint32 absTileZ = 0;

        bool32 isDoorTop = false;
        bool32 isDoorLeft = false;
        bool32 isDoorBottom = false;
        bool32 isDoorRight = false;
        bool32 isDoorUp = false;
        bool32 isDoorDown = false;
        
        for(uint32 screenIndex = 0;
            screenIndex < 100;
            screenIndex++)
        {
            // TODO: Random number generator
            Assert(randomNumberIndex < ArrayCount(randomNumberTable));
            uint32 randomChoice;

            if(isDoorUp || isDoorDown)
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 2;
            }
            else
            {
                randomChoice = randomNumberTable[randomNumberIndex++] % 3;
            }

            bool32 isCreatedZDoor = false;
            if(randomChoice == 2)
            {
                isCreatedZDoor = true;
                
                if(absTileZ == 0)
                {
                    isDoorUp = true;
                }
                else
                {
                    isDoorDown = true;
                }
            }
            else if(randomChoice == 1)
            {
                isDoorRight = true;
            }
            else
            {
                isDoorTop = true;
            }
            
            for(uint32 tileY = 0;
                tileY < tilesPerHeight;
                tileY++)
            {
                for(uint32 tileX = 0;
                    tileX < tilesPerWidth;
                    tileX++)
                {
                    uint32 absTileX = screenX * tilesPerWidth
                        + tileX;
                        
                    uint32 absTileY = screenY * tilesPerHeight
                        + tileY;

                    uint32 tileValue = 1;
                    if(tileX == 0 &&
                       (!isDoorLeft || tileY != tilesPerHeight / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileX == tilesPerWidth - 1 &&
                       (!isDoorRight || tileY != tilesPerHeight / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileY == 0 &&
                       (!isDoorBottom || tileX != tilesPerWidth / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileY == tilesPerHeight - 1 &&
                       (!isDoorTop || tileX != tilesPerWidth / 2))
                    {
                        tileValue = 2;
                    }

                    if(tileX == 10 && tileY == 6)
                    {
                        if(isDoorUp)
                        {
                            tileValue = 3;
                        }
                        else
                        {
                            tileValue = 4;
                        }
                    }
                        
                    SetTileValue
                        (
                            &gameState->worldArena, tileMap,
                            absTileX, absTileY, absTileZ,
                            tileValue
                        );
                }
            }

            isDoorLeft = isDoorRight;
            isDoorBottom = isDoorTop;

            if(isCreatedZDoor)
            {
                isDoorUp = !isDoorUp;
                isDoorDown = !isDoorDown;
            }
            else
            {
                isDoorUp = false;
                isDoorDown = false;
            }
            
            isDoorTop = false;
            isDoorRight = false;

            if(randomChoice == 2)
            {
                if(absTileZ == 0)
                {
                    absTileZ = 1;
                }
                else
                {
                    absTileZ = 0;
                }
            }
            else if(randomChoice == 1)
            {
                screenX++;
            }
            else
            {
                screenY++;
            }
        }
        
        memory->isInitialized = true;
    }
        
    world *worldMap = gameState->worldMap;
    tile_map *tileMap = worldMap->tileMap;
    
    int32 tileSideInPixels = 60;
    real32 metersToPixels = (real32)tileSideInPixels / (real32)tileMap->tileSideInMeters;
    
    real32 lowerLeftX = -(real32)tileSideInPixels/2;
    real32 lowerLeftY = (real32)buffer->height;
    
    for(int controllerIndex = 0;
        controllerIndex < ArrayCount(input->controllers);
        controllerIndex++)
    {
        game_controller_input *controller = GetController(input, controllerIndex);    
        if(controller->isAnalog)
        {
            // NOTE: Analog movement
        }
        else
        {
            // NOTE: Digital movement
            real32 dPlayerX = 0.0f;
            real32 dPlayerY = 0.0f;
            
            if(controller->moveUp.endedDown)
            {
                dPlayerY = 1.0f;
            }
            
            if(controller->moveDown.endedDown)
            {
                dPlayerY = -1.0f;
            }
            
            if(controller->moveLeft.endedDown)
            {
                dPlayerX = -1.0f;
            }
            
            if(controller->moveRight.endedDown)
            {
                dPlayerX = 1.0f;
            }
            
            real32 playerSpeed = 2.0f;
            if(controller->actionUp.endedDown)
            {
                playerSpeed = 10.0f;
            }
            
            dPlayerX *= playerSpeed;
            dPlayerY *= playerSpeed;
            
            // TODO: Diagonal will be faster :D
            tile_map_position newPlayerPos = gameState->playerPos;
            newPlayerPos.tileRelX += input->deltaTime * dPlayerX;
            newPlayerPos.tileRelY += input->deltaTime * dPlayerY;

            newPlayerPos = RecanonicalizePosition(tileMap, newPlayerPos);
            
            tile_map_position playerLeft = newPlayerPos;
            playerLeft.tileRelX -= 0.5f * playerWidth;
            playerLeft = RecanonicalizePosition(tileMap, playerLeft);
            
            tile_map_position playerRight = newPlayerPos;
            playerRight.tileRelX += 0.5f * playerWidth;
            playerRight = RecanonicalizePosition(tileMap, playerRight);
            
            if(IsTileMapPointEmpty(tileMap, newPlayerPos) &&
               IsTileMapPointEmpty(tileMap, playerLeft) &&
               IsTileMapPointEmpty(tileMap, playerRight))
            {
                if(!AreOnSameTile(&gameState->playerPos, &newPlayerPos))
                {
                    uint32 newTileValue = GetTileValue(tileMap, newPlayerPos);
                    
                    if(newTileValue == 3)
                    {
                        newPlayerPos.absTileZ++;
                    }
                    else if(newTileValue == 4)
                    {
                        newPlayerPos.absTileZ--;
                    }
                }
                
                gameState->playerPos = newPlayerPos;
            }
        }
    }
    
    DrawRectangle
        (
            buffer, 0.0f, 0.0f,
            (real32)buffer->width, (real32)buffer->height,
            1.0f, 0.0f, 0.1f
        );

    real32 screenCenterX = 0.5f * (real32)buffer->width;
    real32 screenCenterY = 0.5f * (real32)buffer->height;
    
    for(int32 relRow = -10;
        relRow < 10;
        relRow++)
    {
        for(int32 relColumn = -20;
            relColumn < 20;
            relColumn++)
        {
            uint32 column = gameState->playerPos.absTileX + relColumn;
            uint32 row = gameState->playerPos.absTileY + relRow;
            
            uint32 tileID = GetTileValue
                (
                    tileMap,
                    column, row, gameState->playerPos.absTileZ
                );
            
            if(tileID > 0)
            {
                real32 gray = (tileID == 2) ? 1.0f : 0.5f;

                if(tileID > 2) gray = 0.25f;
                
                if(row == gameState->playerPos.absTileY &&
                   column == gameState->playerPos.absTileX)
                {
                    gray = 0.0f;
                }

                real32 centerX = screenCenterX - metersToPixels * gameState->playerPos.tileRelX +
                    ((real32)relColumn) * tileSideInPixels;
                real32 centerY = screenCenterY + metersToPixels * gameState->playerPos.tileRelY -
                    ((real32)relRow) * tileSideInPixels;
                real32 minX = centerX - 0.5f * tileSideInPixels;
                real32 minY = centerY - 0.5f * tileSideInPixels;
                real32 maxX = centerX + 0.5f * tileSideInPixels;
                real32 maxY = centerY + 0.5f * tileSideInPixels;
            
                DrawRectangle
                    (
                        buffer, minX, minY,
                        maxX, maxY,
                        gray, gray, gray
                    );
            }
        }
    }

    real32 playerR = 1.0f;
    real32 playerG = 1.0f;
    real32 playerB = 0.0f;
    
    real32 playerLeft = screenCenterX - 0.5f * metersToPixels * playerWidth;
    real32 playerTop = screenCenterY - metersToPixels * playerHeight;
    
    DrawRectangle
        (
            buffer,
            playerLeft, playerTop,
            playerLeft + metersToPixels * playerWidth,
            playerTop + metersToPixels * playerHeight,
            playerR, playerG, playerB
        );

    #if 0
    uint32 *source = gameState->pixelPointer;
    uint32 *dest = (uint32 *)buffer->memory;
    for(int32 y = 0; y < buffer->height / 2; y++)
    {
        for(int32 x = 0; x < buffer->width / 2; x++)
        {
            *dest++ = *source++;
        }
    }
    #endif
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
