#include <Windows.h>
#include <iostream>
#include <memory>
#include "../../Common/CompilerSettings.h"

#include "../NM_Engine/NM_Index.h"
#ifdef _DEBUG
	#pragma comment(lib, "../../Bin/Debug/NM_Engine.lib")
#else
	#pragma comment(lib, "../../Bin/Release/NM_Engine.lib")
#endif

#include <xorstr.hpp>

int main()
{
	auto nmCommon = std::make_unique<CNoMercy>();
	if (!nmCommon || !nmCommon.get())
	{
		printf(xorstr("NoMercy class can NOT created! Last error: %u\n").crypt_get());
		return EXIT_FAILURE;
	}

	nmCommon->InitTest(true);

#ifdef _DEBUG
	printf(" # COMPLETED! # \n");
#endif

	std::cin.get();

    return EXIT_SUCCESS;
}

