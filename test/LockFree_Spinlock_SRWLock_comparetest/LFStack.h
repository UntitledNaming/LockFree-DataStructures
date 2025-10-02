#pragma once
#define MAX_LEN         300
#define USER_MEMORY_MAX 0x00007FFFFFFFFFFF
#define BIT_MASK        0x00007FFFFFFFFFFF

#include "Test.h"
#include "LockFreeMemoryPoolLive.h"

using namespace std;

extern UINT64* g_failcount;
extern INTERLOCKCNT* g_cntarray;

template <typename T>
class LFStack
{
private:
	struct Node
	{
		T        data;
		Node*    pNextNode;
	};

public:
	LFStack(int size = 0)
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);

		if (!((UINT64)info.lpMaximumApplicationAddress & USER_MEMORY_MAX))
		{
			wprintf(L"UserMemory Address for Tag bit is not 17Bit\n");
			__debugbreak();
		}

		//메모리 풀 할당
		m_pMemoryPool = new CMemoryPool<Node>(size);

		//멤버 초기화
		Clear();

		m_size = size;

	};
	~LFStack()
	{
		delete m_pMemoryPool;
	};


	void Clear()
	{
		m_pTopNode = nullptr;
		m_size = 0;
		m_topCnt = 0;
	}

	void Push(T InputData, INT idx)
	{
		//메모리 로그 준비
		DWORD    curID = GetCurrentThreadId();
		Node*    newNode = m_pMemoryPool->Alloc();
		Node*    oldTopNext = nullptr;
		Node*    t;
		Node*    real;
		uint64_t retCnt = InterlockedIncrement(&m_topCnt);

		newNode->data = InputData;

		do {
			g_cntarray[idx].s_totalCnt++;

			//CAS 실패하면 newNode에 붙인 tag때기
			newNode = (Node*)(((uint64_t)newNode << 17) >> 17);


			t = m_pTopNode; 
			real = (Node*)((uint64_t)t & BIT_MASK);
			newNode->pNextNode = real;
			newNode = (Node*)((UINT64)newNode | (retCnt << 47));

			if (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newNode, (__int64)t) == (__int64)t)
			{
				g_cntarray[idx].s_succesCnt++;
				break;
			}
			else
				g_cntarray[idx].s_failCnt++;

		} while (1);

		InterlockedIncrement64((volatile LONG64*) & m_size);
	}

	//Data는 OutParameter임.
	bool Pop(T& Data, INT idx)
	{
		//메모리 로그 준비
		DWORD curID = GetCurrentThreadId();

		Node* t;
		Node* real;
		Node* newTopNode;

		uint64_t retCnt = InterlockedIncrement(&m_topCnt);


		do {
			g_cntarray[idx].s_totalCnt++;

			t = m_pTopNode; //기존 탑 노드 저장

			real = (Node*)((uint64_t)t & BIT_MASK);
			if (real == nullptr)
			{
				return false;
			}


			newTopNode = real->pNextNode;
			newTopNode = (Node*)((uint64_t)newTopNode | (retCnt << 47));

			if (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newTopNode, (__int64)t) == (__int64)t)
			{
				g_cntarray[idx].s_succesCnt++;
				break;
			}
			else
				g_cntarray[idx].s_failCnt++;

		} while (1);


		//탑 노드 제거
		Data = real->data;
		if (!(m_pMemoryPool->Free(real)))
			__debugbreak();


		InterlockedDecrement64((volatile LONG64*) & m_size);

		return true;
	}
	bool IsEmpty()
	{
		if (m_pTopNode == nullptr)
			return true;

		return false;
	}

	inline INT64 GetUseCnt() { return m_size; }

private:
	Node*                               m_pTopNode;
	uint64_t                            m_size;
	uint64_t                            m_topCnt;
	CMemoryPool<Node>*                  m_pMemoryPool;
};



