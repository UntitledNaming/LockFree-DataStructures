#include <windows.h>
#include <process.h>
#include <stack>
#include <queue>
#include <string>
#include <pdh.h>
#include <time.h>
#pragma warning (disable:4996)
#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"Pdh.lib")

#include "CPUUsage.h"
#include "ProcessMonitor.h"
#include "Stack.h"
#include "LockFreeMemoryPoolLive.h"
#include "LFStack.h"
#include "Test.h"

FLOAT                                       g_processtotalavg;
FLOAT                                       g_processuseravg;
FLOAT                                       g_processkernelavg;
DOUBLE                                      g_processCSavg;
INT                                         g_thCount;
INT                                         g_testTime;
INT                                         g_testType;
UINT64                                      g_loopCnt;
UINT64*                                     g_count;
UINT64*                                     g_failcount;
UINT64*                                     g_usertime;                // spinlock vs srwlock에서 lock 걸기 전후 시간 차이 저장하는 배열
SRWLOCK                                     g_SRWLock;
HANDLE*                                     g_hThread;
HANDLE                                      g_FileStore;
UINT*                                       g_hThreadID;
BOOL                                        g_EndFlag;
LFStack<int>                                g_testLFStack;
Stack<int>                                  g_testStack;
INTERLOCKCNT*                               g_cntarray;                // interlock, cas 총 시도, 성공 횟수, 실패 횟수 기록하는 배열

ProcessMonitor*                             g_pPDH;
__declspec(align(64)) LONG                  g_spinLock;

unsigned __stdcall TestThread(void* pArguments)
{
    wprintf(L"TestThread Start : %d...\n", GetCurrentThreadId());
    int idx = (int)pArguments;

     StackTest(idx);


    return 0;
}

unsigned __stdcall MonitorThread(void* pArguments)
{
    INT64 loopCnt = 0;
    INT64 sumcount = 0;
    DWORD result;
    float processtotalsum = 0;
    float processusersum = 0;
    float processkernelsum = 0;
    DOUBLE processcssum = 0;

    g_pPDH->UpdateCounter();

    while (!g_EndFlag)
    {
        result = WaitForSingleObject(g_FileStore, 1000);

        if (result == WAIT_OBJECT_0)
        {
            // 파일 저장 작업
            FileStore();
            g_EndFlag = true;
            break;
        }

        else if (result == WAIT_TIMEOUT)
        {
            loopCnt++;
            g_pPDH->UpdateCounter();


            processtotalsum += g_pPDH->ProcessTotal();
            processusersum += g_pPDH->ProcessUser();
            processkernelsum += g_pPDH->ProcessKernel();
            processcssum += g_pPDH->m_processCS;

            for (int i = 0; i < g_thCount; i++)
            {
                g_failcount[i] = 0;
            }
            g_processtotalavg = processtotalsum / loopCnt;
            g_processuseravg = processusersum / loopCnt;
            g_processkernelavg = processkernelsum / loopCnt;
            g_processCSavg = processcssum / loopCnt;

            wprintf(L"[ CPU Average Usage                    : T[%f%] U[%f%] K[%f%]]\n", g_processtotalavg, g_processuseravg, g_processkernelavg);
            wprintf(L"[ CPU Usage                            : T[%f%] U[%f%] K[%f%]]\n", g_pPDH->ProcessTotal(), g_pPDH->ProcessUser(), g_pPDH->ProcessKernel());
            wprintf(L"[ Process Context Switches/sec         : Cur[%4f /sec ] Avg[%4f /sec]]\n", g_pPDH->m_processCS, g_processCSavg);
            wprintf(L"[Count : %lld ]\n", sumcount);
            sumcount = 0;
        }

    }

    return 0;
}

UINT64 GetUserTime()
{
    FILETIME ct;
    FILETIME et;
    FILETIME kt;
    FILETIME ut;

    GetThreadTimes(GetCurrentThread(), &ct, &et, &kt, &ut);

    return (UINT64)(ut.dwHighDateTime << 32 | ut.dwLowDateTime);
}

