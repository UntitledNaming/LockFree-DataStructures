#pragma once
#include "LockFreeMemoryPoolLive.h"

template <typename T>
class Stack
{
public:
	struct Node
	{
		T        data;
		Node* pNextNode;
	};

private:
	Node* m_pTopNode;
	UINT64                    m_size;
	CMemoryPool<Node>* m_pMemoryPool;

public:
	Stack(int size = 0)
	{
		//메모리 풀 할당
		m_pMemoryPool = new CMemoryPool<Node>(size);
		m_pTopNode = nullptr;
		m_size = 0;

	};
	~Stack()
	{
		delete m_pMemoryPool;
	};

	Node* NodeAlloc(T data)
	{
		Node* newNode = m_pMemoryPool->Alloc();
		newNode->data = data;
		newNode->pNextNode = nullptr;

		return newNode;
	}

	void push(Node* newNode)
	{

		newNode->pNextNode = m_pTopNode;
		m_pTopNode = newNode;

		InterlockedIncrement64((volatile LONG64*)&m_size);

	}

	void Push(T data)
	{
		Node* newNode = m_pMemoryPool->Alloc();
		newNode->data = data;
		newNode->pNextNode = nullptr;

		newNode->pNextNode = m_pTopNode;
		m_pTopNode = newNode;

		InterlockedIncrement64((volatile LONG64*)&m_size);
	}

	bool pop(T& data)
	{
		Node* topNode = m_pTopNode;

		if (m_pTopNode == nullptr)
			return false;

		m_pTopNode = m_pTopNode->pNextNode;

		data = topNode->data;

		m_pMemoryPool->Free(topNode);

		InterlockedDecrement64((volatile LONG64*)&m_size);

		return true;
	}

};