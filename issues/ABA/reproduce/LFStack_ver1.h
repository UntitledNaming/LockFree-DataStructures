#pragma once
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

		//메모리 풀 할당
		m_pMemoryPool = new CMemoryPoolQ<Node>;
	}

	void Push(T InputData)
	{
		Node* newNode = m_pMemoryPool->Alloc();
		Node* t;

		newNode->s_data = InputData;

		do {

			t = m_pTopNode;
			newNode->s_pNextNode = t;

		} while (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newNode, (__int64)t) != (__int64)t);

		InterlockedIncrement64((volatile LONG64*)&m_size);
	}

	//Data는 OutParameter임.
	bool Pop(T& Data)
	{
		//메모리 로그 준비
		Node* t;
		Node* newTopNode;

		do {
			t = m_pTopNode; //기존 탑 노드 저장

			if (t == nullptr)
			{
				return false;
			}


			newTopNode = t->s_pNextNode;
			
		} while (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newTopNode, (__int64)t) != (__int64)t);


		//탑 노드 제거
		Data = t->s_data;

		m_pMemoryPool->Free(t);

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
	CMemoryPoolQ<Node>*               m_pMemoryPool;
};

