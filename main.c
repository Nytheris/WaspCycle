#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>

#include <Windows.h>

typedef uint32_t bool32;

#define true 1
#define false 0

//Global variables really aren't a big deal for something like this.
static int Offset = 0;
static bool32 ShiftDown = false;
static int ClientWidth = 400;
static int ClientHeight = 400;

static const char *HelpMessage = "For PC players: Everything should work with no adjustments.\n\n\
For console players:\n - Tap the left/right arrow keys to adjust the offset.\n\
- Hold shift for bigger offset adjustments.\n - Press down arrow to reset the offset.";

LRESULT CALLBACK WaspCycleWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch(Message)
	{
		case WM_CLOSE:
		{
			DestroyWindow(Window);
		} break;

		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;

		case WM_SIZE:
		{
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			ClientWidth = ClientRect.right - ClientRect.left;
			ClientHeight = ClientRect.bottom - ClientRect.top;
		} break;

		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32_t VKCode = (uint32_t)WParam;

			bool32 WasDown = ((LParam & (1 << 30)) != 0);
			bool32 IsDown = ((LParam & (1 << 31)) == 0);

			switch(VKCode)
			{
				case VK_SHIFT:
				{
					ShiftDown = IsDown;
				} break;

				case 'H':
				{
					if(WasDown && !IsDown)
					{
						MessageBoxA(Window, HelpMessage, "Help", MB_OK);
					}
				} break;

				case VK_LEFT:
				{
					if(WasDown && !IsDown) Offset -= (ShiftDown ? 125 : 25);
				} break;

				case VK_RIGHT:
				{
					if(WasDown && !IsDown) Offset += (ShiftDown ? 125 : 25);
				} break;

				case VK_DOWN:
				{
					Offset = 0;
				} break;
			}
		} break;

		default:
		{
			return DefWindowProcA(Window, Message, WParam, LParam);
		} break;
	}

	return 0;
}

//Almost everything in the main function because it's only like 200 lines, deal with it.
//Also no error checking, because I really can't be arsed.
int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCode)
{
	uint64_t PerfCountFreq;
	
	timeBeginPeriod(1);
	
	{
		LARGE_INTEGER PerfCountFreqResult;
		QueryPerformanceFrequency(&PerfCountFreqResult);
		PerfCountFreq = PerfCountFreqResult.QuadPart;
	}

	WNDCLASSA WindowClass = { 0 };

	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	WindowClass.lpfnWndProc = WaspCycleWndProc;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "WaspCycleWindowClass";

	RegisterClassA(&WindowClass);

	int ClientWidth = 400;
	int ClientHeight = 400;

	RECT WindowRect = { 0 };
	WindowRect.right = ClientWidth;
	WindowRect.bottom = ClientHeight;

	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);

	HWND Window = CreateWindowA(WindowClass.lpszClassName, "Wasp Cycle",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top,
		0, 0, Instance, 0);

	HDC DeviceContext = GetDC(Window);
	
	int BufferWidth = ClientWidth;
	int BufferHeight = ClientHeight;

	BITMAPINFO BufferInfo = { 0 };
	BufferInfo.bmiHeader.biSize = sizeof(BufferInfo.bmiHeader);
	BufferInfo.bmiHeader.biWidth = BufferWidth;
	BufferInfo.bmiHeader.biHeight = -BufferHeight;
	BufferInfo.bmiHeader.biPlanes = 1;
	BufferInfo.bmiHeader.biBitCount = 32;
	BufferInfo.bmiHeader.biCompression = BI_RGB;

	//Memory buffer for writing red pixels to.
	void *BufferMemory = VirtualAlloc(0, BufferWidth * BufferHeight * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	//I cba to adjust this to monitor refresh rate. 120 fps is good enough.
	float TargetMS = 1000.0f / 120.0f;

	bool32 Running = true;

	LARGE_INTEGER StartCounter;
	QueryPerformanceCounter(&StartCounter);

	while(Running)
	{
		MSG Message;

		while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
		{
			if(Message.message == WM_QUIT)
			{
				Running = false;
			}

			TranslateMessage(&Message);
			DispatchMessageA(&Message);
		}

		ZeroMemory(BufferMemory, BufferWidth * BufferHeight * 4);


		//Calculate the position of the wasp. (Thanks ActorAnimationWasp::Update)
		LARGE_INTEGER WaspCounter;
		QueryPerformanceCounter(&WaspCounter);

		float C = 317;
		uint64_t TotalMS = (WaspCounter.QuadPart * 1000) / PerfCountFreq + Offset;
		float MovementTimer = (float)(TotalMS % (uint64_t)(C * M_PI * 2)) / C;

		float WaspX = cosf(MovementTimer);
		float WaspY = -sinf(MovementTimer * 2);

		//Draw the square to the buffer. No need for bounds checking as the position never goes close to the edges.
		{
			int DrawX = (int)(100 * WaspX) + (BufferWidth / 2);
			int DrawY = (int)(100 * WaspY) + (BufferHeight / 2);

			int StartX = DrawX - 16;
			int EndX = DrawX + 16;

			int StartY = DrawY - 16;
			int EndY = DrawY + 16;

			uint32_t *Row = (uint32_t *)BufferMemory + (StartY * BufferWidth);

			for(int Y = StartY; Y < EndY; ++Y)
			{
				uint32_t *Pixel = (uint32_t *)Row + StartX;

				for(int X = StartX; X < EndX; ++X)
				{
					*Pixel++ = 0x00FF0000;
				}

				Row += BufferWidth;
			}
		}
		
		StretchDIBits(DeviceContext, 0, 0, ClientWidth, ClientHeight, 0, 0, BufferWidth, BufferHeight, BufferMemory, &BufferInfo, DIB_RGB_COLORS, SRCCOPY);

		//Program runs very fast so sleep for a bit.
		LARGE_INTEGER EndCounter;
		QueryPerformanceCounter(&EndCounter);

		uint64_t CounterElapsed = EndCounter.QuadPart - StartCounter.QuadPart;
		float ElapsedMS = ((float)CounterElapsed / (float)PerfCountFreq) * 1000.0f;

		if(ElapsedMS < TargetMS)
		{
			DWORD MSToSleep = (DWORD)(TargetMS - ElapsedMS);
			Sleep(MSToSleep);

			while(ElapsedMS < TargetMS)
			{
				QueryPerformanceCounter(&EndCounter);
				CounterElapsed = EndCounter.QuadPart - StartCounter.QuadPart;
				ElapsedMS = ((float)CounterElapsed / (float)PerfCountFreq) * 1000.0f;
			}
		}

		QueryPerformanceCounter(&StartCounter);
	}
	
	return 0;
}
