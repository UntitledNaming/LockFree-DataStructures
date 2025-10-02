#ifndef  __MEMORY_POOL__
#define  __MEMORY_POOL__
#include <new.h>
#include <malloc.h>
#include <windows.h>

#define  UPCAPACITYSIZE 50
#define LOG_BUFFER_SIZE 1000

typedef bool     MYBOOL;

template <typename T>
class CMemoryPool
{
private:

	//////////////////////////////////////////////////////////////////////////
	// �޸� Ǯ���� ����� ��� ����ü
	// s_guard  : ��� �տ� �޸� ħ�� üũ�� ����, Free�Ҷ� üũ
	// s_data   : ��忡 ��ü ��ü�� ����
	// s_pNext  : ����� ���� �ּ�
	// s_poolID : Ÿ�Ժ� �޸� Ǯ ���� ID �ο��Ҷ� üũ�ϱ� ���� ����
	//////////////////////////////////////////////////////////////////////////
	struct Node
	{
		T        s_data;   
		Node*    s_pNext;  
		UINT64   s_poolD;
	};

private:
	struct st_DEBUGMEMORY_LOG
	{
		int      s_Type;                // Push�� Pop�̳�
		int      s_LogicTime;           // � �������� �α� �������
		int      s_ThreadID;

		uint64_t s_LogIndex;
		Node*    s_LocalTop;            // �����尡 ������ ������ Top ���������
		Node*    s_LocalNewTop;         // �����尡 ������ ������ ���Ӱ� Top�� �� ��� �ּҰ�
	};

private:
	Node*                               m_pTopNode;
	MYBOOL                              m_bPlacementNew;
	UINT                                m_iCapacity;
	UINT                                m_iUseCnt;
	UINT                                m_iTopCnt;
	UINT64                              m_iOriginID;
	uint64_t                            m_LogIndex;
	st_DEBUGMEMORY_LOG                  m_LOG_BUFFER[LOG_BUFFER_SIZE];
	//SRWLOCK                             m_FileLock;


public:
	//////////////////////////////////////////////////////////////////////////
    // ������, �ı���.
    //
    // Parameters:	(int) �ʱ� �� ����.
    //				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
    // Return:
    //////////////////////////////////////////////////////////////////////////

	CMemoryPool(int iBlockNum = 0, bool bPlacementNew = false) : m_bPlacementNew(bPlacementNew)
	{
		//�޸� Ǯ ID ����. 
		m_iOriginID = (UINT64)& m_pTopNode;

		//��� ���� �ʱ�ȭ
		m_pTopNode = nullptr;
		m_iUseCnt = 0;
		m_iTopCnt = 0;



		for (int i = 0; i < iBlockNum; i++)
		{
			PushBack();
		}

	}

	~CMemoryPool()
	{
		// Free���� �Ҹ��� ȣ�� �������� �Ҹ��� ȣ�����ְ� �޸� Ǯ ��� �����

		Node* tempTop;
		Node* newTop;

		while (m_pTopNode != nullptr)
		{
			newTop = m_pTopNode->s_pNext;

			if (m_bPlacementNew == false)
			{
				//�Ҹ��� ȣ��
				(m_pTopNode->s_data).~T();
			}

			free(m_pTopNode);

			m_pTopNode = newTop;
		}
	}

	void PushBack()
	{
		//��� ���� �� �ʱ�ȭ
		Node* newNode = (Node*)malloc(sizeof(Node));
		newNode->s_pNext = nullptr;
		newNode->s_poolD = m_iOriginID;

		//��ü ������ ȣ��
		new(&newNode->s_data) T;

		//���� TopNode�� ����
		newNode->s_pNext = m_pTopNode;

		m_pTopNode = newNode;

		m_iCapacity++;
	}
	void PushBack(int Num)
	{
		Node* t;
		Node* real;
		UINT64 retCnt;
		UINT64 bitmask = 0x00007FFFFFFFFFFF;
		uint64_t index;



		for (int i = 0; i < Num; i++)
		{
			//��� ���� �� �ʱ�ȭ
			Node* newNode = (Node*)malloc(sizeof(Node));
			newNode->s_poolD = m_iOriginID;

			//��ü ������ ȣ��
			new(&newNode->s_data) T;

			retCnt = InterlockedIncrement(&m_iTopCnt);

			do {
				//CAS �����ϸ� newNode�� ���� tag����
				newNode = (Node*)(((UINT64)newNode << 17) >> 17);

				t = m_pTopNode;
				real = (Node*)((UINT64)t & bitmask);
				newNode->s_pNext = real;

				newNode = (Node*)((UINT64)newNode | (retCnt << 47));

				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				StackCapture(index, 0, 0, GetCurrentThreadId(), t, newNode);
			} while (InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newNode, (__int64)t) != (__int64)t);

			index = InterlockedIncrement64((__int64*)&m_LogIndex);
			StackCapture(index, 0, 1, GetCurrentThreadId(), t, newNode);
		}

