#define _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE 1

#include "wow64log.h"
//#include <string>
//#include <map>

//#include <windows.h>
//#include <winnt.h>
//#include <filesystem>  //_msize
//#include <string.h>
//#include <malloc.h>

//#define SIZE_BUFFER 260

/*typedef struct _STATE {
  std::map<std::wstring, bool, std::less<void>>
    m_CheckedCache;
  bool m_blsSuspicious;
}STATE, *PSTATE;*/

//STATE g_State{ {}, false };
bool IsSospicious = false;

//
// Include NTDLL-related headers.
//
#define NTDLL_NO_INLINE_INIT_STRING
#include <ntdll.h>

#if defined(_M_IX86)
#  define ARCH_A          "x86"
#  define ARCH_W         L"x86"
#elif defined(_M_AMD64)
#  define ARCH_A          "x64"
#  define ARCH_W         L"x64"
#elif defined(_M_ARM)
#  define ARCH_A          "ARM32"
#  define ARCH_W         L"ARM32"
#elif defined(_M_ARM64)
#  define ARCH_A          "ARM64"
#  define ARCH_W         L"ARM64"
#else
#  error Unknown architecture
#endif


// size_t strlen(const char * str)
// {
//   const char *s;
//   for (s = str; *s; ++s) {}
//   return(s - str);
// }

//
// Include support for ETW logging.
// Note that following functions are mocked, because they're
// located in advapi32.dll.  Fortunatelly, advapi32.dll simply
// redirects calls to these functions to the ntdll.dll.
//

#define EventActivityIdControl  EtwEventActivityIdControl
#define EventEnabled            EtwEventEnabled
#define EventProviderEnabled    EtwEventProviderEnabled
#define EventRegister           EtwEventRegister
#define EventSetInformation     EtwEventSetInformation
#define EventUnregister         EtwEventUnregister
#define EventWrite              EtwEventWrite
#define EventWriteEndScenario   EtwEventWriteEndScenario
#define EventWriteEx            EtwEventWriteEx
#define EventWriteStartScenario EtwEventWriteStartScenario
#define EventWriteString        EtwEventWriteString
#define EventWriteTransfer      EtwEventWriteTransfer

#include <evntprov.h>

//
// Include Detours.
//

#include <detours.h>