void ThreadCreate()
{
    // 스레드 생성 작업
    g_hThread = new HANDLE[g_thCount + 1];

    for (int i = 0; i < g_thCount; i++)
    {
        g_hThread[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread, (void*)(i), CREATE_SUSPENDED, &g_hThreadID[i]);
        if (g_hThread[i] == NULL)
        {
            wprintf(L"begintreadex Error : %d", GetLastError());
            return;
        }
    }


    for (int i = 0; i < g_thCount; i++)
    {
        ResumeThread(g_hThread[i]);
    }

    g_pPDH = new ProcessMonitor(g_thCount, g_hThreadID);

    g_hThread[g_thCount] = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, NULL, NULL, NULL);
    if (g_hThread[g_thCount] == NULL)
    {
        wprintf(L"begintreadex Error : %d", GetLastError());
    }

}

void TestClear()
{
    // 대기
    WaitForMultipleObjects(g_thCount, g_hThread, TRUE, INFINITE);

    SetEvent(g_FileStore);

    // 파일 저장 기다리기
    WaitForSingleObject(g_hThread[g_thCount], INFINITE);

    for (int i = 0; i < g_thCount+1; i++)
    {
        CloseHandle(g_hThread[i]);
    }

    delete[] g_hThread;
    delete[] g_hThreadID;
    delete[] g_count;
}

void TestInit()
{
    timeBeginPeriod(1);
    InitializeSRWLock(&g_SRWLock);
    g_testLFStack.Clear();
    g_spinLock = 0;

    // 스레드 갯수 입력
    wprintf(L"Thread Count : ");
    wscanf(L"%d", &g_thCount);

    // 테스트 타입 입력(0 : 유저 동기화 객체 , 1 : SpinLock , 2 : LockFree)
    wprintf(L"Test Type(SRWLOCK : 0 / SpinLock : 1 / LockFree : 2 ) : ");
    wscanf(L"%d", &g_testType);

    // 1회 테스트할 시간 입력
    wprintf(L"Test Time(minute) : ");
    wscanf(L"%d", &g_testTime);

    g_count     = new  UINT64[g_thCount];
    for (int i = 0; i < g_thCount; i++)
    {
        g_count[i] = 0;
    }

    g_failcount = new  UINT64[g_thCount];
    for (int i = 0; i < g_thCount; i++)
    {
        g_failcount[i] = 0;
    }

    g_usertime = new  UINT64[g_thCount];
    for (int i = 0; i < g_thCount; i++)
    {
        g_usertime[i] = 0;
    }

    g_cntarray = new  INTERLOCKCNT[g_thCount];
    for (int i = 0; i < g_thCount; i++)
    {
        g_cntarray[i].s_failCnt = 0;
        g_cntarray[i].s_succesCnt = 0;
        g_cntarray[i].s_totalCnt = 0;
    }

    g_hThreadID = new  UINT[g_thCount];
    g_FileStore = CreateEvent(NULL, FALSE, FALSE, NULL);

}

void StackTest(int idx)
{
    int data;

    DWORD start = timeGetTime();
    UINT64 lockstart;

    int testtime = g_testTime;

    if (g_testType == 0)
    {
        while (1)
        {
            if ((timeGetTime() - start) >= testtime * 60000)
                break;

            lockstart = __rdtsc();
            AcquireSRWLockExclusive(&g_SRWLock);
            g_usertime[idx] += (__rdtsc() - lockstart);
            g_testStack.Push(1);
            g_testStack.pop(data);
            ReleaseSRWLockExclusive(&g_SRWLock);
            g_count[idx]++;

        }
    }

    else if (g_testType == 1)
    {
        while (1)
        {
            if ((timeGetTime() - start) >= testtime * 60000)
                break;

            SpinLock(idx);

            g_testStack.Push(1);
            g_testStack.pop(data);
            SpinUnlock();

            g_count[idx]++;
        }
    }

    else if (g_testType == 2)
    {
        while (1)
        {
            if ((timeGetTime() - start) >= testtime * 60000)
                break;


            g_testLFStack.Push(1, idx);
            g_testLFStack.Pop(data,idx);

            g_count[idx]++;

        }
    }

}

