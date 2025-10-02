#pragma comment(lib, "winmm.lib")

#include <windows.h>
#include <thread>
#include "LFStack_ver2.h"


#define THREAD_COUNT 2
#define NODE_COUNT   256

LFStack<int> g_Stack;
unsigned int WINAPI WorkerThread(LPVOID lpParam);

int main()
{
	timeBeginPeriod(1);

	HANDLE hThread[THREAD_COUNT];

	for (int i = 0; i < NODE_COUNT; i++)
	{
		g_Stack.Push(i);
	}

	//스레드 생성
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		hThread[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, NULL, NULL);
	}

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		if (hThread[i] == 0)
		{
			wprintf(L"Thread Create Error \n");
			return -1;
		}
	}

	WaitForMultipleObjects(THREAD_COUNT, hThread, true, INFINITE);


	for (int i = 0; i < THREAD_COUNT; i++)
	{
		CloseHandle(hThread[i]);
	}

	wprintf(L"maint thread...end\n");

}


unsigned int __stdcall WorkerThread(LPVOID lpParam)
{
	DWORD threadID = GetCurrentThreadId();
	wprintf(L"Thread Start... Thread ID : %d \n", threadID);
	int data;
	bool ESC = true;


		for (int i = 0; i < NODE_COUNT; i++)
		{
			if (!g_Stack.Pop(data))
			{
				__debugbreak();
			}
		}

	wprintf(L"Thread End... ID :%d\n", threadID);
	return 0;
}