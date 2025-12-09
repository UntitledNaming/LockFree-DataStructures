#pragma once
#include <windows.h>

struct INTERLOCKCNT
{
	UINT64 s_totalCnt;
	UINT64 s_succesCnt;
	UINT64 s_failCnt;
};