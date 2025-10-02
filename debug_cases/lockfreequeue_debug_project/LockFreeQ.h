#pragma once
#include "LockFreeMemoryPool.h"

#define LOG_BUFFER_SIZE 5000
#define MAX_LEN         300
#define USER_MEMORY_MAX 0x00007FFFFFFFFFFF
#define BITMASK         0x00007FFFFFFFFFFF
#define TAGMASK         0xFFFF800000000000


template<typename T>
class LFQueue
{
private:
	struct Node
	{
		T     _data;
		Node* _next;
	};

private:
	struct st_DEBUGMEMORY_LOG
	{
		int      s_Type;           // Push�� Pop�̳�
		int      s_LogicTime;      // � �������� �α� �������
		int      s_ThreadID;

		uint64_t s_LogIndex;
		Node*    s_LocalNodePtr;   // �����尡 ������ ������ Head or Tail ��� �ּҰ�
		Node*    s_NextNodePtr;    // �����尡 ������ ������ ���Ӱ� Enqueue or Dequeue�� �ٲ� Head, Tail �ּҰ�
		Node*    s_realNodePtr;
		Node*    s_retNodePtr;
	};

public:
	LFQueue() : m_MemoryPool(2000)
	{
		//�ּ� bit üũ
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		wprintf(L" lpMinimumApplicationAddress : 0x%016llx \n", info.lpMinimumApplicationAddress);
		wprintf(L" lpMaximumApplicationAddress : 0x%016llx \n", info.lpMaximumApplicationAddress);

		if (!((UINT64)info.lpMaximumApplicationAddress & USER_MEMORY_MAX))
		{
			wprintf(L"UserMemory Address for Tag bit is not 17Bit\n");
			__debugbreak();
		}

		//��� ���� �ʱ�ȭ
		m_size     = 0;
		m_HeadCnt  = 0;
		m_TailCnt  = 0;
		m_LogIndex = 0;
		m_FileLogFlag = 0;
		m_CatchCnt = 0;
		InitializeSRWLock(&m_FileLock);


		//���� ��� 1�� ����
		uint64_t headTag;
		uint64_t tailTag;

		Node* dmyNode = m_MemoryPool.Alloc();
		dmyNode->_next = nullptr;

		headTag = InterlockedIncrement64((volatile __int64*) & m_HeadCnt);
		tailTag = InterlockedIncrement64((volatile __int64*)&m_TailCnt);


		m_pHead = (Node*)((UINT64)dmyNode | (headTag << 47));
		m_pTail = (Node*)((UINT64)dmyNode | (tailTag << 47));

		


	}
	~LFQueue() {};

	static void StoreHandle(DWORD threadID)
	{
		uint16_t index = InterlockedIncrement16((volatile short*)&m_HandleIndex);
		m_ThreadID[index] = threadID;
	}
	static bool StopThread()
	{
		HANDLE  hThread;
		DWORD suspendCount;
		
		InterlockedExchange64((volatile __int64 *) & m_FileLogFlag,1);

		for (int i = 0; i < m_HandleIndex; i++)
		{
			if (GetCurrentThreadId() == m_ThreadID[i])
				continue;

			hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, m_ThreadID[i]);
			if (hThread == NULL)
			{
				printf("OpenThread ����. ���� �ڵ�: %d\n", GetLastError());
				return false;
			}

			suspendCount = SuspendThread(hThread);
			if (suspendCount == (DWORD)-1)
			{
				printf("SuspendThread ����. ���� �ڵ�: %d\n", GetLastError());
				CloseHandle(hThread);
				return false;
			}
			wprintf(L"Suspend Thread ID: %d\n", m_ThreadID[i]);
		}

