// https://www.codeproject.com/Articles/499465/Simple-Windows-Service-in-Cplusplus

#include "stdafx.h"
#include "Utils.h"

#include <lazy_importer.hpp>
#include <xorstr.hpp>

#define SERVICE_NAME xorstr(L"\\??\\NoMercySvc").crypt_get()

typedef void(__cdecl* TServiceMessageFunction)(int iMessageID);
typedef bool(__cdecl* TInitializeServiceFunction)(bool bProtected);

static std::string		gs_szModuleName			= xorstr("NoMercy.dll").crypt_get();
static std::string		gs_szLogFile			= xorstr("NoMercyService.log").crypt_get();
static std::string		gs_szLogFileWithPath	= NNoMercyUtils::ExePath() + xorstr("\\").crypt_get() + gs_szModuleName;
static auto				gs_pServiceMsgFunction	= static_cast<TServiceMessageFunction>(nullptr);

SERVICE_STATUS			g_ServiceStatus		= { 0 };
SERVICE_STATUS_HANDLE	g_StatusHandle		= NULL;
HANDLE					g_ServiceStopEvent	= INVALID_HANDLE_VALUE;
HANDLE					g_hServiceThread	= INVALID_HANDLE_VALUE;

// -------------------

bool InitNoMercy(bool bProtected)
{
	LI_FIND(CreateDirectoryA)(xorstr("NoMercy").crypt_get(), NULL);

	if (NNoMercyUtils::IsFileExist(gs_szLogFileWithPath.c_str()) == false)
	{
		NNoMercyUtils::FileLogf(gs_szLogFile, xorstr("Error! log file(%s) not found!").crypt_get(), gs_szLogFileWithPath.c_str());
		NNoMercyUtils::WriteErrorLogEntry(xorstr("NoMercy log file not found").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLog(gs_szLogFileWithPath, xorstr("Initilization started!").crypt_get());

	auto hModule = LI_FIND(LoadLibraryA)(gs_szModuleName.c_str());
	if (!hModule)
	{
		NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("Error! DLL file can not load! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("DLL file succesfully loaded!").crypt_get());

	gs_pServiceMsgFunction = reinterpret_cast<TServiceMessageFunction>(LI_FIND(GetProcAddress)(hModule, xorstr("ServiceMessage").crypt_get()));
	if (!gs_pServiceMsgFunction)
	{
		NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("Error! Service message function not found! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLog(gs_szLogFileWithPath, xorstr("Service message function found!").crypt_get());

	auto InitializeFunction = reinterpret_cast<TInitializeServiceFunction>(LI_FIND(GetProcAddress)(hModule, xorstr("InitializeService").crypt_get()));
	if (!InitializeFunction)
	{
		NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("Error! Initialize function not found! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLog(gs_szLogFileWithPath, xorstr("Initialize function found!").crypt_get());

	if (!InitializeFunction(bProtected))
	{
		NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("Error! Initilization call fail! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLog(gs_szLogFileWithPath, xorstr("Initializion completed!").crypt_get());
	return true;
}

VOID WINAPI ServiceHandler(DWORD dwCtrlCode)
{
#ifdef _DEBUG
	NNoMercyUtils::DebugLogf("Service control code: %u handled!", dwCtrlCode);
#endif
	NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("Service control code: %u handled!").crypt_get(), dwCtrlCode);

	if (gs_pServiceMsgFunction)
		gs_pServiceMsgFunction(dwCtrlCode);

	switch (dwCtrlCode)
	{
		// TODO: implement other handlers
		/*
		case SERVICE_CONTROL_PAUSE:
		case SERVICE_CONTROL_CONTINUE:
		case SERVICE_CONTROL_SHUTDOWN:
		*/

		case SERVICE_CONTROL_STOP:
		{
			if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
				break;

			g_ServiceStatus.dwControlsAccepted	= 0;
			g_ServiceStatus.dwCurrentState		= SERVICE_STOP_PENDING;
			g_ServiceStatus.dwWin32ExitCode		= 0;
			g_ServiceStatus.dwCheckPoint		= 4;

			if (LI_FIND(SetServiceStatus)(g_StatusHandle, &g_ServiceStatus) == FALSE)
			{
#ifdef _DEBUG
				NNoMercyUtils::DebugLogf(xorstr("SetServiceStatus(4) fail! Error: %u\n").crypt_get(), LI_FIND(GetLastError)());
#endif
				NNoMercyUtils::WriteErrorLogEntry(xorstr("SetServiceStatus(4)").crypt_get(), LI_FIND(GetLastError)());
			}
			LI_FIND(SetEvent)(g_ServiceStopEvent);
			LI_FIND(CloseHandle)(g_ServiceStopEvent);

			if (g_hServiceThread)
			{
				LI_FIND(TerminateThread)(g_hServiceThread, dwCtrlCode);
				LI_FIND(CloseHandle)(g_hServiceThread);
			}

		} break;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}
}

DWORD WINAPI ServiceThread(LPVOID lpParam)
{
	if (InitNoMercy(true) == false)
	{
#ifdef _DEBUG
		NNoMercyUtils::DebugLog(xorstr("NoMercy core can not loaded!\n").crypt_get());
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("InitNoMercy").crypt_get(), LI_FIND(GetLastError)());
		return ERROR_INVALID_FUNCTION;
	}
	
	while (LI_FIND(WaitForSingleObject)(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
//		NNoMercyUtils::DebugLog("NoMercy Service still works!\n");
		LI_FIND(Sleep)(100);
	}

#ifdef _DEBUG
	NNoMercyUtils::DebugLog("NoMercy Service will stop!\n");
#endif
	return ERROR_SUCCESS;
}


VOID WINAPI ServiceMain(DWORD wNumServicesArgs, LPWSTR * lpServiceArgVectors)
{
#ifdef _DEBUG
	NNoMercyUtils::DebugLogf("NoMercy service main routine has been started!\n");
#endif

	g_StatusHandle = LI_FIND(RegisterServiceCtrlHandlerW)(SERVICE_NAME, ServiceHandler);
	if (g_StatusHandle == NULL)
	{
		auto dwErrorCode = LI_FIND(GetLastError)();

#ifdef _DEBUG
		NNoMercyUtils::DebugLogf("RegisterServiceCtrlHandlerA fail! Error: %u\n", dwErrorCode);
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("RegisterServiceCtrlHandlerA").crypt_get(), dwErrorCode);
		return;
	}

	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType				= SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted			= 0;
	g_ServiceStatus.dwCurrentState				= SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode				= NO_ERROR;
	g_ServiceStatus.dwServiceSpecificExitCode	= 0;
	g_ServiceStatus.dwCheckPoint				= 0;
	if (LI_FIND(SetServiceStatus)(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
#ifdef _DEBUG
		NNoMercyUtils::DebugLogf("SetServiceStatus(0) fail! Error: %u\n", LI_FIND(GetLastError)());
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("SetServiceStatus(0)").crypt_get(), LI_FIND(GetLastError)());
		return;
	}

	g_ServiceStopEvent = LI_FIND(CreateEventA)(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (LI_FIND(SetServiceStatus)(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
#ifdef _DEBUG
			NNoMercyUtils::DebugLogf("SetServiceStatus(1) fail! Error: %u\n", LI_FIND(GetLastError)());
#endif
			NNoMercyUtils::WriteErrorLogEntry(xorstr("SetServiceStatus(1)").crypt_get(), LI_FIND(GetLastError)());
		}
		return;
	}

	g_ServiceStatus.dwControlsAccepted	= SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState		= SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode		= 0;
	g_ServiceStatus.dwCheckPoint		= 2; // 0

	if (LI_FIND(SetServiceStatus)(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
#ifdef _DEBUG
		NNoMercyUtils::DebugLogf("SetServiceStatus(2) fail! Error: %u\n", LI_FIND(GetLastError)());
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("SetServiceStatus(2)").crypt_get(), LI_FIND(GetLastError)());
	}

	g_hServiceThread = LI_FIND(CreateThread)(NULL, 0, ServiceThread, lpServiceArgVectors[wNumServicesArgs - 1], 0, NULL);
	LI_FIND(WaitForSingleObject)(g_hServiceThread, INFINITE);
	LI_FIND(CloseHandle)(g_ServiceStopEvent);

	g_ServiceStatus.dwControlsAccepted	= 0;
	g_ServiceStatus.dwCurrentState		= SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode		= 0;
	g_ServiceStatus.dwCheckPoint		= 3;

	if (LI_FIND(SetServiceStatus)(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
#ifdef _DEBUG
		NNoMercyUtils::DebugLogf("SetServiceStatus(3) fail! Error: %u\n", LI_FIND(GetLastError)());
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("SetServiceStatus(3)").crypt_get(), LI_FIND(GetLastError)());
	}

	return;
}

void OnTerminate()
{
	if (gs_pServiceMsgFunction)
		gs_pServiceMsgFunction(0xD34D);

	NNoMercyUtils::FileLogf(gs_szLogFileWithPath, xorstr("Service terminate request handled!").crypt_get());
}

int main(int argc, char* argv[])
{
	std::set_terminate(&OnTerminate);

	if (!LI_FIND(LoadLibraryA)(xorstr("advapi32.dll").crypt_get()))
	{
		auto dwErrorCode = LI_FIND(GetLastError)();

#ifdef _DEBUG
		NNoMercyUtils::DebugLogf("Advapi32 can not loaded! Error: %u\n", dwErrorCode);
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("LoadLibrary-Advapi32").crypt_get(), dwErrorCode);

		return dwErrorCode;
	}

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, ServiceMain },
		{ NULL, NULL }
	};
	if (LI_FIND(StartServiceCtrlDispatcherW)(ServiceTable) == FALSE)
	{
		auto dwErrorCode = LI_FIND(GetLastError)();

#ifdef _DEBUG
		NNoMercyUtils::DebugLogf("NoMercy can not loaded! Error: %u\n", dwErrorCode);
#endif
		NNoMercyUtils::WriteErrorLogEntry(xorstr("StartServiceCtrlDispatcher").crypt_get(), dwErrorCode);

#ifdef _DEBUG
		ServiceThread(nullptr); // Allow quick start for debug build
#endif

		return dwErrorCode;
	}

#ifdef _DEBUG
	NNoMercyUtils::DebugLogf("NoMercy service will close!\n");
#endif
	return ERROR_SUCCESS;
}
