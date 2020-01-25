/*
	MIT License

	Copyright (c) 2020 Jason Johnson

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "pch.h"
#include <iostream>
#include <psapi.h>
#include <vector>
#include "error-checking.h"
#include "macrowriter.h"
//
// Memory Differentiation Structure
// Contains the memory page contents and the information about a page
// Contains the checksum of the memory page contents
//

typedef struct _MEM_DIFF
{
	DWORD_PTR Checksum;
	MEMORY_BASIC_INFORMATION BasicInformation;
	std::vector<BYTE> PageData;
} MEM_DIFF;

/*++

Routine Description:

	Retrieves the checksum of the data provided, being the start address and iterates for the range provided

Parameters:

	Start - The start address to for the CRC checksum
	End - How many iterations after the start address

Return Value:

	DWORD_PTR - Returns the checksum (Checksum)

--*/
DWORD_PTR GetChecksum(void* Start, std::size_t End)
{
	DWORD_PTR Checksum = crc_crypt(Start, End);
	return Checksum;
}

/*++

Routine Description:

	Registers a page in the DiffList (the list of pages and their checksums to be validated)

Parameters:
	
	DiffList - A dynamic container (std::vector) of pages which will be checked against after registering all pages in the module
	BasicInformation - The basic memory information of the page being registered, such as the base address and page size

Return Value:

	None

--*/
void EstablishPage(std::vector<MEM_DIFF>& DiffList, MEMORY_BASIC_INFORMATION& BasicInformation)
{

	//
	// Declare and initialize a memory differentiation structure, for later comparison
	// Reserve the page size in the PageData member of the structure
	// Iterate over the memory page by divisions of DWORD_PTR (8 bytes 64-bit, 4 bytes 32-bit) and store it
	//

	MEM_DIFF DiffBlock = { 0 };
	DiffBlock.PageData.reserve(BasicInformation.RegionSize);
	DiffBlock.BasicInformation = BasicInformation;

	for (size_t ByteIter = reinterpret_cast<size_t>(BasicInformation.BaseAddress);
		ByteIter < (reinterpret_cast<size_t>(BasicInformation.BaseAddress) + BasicInformation.RegionSize); ByteIter++)
	{
		DiffBlock.PageData.push_back(*(BYTE*)ByteIter);
	}

	//
	// Get the checksum of the page and push the differentiation structure into the comparison list
	//

	DiffBlock.Checksum = GetChecksum(DiffBlock.PageData.data(), DiffBlock.PageData.size());
	DiffList.push_back(DiffBlock);
	std::cout << "Added Page Base: " << BasicInformation.BaseAddress << "\n";
	std::cout << "Page Checksum: " << std::hex << DiffBlock.Checksum << "\n";
}

/*++

Routine Description:
	
	Retrieves and iterates over each page in the process, registering only the ones that
	are either PAGE_EXECUTE_READ or PAGE_READONLY, registration parameters will be adjustable in the future.
	Enabling it for writeable pages is not recommended, because it is intended behavior for those pages to be written to.
	It is not typical for READONLY/EXECUTE_READ pages to be modified.

Paremeters:

	ModuleName - The name of the module in the process to use for page list registration, currently NULL (first module), adjustable in future
	DiffList - A dynamic container (std::vector) of pages which will be checked against after registering all pages in the module

Return Value:

	DWORD - 0 or GetLastError() indicating a WINAPI error

--*/
DWORD GetModulePages(LPWSTR ModuleName, std::vector<MEM_DIFF>& DiffList)
{
	//
	// Get the module handle (HMODULE) used in getting module information
	//

	HANDLE Module = GetModuleHandle(ModuleName);
	MODULEINFO ModuleInformation;

	//
	// Get information about the module returned from GetModuleHandle
	// Specifically the size of the module is the information required
	// Size is required for calculating when to stop page iteration
	//

	if (!K32GetModuleInformation(GetCurrentProcess(), (HMODULE)Module, &ModuleInformation, sizeof(ModuleInformation)))
	{
		//
		// K32GetModuleInformation failed with FALSE, return the last WINAPI error
		//

		DWORD StatusCode = GetLastError();
		std::cerr << "K32GetModuleInformation encountered an error: " << StatusCode << std::endl;
		return StatusCode;
	}

	//
	// Initial query on the first memory page of the executable module
	//

	MEMORY_BASIC_INFORMATION BasicInformation = { 0 };
	VirtualQuery(Module, &BasicInformation, sizeof(BasicInformation));

	std::cout << "Module EP: " << BasicInformation.BaseAddress << std::endl;

	for (size_t PageIter = reinterpret_cast<size_t>(BasicInformation.BaseAddress);
		PageIter < (reinterpret_cast<size_t>(BasicInformation.BaseAddress) + ModuleInformation.SizeOfImage);  PageIter += BasicInformation.RegionSize)
	{
		//
		// Redundant initial query as it's already used in the loop header
		//

		VirtualQuery(reinterpret_cast<PVOID>(PageIter), &BasicInformation, sizeof(BasicInformation));
		if ((size_t)BasicInformation.BaseAddress < (size_t)Module + ModuleInformation.SizeOfImage)
		{
			if (BasicInformation.Protect == PAGE_EXECUTE_READ || BasicInformation.Protect == PAGE_READONLY)
			{
				//
				// Iterate over all of the data in the page, save it, and register their checksums
				//
				EstablishPage(DiffList, BasicInformation);
			}
		}

	}

	return 0;
}


