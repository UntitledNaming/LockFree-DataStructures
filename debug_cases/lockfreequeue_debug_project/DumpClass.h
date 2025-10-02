#pragma once
#pragma comment(lib,"DbgHelp.lib")
#include <windows.h>
#include <crtdbg.h>
#include <DbgHelp.h>
#include <iostream>
#include <psapi.h>


class CCrashDump
{
public:
	static long _DumpCount;
	static HANDLE m_HeapHandle;

	CCrashDump()
	{
		//자체 힙 생성
		m_HeapHandle = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, NULL, 0);

		_DumpCount = 0;

		_invalid_parameter_handler oldHandler;
		_invalid_parameter_handler newHandler;

		newHandler = myInvalidParameterHandler;
		oldHandler = _set_invalid_parameter_handler(newHandler); //crt함수에 null 포인터 넣었을 때 발생
		_CrtSetReportMode(_CRT_WARN, 0);                        //CRT 오류 메세지 표시 안하고 바로 덤프 남기게
		_CrtSetReportMode(_CRT_ASSERT, 0);                      //CRT 오류 메세지 표시 안하고 바로 덤프 남기게
		_CrtSetReportMode(_CRT_ERROR, 0);                       //CRT 오류 메세지 표시 안하고 바로 덤프 남기게

		_CrtSetReportHook(_custom_Report_hook);

		//Pure virtual function called 에러도 내가 정의한 함수로 우회하도록
		_set_purecall_handler(myPurecallHandler);

		SetHandlerDump();

	}

	static void Crash()
	{
		int* p = nullptr;
		*p = 0;
	}

	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		int iWorkingMemory = 0;
		SYSTEMTIME    stNowTime;


		long DumpCount = InterlockedIncrement(&_DumpCount);

		//현재 날짜와 시간 얻기
		WCHAR filename[MAX_PATH];

		GetLocalTime(&stNowTime);
		wsprintf(filename, L"Dump_%d%02d%02d_%02d.%02d.%02d_%d.dmp", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount);
		wprintf(L"\n\n\n!!! Crash Error !!!  %d.%d.%d / %d:%d:%d \n", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
		wprintf(L"Now Save Dump File... \n");

		HANDLE hDumpFile = ::CreateFile(filename,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);

		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptionInformation;

			MinidumpExceptionInformation.ThreadId = ::GetCurrentThreadId();
			MinidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
			MinidumpExceptionInformation.ClientPointers = TRUE;


			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory,
				&MinidumpExceptionInformation, NULL, NULL);

			CloseHandle(hDumpFile);

			wprintf(L"CrashDump Save Finish");
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}

	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);
	}

	//Invalid Parameter Handler
	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
	{
		Crash();
	}

	static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue)
	{
		Crash();
		return true;
	}

	static void myPurecallHandler()
	{
		Crash();
	}

};

HANDLE CCrashDump::m_HeapHandle;
long CCrashDump::_DumpCount = 0;