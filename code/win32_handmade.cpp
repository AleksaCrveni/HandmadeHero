#include <windows.h>
#include <winuser.h>
#include <stdint.h>

#define internal static 
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};

struct win32_window_dimension
{
	int Height;
	int Width;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Height = ClientRect.bottom - ClientRect.top;
	Result.Width = ClientRect.right - ClientRect.left;
	
	return Result;
}


internal void RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
	// TODO lets see what o ptimized does
	// byte array pretty much
	uint8 *Row = (uint8 *)Buffer.Memory;
	for (int Y = 0; Y < Buffer.Height; ++Y)
	{
		// uint8 *Pixel  = (uint8 *)Row;
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer.Width; ++X)
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
		Row += Buffer.Pitch;
	}
	
}

// DIB => Device Independent Bit 
internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{

	// have to free memory if we are going to allocate again
	// and we have to allocate because we are resizing window so width and height of our bitmap chjanges
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	int BytesPerPixel = 4;

  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	// negative so bitmap is top to btottom and origin is upper left corner
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	// 8 bits each for Red, Green, Blue and 8 extra padded for alignment on 4B boundaries 
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	// don't need to set these to 0 because struct on init sets values to 0
	/*
	Info.bmiHeader.biSizeImage = 0;
	Info.bmiHeader.biXPelsPerMeter = 0;
	Info.bmiHeader.biYPelsPerMeter = 0;
	Info.bmiHeader.biClrUsed = 0;
	Info.bmiHeader.biClrImportant = 0;
	*/
	// because we set biBitCount to 32 bits (4B) for alignment
	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0,BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*BytesPerPixel;
	// TODO: clear to black maybe 
}

internal void Win32CopyBufferToWindow(
	HDC DeviceContext, int WindowWidth,  int WindowHeight,
	win32_offscreen_buffer Buffer)
{
	// TODO: Fix aspect ratio
	// Pretty much copy rectangle from our buffer to the screen
	// That is hwy source and dest coords are the same
	StretchDIBits(
		DeviceContext,
		/*
		X,Y,Width,Height,
		X,Y,Width,Height,
		*/
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory,
		&Buffer.Info,
		DIB_RGB_COLORS,
		SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(
  HWND Window,
  UINT Message,
  WPARAM WParam,
  LPARAM LParam)
{
	LRESULT Result = 0;
	switch (Message)
	{
		case WM_DESTROY:
		{
			// TODO: Hadle with error to the user
			GlobalRunning = false;
		} break;
		case WM_CLOSE:
		{
			// PostQuitMessage(0); can do this to post quit message to our queue, but can also do static var;
			
			// TODO: Hadle with message to the user
			GlobalRunning = false;
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom  - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32CopyBufferToWindow(
				DeviceContext, Dimension.Width, Dimension.Height,
				GlobalBackBuffer);
			EndPaint(Window, &Paint);
			
		} break;
		default:
		{
			//OutputDebugSTringA("default\n");
			// Default win proc that can handle all codes with default behaviour
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
		
	}

	return Result;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCode)
{
	// Init struct with 0 values
	WNDCLASS WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	// this will pain entire window when streching window horizontally or vertically
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	//Pointer to the function (pretty much registering callback)
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	// WindowClass.hIcon
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";
	if (RegisterClass(&WindowClass))
	{
		HWND Window = 
				CreateWindowEx(
					0,
					WindowClass.lpszClassName,
					"Handmade Hero",
					WS_OVERLAPPEDWINDOW|WS_VISIBLE,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					0,
					0,
					Instance,
					0);
		if (Window)
		{
			// Can do this sicnce we add CS_OWNDC
			HDC DeviceContext = GetDC(Window);
			// Have to start pulling messages from queue or kernel wont sent it
			MSG Message;
			GlobalRunning = true;
			
			int BlueOffset = 0;
			int GreenOffset = 0;
			while (GlobalRunning)
			{
				
				// If 0 passed as handle it will retrieve any messages that belong to us
				// GetMessage will block thread if there are no messages
				//BOOL MessageResult  = GetMessage(&Message, 0, 0, 0);
				// have to process all messages in queue , its a must
				MSG Message;
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					// Check if someone else sent us quit message
					if (Message.message == WM_QUIT) 
					{
						GlobalRunning = false;
					}
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				RenderWeirdGradient(GlobalBackBuffer, BlueOffset, GreenOffset);
				RECT ClientRect;
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);

				Win32CopyBufferToWindow(
					DeviceContext, Dimension.Width, Dimension.Height,
					GlobalBackBuffer);
				++BlueOffset;
				GreenOffset += 2;
			}
		} else
		{
			// TODO: logging
		}

	} else
	{
		// TODO: logging
	}
  return 0;
}