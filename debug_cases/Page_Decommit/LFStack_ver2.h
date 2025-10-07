#pragma once
#define BIT_MASK        0x00007FFFFFFFFFFF

#include <windows.h>
////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Lock Free Stack ver 1. 초기 버전
////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LOG_BUFFER_SIZE 100000
#define MAX_LEN         300

template <typename T>
class LFStack
{
private:
	struct Node
	{
		T        s_data;
		Node*    s_pNextNode;
		//멤버 초기화

	};
	struct st_DEBUGMEMORY_LOG
	{
		int s_Type;                // Push냐 Pop이냐
		int s_LogicTime;           // 어떤 구간에서 로그 남겼는지
		int s_ThreadID;

		uint64_t s_LogIndex;
		Node* s_LocalTop;          // 스레드가 지역에 저장한 Top 멤버변수값
		Node* s_LocalNewTop;       // 스레드가 지역에 저장한 새롭게 Top이 될 노드 주소값
	};

public:
	~LFStack()
	{
	};

	LFStack()
	{
		Clear();
	};

	void FileLog()
	{
		errno_t err;
		FILE* fp;
		HANDLE  hThread;
		DWORD suspendCount;


		//파일 이름
		const WCHAR* fileName = L"LFSLog.txt";
		err = _wfopen_s(&fp, fileName, L"w+");
		if (err != 0)
		{
			wprintf(L"파일 오픈 실패 \n");
			return;
		}

		WCHAR Buffer[MAX_LEN];

		for (int i = 0; i < LOG_BUFFER_SIZE; i++)
		{
			if (m_LOG_BUFFER[i].s_Type == 0)
			{
				fwprintf(fp, L"[%llu] [Thread ID : %05d] [Push] [LogicTime : %d] [LocalTopAddr : 0x%08x] [LocalNewTopAddr : 0x%08x]\n", m_LOG_BUFFER[i].s_LogIndex, m_LOG_BUFFER[i].s_ThreadID, m_LOG_BUFFER[i].s_LogicTime
					, m_LOG_BUFFER[i].s_LocalTop, m_LOG_BUFFER[i].s_LocalNewTop);
			}
			else
			{
				fwprintf(fp, L"[%llu] [Thread ID : %05d] [Pop]   [LogicTime : %d] [LocalTopAddr : 0x%08x] [LocalNewTopAddr : 0x%08x]\n", m_LOG_BUFFER[i].s_LogIndex, m_LOG_BUFFER[i].s_ThreadID, m_LOG_BUFFER[i].s_LogicTime
					, m_LOG_BUFFER[i].s_LocalTop, m_LOG_BUFFER[i].s_LocalNewTop);
			}

		}


		fclose(fp);
		wprintf(L"Thread FileLog End... ThreadID :%d \n", GetCurrentThreadId());
	}

	void StackCapture(unsigned __int64 index, int type, int LogicTime, DWORD threadID, Node* localTop, Node* localNewTop)
	{
		//로그 저장
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_Type = type;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LogicTime = LogicTime;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_ThreadID = threadID;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LocalTop = localTop;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LocalNewTop = localNewTop;
		m_LOG_BUFFER[index % LOG_BUFFER_SIZE].s_LogIndex = index;

	}

	void Clear()
	{
		m_pTopNode = nullptr;
		m_topCnt = 0;
		m_size = 0;
	}

	void Push(T InputData)
	{
		Node* newNode = new Node;
		Node* t;
		Node* real;
		DWORD curID = GetCurrentThreadId();
		UINT64 retCnt = InterlockedIncrement(&m_topCnt);

		newNode->s_data = InputData;

		do {

			//CAS 실패하면 newNode에 붙인 tag때기
			newNode = (Node*)(((UINT64)newNode << 17) >> 17);

			t = m_pTopNode;
			real = (Node*)((UINT64)t & BIT_MASK);
			newNode->s_pNextNode = real;
			newNode = (Node*)((UINT64)newNode | (retCnt << 47));

			StackCapture(InterlockedIncrement64((__int64*)&m_LogIndex), 0, 0, curID, t, newNode);

		} while (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newNode, (__int64)t) != (__int64)t);

		InterlockedIncrement64((volatile LONG64*)&m_size);
		StackCapture(InterlockedIncrement64((__int64*)&m_LogIndex), 0, 1, curID, t, newNode);
	}

	//Data는 OutParameter임.
	bool Pop(T& Data)
	{
		//메모리 로그 준비
		Node* t = nullptr;
		Node* real = nullptr;
		Node* newTopNode = nullptr;
		DWORD curID = GetCurrentThreadId();
		UINT64 retCnt = InterlockedIncrement(&m_topCnt);

		__int64 ret;

		__try {
			do {
				t = m_pTopNode; //기존 탑 노드 저장

				real = (Node*)((UINT64)t & BIT_MASK);
				if (real == nullptr)
				{
					return false;
				}

				ret = InterlockedIncrement64((__int64*)&m_LogIndex);
				StackCapture(ret, 1, 3, curID, t, real);

				newTopNode = real->s_pNextNode;
				newTopNode = (Node*)((UINT64)newTopNode | (retCnt << 47));

				StackCapture(InterlockedIncrement64((__int64*)&m_LogIndex), 1, 0, curID, t, newTopNode);


			} while (_InterlockedCompareExchange64((volatile __int64*)&m_pTopNode, (__int64)newTopNode, (__int64)t) != (__int64)t);
		}

		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			FileLog();
			__debugbreak();
		}

		ret = InterlockedIncrement64((__int64*)&m_LogIndex);
		StackCapture(ret, 1, 1, curID, t, newTopNode);

		//탑 노드 제거
		Data = real->s_data;

		delete real;

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

	//로그 버퍼
	st_DEBUGMEMORY_LOG           m_LOG_BUFFER[LOG_BUFFER_SIZE];
	uint64_t                     m_LogIndex;
};

