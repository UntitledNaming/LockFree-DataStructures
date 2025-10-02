#include <windows.h>
#include <thread>
#include <iostream>
#include "LFQSingleLive.h"
#pragma comment(lib, "winmm.lib")

#define THREAD_COUNT 3

LFQueue<int> g_queue;
unsigned int WINAPI WorkerThread1(LPVOID lpParam);
unsigned int WINAPI WorkerThread2(LPVOID lpParam);
unsigned int WINAPI WorkerThread3(LPVOID lpParam);

int main()
{
	timeBeginPeriod(1);

	HANDLE hThread[THREAD_COUNT];

	// 1, 2, 3 노드 큐에 넣기
	for (int i = 1; i <= 2; i++)
	{
		g_queue.Enqueue(i);
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

	g_queue.Enqueue(1000);
	g_queue.Enqueue(1001);

	return 0;
}

unsigned int __stdcall WorkerThread2(LPVOID lpParam)
{

	g_queue.Enqueue(200);

	return 0;
}

unsigned int __stdcall WorkerThread3(LPVOID lpParam)
{
	int data;
	g_queue.Enqueue(100);
	g_queue.Enqueue(101);
	g_queue.Enqueue(102);

	g_queue.Dequeue(data);
	g_queue.Dequeue(data);
	g_queue.Dequeue(data);

	while (g_queue.Dequeue(data))
	{
		printf("%d \n", data);
	}

	return 0;
}
