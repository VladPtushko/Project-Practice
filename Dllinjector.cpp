#include <Windows.h>
#include <iostream>
#include <string>
#include <TlHelp32.h>

DWORD dwLoadLibAddr = NULL;
HANDLE hProc = NULL;
std::string strDllName = "C:\\Hooks\\Debug\\DllForTest.dll";

void OpenProc(std::string strProcName)
{
	HANDLE hSnapProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe32;

	if (Process32First(hSnapProcess, &pe32))
	{
		while (Process32Next(hSnapProcess, &pe32))
		{
			if (!strcmp(strProcName.c_str(), pe32.szExeFile))
			{
				hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
				return;
			}
		}
	}
}

int main()
{
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	dwLoadLibAddr = (DWORD)GetProcAddress(hKernel32, "LoadLibraryA");

	while (!hProc)
	{
		printf("Waiting process...\n");
		OpenProc("TestForHookLL.exe");
		Sleep(1000);
		system("cls");
		Sleep(100);
	}
	printf("Dll was injected");
	DWORD Allocated = (DWORD)VirtualAllocEx(hProc, NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	size_t strSize = strlen(strDllName.c_str());
	WriteProcessMemory(hProc, (LPVOID)Allocated, strDllName.c_str(), strSize + 1, NULL);
	CreateRemoteThread(hProc, NULL, NULL, (LPTHREAD_START_ROUTINE)dwLoadLibAddr, (LPVOID)Allocated, NULL, NULL);
	return 0;
}