#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <iostream>

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME  L"MySampleService"

int wmain(int argc, LPWSTR* argv)
{

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL}
	};

	if (argc == 2)
	{
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

		if (!wcscmp(argv[1], L"-create"))
		{
			std::wcout << L"Creating service..." << std::endl;

			SC_HANDLE handErr = CreateService(
				hSCManager,
				SERVICE_NAME,
				SERVICE_NAME,
				SERVICE_ALL_ACCESS,
				SERVICE_WIN32_OWN_PROCESS,
				SERVICE_DEMAND_START,
				SERVICE_ERROR_NORMAL,
				argv[0],
				NULL, NULL, NULL, NULL, NULL);

			if (handErr == NULL)
			{
				std::wcout << L"Error of creating service!" << std::endl;
				return 1;
			}
			std::wcout << L"Service created!" << std::endl;
		}
		else if (!wcscmp(argv[1], L"-start"))
		{
			std::wcout << L"Starting service..." << std::endl;
			SC_HANDLE service = OpenService(hSCManager, SERVICE_NAME, SERVICE_START);
			if (!StartService(service, 0, nullptr))
			{
				std::wcout << L"Error of starting service!" << std::endl;
				return 2;
			}
			std::wcout << L"Service started!" << std::endl;
		}
		else if (!wcscmp(argv[1], L"-delete"))
		{
			DeleteService(OpenService(hSCManager, SERVICE_NAME, SERVICE_STOP | DELETE));
			std::wcout << L"Service deleted!" << std::endl;
		}
		/*else if (!wcscmp(argv[1], L"-stop"))
		{
			std::wcout << L"Stopping service..." << std::endl;
			DeleteService(OpenService(hSCManager, SERVICE_NAME, SERVICE_STOP | DELETE));
			SC_HANDLE handErr = CreateService(
				hSCManager,
				SERVICE_NAME,
				SERVICE_NAME,
				SERVICE_ALL_ACCESS,
				SERVICE_WIN32_OWN_PROCESS,
				SERVICE_DEMAND_START,
				SERVICE_ERROR_NORMAL,
				argv[0],
				NULL, NULL, NULL, NULL, NULL);

			if (handErr == NULL)
			{
				std::wcout << L"Error of creating service!" << std::endl;
				return 1;
			}
			std::wcout << L"Service stopped!" << std::endl;

		}*/
	}

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		system("pause");
		return GetLastError();
	}

	system("pause");
	return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	DWORD Status = E_FAIL;

	// Register our service control handler with the SCM
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
		return;

	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(
			L"My Sample Service: ServiceMain: SetServiceStatus returned error");
	}

	/*
	 * Perform tasks necessary to start the service here
	 */

	 // Create a service stop event to wait on later
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		// Error creating event
		// Tell service controller we are stopped and exit
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(
				L"My Sample Service: ServiceMain: SetServiceStatus returned error");
		}
		return;
	}

	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(
			L"My Sample Service: ServiceMain: SetServiceStatus returned error");
	}

	// Start a thread that will perform the main task of the service
	HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	// Wait until our worker thread exits signaling that the service needs to stop
	WaitForSingleObject(hThread, INFINITE);


	/*
	 * Perform any cleanup tasks
	 */

	CloseHandle(g_ServiceStopEvent);

	// Tell the service controller we are stopped
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(
			L"My Sample Service: ServiceMain: SetServiceStatus returned error");
	}
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		/*
		 * Perform tasks necessary to stop the service here
		 */

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(
				L"My Sample Service: ServiceCtrlHandler: SetServiceStatus returned error");
		}

		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);

		break;

	default:
		break;
	}
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		/*
		 * Perform main service function here
		 */

		 //  Simulate some work by sleeping
		Sleep(10000);
		Beep(500, 30);
		OutputDebugString( L"Work...\n");
	}

	return ERROR_SUCCESS;
}