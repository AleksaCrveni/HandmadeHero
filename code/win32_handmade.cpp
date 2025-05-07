#include <stdint.h>

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

#define internal static 
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

#include <math.h>
#include "handmade.cpp"

#include <windows.h>
#include <winuser.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>
#include <malloc.h>

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
	real32 TSine;
	int LatencySampleCount;
};

global_variable bool32 GlobalRunning;
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
		XInputLibrary = LoadLibrary("xinput9_1_0.dll");
	}

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
			// comparsion in first () will return value and we want to turn it into 1 or 0 ( bool32)
			// so we compare to 0 after
			bool32 WasDown = ((LParam & (1 << 30)) != 0);
			// 31st bit is always 0 for SYSKEYDOWN
			bool32 IsDown = ((LParam & (1 << 31)) == 0);
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


internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0,SoundOutput->SecondaryBufferSize,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		uint8 *DestSample = (uint8 *)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
		{
			// set to 0
			*DestSample++ = 0;
		}
		DestSample = (uint8 *)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
		{
			// set to 0
			*DestSample++ = 0;
		}
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock,
																	DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
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

		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;

		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			// Just copy, if you see perf issue, fix
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DestSample = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{ 
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		// have to unlock to tell direct soudn that you finished writing to the buffer
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}

}

void * PlatformLoadFile(char *FileName)
{
	return 0;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCode)
{
	LARGE_INTEGER PerfCounterFreqResult;
	QueryPerformanceFrequency(&PerfCounterFreqResult);
	int64 PerfCounterFreq = PerfCounterFreqResult.QuadPart;

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
			// we will write 1/15th of a second ahead of cursor
			SoundOutput.LatencySampleCount =  SoundOutput.SamplePerSecond / 15;
			Win32InitSound(Window, SoundOutput.SamplePerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			LARGE_INTEGER LastCounter;
			QueryPerformanceCounter(&LastCounter);

			uint64 LastCycleCount = __rdtsc();

			while (GlobalRunning)
			{
				
				// If 0 passed as handle it will retrieve any messages that belong to us
				// GetMessage will block thread if there are no messages
				//bool32 MessageResult  = GetMessage(&Message, 0, 0, 0);
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
						bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool32 Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool32 Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool32 LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool32 RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool32 AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool32 BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool32 XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool32 YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;

					}
					else
					{
						// not available
					}
				}

				DWORD ByteToLock;
				DWORD BytesToWrite;
				DWORD WriteCursor;
				DWORD PlayCursor;
				DWORD TargetCursor;
				bool32 SoundIsValid = false;

				// Thighen up sound logic so that we knowwhere we should be writing
				// and anticipate time spent in the game update
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
				{
					
					// we mod (%) to get remainder which is pretty much where we are because its ring buffer
					ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);

					// Mod by buffer size because Targetcursor can reach the end and wrap
					TargetCursor = ((PlayCursor +
														 	 (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
															 SoundOutput.SecondaryBufferSize);

					if (ByteToLock > TargetCursor)
					{ // ByteToLock is in front of PlayCursor, we have to handle 2 regions
						// Day 008 ~45min in case I forget how this works
						BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
						BytesToWrite += TargetCursor;
					}
					else
					{
						BytesToWrite = TargetCursor - ByteToLock;
					}
					SoundIsValid = true;
				}

				game_offscreen_buffer GameBuffer = {}; // clear to zero!
				GameBuffer.Memory = GlobalBackBuffer.Memory;
				GameBuffer.Width = GlobalBackBuffer.Width;
				GameBuffer.Height = GlobalBackBuffer.Height;
				GameBuffer.Pitch = GlobalBackBuffer.Pitch;

				
				game_sound_output_buffer SoundBuffer = {};
				SoundBuffer.SamplePerSecond = SoundOutput.SamplePerSecond;
				SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
				SoundBuffer.Samples = Samples;
				GameUpdateAndRender(&GameBuffer, BlueOffset, GreenOffset, &SoundBuffer);
				// Direct sound output test
				
				if (SoundIsValid)
				{	
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
				}
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32CopyBufferToWindow(
					DeviceContext, Dimension.Width, Dimension.Height,
					&GlobalBackBuffer);

				++BlueOffset;
				GreenOffset += 2;

				LARGE_INTEGER EndCounter;
				uint64 EndCycleCount = __rdtsc();
				QueryPerformanceCounter(&EndCounter);
				
				

				int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
				real32 MSPerFrame = (real32)(((1000.0f*(real32)CounterElapsed) / (real32)PerfCounterFreq));
				//int32 d = (CounterElapsed * (1/PerfCounterFreq));
				real32 FPS = (real32)PerfCounterFreq / (real32)CounterElapsed;
				uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
				// Mega cycles per second - so whatever value here we executed MCPF * 1000 * 1000 instructions
				// this is just for easier viewing 
				real32 MCPF = (real32)CyclesElapsed / (1000.0f * 1000.0f);
				char Buffer[256];
				
				sprintf(Buffer, "Milliseconds/frame: %fms FPS: %f Cycles: %f \n", MSPerFrame, FPS, MCPF);
				OutputDebugString(Buffer);
				LastCounter = EndCounter;
				LastCycleCount = EndCycleCount;
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