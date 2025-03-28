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

// global for now
global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;
internal void RenderWeirdGradient(int BlueOffset, int GreenOffset)
{
	int Width = BitmapWidth;
	int Height = BitmapHeight;

	int Pitch = Width*BytesPerPixel;
	// byte array pretty much
	uint8 *Row = (uint8 *) BitmapMemory;
	for (int Y = 0; Y < BitmapHeight; ++Y)
	{
		// uint8 *Pixel  = (uint8 *)Row;
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < BitmapWidth; ++X)
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
		Row += Pitch;
	}
	
}

// DIB => Device Independent Bit 
internal void Win32ResizeDIBSection(int Width, int Height)
{

	// have to free memory if we are going to allocate again
	// and we have to allocate because we are resizing window so width and height of our bitmap chjanges
	if (BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = Width;
	BitmapHeight = Height;

  BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = BitmapWidth;
	// negative so bitmap is top to btottom and origin is upper left corner
	BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
	BitmapInfo.bmiHeader.biPlanes = 1;
	// 8 bits each for Red, Green, Blue and 8 extra padded for alignment on 4B boundaries 
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;
	// don't need to set these to 0 because struct on init sets values to 0
	/*
	BitmapInfo.bmiHeader.biSizeImage = 0;
	BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	BitmapInfo.bmiHeader.biClrUsed = 0;
	BitmapInfo.bmiHeader.biClrImportant = 0;
	*/
	// because we set biBitCount to 32 bits (4B) for alignment
	int BitmapMemorySize = (Width * Height) * BytesPerPixel;
	BitmapMemory = VirtualAlloc(0,BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	
	// TODO: clear to black maybe 
}

internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
	int WindowWidth = ClientRect->right - ClientRect->left;
	int WindowHeight = ClientRect->bottom - ClientRect->top;

	// Pretty much copy rectangle from our buffer to the screen
	// That is hwy source and dest coords are the same
	StretchDIBits(
		DeviceContext,
		/*
		X,Y,Width,Height,
		X,Y,Width,Height,
		*/
		0, 0, BitmapWidth, BitmapHeight,
		0, 0, WindowWidth, WindowHeight,
		BitmapMemory,
		&BitmapInfo,
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
		case WM_SIZE:
		{
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			int Height = ClientRect.bottom - ClientRect.top;
			int Width = ClientRect.right - ClientRect.left;
			Win32ResizeDIBSection(Width, Height);
		} break;
		case WM_DESTROY:
		{
			// TODO: Hadle with error to the user
			Running = false;
		} break;
		case WM_CLOSE:
		{
			// PostQuitMessage(0); can do this to post quit message to our queue, but can also do static var;
			
			// TODO: Hadle with message to the user
			Running = false;
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

			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
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

	// this will pain entire window when streching window horizontally or vertically
	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
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
			// Have to start pulling messages from queue or kernel wont sent it
			MSG Message;
			Running = true;
			int BlueOffset = 0;
			int GreenOffset = 0;
			while (Running)
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
						Running = false;
					}
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				RenderWeirdGradient(BlueOffset, GreenOffset);
				HDC DeviceContext = GetDC(Window);
				RECT ClientRect;
				GetClientRect(Window, &ClientRect);
				int WindowHeight = ClientRect.bottom - ClientRect.top;
				int WindowWidth = ClientRect.right - ClientRect.left;

				Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
				ReleaseDC(Window, DeviceContext);
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