#include "handmade.h"

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	// TODO lets see what o ptimized does
	// byte array pretty much
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		// uint8 *Pixel  = (uint8 *)Row;
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{
			/* 8 - bit red 8 bits of green 8 bits of blue and 8 bits of padding
			Pixel in memory: RR GG BB xx
			But due to little endian format of our CPU bytes are loaded in a way where
			least significant byte is stored at  lowest memory address and so on, so it appears that bytes are read from right to left
			so RR GG BB xx will be read as 0xXXBBGGRR (backwards)
			
			BUT windows people didn't like that so they reorder it in memory where blue bytes are first  so in memory its BB GG RR xx
			0xXXRRGGBB
			
			// Blue bits
			*Pixel = (uint8)(X + BlueOffset);
			++Pixel;

			// Green bits
			*Pixel = (uint8)(Y + GreenOffset);
			++Pixel;

			// Red bits
			*Pixel = 0;
			++Pixel;

			// Padding 
			*Pixel = 0;
			++Pixel;
			*/

			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
			/*
				Memory:				BB GG RR xx
				Register:  		xx RR GG BB

			*/
			// Shift green and or blue to get 32 bit value since we Pixel pointer is uint32
			uint32 res = ((Green << 8) | Blue);
			*Pixel++ = res;

		}

		// Pointer arithimic, pretty much moving pointer to next row (memory is 1D but we think of it as 2D since its bitmap)
		Row += Buffer->Pitch;
	}
	
}

void GameUpdateAndRender(game_offscreen_buffer *Buffer, int a , int b)
{
	RenderWeirdGradient(Buffer,a, b);
	return;
}