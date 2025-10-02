#pragma once
#pragma once
#include "LockFreeMemoryPoolLive.h"

#define MAX_LEN         300
#define USER_MEMORY_MAX 0x00007FFFFFFFFFFF
#define BITMASK         0x00007FFFFFFFFFFF
#define TAGMASK         0xFFFF800000000000


/*
*  Ư¡ : �����ڿ��� ���� ������ Ǯ �����Ҵ� ��.
*         ���� ������ ��ü �迭�� main ������ �� �� �����尡 ��ü �������ִµ� �̶� ���� ��� Ǯ ����
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
		//�ּ� bit üũ
		SYSTEM_INFO info;
		GetSystemInfo(&info);

		if (!((UINT64)info.lpMaximumApplicationAddress & USER_MEMORY_MAX))
		{
			__debugbreak();
		}

		// static �޸� Ǯ ���� Ȯ��
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