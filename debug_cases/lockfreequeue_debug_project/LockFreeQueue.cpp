#include <iostream>
#include <windows.h>
#include <process.h>
#include "LockFreeQ.h"
#include "DumpClass.h"
#define THREAD_COUNT 4

unsigned int WINAPI WorkerThread(LPVOID lpParam);

LFQueue<int> g_LFQ;
CCrashDump dump;



int main()
{
	wprintf(L"MainThread...Start!\n");


	HANDLE hThread[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		hThread[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, NULL);
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

}

unsigned int __stdcall WorkerThread(LPVOID lpParam)
{
	DWORD curThreadID = GetCurrentThreadId();
	int Param;
	int Cnt = 0;
	bool ESC = false;

	wprintf(L"Thread ID : %d ...Start!\n", curThreadID);

	g_LFQ.StoreHandle(curThreadID);

	Param = 0;

	while (!ESC)
	{
		for (int i = 0; i < 2; i++)
		{
			g_LFQ.Enqueue(Param);
		}

		

		for (int i = 0; i < 2; i++)
		{
			if (!g_LFQ.Dequeue(Param))
				__debugbreak();
		}

		Cnt++;
	}

	wprintf(L"Thread ID : %d ... End! \n", curThreadID);
	return 0;
}

