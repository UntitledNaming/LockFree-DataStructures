#include <windows.h>
#include <process.h>
#include <stack>
#include <queue>
#include <thread>
#include <string>
#include <pdh.h>
#include <time.h>
#pragma warning (disable:4996)
#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"Pdh.lib")

#include "Interlocked.h"
#include "CPUUsage.h"
#include "ProcessMonitor.h"
#include "Stack.h"
#include "LFStack.h"
#include "Test.h"


void CTest::TestThread(void* pArguments)
{
    wprintf(L"TestThread Start : %d...\n", GetCurrentThreadId());
    int idx = (int)pArguments;

    if (m_thCount <= 6)
    {
        SetThreadAffinityMask(GetCurrentThread(), 1ULL << (idx * 2));
    }


    StackTest(idx);

}
void CTest::MonitorThread(void* pArguments)
{
    INT64 loopCnt = 0;
    DWORD result;
    bool  endflag = false;
    float processtotalsum = 0;
    float processusersum = 0;
    float processkernelsum = 0;
    DOUBLE processcssum = 0;

    m_pPDH->UpdateCounter();

    while (!endflag)
    {
        result = WaitForSingleObject(m_FileStore, 1000);

        if (result == WAIT_OBJECT_0)
        {
            // 파일 저장 작업
            FileStore();
            endflag = true;
            break;
        }

        else if (result == WAIT_TIMEOUT)
        {
            loopCnt++;
            m_pPDH->UpdateCounter();


            processtotalsum += m_pPDH->ProcessTotal();
            processusersum += m_pPDH->ProcessUser();
            processkernelsum += m_pPDH->ProcessKernel();
            processcssum += m_pPDH->m_processCS;

            m_processtotalavg = processtotalsum / loopCnt;
            m_processuseravg = processusersum / loopCnt;
            m_processkernelavg = processkernelsum / loopCnt;
            m_processCSavg = processcssum / loopCnt;

            wprintf(L"[ CPU Average Usage                    : T[%f%%] U[%f%%] K[%f%%]]\n", m_processtotalavg, m_processuseravg, m_processkernelavg);
            wprintf(L"[ CPU Usage                            : T[%f%%] U[%f%%] K[%f%%]]\n", m_pPDH->ProcessTotal(), m_pPDH->ProcessUser(), m_pPDH->ProcessKernel());
            wprintf(L"[ Process Context Switches/sec         : Cur[%4f /sec ] Avg[%4f /sec]]\n", m_pPDH->m_processCS, m_processCSavg);
        }

    }

}

void CTest::ThreadCreate()
{
    // 스레드 생성 작업
    m_hThread = new std::thread[m_thCount + 1];

    for (int i = 0; i < m_thCount; i++)
    {
        m_hThread[i] = std::thread(&CTest::TestThread, this, (void*)i);
        m_hThreadID[i] = GetThreadId(m_hThread[i].native_handle());
    }

    m_pPDH = new ProcessMonitor(m_thCount, m_hThreadID);

    m_hThread[m_thCount] = std::thread(&CTest::MonitorThread, this, (void*)m_thCount);
}

void CTest::TestClear()
{
    // 테스트 스레드 대기
    for (int i = 0; i < m_thCount; i++)
    {
        if (m_hThread[i].joinable())
        {
            m_hThread[i].join();
        }
    }

    SetEvent(m_FileStore);

    // 파일 저장 기다리기
    if (m_hThread[m_thCount].joinable())
    {
        m_hThread[m_thCount].join();
    }

    delete   m_testLFStack;
    delete   m_testStack;
    delete[] m_hThread;
    delete[] m_hThreadID;
    delete[] m_count;
    delete[] m_cntarray;
}

void CTest::TestInit()
{
    timeBeginPeriod(1);

    // 스레드 갯수 입력
    wprintf(L"Thread Count : ");
    wscanf(L"%d", &m_thCount);

    // 테스트 타입 입력(0 : 유저 동기화 객체 , 1 : SpinLock , 2 : LockFree)
    wprintf(L"Test Type(SRWLOCK : 0 / SpinLock : 1 / LockFree : 2 ) : ");
    wscanf(L"%d", &m_testType);

    // 1회 테스트할 시간 입력
    wprintf(L"Test Time(minute) : ");
    wscanf(L"%d", &m_testTime);

    m_count = new  PaddedCounter[m_thCount];
    for (int i = 0; i < m_thCount; i++)
    {
        m_count[i].count = 0;
    }

    m_cntarray = new  INTERLOCKCNT[m_thCount];
    for (int i = 0; i < m_thCount; i++)
    {
        m_cntarray[i].s_failCnt = 0;
        m_cntarray[i].s_succesCnt = 0;
        m_cntarray[i].s_totalCnt = 0;
        m_cntarray[i].s_topCnt = 0;
        m_cntarray[i].s_unlockCnt = 0;
    }

    m_hThreadID = new  DWORD[m_thCount];
    m_FileStore = CreateEvent(NULL, FALSE, FALSE, NULL);

    InitializeSRWLock(&m_SRWLock);
    m_testLFStack = new LFStack<int>;
    m_testStack = new Stack<int>;
    m_testLFStack->Clear();
    m_spinLock = 0;

}

