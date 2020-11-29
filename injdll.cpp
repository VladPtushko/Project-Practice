#define _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE 1

#include "wow64log.h"

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

UNICODE_STRING LookupPath = RTL_CONSTANT_STRING((PWCH)L"C:\\TestInjector\\DLLinjector1.exe");

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
//
//

BOOL g_bIsSuspicious;
UNICODE_STRING g_ModuleName;

//
// Hooking functions and prototypes.
//

#define SetHook(_Function)                                     \
  Orig ## _Function = _Function;                               \
  DetourAttach((PVOID*)&Orig ## _Function, Hook ## _Function);

#define Unhook(_Function) \
  DetourDetach((PVOID*)&Orig ## _Function, Hook ## _Function);

inline decltype(LoadLibraryW)* OrigLoadLibraryW = nullptr;

EXTERN_C
_Ret_maybenull_
HMODULE
WINAPI
HookLoadLibraryW(
  _In_ LPCWSTR lpLibFileName
)
{
  //
  // Log the function call.
  //

  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"LoadLibrary(%s)",
    lpLibFileName);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);

  OutputDebugString(Buffer);

  EtwEventWriteString(ProviderHandle, 0, 0, g_ModuleName.Buffer);

  //
  // Call original function.
  //

  return OrigLoadLibraryW(lpLibFileName);
}

inline decltype(LoadLibraryA)* OrigLoadLibraryA = nullptr;

EXTERN_C
_Ret_maybenull_
HMODULE
WINAPI
HookLoadLibraryA(
  _In_ LPCSTR lpLibFileName
)
{

  //
  // Log the function call.
  //

  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"LoadLibrary(%S)",
    lpLibFileName);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);

  OutputDebugString(Buffer);

  EtwEventWriteString(ProviderHandle, 0, 0, g_ModuleName.Buffer);

  //
  // Call original function.
  //

  return OrigLoadLibraryA(lpLibFileName);
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
  //
  // Log the function call.
  //

  WCHAR Buffer[128];
  _snwprintf(Buffer,
    RTL_NUMBER_OF(Buffer),
    L"CreateRemoteThread(%p, %p, %u, %p, %p, %u, %p)",
    hProcess,
    lpThreadAttributes,
    dwStackSize,
    lpStartAddress,
    lpParameter,
    dwCreationFlags,
    lpThreadId);

  EtwEventWriteString(ProviderHandle, 0, 0, Buffer);

  OutputDebugString(Buffer);

  if (!RtlCompareUnicodeString(&g_ModuleName, &LookupPath, TRUE))
  {
    WCHAR Buf[1024];
    _snwprintf(Buf, RTL_NUMBER_OF(Buf), L"Malicious DLL injection detected! Access denied.");
    SetLastError(ERROR_ACCESS_DENIED);
    return nullptr;
  }

  //
  // Call original function.
  //

  return OrigCreateRemoteThread(hProcess,
                                lpThreadAttributes,
                                dwStackSize, 
                                lpStartAddress, 
                                lpParameter, 
                                dwCreationFlags, 
                                lpThreadId);
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
    SetHook(LoadLibraryA);
    SetHook(CreateRemoteThread);
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
    Unhook(LoadLibraryA);
    Unhook(CreateRemoteThread);
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

  GetModuleFileNameW(NULL, g_ModuleName.Buffer, MAX_PATH);
  g_ModuleName.Length = 0;
  g_ModuleName.MaximumLength = MAX_PATH;
  for (; g_ModuleName.Buffer[g_ModuleName.Length]; g_ModuleName.Length++)
    ;

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

