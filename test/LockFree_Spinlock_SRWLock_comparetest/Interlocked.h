#pragma once
#include <windows.h>

struct INTERLOCKCNT
{
	alignas(64) UINT64 s_totalCnt;
	alignas(64) UINT64 s_succesCnt;
	alignas(64) UINT64 s_failCnt;
	alignas(64) UINT64 s_topCnt;
	alignas(64) UINT64 s_unlockCnt;
}; 