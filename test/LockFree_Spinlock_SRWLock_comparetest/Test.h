#pragma once

struct INTERLOCKCNT
{
	UINT64 s_totalCnt;
	UINT64 s_succesCnt;
	UINT64 s_failCnt;
};

unsigned __stdcall TestThread(void* pArguments);
unsigned __stdcall MonitorThread(void* pArguments);

UINT64 GetUserTime();

void ThreadCreate();
void TestClear();
void TestInit();
void StackTest(int idx);
void FileStore();
void SpinLock(int idx);
void SpinUnlock();
