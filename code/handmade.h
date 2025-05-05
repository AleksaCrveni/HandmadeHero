#if !defined(HANDMADE_H)

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int SamplePerSecond;
    int SampleCount;
    int16 *Samples;
};
// Services that the platform layer provides to the game.

// Services that the game provides to the platform layer.
void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset, game_sound_output_buffer *SoundBuffer);
#define HANDMADE_H
#endif