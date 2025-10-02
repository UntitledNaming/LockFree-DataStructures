#pragma once

#include <windows.h>
#include <unordered_map>
#include <string.h>
#include <strsafe.h>
#pragma warning(disable:4996)
#pragma comment(lib,"winmm.lib")

#ifdef PROFILE
#define PRO_BEGIN(TagName) ProfileBegin(TagName)
#define PRO_END(TagName) ProfileEnd(TagName)

#else
#define PRO_BEGIN(TagName)
#define PRO_END(TagName)

#endif

#define MAX_THREAD_COUNT 30
#define MAX_HEADER_SIZE 512
#define MAX_STRING_SIZE  320

class CProfilerManager
{
private:

	// �������Ϸ� ���ø� ������ ����ü
	struct st_PROFILE_DATA
	{
		uint16_t                 s_ErrorFlag;
		std::wstring             s_Name;
		LARGE_INTEGER            s_StartTick;
		uint64_t                 s_TotalTime;
		uint64_t                 s_Min[2];
		uint64_t                 s_Max[2];
		uint64_t                 s_CallTime;
	};

	typedef std::unordered_map<std::wstring, st_PROFILE_DATA>* PROFILE_MAP_PTR;

	struct st_TABLE_PTR
	{
		PROFILE_MAP_PTR s_TablePtr;
		DWORD           s_ThreadID;
	};
	


	struct st_TOTAL_DATA
	{
		BOOL            s_UseFlag;
		std::wstring    s_Name;
		DOUBLE          s_TotalAvgTime;
		DOUBLE          s_TotalMaxTime;
		DOUBLE          s_TotalMinTime;
		uint64_t        s_TotalCallTime;
		INT             s_Cnt; //�����ٶ� ���� ������Ű��
	};


private:
	DWORD                        m_TlsIndex;
	uint16_t                     m_ArrayIndex;
	SRWLOCK                      m_ArrayLock;
	st_TABLE_PTR                 m_SampleArray[MAX_THREAD_COUNT];
						
	std::unordered_map<std::wstring, st_TOTAL_DATA> m_TotalDataTable;


private:
	                             CProfilerManager()
	{

	}
	                             ~CProfilerManager()
	{

	}

public:
	/////////////////////////////////////////////////////////////////////////////
	// �������Ϸ��� ���� TlsIndex ���� �� ����ȭ ��ü �ʱ�ȭ
	// 
	// Parameters: ����
	// Return: TlsAlloc ����(true), ����(false)
	/////////////////////////////////////////////////////////////////////////////
	bool                         Init()
	{
		//TlsIndex ����
		m_TlsIndex = TlsAlloc();
		if (m_TlsIndex == TLS_OUT_OF_INDEXES)
		{
			wprintf(L"CProfilerManger_Init()_TlsAlloc Error : %d \n", GetLastError());
			return false;
		}

		//����ȭ ��ü �ʱ�ȭ
		InitializeSRWLock(&m_ArrayLock);

		m_ArrayIndex = 0;

		return true;
	}


	/////////////////////////////////////////////////////////////////////////////
	// ��ü ���ø� ������ �ڷᱸ���� Index�� ���ϰ� TlsIndex�� ����
	// 
	// Parameters: ����
	// Return: ��ü ���ø� ������ �迭�� ���� �����ϸ� ���� �ȵǸ� ����
	/////////////////////////////////////////////////////////////////////////////
	bool                         ProfileInit()
	{
		AcquireSRWLockExclusive(&m_ArrayLock);

		if (m_ArrayIndex >= MAX_THREAD_COUNT)
		{
			wprintf(L"ProfilerManager_ProfileInit()_Thread Count Max Error \n");
			ReleaseSRWLockExclusive(&m_ArrayLock);
			return false;
		}

		m_SampleArray[m_ArrayIndex].s_ThreadID = GetCurrentThreadId();
		m_SampleArray[m_ArrayIndex].s_TablePtr = new std::unordered_map<std::wstring, st_PROFILE_DATA>;

		if (!TlsSetValue(m_TlsIndex, m_SampleArray[m_ArrayIndex].s_TablePtr))
		{
			wprintf(L"ProfilerManager_ProfileInit()_TlsSetValue Error : %d \n", GetLastError());
			ReleaseSRWLockExclusive(&m_ArrayLock);
			return false;
		}

		m_ArrayIndex++;
		ReleaseSRWLockExclusive(&m_ArrayLock);

		return true;
	}


