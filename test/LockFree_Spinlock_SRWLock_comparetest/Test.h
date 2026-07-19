#pragma once

template <typename T>
class LFStack;

template <typename T>
class Stack;

class ProcessMonitor;

struct INTERLOCKCNT;

struct PaddedCounter
{
	UINT64 count;
	char   pad[56];
};

class CTest
{
public:

	void TestThread(void* pArguments);
	void MonitorThread(void* pArguments);

	void ThreadCreate();
	void TestClear();
	void TestInit();
	void StackTest(int idx);
	void FileStore();
	void SpinLock(int idx);
	void SpinUnlock(int idx);


public:
	FLOAT                                       m_processtotalavg;
	FLOAT                                       m_processuseravg;
	FLOAT                                       m_processkernelavg;
	DOUBLE                                      m_processCSavg;
	INT                                         m_thCount;
	INT                                         m_testTime;
	INT                                         m_testType;
	SRWLOCK                                     m_SRWLock;
	std::thread*                                m_hThread;
	HANDLE                                      m_FileStore;
	DWORD*                                      m_hThreadID;
	LFStack<int>*                               m_testLFStack;
	Stack<int>*                                 m_testStack;
	PaddedCounter*                              m_count;                       // 총 테스트 시간에 따라서 Push, Pop 처리 횟수 카운팅 해서 성능 측정
	INTERLOCKCNT*                               m_cntarray;
	ProcessMonitor*                             m_pPDH;
	__declspec(align(64)) LONG                  m_spinLock;
};

