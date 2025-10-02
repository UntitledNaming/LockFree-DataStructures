#pragma once
#pragma once
#include "LockFreeMemoryPoolLive.h"

#define MAX_LEN         300
#define USER_MEMORY_MAX 0x00007FFFFFFFFFFF
#define BITMASK         0x00007FFFFFFFFFFF
#define TAGMASK         0xFFFF800000000000


/*
*  특징 : 생성자에서 공용 락프리 풀 동적할당 함.
*         전역 락프리 객체 배열이 main 스레드 전 한 스레드가 객체 생성해주는데 이때 공용 노드 풀 동적
*/
template<typename T>
class LFQueueMul
{
public:
	struct Node
	{
		T     s_data;
		Node* s_next;
	};


private:
	Node*                               m_pHead;
	Node*                               m_pTail;
	LONG                                m_size;
	UINT64                              m_HeadCnt;


public:
	static CMemoryPool<Node>*           m_pMemoryPool;
	static LONG                         m_refCnt;

public:
	LFQueue(int size = 0)
	{
		//주소 bit 체크
		SYSTEM_INFO info;
		GetSystemInfo(&info);

		if (!((UINT64)info.lpMaximumApplicationAddress & USER_MEMORY_MAX))
		{
			__debugbreak();
		}

		// static 메모리 풀 생성 확인
		if (InterlockedIncrement(&m_refCnt) == 1)
			m_pMemoryPool = new CMemoryPool<Node>;
		else
		{
			while (m_pMemoryPool == nullptr)
			{

			}
		}






	}
};