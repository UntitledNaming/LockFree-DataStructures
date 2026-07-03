#include <windows.h>
#include <process.h>
#include <stack>
#include <queue>
#include <string>
#include <thread>
#include <pdh.h>
#include <time.h>
#pragma warning (disable:4996)

#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"Pdh.lib")

#include "Test.h"

int main()
{
    wprintf(L"Test Start...\n");

    CTest* p = new CTest;


    // 사전 작업
    p->TestInit();

    // 스레드 생성
    p->ThreadCreate();

    // 스레드 제거 및 정리
    p->TestClear();


    wprintf(L"Test End...\n");
}