		return true;
	}

	void Capture(uint64_t index, int type, int LogicTime, DWORD threadID, Node* LocalNodePtr, Node* NextNodePtr, Node* readNodePtr, Node* retCAS)
	{
		//�α� ����
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_Type = type;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LogicTime = LogicTime;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_ThreadID = threadID;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LocalNodePtr = LocalNodePtr;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_NextNodePtr = NextNodePtr;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_realNodePtr = readNodePtr;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_retNodePtr = retCAS;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LogIndex = index;
	}
	void FileLog()
	{
		errno_t err;
		FILE* fp;

		AcquireSRWLockExclusive(&m_FileLock);
		wprintf(L"Thread FileLog Start...ThreadID :%d \n", GetCurrentThreadId());
		InterlockedExchange64((volatile __int64*)&m_FileLogFlag, 1);


		//���� �̸�
		const WCHAR* fileName = L"LFSLog.txt";
		err = _wfopen_s(&fp, fileName, L"w+");
		if (err != 0)
		{
			wprintf(L"���� ���� ���� \n");
			ReleaseSRWLockExclusive(&m_FileLock);
			return;
		}

		WCHAR Buffer[MAX_LEN];

		for (int i = 0; i < LOG_BUFFER_SIZE; i++)
		{
			if (m_LOG_BUFFER[i].s_Type == 0)
			{
				fwprintf(fp, L"[%llu] [Thread ID : %05d] [Enqueue] [LogicTime : %d] [LocalNodeAddr : 0x%016llx] [LocalNewNodeAddr : 0x%016llx] [RealNodeAddr : 0x%016llx] [RetCAS : 0x%016llx] \n", m_LOG_BUFFER[i].s_LogIndex, m_LOG_BUFFER[i].s_ThreadID, m_LOG_BUFFER[i].s_LogicTime
					, m_LOG_BUFFER[i].s_LocalNodePtr, m_LOG_BUFFER[i].s_NextNodePtr, m_LOG_BUFFER[i].s_realNodePtr, m_LOG_BUFFER[i].s_retNodePtr);
			}
			else
			{
				fwprintf(fp, L"[%llu] [Thread ID : %05d] [Dequeue] [LogicTime : %d] [LocalNodeAddr : 0x%016llx] [LocalNewNodeAddr : 0x%016llx] [RealNodeAddr : 0x%016llx] [RetCAS : 0x%016llx] \n", m_LOG_BUFFER[i].s_LogIndex, m_LOG_BUFFER[i].s_ThreadID, m_LOG_BUFFER[i].s_LogicTime
					, m_LOG_BUFFER[i].s_LocalNodePtr, m_LOG_BUFFER[i].s_NextNodePtr, m_LOG_BUFFER[i].s_realNodePtr, m_LOG_BUFFER[i].s_retNodePtr);
			}

		}


		fclose(fp);
		ReleaseSRWLockExclusive(&m_FileLock);
		wprintf(L"Thread FileLog End... ThreadID :%d \n", GetCurrentThreadId());
	}

	void Enqueue(T InputParam)
	{
		DWORD    curID = GetCurrentThreadId();
		Node*    newNode;
		Node*    localTail;
		Node*    localTailNext;
		Node*    realTail;
		Node*    retReal2ndCAS;
		uint64_t index;
		uint64_t retCnt;
		uint64_t ret2ndCAS;
		uint64_t retCAS;
		uint64_t tailTag;

		int loopCnt1 = 0;
		int loopCnt2 = 0;
		int loopCnt3 = 0;

		//�ű� ��� ����
		newNode = m_MemoryPool.Alloc();
		newNode->_data = InputParam;
		newNode->_next = (Node*)0xFFFFFFFFFFFFFFFF;

		retCnt = InterlockedIncrement(&m_TailCnt);


		while (1)
		{
			localTail = m_pTail;
			realTail = (Node*)((uint64_t)localTail & BITMASK);
			tailTag = (uint64_t)localTail & (TAGMASK);
			localTailNext = realTail->_next;
			
			if (loopCnt1 < 500)
			{
				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 3, curID, localTail, localTailNext, realTail, (Node*)tailTag);
			}
			loopCnt1++;


			if ((localTailNext == nullptr) || localTailNext == (Node*)0xFFFFFFFFFFFFFFFF)
				break;

			localTailNext = (Node*)((UINT64)localTailNext | (tailTag));

			//next�� nullptr�� �ƴ϶�� tail�� �ٲ���.
			retCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)localTailNext, (__int64)localTail);

			if (loopCnt3 < 500)
			{
				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 4, curID, localTail, localTailNext, realTail, (Node*)retCAS);
			}
			loopCnt3++;
		}


		while (1)
		{

			localTail = m_pTail;
			realTail = (Node*)((uint64_t)localTail & BITMASK);
			localTailNext = realTail->_next;

			if (loopCnt2 < 500)
			{
				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 0, curID, localTail, localTailNext, realTail, (Node*)nullptr);
			}
			loopCnt2++;

			//_tail->next ���������� ���� �õ�
			if (InterlockedCompareExchange64((__int64*)&realTail->_next, (__int64)newNode, (__int64)nullptr) == (__int64)nullptr)
			{
				newNode->_next = nullptr;
				newNode = (Node*)((UINT64)newNode | (retCnt << 47));

				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 1, curID, (Node*)localTail, newNode, localTailNext, (Node*)nullptr);

