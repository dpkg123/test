/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include	"edgegeomcompiler.h"

#if defined(WIN32)
#include	"crtdbg.h"
#endif

#include <iostream>

//--------------------------------------------------------------------------------------------------
/**
	Main application entry point.
**/
//--------------------------------------------------------------------------------------------------

int	main( int argc, char** argv )
{
#if defined(WIN32)
	// Enables Visual Studio's built-in memory leak detection and configures it to
	// run automatically at program termination...
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	// ...and to print its output to stderr in addition to the debugger Output window.
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
#endif // defined(WIN32)

	// check arguments
	unsigned processingFlags = 0;
	std::string invBindMatsFile;
	int iArg;
	for(iArg=1; iArg<argc-2; ++iArg)
	{
		if (strncmp(argv[iArg], "--enable-scaled-skinning", 25) == 0)
		{
			processingFlags |= EDGE_GEOM_FLAG_ENABLE_SCALED_SKINNING;
		}
		else if (strncmp(argv[iArg], "--inv-bind-mats-out", 20) == 0)
		{
			processingFlags |= EDGE_GEOM_FLAG_OUTPUT_INV_BIND_MATRICES;
			invBindMatsFile = argv[iArg+1];
			iArg++;
		}
		else
		{
			// exit loop and grab required arguments;
			break;
		}
	}

	if (iArg+2 > argc)
	{
	    std::cout << argv[0] << " (PlayStation(R)Edge 1.2.0)" << std::endl;

		std::cout << "Usage: " << argv[0] << " [OPTIONS] <inputfile> <outputfile>" << std::endl;
		std::cout << "Options: " << std::endl;
		std::cout << "  --enable-scaled-skinning      Allow scaling in runtime skinning code." << std::endl;
		std::cout << "  --inv-bind-mats-out <file>    Output array of transposed 3x4 inverse bind matrices to <file>." << std::endl;
		return 1;
	}
	const std::string inputFile( argv[iArg++] );
	const std::string outputFile( argv[iArg++] );
	
	FCollada::Initialize();

	int errCode = 0;
	try
	{
		// call the main processing function
		Edge::Tools::GeomProcessFile( inputFile, outputFile, invBindMatsFile, processingFlags );
	}
	catch( std::exception& excuse )
	{
		std::cout << "Failed: " << excuse.what() << std::endl;
		errCode = 1;
	}
	catch( const char* excuse )
	{
		std::cout << "Failed: " << excuse << std::endl;
		errCode = 1;
	}
	catch( ... )
	{
		std::cout << "Failed: unknown error" << std::endl;
		errCode = 1;
	}

	FCollada::Release();

#if defined(WIN32) && defined(_DEBUG) // useful for tracking memory leaks.
	// NOTE: FCollada seems to leave a ton of memory allocations hanging (even if you
	// properly initialize and release it), so this information may not be terribly
	// useful.
	printf("Printing leaked allocations...\n");
	_CrtMemDumpAllObjectsSince( NULL );
	printf(""); // put a breakpoint here
#endif

	return errCode;
}
