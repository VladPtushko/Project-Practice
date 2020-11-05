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

bool find(char* arr1, int size1, char* arr2, int size2)
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

bool find1(char* arr1, char* arr2)
{
	char *pch;
	int length = _msize(arr1) / sizeof(char), check;
	pch = (char*)memchr(arr1, arr2[0], length);
	while (pch != NULL)
	{
		check = memcmp(pch, arr2, sizeof(arr2));
		if (!check)
			return true;
		length = _msize(pch) / sizeof(char);
		pch = (char*)memchr(pch, arr2[0], length);
	}
	return false;
}

int main()
{
	char MB[5] = { 0xE8, 0xED, 0xCF, 0xFF, 0xFF };

	std::fstream file("C:\\MASM\\okno.exe");
	int size;
	file.seekg(0, std::ios::end);
	size = file.tellg();
	std::cout << "Size of file is : " << size << " bytes" << std::endl;
	file.close();
	FILE *fp = fopen("C:\\MASM\\okno.exe", "rb");
	char *list = new char[size];
	fread((BYTE *)list, 1, size, fp);
	for (int i = 0; i < size; i++)
		printf(" % 02X", list[i]);
	for (int i{ 0 }; i < 5; i++)
		printf("\n % 02X", MB[i]);
	//std::cout << '\n' << find(list, size, MB, sizeof(MB)) << '\n';
	std::cout << '\n' << find1(list, MB) << '\n';
	
	system("pause");
	return 0;
}