#pragma region method

#ifdef method2

				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 1, curID, localTail, newNode, realTail);


				//�����ϸ� tail�� ���������� ����
				ret2ndCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);

				//tail��� �簻��
				refreshTail = m_pTail;
				RealrefreshTail = (Node*)((uint64_t)refreshTail & BITMASK);
				if (RealrefreshTail->_next == nullptr)
					break;

				//2nd CAS ����
				if (ret2ndCAS != (uint64_t)refreshTail)
				{


					retRefreshTail = m_pTail;
					retRealRefreshTail = (Node*)((uint64_t)retRefreshTail & BITMASK);

					//local�� refresh�� tag�� �ٸ� ���(��, bitmask ���Ž� ��� �ּ� ���� ���)
					if (retRealRefreshTail == realTail)
					{
						InterlockedExchange64((volatile __int64*)&m_pTail, (__int64)newNode);
					}

				}
#endif

#ifdef method3
				newNode = (Node*)((UINT64)newNode | (retCnt << 47));

				do
				{
					refreshTail = m_pTail;
					RealrefreshTail = (Node*)((uint64_t)refreshTail & BITMASK);
					if (RealrefreshTail->_next == nullptr)
					{
						index = InterlockedIncrement64((__int64*)&m_LogIndex);
						Capture(index, 0, , curID, refreshTail, newNode, RealrefreshTail);
						break;
					}

					index = InterlockedIncrement64((__int64*)&m_LogIndex);
					Capture(index, 0, 1, curID, refreshTail, newNode, RealrefreshTail);

					if (m_FileLogFlag == 1)
						__debugbreak();

				} while (InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)refreshTail));


#endif

#ifdef method4
				if (newNode->_next == nullptr)
				{
					newNode = (Node*)((UINT64)newNode | (retCnt << 47));
					ret2ndCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);

					index = InterlockedIncrement64((__int64*)&m_LogIndex);
					Capture(index, 0, 2, curID, ret2ndCAS, newNode, localTail);
				}

				

#endif

#ifdef method5

						localTailNext = localTempTail->_next;
						if (localTailNext == nullptr)
						{
							newNode = localTempTail;
							break;
						}

						//�ǳ� ��尡 �ƴϸ� ����
						localTempTail = localTailNext;
					}

					//2ndCAS ����
					newNode = (Node*)((UINT64)newNode | (retCnt << 47));
					ret2ndCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);
					if ((Node*)ret2ndCAS != localTail)
						localTail = (Node*)ret2ndCAS;
					else
						break;


				}


#endif

#ifdef method6
				//�����ϸ� tail�� ���������� ����
				ret2ndCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);
				if (ret2ndCAS != (uint64_t)localTail)
				{
					//���߿� 1st CAS�� �����尡 tail�� �ᱹ ���ٲٴ°� ������ tail->next�� nullptr�� �� ������
					//���⼭ �������ִ� ����.
					while (1)
					{
						//tag����
						retReal2ndCAS = (Node*)((uint64_t)ret2ndCAS & BITMASK);

						//next ���ϱ�
						retNextNode = retReal2ndCAS->_next;

						//nullptr�̸� Ż��
						if (retNextNode == nullptr)
						{
							m_pTail = (Node*)((UINT64)retReal2ndCAS | (retCnt << 47));
							break;
						}

						//�ƴϸ� ���� next ��� �ּҸ� ret2ndCAS�� �����
						ret2ndCAS = (uint64_t)retNextNode;

					}
				}


