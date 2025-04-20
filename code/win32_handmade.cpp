#include <windows.h>
#include <winuser.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

#define internal static 
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;


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

struct win32_sound_output
{
	int SamplePerSecond;
	int ToneHz;
	uint32 RunningSampleIndex;
	int WavePeriod;
	int HalfWavePeriod;
	int BytesPerSample;
	int SecondaryBufferSize;
	int16 ToneVolume;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable int BlueOffset = 0;
global_variable int GreenOffset = 0;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// Support for DirectSound
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN  pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
// Don't need stub since we only call it when we know it exists

// Support for XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state); 
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
// pointer to external function
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// Support for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state); 

X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void Win32LoadXInput()
{
	// We try to load xinput dll ( some machnes may not have andd you dont need if you want to play with keyboard and mouse)
	// so if can't be loaded program will still work because we hgave defined default function stubs

	HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
	if (!XInputLibrary)
	{
		// TODO Diagnostics
		XInputLibrary = LoadLibrary("xinput1_3.dll");
	}

	if (XInputLibrary)
	{
		// GetProcAddress doesn't  know what is signature of function it tries to load
		// So we have to cast it out function signature
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		// TODO Diagnostics
	}
	else 
	{
		// TODO Diagnostics
	}
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Height = ClientRect.bottom - ClientRect.top;
	Result.Width = ClientRect.right - ClientRect.left;
	
	return Result;
}

internal void Win32InitSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// Load Library
	HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
	if (DSoundLibrary)
	{
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;
		// SUCCEEDED is directx macro
		if (DirectSoundCreate &&  SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat;
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			// from docs
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			// from docs
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			// Get DirectSound object -- cooperative mode
			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				// "Create" primary buffer so we can set mode of it
				// (actually we are getting handle to it, because Primary buffer is created by system)
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(Error))
					{
						OutputDebugString("Primary buffer format was set");	
					}
					else
					{
						// TODO Diagnostics
					}

				}
			}
			else 
			{
				// TODO Diagnostics
			}
			
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			// Create secondary buffer
			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
			if (SUCCEEDED(Error))
			{
				OutputDebugString("Secondary Buffer CReated Successfully!");
			}
		}
		else 
		{
			// TODO Diagnostics
		}
	}
}

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
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
	Buffer->Memory = VirtualAlloc(0,BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*BytesPerPixel;
	// TODO: clear to black maybe 
}

internal void Win32CopyBufferToWindow(
	HDC DeviceContext, int WindowWidth,  int WindowHeight,
	win32_offscreen_buffer *Buffer)
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
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory,
		&Buffer->Info,
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
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VkCode = WParam;
			// 30th bit is this
			// comparsion in first () will return value and we want to turn it into 1 or 0 ( bool)
			// so we compare to 0 after
			bool WasDown = ((LParam & (1 << 30)) != 0);
			// 31st bit is always 0 for SYSKEYDOWN
			bool IsDown = ((LParam & (1 << 31)) == 0);
			if (WasDown != IsDown)
			{
				if (VkCode == 'W')
				{
				}
				else if (VkCode == 'A')
				{
				}
				else if (VkCode == 'S')
				{
				}
				else if (VkCode == 'D')
				{
				}
				else if (VkCode == 'Q')
				{	
				}
				else if (VkCode == 'E')
				{	
				}
				else if (VkCode == VK_UP)
				{
				}
				else if (VkCode == VK_LEFT)
				{
				}
				else if (VkCode == VK_DOWN)
				{
				}
				else if (VkCode == VK_RIGHT)
				{
				}
				else if (VkCode == VK_ESCAPE)
				{
				}
				else if (VkCode == VK_SPACE)
				{
				}
			}

			bool32 IsAltKeyDown = (LParam & (1 << 29));
			if (VkCode == VK_F4 && IsAltKeyDown)
			{
				GlobalRunning = false;
			}
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
				&GlobalBackBuffer);
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

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
		// We can have 2 Regions if write pointer is near the end and we ask to write too much it will try to write lefover
		// from the beggining of the buffer (Region 2)
		VOID *Region1;
		DWORD Region1Size;
		VOID *Region2;
		DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock,
		BytesToWrite,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		// TODO Assert that region1&2Size are valid

		int16 *SampleOut = (int16 *)Region1;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;

		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			real32 T = 2.0f * Pi32 *((real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod);
			real32 SineValue = sinf(T);
			// Sin give number between -1 and 1 so we want to scale it to tone volume
			int16 SampleValue = int16(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}

		SampleOut = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{ 
			real32 T = 2.0f * Pi32 *((real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod);
			real32 SineValue = sinf(T);
			// Sin give number between -1 and 1 so we want to scale it to tone volume
			int16 SampleValue = int16(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}
		// have to unlock to tell direct soudn that you finished writing to the buffer
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}

}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCode)
{
	Win32LoadXInput();
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
			
			win32_sound_output SoundOutput = {};
			SoundOutput.SamplePerSecond = 48000;
			// Close to middle C, Hz is sample per second
			SoundOutput.ToneHz = 256;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplePerSecond / SoundOutput.ToneHz;
			SoundOutput.HalfWavePeriod = SoundOutput.WavePeriod / 2;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplePerSecond * SoundOutput.BytesPerSample;
			SoundOutput.ToneVolume = 3000;

			Win32InitSound(Window, SoundOutput.SamplePerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

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

				// TODO: Should we poll this more frequently?
				for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++)
				{
					XINPUT_STATE ControllerState;
 					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						// this controller is plugged In
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;

					}
					else
					{
						// not available
					}
				}

				RenderWeirdGradient(&GlobalBackBuffer, BlueOffset, GreenOffset);
				// Direct sound output test

				DWORD WriteCursor;
				DWORD PlayCursor;
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
				{
					DWORD BytesToWrite;
					// we mod (%) to get remainder which is pretty much where we are because its ring buffer
					DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					if (ByteToLock == PlayCursor)
					{
						BytesToWrite = 0;
					}
					else if (ByteToLock > PlayCursor)
					{ // ByteToLock is in front of PlayCursor, we have to handle 2 regions
						// Day 008 ~45min in case I forget how this works
						BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
						BytesToWrite += PlayCursor;
					}
					else
					{
						BytesToWrite = PlayCursor - ByteToLock;
					}
				
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
				}
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32CopyBufferToWindow(
					DeviceContext, Dimension.Width, Dimension.Height,
					&GlobalBackBuffer);

				++BlueOffset;
				GreenOffset += 2;
			}
		} 
		else
		{
			// TODO: logging
		}
 
	} else
	{
		// TODO: logging
	}
  return 0;
}