	/////////////////////////////////////////////////////////////////////////////
    // Profiling �� ����Ÿ�� Text ���Ϸ� ����Ѵ�.
    //
    // Parameters: (char *)��µ� ���� �̸�.
    // Return: ����.
    /////////////////////////////////////////////////////////////////////////////
	void ProfileDataOutText(WCHAR* szName)
	{
		errno_t err;
		FILE* fp;
		SYSTEMTIME    stNowTime;


		//���� ��¥�� �ð� ���
		WCHAR filename[MAX_PATH];
		GetLocalTime(&stNowTime);
		wsprintf(filename, L"Profiling [%s]_%d%02d%02d_%02d.%02d.%02d.txt", szName, stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);

		//=======================================================================================================
		// 
		//�� ������ TLS�� ����� ���ø� �ڷᱸ�� ��ȸ�ϸ鼭 Tagging ������ Copy
		// 
		//=======================================================================================================
		st_PROFILE_DATA* ptr[MAX_THREAD_COUNT];
		size_t           size;
		INT              j = 0;
		for (int i = 0; i < m_ArrayIndex; i++)
		{
			size = m_SampleArray[i].s_TablePtr->size();

			//�ؽ� ���̺� size ��ŭ st_PROFILE_DATA �迭 �����ؼ� �� �����͸� ptr[i]�� ����
			ptr[i] = new st_PROFILE_DATA[size];

			std::unordered_map<std::wstring, st_PROFILE_DATA>::iterator it = m_SampleArray[i].s_TablePtr->begin();
			j = 0;

			//�� �������� �ؽ� ���̺��� iterator�� ��ȸ�ϸ鼭 ����� value�� ptr[i][j]�� �ִ� st_PROFILE_DATA ���ҿ� Copy
			for (; it != m_SampleArray[i].s_TablePtr->end();)
			{
				ptr[i][j].s_CallTime = it->second.s_CallTime;
				ptr[i][j].s_TotalTime = it->second.s_TotalTime;
				ptr[i][j].s_Name = it->second.s_Name;

				for (int k = 0; k < 2; k++)
				{
					ptr[i][j].s_Max[k] = it->second.s_Max[k];
					ptr[i][j].s_Min[k] = it->second.s_Min[k];
				}

				j++;
				++it;
			}
		}

		//=======================================================================================================
		// 
		//�� ������ �� ���ø� ������ ��� �� ���, ���� ���
		// 
		//=======================================================================================================


		const WCHAR* pHeader[3];
		for (int i = 0; i < 3; i++)
		{
			if (i == 1)
			{
				pHeader[i] = L"                                     Name |                     Average  |                            Min  |                            Max  |       Call  |\n";
				continue;
			}
			pHeader[i] = L"-------------------------------------------------------------------------------------------------------\n";
		}

		//�� ������ �ؽ� ���̺� size �ջ�
		size_t totalsize = 0;
		size_t maxsize = 0;
		for (int i = 0; i < m_ArrayIndex; i++)
		{
			maxsize = max(maxsize, m_SampleArray[i].s_TablePtr->size());
			totalsize += m_SampleArray[i].s_TablePtr->size();
		}


		//Total �ջ��� ���� �迭 ���� �� Tag �ʱ�ȭ �۾�
		st_TOTAL_DATA* pTotal = new st_TOTAL_DATA[totalsize];
		for (int i = 0; i < totalsize; i++)
		{
			pTotal[i].s_UseFlag = 0;
			pTotal[i].s_Cnt = 0;
		}


		// ���� ������ ���Ͽ� ������ ���ۿ� Copy(����� �̶� ����)
		//�������Ϸ� ��� �� ������ ����
		//MAX_STRING_SIZE : ���Ͽ� ������ ������ ���ڿ� ����� 1�� �ִ� ũ��
		size_t reMain = (MAX_HEADER_SIZE + (MAX_STRING_SIZE) * (totalsize + maxsize + m_ArrayIndex))*sizeof(WCHAR);
		WCHAR* pDataBuffer = (WCHAR*)malloc(reMain);
		WCHAR* pEnd = nullptr;
		HRESULT ret;

		memset(pDataBuffer, NULL, reMain);

		// 1. ��� ����
		//3��° ���� : ���ڿ� Copy�� ���ڿ� �� NULL ����Ű�� ������
		//4��° ���� : Dest ���ۿ� ���ڿ� Copy�� ���� ũ��
		ret = StringCchPrintfExW(pDataBuffer, reMain, &pEnd, &reMain, 0, pHeader[0]);
		if (ret != S_OK)
		{
			__debugbreak();
		}

		for (int i = 1; i < 3; i++)
		{
			ret = StringCchPrintfExW(pEnd, reMain, &pEnd, &reMain, 0, pHeader[i]);
			if (ret != S_OK)
			{
				__debugbreak();
			}

		}



		// 2. ������ ��� �� ���ۿ� ����
		size_t tableSize = 0;
		DOUBLE avgTime;
		DOUBLE maxTime;
		DOUBLE minTime;
		DWORD  threadID;
		DWORD  realSize = 0;
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);