#define SetHook(_FunctionName) \
Orig ## _FunctionName = _FunctionName;\
DetourAttach((PVOID*)&Orig ## _FunctionName, Hook ## _FunctionName)

#define Unhook(_FunctionName) \
DetourDetach((PVOID*)&Orig ## _FunctionName, Hook ## _FunctionName)

//
// This is necessary for x86 builds because of SEH,
// which is used by Detours.  Look at loadcfg.c file
// in Visual Studio's CRT source codes for the original
// implementation.
//

#if defined(_M_IX86) || defined(_X86_)

EXTERN_C PVOID __safe_se_handler_table[]; /* base of safe handler entry table */
EXTERN_C BYTE  __safe_se_handler_count;   /* absolute symbol whose address is
                                             the count of table entries */
EXTERN_C
CONST
DECLSPEC_SELECTANY
IMAGE_LOAD_CONFIG_DIRECTORY
_load_config_used = {
    sizeof(_load_config_used),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    (SIZE_T)__safe_se_handler_table,
    (SIZE_T)&__safe_se_handler_count,
};

#endif

//
// Unfortunatelly sprintf-like functions are not exposed
// by ntdll.lib, which we're linking against.  We have to
// load them dynamically.
//

using _snwprintf_fn_t = int (__cdecl*)(
  wchar_t *buffer,
  size_t count,
  const wchar_t *format,
  ...
  );

inline _snwprintf_fn_t _snwprintf = nullptr;

//
// ETW provider GUID and global provider handle.
//

//
// GUID:
//   {a4b4ba50-a667-43f5-919b-1e52a6d69bd5}
//

GUID ProviderGuid = {
  0xa4b4ba50, 0xa667, 0x43f5, { 0x91, 0x9b, 0x1e, 0x52, 0xa6, 0xd6, 0x9b, 0xd5 }
};

REGHANDLE ProviderHandle;

//
// Hooking functions and prototypes.
//

/*inline decltype(NtQuerySystemInformation)* OrigNtQuerySystemInformation = nullptr;

EXTERN_C
NTSTATUS
NTAPI
HookNtQuerySystemInformation(
  _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
  _Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
  _In_ ULONG SystemInformationLength,
  _Out_opt_ PULONG ReturnLength
  )
{
  //
  // Log the function call.
  //

  WCHAR Buffer[128];
  _snwprintf(Buffer,
             RTL_NUMBER_OF(Buffer),
             L"NtQuerySystemInformation(%i, %p, %i)",
             SystemInformationClass,
             SystemInformation,
             SystemInformationLength);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);

  //
  // Call original function.
  //

  return OrigNtQuerySystemInformation(SystemInformationClass,
                                      SystemInformation,
                                      SystemInformationLength,
                                      ReturnLength);
}*/

bool find2(unsigned char* arr1, int size1, unsigned char* arr2, int size2)
{
  bool check = false;
  for (int i = 0; i < size1 - size2; i++)
  {
    if (arr1[i] == arr2[0])
    {
      check = true;
      for (int j = 1; j < size2; j++)
      {
        if (arr1[j + i] != arr2[j])
        {
          check = false;
          break;
        }
      }
      if (check)
        return check;
    }
  }
  return check;
}

/*bool find1(unsigned char* arr1, int length1, char* arr2)
{
  char* pch;
  int check;
  pch = (char*)memchr(arr1, arr2[0], length1);
  while (pch != NULL)
  {
    check = memcmp(pch, arr2, sizeof(arr2));
    if (!check)
      return true;
    length1 = _msize(pch) / sizeof(char);
    pch = (char*)memchr(pch, arr2[0], length1);
  }
  return false;
}*/

bool skan(_In_ LPCWSTR lpLibFileName)
{
  unsigned char MB[5] = { 0xE8, 0xED, 0xCF, 0xFF, 0xFF };

  HANDLE hFile = CreateFile(
    lpLibFileName,
    GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
    OPEN_EXISTING, 0, 0);

  LARGE_INTEGER iSize = { 0 };
  GetFileSizeEx(hFile, &iSize);
    
  HANDLE hMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY,
    0, 0, nullptr);

  unsigned char* list = (unsigned char*)MapViewOfFile(
    hMapping,
    FILE_MAP_READ,
    0,
    0,
    0);

  CloseHandle(hFile);
  CloseHandle(hMapping);
  return find2(list, int(iSize.QuadPart), MB, 5);
}

inline decltype(LoadLibraryW)* OrigLoadLibraryW = nullptr;
EXTERN_C
HMODULE
WINAPI
HookLoadLibraryW(
  _In_ LPCWSTR lpLibFileName
)
{
  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"LoadLibraryW(%s)",
    lpLibFileName);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);
  bool bNeedToBlock = false;
  /*if (g_State.m_blsSuspicious)
  {
    auto Iter = g_State.m_CheckedCache.find(lpLibFileName);
    if (Iter == g_State.m_CheckedCache.end())
    {
      Iter = g_State.m_CheckedCache.emplace_hint(Iter, lpLibFileName, true); //skan

    }
    bNeedToBlock = Iter->second;
  }*/
  bNeedToBlock = skan(lpLibFileName);
  SetLastError(bNeedToBlock ? ERROR_ACCESS_DENIED : ERROR_SUCCESS);
  if (!bNeedToBlock)
  {
    OutputDebugString(L"Malitious library ");
    OutputDebugString(lpLibFileName);
  }
  return bNeedToBlock ? nullptr : OrigLoadLibraryW(lpLibFileName);
}

