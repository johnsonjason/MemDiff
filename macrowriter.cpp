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
#include "macrowriter.h"

/*++

Routine Description:

	Generates a C-compatible macro function from the list of changed bytes and their locations
	Operates on the notion that the process handle will be passed upon call

Parameters:

	MacroName - The name of the macro generated
	List - the list of changes, to be parsed into program statements for a macro

Return Value:

	A macro, a sequence of program statements

--*/
std::vector<std::pair<std::string, std::string>> GeneratePairMacro(std::string MacroName, std::vector<std::pair<BYTE, PVOID>> List)
{
	//
	// Store program statements in a vector to parse for output
	//

	std::vector<std::pair<std::string, std::string>> Macros;

	//
	// If macro/function name is not specified, use a default name
	//

	if (MacroName == "")
	{
		MacroName = "DefaultMacroName";
	}

	//
	// Parse the function name into a declaration, then iterate over each needed WriteProcessMemory sequence
	//

	std::string FunctionInit = "void " + MacroName + "(HANDLE ProcessHandle)\n{\n";
	std::string FunctionEnd = "}";

	Macros.push_back({ FunctionInit, FunctionEnd });

	for (size_t Item = 0; Item < List.size(); Item++)
	{
		//
		// Create a buffer named after the item index
		// Set the buffer to the first value of the Item in List[Item]
		//

		std::string VarInit = "\tBYTE Buffer" + std::to_string(Item);
		VarInit += " = " + std::to_string(List[Item].first) + "L;\n";

		//
		// Write the BufferN to the process using the decimal formatted second value of the Item in List[Item], indicating the virtual address
		//

		std::string WriteInit = "\tWriteProcessMemory(ProcessHandle, (PVOID)";
		WriteInit += std::to_string((DWORD_PTR)List[Item].second) + "L, &Buffer" + std::to_string(Item) + ", 1, NULL);\n\n";

		Macros.push_back({ VarInit, WriteInit });
	}

	return Macros;
}


/*++

Routine Description:

	Outputs the parsed macro to the CLI

Parameters:

	Macros - The sequence of program statements that make up the macro to be parsed and displayed

Return Value:

	None

--*/
void OutputMacro(std::vector<std::pair<std::string, std::string>> Macros)
{
	//
	// Function header initialization
	//

	std::cout << Macros[0].first;

	for (std::size_t MacIndex = 1; MacIndex < Macros.size(); MacIndex++)
	{
		std::cout << Macros[MacIndex].first << Macros[MacIndex].second;
	}

	//
	// End of function, second pair value of the first macro
	//

	std::cout << Macros[0].second << "\n" << std::endl;
}