		//������ �� ���ø� ������ ���ۿ� Copy
 		for (int i = 0; i < m_ArrayIndex; i++)
		{
			//�ؽ� ���̺� �� ������ �迭 ��ȸ
			tableSize = m_SampleArray[i].s_TablePtr->size();
			threadID = m_SampleArray[i].s_ThreadID;

			for (int j = 0; j < tableSize;j++)
			{
				//��ü TotalTick���� Max, Min �� ���� call Ƚ���� ����
 				maxTime = (((ptr[i][j].s_Max[0] + ptr[i][j].s_Max[1]) / 2) * (double)1000000 / freq.QuadPart);
				minTime = (((ptr[i][j].s_Min[0] + ptr[i][j].s_Min[1]) / 2) * (double)1000000 / freq.QuadPart);
				avgTime = (((ptr[i][j].s_TotalTime - ptr[i][j].s_Max[0] - ptr[i][j].s_Max[1] - ptr[i][j].s_Min[0] - ptr[i][j].s_Min[1]) / (ptr[i][j].s_CallTime - 4)) * (double)1000000 / freq.QuadPart);



				ret = StringCchPrintfExW(pEnd, reMain, &pEnd, &reMain, 0, L"Thread ID : %d |         %s |          %4lf �� |          %4lf �� |           %4lf �� |      %ld | \n", threadID, ptr[i][j].s_Name, avgTime, minTime, maxTime, ptr[i][j].s_CallTime);
				if (ret != S_OK)
				{
					__debugbreak();
				}

				//Total�迭 ��ȸ �ϸ鼭 ���� Tag������ �����ֱ�
				int k;
				for (k = 0; k < totalsize; k++)
				{
					//���� Tag�� ������
					if (pTotal[k].s_Name == ptr[i][j].s_Name)
					{
						pTotal[k].s_TotalCallTime += ptr[i][j].s_CallTime;
						pTotal[k].s_TotalAvgTime += avgTime;
						pTotal[k].s_TotalMaxTime += maxTime;
						pTotal[k].s_TotalMinTime += minTime;
						pTotal[k].s_UseFlag = 1;
						pTotal[k].s_Cnt++;
						break;
					}

				}

				//���� ptr[i][j]�� ����� st_PROFILE_DATA�� Tag�� ������ Total�迭 �ݺ��� �� ���Ƶ� ���� Tag�� ��ã������ k�� totalsize���� ����.
				if (k == totalsize)
				{
					//�ٽ� Total �迭 ���鼭 �����ϴ� ���� ã��
					for (int l = 0; l < totalsize; l++)
					{
						//��� ���ϴ� �����̶�� 
						if (pTotal[l].s_UseFlag == 0)
						{
							pTotal[l].s_Name = ptr[i][j].s_Name;
							pTotal[l].s_TotalCallTime = ptr[i][j].s_CallTime;
							pTotal[l].s_TotalAvgTime = avgTime;
							pTotal[l].s_TotalMaxTime = maxTime;
							pTotal[l].s_TotalMinTime = minTime;
							pTotal[l].s_UseFlag = 1;
							pTotal[l].s_Cnt = 1;
							break;
						}
					}

				}

			}

			//�� ������ ���� ������ Copy������ ----- �����
			ret = StringCchPrintfExW(pEnd, reMain, &pEnd, &reMain, 0, L"-------------------------------------------------------------------------------------------------------\n");
			if (ret != S_OK)
			{
				__debugbreak();
			}
		}

