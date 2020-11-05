#include "stdafx.h"
#include <fstream>
#include "mhook/mhook-lib/mhook.h"

// Defines and typedefs
#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)
/*typedef struct _MY_MB 
{
	HWND    hWnd;
	LPCTSTR lpText;
	LPCTSTR lpCaption;
	UINT    uType;
} MY_MB, *PMY_MB;
typedef NTSTATUS (WINAPI *PNT_MB)(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
// Original function
PNT_MB OriginalMB = (PNT_MB)::GetProcAddress(::GetModuleHandle(L"User32"), "MessageBoxA");
// Hooked function
NTSTATUS WINAPI HookedMB(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption,	UINT uType)
{
    NTSTATUS status = OriginalMB(hWnd, L"Hooked", L"Hooked", uType);
	std::ofstream fout("C:\\login.txt");
	fout << "MessageBox called" << std::endl;
	fout.close();
    return status;
}*/

typedef struct _MY_LL
{
	LPCSTR lpLibFileName;
} MY_LL, *PMY_LL;
typedef NTSTATUS(WINAPI *PNT_LL)(LPCSTR lpLibFileName);
PNT_LL OriginalLL = (PNT_LL)::GetProcAddress(::GetModuleHandle(L"Kernel32"), "LoadLibraryA");
NTSTATUS WINAPI HookedLL(LPCSTR lpLibFileName)
{
	NTSTATUS status = OriginalLL(lpLibFileName);
	std::ofstream fout("C:\\login.txt", std::ios::app);
	fout << "LoadLibrary called " << lpLibFileName << std::endl;
	fout.close();
	return status;
}

// Entry point
BOOL WINAPI DllMain(__in HINSTANCE  hInstance, __in DWORD Reason, __in LPVOID Reserved)
{        
    switch (Reason)
    {

		case DLL_PROCESS_ATTACH:
		{
			std::ofstream fout("C:\\login.txt", std::ios::app);
			fout << "Dll start work " << std::endl;
			fout.close();
			Mhook_SetHook((PVOID*)&OriginalLL, HookedLL);
			break;
		}
    case DLL_PROCESS_DETACH:
        Mhook_Unhook((PVOID*)&OriginalLL);
        break;
    }

    return TRUE;
}
