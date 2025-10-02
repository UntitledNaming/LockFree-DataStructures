#pragma once
#define BIT_MASK        0x00007FFFFFFFFFFF

#include <windows.h>
#include "CMemoryPoolByQueue.h"
////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Lock Free Stack ver 1. 초기 버전
////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class LFStack
{
private:
	struct Node
	{
		T        s_data;
		Node*    s_pNextNode;
	};

public:
	LFStack()
	{

		//멤버 초기화
		Clear();


	};
	~LFStack()
	{

	};


	void Clear()
	{
		m_pTopNode = nullptr;
		m_topCnt = 0;
		//메모리 풀 할당
		m_pMemoryPool = new CMemoryPoolQ<Node>;
	}

	void Push(T InputData)
	{
		Node* newNode = m_pMemoryPool->Alloc();
		Node* t;
		Node* real;

		UINT64 retCnt = InterlockedIncrement(&m_topCnt);

		newNode->s_data = InputData;

		do {

			//CAS 실패하면 newNode에 붙인 tag때기
			newNode = (Node*)(((UINT64)newNode << 17) >> 17);

			t = m_pTopNode;
			real = (Node*)((UINT64)t & BIT_MASK);
			newNode->s_pNextNode = real;
			newNode = (Node*)((UINT64)newNode | (retCnt << 47));

		} while (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newNode, (__int64)t) != (__int64)t);

		InterlockedIncrement64((volatile LONG64*)&m_size);
	}

	//Data는 OutParameter임.
	bool Pop(T& Data)
	{
		//메모리 로그 준비
		Node* t;
		Node* real;
		Node* newTopNode;

		UINT64 retCnt = InterlockedIncrement(&m_topCnt);

		do {
			t = m_pTopNode; //기존 탑 노드 저장

			real = (Node*)((UINT64)t & BIT_MASK);
			if (real == nullptr)
			{
				return false;
			}


			newTopNode = real->s_pNextNode;
			newTopNode = (Node*)((UINT64)newTopNode | (retCnt << 47));
			
		} while (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newTopNode, (__int64)t) != (__int64)t);


		//탑 노드 제거
		Data = real->s_data;

		m_pMemoryPool->Free(real);

		InterlockedDecrement64((volatile LONG64*)&m_size);

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
	Node*                             m_pTopNode;
	INT                               m_size;
	UINT64                            m_topCnt;
	CMemoryPoolQ<Node>*               m_pMemoryPool;
};