inline decltype(LoadLibraryExW)* OrigLoadLibraryExW = nullptr;
EXTERN_C
HMODULE
WINAPI
HookLoadLibraryExW(
  _In_ LPCWSTR lpLibFileName,
  _Reserved_ HANDLE hFile,
  _In_ DWORD dwFlags
)
{
  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"LoadLibraryExW(%s, %p, %u)",
    lpLibFileName, hFile, dwFlags);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);
  bool bNeedToBlock = false;
  /*if (g_State.m_blsSuspicious)
  {
    auto Iter = g_State.m_CheckedCache.find(lpLibFileName);
    if (Iter == g_State.m_CheckedCache.end())
    {
      Iter = g_State.m_CheckedCache.emplace_hint(Iter, lpLibFileName, true);

    }
    bNeedToBlock = Iter->second;
  }*/
  bNeedToBlock = (IsSospicious && skan(lpLibFileName));
  SetLastError(bNeedToBlock ? ERROR_ACCESS_DENIED : ERROR_SUCCESS);
  if (!bNeedToBlock)
  {
    OutputDebugString(L"Malitious library ");
    OutputDebugString(lpLibFileName);
  }
  SetLastError(bNeedToBlock ? ERROR_ACCESS_DENIED : ERROR_SUCCESS);
  return bNeedToBlock ? nullptr : OrigLoadLibraryExW(lpLibFileName, hFile, dwFlags);
}

inline decltype(CreateRemoteThread)* OrigCreateRemoteThread = nullptr;
EXTERN_C
_Ret_maybenull_
HANDLE
WINAPI
HookCreateRemoteThread(
  _In_ HANDLE hProcess,
  _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_ SIZE_T dwStackSize,
  _In_ LPTHREAD_START_ROUTINE lpStartAddress,
  _In_opt_ LPVOID lpParameter,
  _In_ DWORD dwCreationFlags,
  _Out_opt_ LPDWORD lpThreadId
)
{
  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"CreateRemoteThread(%p, %p, %u, %p, %p, %u, %p)",
    hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
  IsSospicious = false;
  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);
  OutputDebugString(Buffer);
  if(lpStartAddress == (LPTHREAD_START_ROUTINE)OrigLoadLibraryW || lpStartAddress == (LPTHREAD_START_ROUTINE)OrigLoadLibraryExW)
  {
    IsSospicious = true;
  }

  return OrigCreateRemoteThread(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

inline decltype(CreateRemoteThreadEx)* OrigCreateRemoteThreadEx = nullptr;
EXTERN_C
_Ret_maybenull_
HANDLE
WINAPI
HookCreateRemoteThreadEx(
  _In_ HANDLE hProcess,
  _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_ SIZE_T dwStackSize,
  _In_ LPTHREAD_START_ROUTINE lpStartAddress,
  _In_opt_ LPVOID lpParameter,
  _In_ DWORD dwCreationFlags,
  _In_opt_ LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
  _Out_opt_ LPDWORD lpThreadId
)
{
  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"CreateRemoteThreadEx(%p, %p, %u, %p, %p, %u, %p, %p)",
    hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
  IsSospicious = false;
  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);
  OutputDebugString(Buffer);
  if (lpStartAddress == (LPTHREAD_START_ROUTINE)OrigLoadLibraryW || lpStartAddress == (LPTHREAD_START_ROUTINE)OrigLoadLibraryExW)
  {
    IsSospicious = true;
  }

  return OrigCreateRemoteThreadEx(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpAttributeList, lpThreadId);
}

