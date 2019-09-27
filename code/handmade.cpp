#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *soundBuffer, int toneHz)
{
    local_persist real32 tSine;
    int16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / toneHz;
    int16 *sampleOut = soundBuffer->samples;
    
    for(int sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {            
        real32 sineValue = sinf(tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
                            
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
            
        tSine += 2.0f * Pi32 / (real32)wavePeriod;
        if(tSine > 2.0f * Pi32)
        {
            tSine -= 2.0f * Pi32;
        }
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
GameUpdateAndRender
(
    game_memory *memory,
    game_input *input,
    game_offscreen_buffer *buffer
)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        char *fileName = __FILE__;
        
        debug_read_file_result file = DEBUGPlatformReadEntireFile(fileName);
        if(file.contents)
        {
            DEBUGPlatformWriteEntireFile("test.out", file.contentsSize, file.contents);            
            DEBUGPlatformFreeFileMemory(file.contents);            
        }
                
        gameState->toneHz = 256;

        memory->isInitialized = true;
    }

    for(int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); controllerIndex++)
    {
        game_controller_input *controller = GetController(input, controllerIndex);    
        if(controller->isAnalog)
        {
            gameState->blueOffset += (int)(4.0f * controller->stickAverageX);
            gameState->toneHz = 256 + (int)(128.0f * (controller->stickAverageY));
        }
        else
        {
            // NOTE: Digital movement

            if(controller->moveLeft.endedDown)
            {
                gameState->blueOffset -= 1;
            }
            if(controller->moveRight.endedDown)
            {
                gameState->blueOffset += 1;
            }
        }

        if(controller->actionDown.endedDown)
        {
            gameState->greenOffset += 1;
        }
    }
    
    RenderWeirdGradient(buffer, gameState->blueOffset, gameState->greenOffset);
}

internal void
GameGetSoundSamples(game_memory *memory, game_sound_output_buffer *soundBuffer)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(soundBuffer, gameState->toneHz);
}