		//Total �迭�� �ִ� �� ����ϱ�
		INT cnt;
 		for (int i = 0; i < totalsize; i++)
		{
			//�����ϴ� �迭�̸� Pass
			if (pTotal[i].s_UseFlag == 0)
				continue;

			cnt = pTotal[i].s_Cnt;

			ret = StringCchPrintfExW(pEnd, reMain, &pEnd, &reMain, 0, L"         Total Data   |         %s |          %4lf �� |          %4lf �� |           %4lf �� |      %ld | \n", pTotal[i].s_Name,pTotal[i].s_TotalAvgTime / cnt, pTotal[i].s_TotalMinTime / cnt, pTotal[i].s_TotalMaxTime / cnt, pTotal[i].s_TotalCallTime);
			if (ret != S_OK)
			{
				__debugbreak();
			}
		}



		// 3. ���� ��Ʈ�� ����
		err = _wfopen_s(&fp, filename, L"wb");
		if (err != 0)
		{
			wprintf(L"���� ���� ����. ���� �ڵ�: %d \n", err);
			__debugbreak();
			return;
		}


		//����
		fwrite(pDataBuffer, MAX_HEADER_SIZE + (MAX_STRING_SIZE) * (totalsize + maxsize + m_ArrayIndex), 1, fp);


		// 4. ��Ʈ�� �ݱ� �� �� ����
		fclose(fp);

		for (int i = 0; i < m_ArrayIndex; i++)
			delete[] ptr[i];

		delete[] pTotal;

 		free(pDataBuffer);
	}


	inline        uint16_t              GetTlsIndex()
	{
		return m_TlsIndex;
	}

	inline static CProfilerManager*     GetInstance()
	{
		static CProfilerManager Cpm;
		return &Cpm;
	}

};


class CProfiler 
{
private:
	// �������Ϸ� ���ø� ������ ����ü
	struct st_PROFILE_DATA
	{
		uint16_t       s_ErrorFlag;
		std::wstring   s_Name;
		LARGE_INTEGER  s_StartTick;
		uint64_t       s_TotalTime;
		uint64_t       s_Min[2];
		uint64_t       s_Max[2];
		uint64_t       s_CallTime;
	};

	typedef std::unordered_map<std::wstring, st_PROFILE_DATA>* PROFILE_MAP_PTR;

private:
	// ��� ����
	PCWCHAR            m_Tag;               //�����ڿ��� ���޵� Tag ���ڿ� ������
	st_PROFILE_DATA    m_SampleData;        //Begin, End���� ����� ���ø� ������ ���� ����
	DWORD              m_threadid;

public:
	CProfiler(const WCHAR* tag)
	{
		PRO_BEGIN(const_cast<WCHAR*>(tag));
		m_Tag = tag;
	}

	~CProfiler()
	{
		PRO_END(const_cast<WCHAR*>(m_Tag));
	}

