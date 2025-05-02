#if !defined(HANDMADE_H)

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

// Services that the platform layer provides to the game.

// Services that the game provides to the platform layer.
void GameUpdateAndRender(game_offscreen_buffer *Buffer, int a, int b);

#define HANDMADE_H
#endif