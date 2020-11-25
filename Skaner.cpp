#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <iostream>
#include <windows.h>
#include <winnt.h>
#include <fstream>
#include <io.h>
#include <filesystem>
#include <string.h>
#include <malloc.h>

#define SIZE_BUFFER 260

bool find1(unsigned char* arr1, int length1, char* arr2)
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
}

int main()
{
	char MB[5] = { 0xE8, 0xED, 0xCF, 0xFF, 0xFF };

	HANDLE hFile = CreateFile(
		L"C:\\MASM\\okno.exe",
		GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, 0);
	if (INVALID_HANDLE_VALUE == hFile)
		std::cout << "Create failed " << GetLastError() << std::endl;
	std::cout << "Crete file succes";
	LARGE_INTEGER iSize = { 0 };
	if (!GetFileSizeEx(hFile, &iSize))
		wprintf(L"Can't get size of file!\n");
	std::cout << iSize.QuadPart << std::endl;

	HANDLE hMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY,
		0, 0, nullptr);
	if (hMapping == nullptr)
		std::cerr << "fileMappingCreate - CreateFileMapping failed, fname = " << std::endl;

	unsigned char* list = (unsigned char*)MapViewOfFile(
		hMapping,
		FILE_MAP_READ,
		0,
		0,
		(SIZE_T)iSize.QuadPart);
	for (int i = 0; i < iSize.QuadPart; i++)
		printf(" % 02X", list[i]);
	std::cout << '\n' << find1(list, int(iSize.QuadPart), MB) << '\n';

	CloseHandle(hFile);
	system("pause");
	return 0;
}