/*++

Routine Description:
	
	EvaluatePage is used for verifying that a page is maintaining its data integrity.
	If the page data is not intact, a checksum mismatch is generated

Parameters:

	Comparator - A MEM_DIFF structure which contains the page address resident in module memory, as well as a snapshot of that page

Return Value:

	DWORD_PTR - Returns the unexpected/invalid checksum

--*/
DWORD_PTR EvaluatePage(MEM_DIFF Comparator)
{
	//
	// Calculate the checksum of the page and compare it with the checksum we have on record
	//

	DWORD_PTR TargetChecksum = GetChecksum(Comparator.BasicInformation.BaseAddress, Comparator.BasicInformation.RegionSize);
	if (TargetChecksum != Comparator.Checksum)
	{
		//
		// Checksum mismatch, returning the incorrect checksum
		//

		return TargetChecksum;
	}
	return NULL;
}

/*++

Routine Description:

	Compares the memory contents of the pages for where the changes occurred.
	Called when EvaluatePage detects a checksum mismatch

Parameters:

	Page - the virtual address of the page's snapshot, used in comparing against Page
	AltPage - The virtual address of the page resident in the module's memory, used in comparison and iteration
	PageSize - The memory size of the pages to compare, indicates when to stop comparing

Return Value:

	std::vector<std::pair<BYTE, PVOID>> - A list of paired memory changes, along with the location (virtual address) of each change

--*/
std::pair<std::vector<std::pair<BYTE, PVOID>>, std::vector<std::pair<BYTE, PVOID>>> ComparePages(void* Page, void* AltPage, size_t PageSize)
{
	//
	// ChangedBytes stores a list of byte+address combinations where changes occurred
	//

	std::vector<std::pair<BYTE, PVOID>> ChangedBytes;
	std::vector<std::pair<BYTE, PVOID>> OriginBytes;
	size_t AltPageIter = reinterpret_cast<size_t>(AltPage);

	for (size_t PageIter = reinterpret_cast<size_t>(Page); PageIter < (reinterpret_cast<size_t>(Page) + PageSize); PageIter++)
	{
		//
		// Compare the byte of the page in original memory and the alternate saved page
		//

		if (*(BYTE*)PageIter != *(BYTE*)AltPageIter)
		{
			//
			// Add a pair (byte, virtual address) to the ChangedBytes list indicating where a change occurred
			//

			ChangedBytes.push_back({ *(BYTE*)AltPageIter, (PVOID)AltPageIter });
			OriginBytes.push_back({ *(BYTE*)PageIter, (PVOID)AltPageIter });
		}
		AltPageIter++;
	}

	for (std::pair<BYTE, PVOID> ChangePair : ChangedBytes)
	{
		std::cout << std::hex << "Change Address: " << ChangePair.second << " | Changed byte: 0x" << +ChangePair.first << "\n\n";
	}
	return { ChangedBytes, OriginBytes };
}

/*++

Routine Description:
	
	Acquires all the pages in the module using GetModulePages
	Repeatedly loops over each page, comparing the checksum with the corresponding snapshot, detecting any mismatches

Parameters:

	lpParam - Thread parameter, currently not in use

Return Value:
	
	DWORD - Redundant value (NULL) currently

--*/
DWORD WINAPI EvaluatePageList(LPVOID lpParam)
{
	//
	// Acquire the list of memory pages within the module and input their configurations in MEM_DIFF, applying to the PageSet
	// Constantly evaluate the checksums of PageSet until PageEval is no longer true (indefinitely)
	// WIP: PageEval flag
	//

	bool PageEval = true;
	std::vector<MEM_DIFF> PageSet;
	std::wstring ModuleName;

	std::cout << "Module name: ";
	std::getline(std::wcin, ModuleName);
	GetModulePages(const_cast<LPWSTR>(ModuleName.c_str()), PageSet);

	std::cout << "Page list initialized. " << std::endl;

	while (PageEval)
	{
		for (MEM_DIFF Page : PageSet)
		{
			//
			// If EvaluatePage returns non-zero, then a mismatch in checksums occurred
			//

			DWORD_PTR Checksum = EvaluatePage(Page);
			if (Checksum)
			{
				std::cout << "Page change: " << Page.BasicInformation.BaseAddress << " | Changed Checksum: " << Checksum <<
					" | Expected Checksum: " << Page.Checksum << "\n";

				//
				// Compare and extract the changed memory with their corresponding addresses indicating where the pages differ
				//

				auto ChangedData = ComparePages(Page.PageData.data(), Page.BasicInformation.BaseAddress, Page.BasicInformation.RegionSize);

				std::string MacroName = "";
				std::cout << "Macro name? : ";
				std::getline(std::cin, MacroName);

				//
				// Output the generated macro statement utilizing WriteProcessMemory
				//

				OutputMacro(GeneratePairMacro(MacroName, ChangedData.first));

				//
				// Output the inverse of the macro statement operation (undo)
				//

				OutputMacro(GeneratePairMacro("Undo" + MacroName, ChangedData.second));
			}
		}
	}
	return NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		CloseHandle(CreateThread(0, 0, EvaluatePageList, 0, 0, 0));
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