void FileStore()
{
    errno_t err;
    FILE* fp;
    std::wstring fileName;
    std::wstring data;
    time_t start;
    tm* local_time;
    UINT64 sum =0;
    UINT64 usertimesum =0;
    UINT64 totalcntsum =0;
    UINT64 successcntsum =0;
    UINT64 failcntsum =0;
    DOUBLE successratio = 0;
    DOUBLE failratio = 0;

    for (int i = 0; i < g_thCount; i++)
    {
        sum += g_count[i];
    }

    for (int i = 0; i < g_thCount; i++)
    {
        usertimesum += g_usertime[i];
    }

    for (int i = 0; i < g_thCount; i++)
    {
        totalcntsum += g_cntarray[i].s_totalCnt;
    }
    for (int i = 0; i < g_thCount; i++)
    {
        successcntsum += g_cntarray[i].s_succesCnt;
    }
    for (int i = 0; i < g_thCount; i++)
    {
        failcntsum += g_cntarray[i].s_failCnt;
    }

    successratio = (DOUBLE)successcntsum / totalcntsum * 100.0;
    failratio = (DOUBLE)failcntsum / totalcntsum * 100.0;

    start = time(NULL);
    local_time = localtime(&start);

    if (g_testType == 0)
    {
        fileName += L"srwlock_threadCount";
    }
    else if (g_testType == 1)
    {
        fileName += L"spinlock_threadCount";
    }
    else if (g_testType == 2)
    {
        fileName += L"lockfree_threadCount";
    }

    fileName += to_wstring(g_thCount);

    fileName += L"_";

    fileName += L"testTime";

    fileName += to_wstring(g_testTime);

    fileName += L".txt";

    err = _wfopen_s(&fp, fileName.c_str(), L"ab");
    if (err != 0)
    {
        wprintf(L"파일 열기 실패. 에러 코드: %d \n", err);
        __debugbreak();
        return;
    }


    start = time(NULL);
    local_time = localtime(&start);

    data = L"[" + to_wstring(local_time->tm_year + 1900)+L"." + to_wstring(local_time->tm_mon + 1) + L"." + to_wstring(local_time->tm_mday) + L"." + to_wstring(local_time->tm_hour) + L":" + to_wstring(local_time->tm_min) + L":" + to_wstring(local_time->tm_sec) + L"] \n";

    data += L"Total Count : ";
    data += to_wstring(sum);
    data += L" / ";

    data += L"UserTime Sum : ";
    data += to_wstring(usertimesum);
    data += L" (100ns) / \n";

    data += L"Interlock Try Total Count : ";
    data += to_wstring(totalcntsum);
    data += L" / ";

    data += L"Interlock Success Total Count : ";
    data += to_wstring(successcntsum);
    data += L" / ";

    data += L"Interlock Failed Total Count : ";
    data += to_wstring(failcntsum);
    data += L" / \n";

    data += L"Interlock Success Ratio : ";
    data += to_wstring(successratio);
    data += L" % / \n";

    data += L"Interlock Failed Ratio : ";
    data += to_wstring(failratio);
    data += L" % / \n";

    data += L"Average Process Total Time : ";
    data += to_wstring(g_processtotalavg);
    data += L" % / \n";

    data += L"Average Process User Time : ";
    data += to_wstring(g_processuseravg);
    data += L" % / \n";

    data += L"Average Process Kernel Time : ";
    data += to_wstring(g_processkernelavg);
    data += L" % / \n";

    data += L"Average Process Context Switches : ";
    data += to_wstring(g_processCSavg);
    data += L" /sec \n";

    for (int i = 0; i < g_thCount; i++)
    {
        data += L"Thread Count : ";
        data += to_wstring(g_count[i]);
        data += L"\n";
    }


    fseek(fp, 0, SEEK_END);
    fwrite(data.c_str(), sizeof(WCHAR) * data.size(), 1, fp);

    fclose(fp);
}

void SpinLock(int idx)
{
    while (1)
    {
        g_cntarray[idx].s_totalCnt++;
        if (InterlockedExchange(&g_spinLock, 1) == 0)
        {
            g_cntarray[idx].s_succesCnt++;
            break;
        }
        else
            g_cntarray[idx].s_failCnt++;

    }
}

void SpinUnlock()
{
    InterlockedExchange(&g_spinLock, 0);
}

