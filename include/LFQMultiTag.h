#pragma once
#include "LockFreeMemoryPoolLive.h"

#define LOG_BUFFER_SIZE 5000
#define MAX_LEN         300
#define USER_MEMORY_MAX 0x00007FFFFFFFFFFF
#define BITMASK         0x00007FFFFFFFFFFF
#define TAGMASK         0xFFFF800000000000


template<typename T>
class LFQueueMul
{
private:
	struct Node
	{
		T     _data;
		Node* _next;
	};

private:
	Node* m_pHead;
	Node* m_pTail;
	LONG                          m_size;
	UINT64                        m_HeadCnt;
	UINT64                        m_TailCnt;

	static CMemoryPool<Node>*     m_pMemoryPool;
	static UINT64                 m_NextCnt;
	static LONG                   m_refCnt;

public:
	LFQueueMul()
	{
		//주소 bit 체크
		SYSTEM_INFO info;
		GetSystemInfo(&info);

		if (!((UINT64)info.lpMaximumApplicationAddress & USER_MEMORY_MAX))
		{
			wprintf(L"UserMemory Address for Tag bit is not 17Bit\n");
			__debugbreak();
		}

		//멤버 변수 초기화
		m_size = 0;
		m_HeadCnt = 0;
		m_TailCnt = 0;
		m_NextCnt = 0;

		// static 메모리 풀 생성 확인
		if (InterlockedIncrement(&m_refCnt) == 1)
			m_pMemoryPool = new CMemoryPool<Node>(0);
		else
		{
			while (m_pMemoryPool == nullptr)
			{

			}
		}


		//더미 노드 1개 생성
		Node* dmyNode = m_pMemoryPool->Alloc();
		dmyNode->_next = nullptr;

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
		T temp;
		while (Dequeue(temp))
		{

		}

		m_size = 0;
	}

	void Enqueue(T InputParam)
	{
		Node*    newNode;
		Node*    localTail;
		Node*    localTailNext;
		Node*    localRealTail;
		Node*    localRealTailNext;
		Node*    cmpTailNext;
		UINT64   retCnt;
		UINT64   retNextCnt;

		retCnt = InterlockedIncrement64((long long*)&m_TailCnt);
		retNextCnt = InterlockedIncrement64((long long*)&m_NextCnt);

		//신규 노드 생성
		newNode = m_pMemoryPool->Alloc();
		newNode->_data = InputParam;

		// 이제 Enq할 노드의 next는 enq할때 nullptr로 미는게 아니라 최상위에 tag값을 넣어줘야함.
		newNode->_next = (Node*)((UINT64)nullptr | (retNextCnt << 47));;

		//사전 작업
		while (1)
		{
			localTail = m_pTail;
			localRealTail = (Node*)((UINT64)localTail & BITMASK);
			localTailNext = localRealTail->_next;

			localRealTailNext = (Node*)((UINT64)localTailNext & BITMASK);

			if (localRealTailNext == nullptr)
				break;

			localTailNext = (Node*)((UINT64)localRealTailNext | (retCnt << 47));

			//next가 nullptr이 아니라면 tail을 바꾸자.
			if (InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)localTailNext, (__int64)localTail) == (__int64)localTail)
			{
				retCnt = InterlockedIncrement64((long long*)&m_TailCnt);
			}

		}

		//CAS 작업
		while (1)
		{
			localTail = m_pTail;
			localRealTail = (Node*)((UINT64)localTail & BITMASK);
			localTailNext = localRealTail->_next;

			// 하위 47bit에 있는 실제 노드 주소값 지우고 next 변수의 tag값만 보기
			cmpTailNext = (Node*)(((UINT64)localTailNext & TAGMASK));

			//_tail->next 원자적으로 변경 시도
			if (InterlockedCompareExchange64((__int64*)&localRealTail->_next, (__int64)newNode, (__int64)cmpTailNext) == (__int64)cmpTailNext)
			{
				newNode = (Node*)((UINT64)newNode | (retCnt << 47));

				//성공하면 tail도 원자적으로 변경
				InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);
				break;
			}
		}


		//size 증가는 모든 처리 끝나고
		InterlockedIncrement(&m_size);
	}


	bool Dequeue(T& OutputParam)
	{
		Node* localHead = nullptr;
		Node* localHeadNext = nullptr;
		Node* realHead = nullptr;
		Node* realHeadNext = nullptr;
		Node* localTail;
		Node* localRealTail;
		Node* localTailNext;
		Node* localRealTailNext;
		UINT64   retCntHead;
		UINT64   retCntTail;
		T        temp;

		retCntTail = InterlockedIncrement64((long long*)&m_TailCnt);

		//사전 작업
		while (1)
		{
			localTail = m_pTail;
			localRealTail = (Node*)((UINT64)localTail & BITMASK);
			localTailNext = localRealTail->_next;
			localRealTailNext = (Node*)((UINT64)localTailNext & BITMASK);

			if (localRealTailNext == nullptr)
				break;

			localTailNext = (Node*)((UINT64)localRealTailNext | (retCntTail << 47));

			//next가 nullptr이 아니라면 tail을 바꾸자.
			if (InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)localTailNext, (__int64)localTail) == (__int64)localTail)
			{
				retCntTail = InterlockedIncrement64((long long*)&m_TailCnt);
			}

		}

		retCntHead = InterlockedIncrement64((long long*)&m_HeadCnt);

		while (1)
		{
			localHead = m_pHead;
			realHead = (Node*)((UINT64)localHead & BITMASK);
			localHeadNext = realHead->_next;
			realHeadNext = (Node*)((UINT64)localHeadNext & BITMASK);

			if (localHead != m_pHead)
				continue;

			if (realHeadNext == nullptr)
				return false;


			localHeadNext = (Node*)((UINT64)realHeadNext | (retCntHead << 47));
			temp = realHeadNext->_data;

			if (InterlockedCompareExchange64((volatile __int64*)&m_pHead, (__int64)localHeadNext, (__int64)localHead) != (UINT64)localHead)
				continue;

			break;
		}


		//데이터 반환
		OutputParam = temp;

		//노드 제거
		if (!m_pMemoryPool->Free(realHead))
			__debugbreak();

		InterlockedDecrement(&m_size);

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

template <typename T>
UINT64 LFQueueMul<T>::m_NextCnt = 0;