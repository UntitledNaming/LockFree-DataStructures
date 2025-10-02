
#include <windows.h>
#include <iostream>
#include <process.h>
#include <stack>
#include <queue>
#include <string>
#include <pdh.h>
#include "Test.h"
#include "DumpClass.h"


CCrashDump dump;

int main()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x80000007);
    if (cpuInfo[3] & (1 << 8)) {
        // Invariant TSC supported
    }
    else
        __debugbreak();

    wprintf(L"Test Start...\n");

    // 사전 작업
    TestInit();

    // 스레드 생성
    ThreadCreate();

    // 스레드 제거 및 정리
    TestClear();


    wprintf(L"Test End...\n");
}