NTSTATUS
NTAPI 
EnableDetours(
  VOID
  )
{
  DetourTransactionBegin();
  {
    SetHook(LoadLibraryW);
    SetHook(LoadLibraryExW);
    SetHook(CreateRemoteThread);
    SetHook(CreateRemoteThreadEx);
  }
  DetourTransactionCommit();

  return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
DisableDetours(
  VOID
  )
{
  DetourTransactionBegin();
  {
    Unhook(LoadLibraryW);
    Unhook(LoadLibraryExW);
    Unhook(CreateRemoteThread);
    Unhook(CreateRemoteThreadEx);
  }
  DetourTransactionCommit();

  return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
OnProcessAttach(
  _In_ PVOID ModuleHandle
  )
{
  //
  // First, resolve address of the _snwprintf function.
  //

  ANSI_STRING RoutineName;
  RtlInitAnsiString(&RoutineName, (PSTR)"_snwprintf");

  UNICODE_STRING NtdllPath;
  RtlInitUnicodeString(&NtdllPath, (PWSTR)L"ntdll.dll");

  HANDLE NtdllHandle;
  LdrGetDllHandle(NULL, 0, &NtdllPath, &NtdllHandle);
  LdrGetProcedureAddress(NtdllHandle, &RoutineName, 0, (PVOID*)&_snwprintf);

  //
  // Make us unloadable (by FreeLibrary calls).
  //

  LdrAddRefDll(LDR_ADDREF_DLL_PIN, ModuleHandle);

  //
  // Hide this DLL from the PEB.
  //

  PPEB Peb = NtCurrentPeb();
  PLIST_ENTRY ListEntry;

  for (ListEntry =   Peb->Ldr->InLoadOrderModuleList.Flink;
       ListEntry != &Peb->Ldr->InLoadOrderModuleList;
       ListEntry =   ListEntry->Flink)
  {
    PLDR_DATA_TABLE_ENTRY LdrEntry = CONTAINING_RECORD(ListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

    //
    // ModuleHandle is same as DLL base address.
    //

    if (LdrEntry->DllBase == ModuleHandle)
    {
      RemoveEntryList(&LdrEntry->InLoadOrderLinks);
      RemoveEntryList(&LdrEntry->InInitializationOrderLinks);
      RemoveEntryList(&LdrEntry->InMemoryOrderLinks);
      RemoveEntryList(&LdrEntry->HashLinks);

      break;
    }
  }

  //
  // Create exports for Wow64Log* functions in
  // the PE header of this DLL.
  //

  Wow64LogCreateExports(ModuleHandle);


  //
  // Register ETW provider.
  //

  EtwEventRegister(&ProviderGuid,
                   NULL,
                   NULL,
                   &ProviderHandle);

  //
  // Create dummy thread - used for testing.
  //

  // RtlCreateUserThread(NtCurrentProcess(),
  //                     NULL,
  //                     FALSE,
  //                     0,
  //                     0,
  //                     0,
  //                     &ThreadRoutine,
  //                     NULL,
  //                     NULL,
  //                     NULL);

  //
  // Get command line of the current process and send it.
  //

  PWSTR CommandLine = Peb->ProcessParameters->CommandLine.Buffer;

  WCHAR Buffer[1024];
  _snwprintf(Buffer,
             RTL_NUMBER_OF(Buffer),
             L"Arch: %s, CommandLine: '%s'",
             ARCH_W,
             CommandLine);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);

  //
  // Hook all functions.
  //

  return EnableDetours();
}

NTSTATUS
NTAPI
OnProcessDetach(
  _In_ HANDLE ModuleHandle
  )
{
  //
  // Unhook all functions.
  //

  return DisableDetours();
}

EXTERN_C
BOOL
NTAPI
NtDllMain(
  _In_ HANDLE ModuleHandle,
  _In_ ULONG Reason,
  _In_ LPVOID Reserved
  )
{
  switch (Reason)
  {
    case DLL_PROCESS_ATTACH:
      OnProcessAttach(ModuleHandle);
      break;

    case DLL_PROCESS_DETACH:
      OnProcessDetach(ModuleHandle);
      break;

    case DLL_THREAD_ATTACH:

      break;

    case DLL_THREAD_DETACH:

      break;
  }

  return TRUE;
}

