#pragma once

template <typename T>
class LFStack;

template <typename T>
class Stack;

class ProcessMonitor;

struct INTERLOCKCNT;

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
	void SpinUnlock();


public:
	FLOAT                                       m_processtotalavg;
	FLOAT                                       m_processuseravg;
	FLOAT                                       m_processkernelavg;
	DOUBLE                                      m_processCSavg;
	INT                                         m_thCount;
	INT                                         m_testTime;
	INT                                         m_testType;
	UINT64*                                     m_count;
	UINT64*                                     m_failcount;
	UINT64*                                     m_usertime;
	SRWLOCK                                     m_SRWLock;
	std::thread*                                m_hThread;
	HANDLE                                      m_FileStore;
	DWORD*                                      m_hThreadID;
	BOOL                                        m_EndFlag;
	LFStack<int>*                               m_testLFStack;
	Stack<int>*                                 m_testStack;
	INTERLOCKCNT*                               m_cntarray;
	ProcessMonitor*                             m_pPDH;
	__declspec(align(64)) LONG                  m_spinLock;
};

