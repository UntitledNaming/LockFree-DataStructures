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

		//���� Ǯ�� ��尡 ������ 
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

	void Free(T* pData)
	{
		EnterCriticalSection(&cs);

		if (!pool.empty())
		{
			if (pData == pool.front())
				__debugbreak();
		}

		pool.push(pData);
		LeaveCriticalSection(&cs);
	}


private:
	CRITICAL_SECTION        cs;
	std::queue<T*>        pool;
};