void CTest::StackTest(int idx)
{
    int data;

    DWORD start = timeGetTime();

    int testtime = m_testTime;

    if (m_testType == 0)
    {
        while (1)
        {
            if ((timeGetTime() - start) >= testtime * 60000)
                break;

            AcquireSRWLockExclusive(&m_SRWLock);
            m_testStack->Push(1);
            m_testStack->pop(data);
            ReleaseSRWLockExclusive(&m_SRWLock);
            m_count[idx].count++;

        }
    }

    else if (m_testType == 1)
    {
        while (1)
        {
            if ((timeGetTime() - start) >= testtime * 60000)
                break;


            SpinLock(idx);
            m_testStack->Push(1);
            m_testStack->pop(data);
            SpinUnlock(idx);

            m_count[idx].count++;
        }
    }

    else if (m_testType == 2)
    {
        LONG val = 1;
        while (1)
        {
            if ((timeGetTime() - start) >= testtime * 60000)
                break;

            m_testLFStack->Push(1, idx, m_cntarray);
            m_testLFStack->Pop(data, idx, m_cntarray);


            m_count[idx].count++;

        }
    }
}

void CTest::FileStore()
{
    errno_t err;
    FILE* fp;
    std::wstring fileName;
    std::wstring data;
    time_t start;
    tm* local_time;
    UINT64 sum = 0;
    UINT64 totaltrycntsum = 0;
    UINT64 successcntsum = 0;
    UINT64 failcntsum = 0;
    UINT64 toptotalcntsum = 0;
    UINT64 totalunlockcntsum = 0;
    DOUBLE successratio = 0;
    DOUBLE failratio = 0;

    for (int i = 0; i < m_thCount; i++)
    {
        sum += m_count[i].count;
    }

    for (int i = 0; i < m_thCount; i++)
    {
        toptotalcntsum += m_cntarray[i].s_topCnt;
    }

    for (int i = 0; i < m_thCount; i++)
    {
        totalunlockcntsum += m_cntarray[i].s_unlockCnt;
    }

    for (int i = 0; i < m_thCount; i++)
    {
        totaltrycntsum += m_cntarray[i].s_totalCnt;
    }
    for (int i = 0; i < m_thCount; i++)
    {
        successcntsum += m_cntarray[i].s_succesCnt;
    }
    for (int i = 0; i < m_thCount; i++)
    {
        failcntsum += m_cntarray[i].s_failCnt;
    }

    if (m_testType != 0)
    {
        successratio = (DOUBLE)successcntsum / totaltrycntsum * 100.0;
        failratio = (DOUBLE)failcntsum / totaltrycntsum * 100.0;
    }
    else
    {
        successratio = 0;
        failratio = 0;
    }

    start = time(NULL);
    local_time = localtime(&start);

    if (m_testType == 0)
    {
        fileName += L"srwlock_threadCount";
    }
    else if (m_testType == 1)
    {
        fileName += L"spinlock_threadCount";
    }
    else if (m_testType == 2)
    {
        fileName += L"lockfree_threadCount";
    }

    fileName += to_wstring(m_thCount);

    fileName += L"_";

    fileName += L"testTime";

    fileName += to_wstring(m_testTime);

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

    data = L"[" + to_wstring(local_time->tm_year + 1900) + L"." + to_wstring(local_time->tm_mon + 1) + L"." + to_wstring(local_time->tm_mday) + L"." + to_wstring(local_time->tm_hour) + L":" + to_wstring(local_time->tm_min) + L":" + to_wstring(local_time->tm_sec) + L"] \n";

    data += L"Total Count : ";
    data += to_wstring(sum);
    data += L" / \n";

    data += L"Interlock TopCnt Total Count : ";
    data += to_wstring(toptotalcntsum);
    data += L" / \n";

    data += L"Interlock Unlock Total Count : ";
    data += to_wstring(totalunlockcntsum);
    data += L" / \n";

    data += L"Interlock Try Total Count : ";
    data += to_wstring(totaltrycntsum);
    data += L" / \n";

    data += L"Interlock Success Total Count : ";
    data += to_wstring(successcntsum);
    data += L" / \n";

    data += L"Interlock Failed Total Count : ";
    data += to_wstring(failcntsum);
    data += L" / \n";

    data += L"Interlock Success Ratio : ";
    data += to_wstring(successratio);
    data += L" % / \n";

    data += L"Interlock Failed Ratio : ";
    data += to_wstring(failratio);
    data += L" % / \n";

    data += L"Interlock Real Total Count : ";
    data += to_wstring(toptotalcntsum + totaltrycntsum + totalunlockcntsum);
    data += L" / \n";

    data += L"Average Process Total Time : ";
    data += to_wstring(m_processtotalavg);
    data += L" % / \n";

    data += L"Average Process User Time : ";
    data += to_wstring(m_processuseravg);
    data += L" % / \n";

    data += L"Average Process Kernel Time : ";
    data += to_wstring(m_processkernelavg);
    data += L" % / \n";

    data += L"Average Process Context Switches : ";
    data += to_wstring(m_processCSavg);
    data += L" /sec \n";

    for (int i = 0; i < m_thCount; i++)
    {
        data += L"Thread Count : ";
        data += to_wstring(m_count[i].count);
        data += L"\n";
    }


    fseek(fp, 0, SEEK_END);
    fwrite(data.c_str(), sizeof(WCHAR) * data.size(), 1, fp);

    fclose(fp);
}

void CTest::SpinLock(int idx)
{
    unsigned delay = 1;

    while (1)
    {
        m_cntarray[idx].s_totalCnt++;
        if (InterlockedExchange(&m_spinLock, 1) == 0)
        {
            m_cntarray[idx].s_succesCnt++;
            break;
        }
        else
            m_cntarray[idx].s_failCnt++;

    }
}

void CTest::SpinUnlock(int idx)
{
    InterlockedExchange(&m_spinLock, 0);
    m_cntarray[idx].s_unlockCnt++;
}
