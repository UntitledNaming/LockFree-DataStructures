#pragma once
#include <queue>
#include <windows.h>

template <typename T>
class CMemoryPoolQ
{
public:
	CMemoryPoolQ()
	{
		InitializeCriticalSection(&cs);
	}

	T* Alloc()
	{
		EnterCriticalSection(&cs);

		//만약 풀에 노드가 있으면 
		if (!pool.empty())
		{
			T* pData = pool.front();
			pool.pop();

			LeaveCriticalSection(&cs);
			return pData;
		}
		else
		{
			T* pData = new T;
			LeaveCriticalSection(&cs);

			return pData;
		}
	}

	bool Free(T* pData)
	{
		EnterCriticalSection(&cs);

		if (!pool.empty())
		{
			if (pData == pool.front())
				return false;
		}

		pool.push(pData);
		LeaveCriticalSection(&cs);

		return true;
	}


private:
	CRITICAL_SECTION        cs;
	std::queue<T*>        pool;
};