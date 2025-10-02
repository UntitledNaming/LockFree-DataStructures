
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

    // ���� �۾�
    TestInit();

    // ������ ����
    ThreadCreate();

    // ������ ���� �� ����
    TestClear();


    wprintf(L"Test End...\n");
}
