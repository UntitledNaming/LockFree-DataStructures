#include <iostream>
#include <windows.h>
#include <process.h>
#include <stdio.h>



#ifdef ORDER_REVERSAL
#include "LFQSingle.h"
#endif
#include "LFQSingle.h"

LFQueue<int> g_LFQ;

unsigned __stdcall ThreadFunc1(void* arg)
{
#ifdef ORDER_REVERSAL
    g_LFQ.Enqueue(1000);
    g_LFQ.Enqueue(1001);
#endif

    while (1)
    {
        for (int i = 0; i < 3; i++)
        {
            g_LFQ.Enqueue(1);
        }
        for (int i = 0; i < 3; i++)
        {
            int val;
            if (!g_LFQ.Dequeue(val))
                __debugbreak();
        }
    }

    return 0;  // 스레드 종료 코드
}

unsigned __stdcall ThreadFunc2(void* arg)
{
#ifdef ORDER_REVERSAL
    int val1;
    g_LFQ.Eequeue(200);
#endif

    while (1)
    {
        for (int i = 0; i < 3; i++)
        {
            g_LFQ.Enqueue(1);
        }
        for (int i = 0; i < 3; i++)
        {
            int val;
            if (!g_LFQ.Dequeue(val))
                __debugbreak();
        }
    }

    return 0;  // 스레드 종료 코드
}

unsigned __stdcall ThreadFunc3(void* arg)
{
#ifdef ORDER_REVERSAL
    int val;
    g_LFQ.Enqueue(100);
    g_LFQ.Enqueue(101);
    g_LFQ.Enqueue(102);
    g_LFQ.Dequeue(val);
    g_LFQ.Dequeue(val);
    g_LFQ.Dequeue(val);

    while (g_LFQ.Dequeue(val))
    {
        printf("%d\n", val);
    }

#endif

    return 0;  // 스레드 종료 코드
}

int main()
{
#ifdef ORDER_REVERSAL
    g_LFQ.Enqueue(1);
    g_LFQ.Enqueue(2);
#endif

    HANDLE hThread[2];

    hThread[0] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc1, NULL, 0, NULL);
    hThread[1] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc2, NULL, 0, NULL);

#ifdef ORDER_REVERSAL
    hThread[2] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc3, NULL, 0, NULL);
#endif
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
}