		m_iCapacity += Num;
		wprintf(L"MemoryPool Capacity Up : %d \n", m_iCapacity);
	}
	void StackCapture(unsigned __int64 index, int type, int LogicTime, DWORD threadID, Node* localTop, Node* localNewTop)
	{
		//�α� ����
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_Type = type;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LogicTime = LogicTime;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_ThreadID = threadID;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LocalTop = localTop;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LocalNewTop = localNewTop;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LogIndex = index;

	}


	//////////////////////////////////////////////////////////////////////////
    // ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
    //
    // Parameters: ����.
    // Return: (int) �޸� Ǯ ���� ��ü ����
    //////////////////////////////////////////////////////////////////////////
	int GetCapacityCnt()
	{
		return m_iCapacity;
	}

	//////////////////////////////////////////////////////////////////////////
    // ���� ������� �� ������ ��´�.
    //
    // Parameters: ����.
    // Return: (int) ������� �� ����.
    //////////////////////////////////////////////////////////////////////////
	int GetUseCnt()
	{
		return m_iUseCnt;
	}


	//////////////////////////////////////////////////////////////////////////
	// �� �ϳ��� �Ҵ�޴´�.  
	//
	// Parameters: ����.
	// Return: (DATA *) ����Ÿ �� ������.
	//////////////////////////////////////////////////////////////////////////
	T* Alloc()
	{
		DWORD curID = GetCurrentThreadId();
		UINT64 retCnt;
		UINT64 bitmask = 0x00007FFFFFFFFFFF;
		Node* t;
		Node* real;
		Node* newTopNode;
		UINT64 useCnt = m_iUseCnt;
		UINT64 Capacity = m_iCapacity;
		uint64_t index;

		if (useCnt == Capacity)
			PushBack(50);

		retCnt = InterlockedIncrement(&m_iTopCnt);

		do{
			t = m_pTopNode;

			real = (Node*)((UINT64)t & bitmask);
			if (real == nullptr)
				return nullptr;

			newTopNode = real->s_pNext;
			newTopNode = (Node*)((UINT64)newTopNode | (retCnt << 47));

			index = InterlockedIncrement64((__int64*)&m_LogIndex);
			StackCapture(index, 1, 0, GetCurrentThreadId(), t, newTopNode);
		}while(InterlockedCompareExchange64((volatile __int64*) & m_pTopNode, (__int64)newTopNode, (__int64)t) != (__int64)t);

		index = InterlockedIncrement64((__int64*)&m_LogIndex);
		StackCapture(index, 1, 1, GetCurrentThreadId(), t, newTopNode);


		//���� Top��� �޸� Ǯ�� �и������� �߰� �۾��ϴ��� �ٷ� ��ȯ
		if (m_bPlacementNew == true)
		{
			new(&(t->s_data)) T;
		}

		//������ ��� ������ �� ������ �̹� 1�� ȣ���ؼ� false�϶� ������ �ʿ� ����.

		InterlockedIncrement(&m_iUseCnt);
		
		return &(real->s_data);
	}


	//////////////////////////////////////////////////////////////////////////
	// ������̴� ���� �����Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool Free(T* pData)
	{
		//��ȯ���� ��ü �ּҸ� ���� ���� �޸� Ǯ ��� �ּ� ���ϱ�
		Node* newNode = (Node*)pData;

		//�޸� Ǯ�� ���̱� ���� �˻�

		//todo : ���� �ǵ������ �޸� ���� ������Ű�� �ִ� ���̴� �ٷ� Crash

		//�ٸ� �޸�Ǯ�� ��带 ��ȯ���� �� �׳� false return�ϱ�
		if (newNode->s_poolD != m_iOriginID)
			return false;

		//bPlacementNew üũ�ؼ� true �� �Ҹ��� ȣ�����ֱ�
		if (m_bPlacementNew == true)
			pData->~T();

		Node* t;
		Node* real;
		UINT64 retCnt = InterlockedIncrement(&m_iTopCnt);
		UINT64 bitmask = 0x00007FFFFFFFFFFF;
		uint64_t index;

		do {
			//CAS �����ϸ� newNode�� ���� tag����
			newNode = (Node*)(((UINT64)newNode << 17) >> 17);

			t = m_pTopNode;
			real = (Node*)((UINT64)t & bitmask);
			newNode->s_pNext = real;

			newNode = (Node*)((UINT64)newNode | (retCnt << 47));

			index = InterlockedIncrement64((__int64*)&m_LogIndex);
			StackCapture(index, 2, 0, GetCurrentThreadId(), t, newNode);
		} while (InterlockedCompareExchange64((volatile __int64*) & m_pTopNode, (__int64)newNode, (__int64)t) != (__int64)t);

		index = InterlockedIncrement64((__int64*)&m_LogIndex);
		StackCapture(index, 2, 1, GetCurrentThreadId(), t, newNode);

		InterlockedDecrement(&m_iUseCnt);

		return true;
	}



};

#endif
 