	/////////////////////////////////////////////////////////////////////////////
    // �ϳ��� �Լ� Profiling ����, �� �Լ�.
    //
    // Parameters: (char *)Profiling�̸�.
    // Return: ����.                    
    /////////////////////////////////////////////////////////////////////////////
	void ProfileBegin(WCHAR* szName)
	{
		std::wstring str = szName;


		if (str.size() >= MAX_STRING_SIZE)
			__debugbreak();

		//������ ���ø� ������ �ڷᱸ���� ����
		PROFILE_MAP_PTR pTable;
		pTable = (PROFILE_MAP_PTR)TlsGetValue(CProfilerManager::GetInstance()->GetTlsIndex());

		std::unordered_map<std::wstring,st_PROFILE_DATA>::iterator it = pTable->find(str);


		// wstring str�� key�� �Ҷ� �ؽ� ���̺� �ش� key�� ���� ��� �ʱⰪ ���� �� ���̺� ����
		if (it == pTable->end())
		{
			m_SampleData.s_CallTime = 0;
			m_SampleData.s_TotalTime = 0;
			m_SampleData.s_ErrorFlag = 0;
			m_SampleData.s_Name = str;

			for (int i = 0; i < 2; i++)
			{
				m_SampleData.s_Max[i] = 0;
				m_SampleData.s_Min[i] = ULLONG_MAX;
			}

		}

		else
		{
			//�ؽ� ���̺� ������ �� ������ SampleData ����
			m_SampleData.s_CallTime = it->second.s_CallTime;
			m_SampleData.s_ErrorFlag = it->second.s_ErrorFlag;
			m_SampleData.s_TotalTime = it->second.s_TotalTime;
			m_SampleData.s_Name = it->second.s_Name;

			for (int i = 0; i < 2; i++)
			{
				m_SampleData.s_Max[i] = it->second.s_Max[i];
				m_SampleData.s_Min[i] = it->second.s_Min[i];
			}
		}


		//ErrorFlag�� 0�̰ų� End���� ������ �÷��� ���� 0x0010�� �ƴϸ� Error�� �߻���Ŵ
		if (m_SampleData.s_ErrorFlag != 0 && m_SampleData.s_ErrorFlag != 0x0010)
			throw szName;


		//���������� ���� �Ǿ����� Begin ����� ������ ErrorFlag ����
		m_SampleData.s_ErrorFlag = 0xFFFF;
		 
		m_threadid = GetCurrentThreadId();
		//�ð� ���� ����
		QueryPerformanceCounter(&m_SampleData.s_StartTick);
	}

	void ProfileEnd(WCHAR* szName)
	{
		//���� �ð� ����
		LARGE_INTEGER endTick;
		LARGE_INTEGER freq;
		uint64_t DiffTick;
		DWORD id;

		QueryPerformanceCounter(&endTick);
		QueryPerformanceFrequency(&freq);
		id = GetCurrentThreadId();
		if (m_threadid != id)
			__debugbreak();

		//�����ڿ��� ������ ��� ���� ���� �÷��׶� �ٸ��� ����
		if (m_SampleData.s_ErrorFlag != 0xFFFF)
			throw szName;

		//Total�� �ջ�
		DiffTick = (endTick.QuadPart - m_SampleData.s_StartTick.QuadPart);
		m_SampleData.s_TotalTime += DiffTick;

		//MAX�� ����
		if (m_SampleData.s_Max[0] < DiffTick)
		{
			uint64_t temp = m_SampleData.s_Max[0];
			m_SampleData.s_Max[0] = DiffTick;
			m_SampleData.s_Max[1] = temp;
		}

		//MIN�� ����
		if (m_SampleData.s_Min[0] > DiffTick)
		{
			uint64_t temp = m_SampleData.s_Min[0];
			m_SampleData.s_Min[0] = DiffTick;
			m_SampleData.s_Min[1] = temp;
		}

		//ȣ�ⷮ ����
		m_SampleData.s_CallTime++;

		//���� �÷��� ����
		m_SampleData.s_ErrorFlag = 0x0010;


		//��� ������ ����� �� ���� �� �ؽ� ���̺� ����
		PROFILE_MAP_PTR pTable;
		pTable = (PROFILE_MAP_PTR)TlsGetValue(CProfilerManager::GetInstance()->GetTlsIndex());
		
		std::unordered_map<std::wstring, st_PROFILE_DATA>::iterator it = pTable->find(m_SampleData.s_Name);

		//���ο� Tag�϶�
		if (it == pTable->end())
		{
			pTable->insert(std::pair<std::wstring, st_PROFILE_DATA>(m_SampleData.s_Name, m_SampleData));
			return;
		}

		//�̹� ������
		it->second = m_SampleData;
		return;
	}

	/////////////////////////////////////////////////////////////////////////////
    // �������ϸ� �� �����͸� ��� �ʱ�ȭ �Ѵ�.
    // 
    // Parameters: ����.
    // Return: ����.
    /////////////////////////////////////////////////////////////////////////////
	void ProfileReset()
	{

	}

};

