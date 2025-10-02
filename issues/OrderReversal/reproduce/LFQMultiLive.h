#pragma once
#include "LockFreeMemoryPoolLive.h"

#define LOG_BUFFER_SIZE 5000
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
		T                               _data;
		alignas(16)Node*                _next;
		UINT64                          _Qid;
	};					                
	
	struct CmpNode
	{
		Node*                           s_next;
		UINT64                          s_Qid;
	};

private:				                
	Node*                               m_pHead;
	Node*                               m_pTail;
	LONG                                m_size;
	UINT64                              m_HeadCnt;
	UINT64                              m_TailCnt;
	UINT64                              m_Qid;
	

public:
	static CMemoryPool<Node>*           m_pMemoryPool;
	static LONG                         m_refCnt;

public:
	
	LFQueueMul(int size = 0)
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

		//��� ���� �ʱ�ȭ
		m_size = size;
		m_HeadCnt = 0;
		m_TailCnt = 0;
		m_Qid = (UINT64) & m_size;


		//���� ��� 1�� ����
		Node* dmyNode =  m_pMemoryPool->Alloc();
		dmyNode->_next = nullptr;
		dmyNode->_Qid = m_Qid;

		m_pHead = (Node*)((UINT64)dmyNode | (InterlockedIncrement64((volatile __int64*)&m_HeadCnt) << 47));
		m_pTail = (Node*)((UINT64)dmyNode | (InterlockedIncrement64((volatile __int64*)&m_TailCnt) << 47));

	}
	~LFQueueMul()
	{
		if (InterlockedDecrement(&m_refCnt) == 0)
		{
			delete m_pMemoryPool;
			m_pMemoryPool = nullptr;
		}
	}

	void Clear()
	{
		m_size = 0;
		m_HeadCnt = 0;
		m_TailCnt = 0;
	}

	void Enqueue(T InputParam)
	{
		DWORD    curID = GetCurrentThreadId();
		Node* newNode;
		Node* localTail;
		Node* localTailNext;
		Node* realTail;
		UINT64   retCnt;
		UINT64   tailTag;

		//�ű� ��� ����
		newNode = m_pMemoryPool->Alloc();
		newNode->_data = InputParam;
		newNode->_next = (Node*)0xFFFFFFFFFFFFFFFF;
		newNode->_Qid = m_Qid;

		retCnt = InterlockedIncrement(&m_TailCnt);

		//�� ���
		CmpNode cmp;


		//���� �۾�
		while (1)
		{
			localTail = m_pTail;
			realTail = (Node*)((UINT64)localTail & BITMASK);
			tailTag = (UINT64)localTail & (TAGMASK);
			localTailNext = realTail->_next;

			if ((localTailNext == (Node*)0xFFFFFFFFFFFFFFFF) || (localTailNext == nullptr))
				break;

			localTailNext = (Node*)((UINT64)localTailNext | (tailTag));

			//next�� nullptr�� �ƴ϶�� tail�� �ٲ���.
			InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)localTailNext, (__int64)localTail);

		}

		//CAS �۾�
		while (1)
		{

			localTail = m_pTail;
			realTail = (Node*)((UINT64)localTail & BITMASK);
			localTailNext = realTail->_next;
			cmp.s_Qid = m_Qid;
			cmp.s_next = nullptr;

			//_tail->next ���������� ���� �õ�
			if (InterlockedCompareExchange128((__int64*) & realTail->_next, (__int64)m_Qid, (__int64)newNode, (long long*)&cmp))
			{

				newNode->_next = nullptr;
				newNode = (Node*)((UINT64)newNode | (retCnt << 47));


				//�����ϸ� tail�� ���������� ����
				InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);

				break;
			}
		}

		localTail = m_pTail;
		realTail = (Node*)((UINT64)localTail & BITMASK);

		//size ������ ��� ó�� ������
		InterlockedIncrement(&m_size);
	}

	bool Dequeue(T& OutputParam)
	{
		DWORD    curID = GetCurrentThreadId();
		Node* localHead = nullptr;
		Node* localHeadNext = nullptr;
		Node* realHead = nullptr;
		Node* realHeadNext = nullptr;
		Node* localTail;
		Node* localTailNext;
		Node* realTail;
		UINT64   retCnt;
		UINT64   tailTag;

		//size üũ�� ������ ������Ű�� ����
		if (InterlockedDecrement(&m_size) < 0)
		{
			InterlockedIncrement(&m_size);
			return false;
		}


		//���� �۾�
		while (1)
		{
			localTail = m_pTail;
			realTail = (Node*)((UINT64)localTail & BITMASK);
			tailTag = (UINT64)localTail & (TAGMASK);
			localTailNext = realTail->_next;


			if ((localTailNext == (Node*)0xFFFFFFFFFFFFFFFF) || (localTailNext == nullptr))
				break;

			localTailNext = (Node*)((UINT64)localTailNext | (tailTag));

			//next�� nullptr�� �ƴ϶�� tail�� �ٲ���.
			InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)localTailNext, (__int64)localTail);

		}

		retCnt = InterlockedIncrement(&m_HeadCnt);

		while (1)
		{
			localHead = m_pHead;
			realHead = (Node*)((UINT64)localHead & BITMASK);
			realHeadNext = realHead->_next;
			if (realHeadNext == nullptr)
				continue;


			localHeadNext = (Node*)((UINT64)realHeadNext | (retCnt << 47));

			if (InterlockedCompareExchange64((volatile __int64*)&m_pHead, (__int64)localHeadNext, (__int64)localHead) != (UINT64)localHead)
				continue;

			break;
		}


		//������ ��ȯ
		localHeadNext = (Node*)((UINT64)localHeadNext & BITMASK);

		OutputParam = localHeadNext->_data;

		//��� ����
		if (!m_pMemoryPool->Free(realHead))
			__debugbreak();

		return true;
	}

	inline int GetUseSize()
	{
		return m_size;
	}

};

template <typename T>
CMemoryPool<typename LFQueueMul<T>::Node>* LFQueueMul<T>::m_pMemoryPool = nullptr;

template <typename T>
LONG LFQueueMul<T>::m_refCnt = 0;