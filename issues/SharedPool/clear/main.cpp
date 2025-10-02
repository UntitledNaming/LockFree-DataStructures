#include <windows.h>
#include <thread>
#include "LFQMultiLive.h"
#pragma comment(lib, "winmm.lib")

#define THREAD_COUNT 3

LFQueueMul<int> g_queue1;
LFQueueMul<int> g_queue2;
unsigned int WINAPI WorkerThread1(LPVOID lpParam);
unsigned int WINAPI WorkerThread2(LPVOID lpParam);
unsigned int WINAPI WorkerThread3(LPVOID lpParam);

int main()
{
	timeBeginPeriod(1);

	HANDLE hThread[THREAD_COUNT];

	// 1, 2 노드 1번 큐에 넣기
	for (int i = 1; i <= 2; i++)
	{
		g_queue1.Enqueue(i);
	}

	// 1001, 1002 노드 2번 큐에 넣기
	for (int i = 1001; i <= 1002; i++)
	{
		g_queue2.Enqueue(i);
	}

	//스레드 생성
	hThread[0] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread1, NULL, NULL, NULL);
	hThread[1] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread2, NULL, NULL, NULL);
	hThread[2] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread3, NULL, NULL, NULL);

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

unsigned int __stdcall WorkerThread1(LPVOID lpParam)
{

	g_queue1.Enqueue(10);

	return 0;
}

unsigned int __stdcall WorkerThread2(LPVOID lpParam)
{
	int data;

	for (int i = 3; i <= 5; i++)
	{
		g_queue1.Enqueue(i);
	}

	for (int i = 3; i <= 5; i++)
	{
		g_queue1.Dequeue(data);
	}

	g_queue2.Enqueue(1003);


	return 0;
}

unsigned int __stdcall WorkerThread3(LPVOID lpParam)
{
	int data;
	while (g_queue1.Dequeue(data))
	{
		wprintf(L"1 queue : %d \n", data);
	}

	while (g_queue2.Dequeue(data))
	{
		wprintf(L"2 queue : %d \n", data);
	}
	return 0;
}