#endif
#pragma endregion
				//�����ϸ� tail�� ���������� ����
				ret2ndCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)newNode, (__int64)localTail);
				if (ret2ndCAS != (uint64_t)localTail)
				{
					index = InterlockedIncrement64((__int64*)&m_LogIndex);
					Capture(index, 0, 5, curID, (Node*)localTail, newNode, realTail, (Node*)ret2ndCAS);
				}


				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 2, curID, (Node*)localTail, newNode, realTail, (Node*)ret2ndCAS);


				break;
			}

		}

		if (false)
		{
			__debugbreak();
			FileLog();
			__debugbreak();
		}

		//size ������ ��� ó�� ������
		InterlockedIncrement(&m_size);

		while (1)
		{
			localTail = m_pTail;
			realTail = (Node*)((uint64_t)localTail & BITMASK);
			tailTag = (uint64_t)localTail & (TAGMASK);
			localTailNext = realTail->_next;

			if (loopCnt1 < 500)
			{
				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 6, curID, localTail, localTailNext, realTail, (Node*)tailTag);
			}
			loopCnt1++;


			if ((localTailNext == nullptr) || localTailNext == (Node*)0xFFFFFFFFFFFFFFFF)
				break;

			localTailNext = (Node*)((UINT64)localTailNext | (tailTag));

			//next�� nullptr�� �ƴ϶�� tail�� �ٲ���.
			retCAS = InterlockedCompareExchange64((__int64*)&m_pTail, (__int64)localTailNext, (__int64)localTail);

			if (loopCnt3 < 500)
			{
				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 0, 7, curID, localTail, localTailNext, realTail, (Node*)retCAS);
			}
			loopCnt3++;
		}

	}

	bool Dequeue(T& OutputParam)
	{
		DWORD    curID = GetCurrentThreadId();
		Node*    localHead = nullptr;
		Node*    localHeadNext = nullptr;
		Node*    realHead = nullptr;
		Node*    realHeadNext = nullptr;
		Node*    localTail;
		Node*    localTailNext;
		Node*    realTail;
		Node*    retReal2ndCAS;
		uint64_t index;
		uint64_t retCnt;
		uint64_t retCAS;
		uint64_t retDeqCAS;
		uint64_t tailTag;
		uint64_t retSize;

		int loopCnt = 0;

		retCnt = InterlockedIncrement(&m_TailCnt);

		while (1)
		{
			localHead = m_pHead;
			realHead = (Node*)((uint64_t)localHead & BITMASK);
			realHeadNext = realHead->_next;
			if (realHeadNext == nullptr)
			{
				continue;
			}


			localHeadNext = (Node*)((uint64_t)realHeadNext | (retCnt << 47));

			if (loopCnt < 500)
			{
				index = InterlockedIncrement64((__int64*)&m_LogIndex);
				Capture(index, 1, 0, curID, localHead, localHeadNext, realHead, nullptr);
			}

			loopCnt++;

			retDeqCAS = InterlockedCompareExchange64((volatile __int64*)&m_pHead, (__int64)localHeadNext, (__int64)localHead);
			if (retDeqCAS != (uint64_t)localHead)
				continue;

			break;
		}

		if (localHeadNext == nullptr)
		{
			__debugbreak();
			FileLog();
			__debugbreak();
		}

		if (localHead == nullptr)
		{
			__debugbreak();
			FileLog();
			__debugbreak();
		}

		index = InterlockedIncrement64((__int64*)&m_LogIndex);
		Capture(index, 1, 1, curID, localHead, localHeadNext, realHead, nullptr);

		//������ ��ȯ
		localHeadNext = (Node*)((uint64_t)localHeadNext & BITMASK);

		OutputParam = localHeadNext->_data;
		InterlockedDecrement64((__int64*) & m_size);
		//��� ����
		if (!m_MemoryPool.Free(realHead))
			__debugbreak();


		return true;
	}



	LONG GetUseSize()
	{
		return m_size;
	}

private:
	Node*                 m_pHead;
	Node*                 m_pTail;
    LONG                  m_size;
	uint64_t              m_HeadCnt;
	uint64_t              m_TailCnt;
	uint64_t              m_CatchCnt;
	

	SRWLOCK               m_FileLock;
	CMemoryPool<Node>     m_MemoryPool;

private:
	//������ ����
	uint64_t              m_LogIndex;
	st_DEBUGMEMORY_LOG    m_LOG_BUFFER[LOG_BUFFER_SIZE];
	

private:
	//���� ���� ������
	static uint16_t       m_HandleIndex;
	static uint64_t       m_FileLogFlag;
	static DWORD          m_ThreadID[MAX_LEN];

};

template<typename T>
uint16_t LFQueue<T>::m_HandleIndex = -1;

template<typename T>
uint64_t LFQueue<T>::m_FileLogFlag = 0;

template<typename T>
DWORD LFQueue<T>::m_ThreadID[MAX_LEN];