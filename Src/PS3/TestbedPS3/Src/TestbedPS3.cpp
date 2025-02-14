/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */


// TestbedPS3.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Commandlet.h"


#define PS3ERROR_ASSERT		1
#define PS3ERROR_CRASH		2
#define PS3ERROR_TIMEOUT	3
#define PS3ERROR_PS3		4
#define PS3ERROR_LOGFILE	5

#define DEFAULT_MAXSECONDS	30	// Seconds


void Dump( HANDLE hFile, const char *Buffer)
{
	DWORD BytesWritten = 0;
	DWORD StrLength = (DWORD) strlen( Buffer );
	OutputDebugString( Buffer );
	WriteFile( hFile, Buffer, StrLength, &BytesWritten, NULL );
	printf( Buffer );
}

char *StripTrailingWhitespace( char *Buffer )
{
	char *LastValidChar = Buffer;
	char *Curr = Buffer;
	while ( *Curr )
	{
		if ( !isspace(*Curr) )
		{
			LastValidChar = Curr;
		}
		++Curr;
	}
	LastValidChar[1] = 0;
	return Buffer;
}


int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE LogFile;
	char CmdLine[1024];
	char ElfName[256];
	char XElfName[256];
	_TCHAR Buffer[1024 * 2];
	DWORD BufferSize = sizeof(Buffer);
	clock_t MaxSeconds = DEFAULT_MAXSECONDS;

	if ( argc < 2 )
	{
		printf( "Usage 1:  testbedps3 <fname.elf> [timeout-secs]\n"
				"Usage 2:  testbedps3 <optionfname.opt>\n"
				"\n"
			    "Ex 1:     testbedps3 debug-examplegame.elf 120\n"
				"Ex 2:     testbedps3 myoptions.opt\n"
				"\n"
				"Note:     Default timeout is 30 seconds. (If 0, testbedps3 will run indefinately.)\n"
				"          Map and arguments are specified in PS3\\PS3CommandLine.txt.\n"
				"          Remember to turn on dev-kit and have SNCLM and TM running.\n"
				"          Have either the desired Target already connected in TM, or set the\n"
				"          PS3TARGET environment variable (see SN docs).\n"
				"          Beware orphaned processes named \"ps3run.exe\"... :(\n"
				"\n"
				"Returns:  0 if succesful run (\"PS3Progress_Running\" detected)\n"
				"          1 on assert\n"
				"          2 on crash\n"
				"          3 on timeout\n"
				"          4 on internal PS3 connection error\n"
				"          5 on internal logfile error\n"
				"\n"
				"Option file format:\n"
				"          <fname.elf>\n"
				"          <commandline>\n"
				"          <timeout-secs>\n"
				);
		return 0;
	}

	LogFile = CreateFile( "TestbedPS3.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( LogFile == NULL )
	{
		return PS3ERROR_LOGFILE;
	}

	// Check for Usage 2:
	size_t FileNameLength = strlen( argv[1] );
	if ( FileNameLength > 4 && stricmp( argv[1] + FileNameLength - 4, ".opt" ) == 0 )
	{
		FILE *OptFile = fopen( argv[1], "rt" );
		FILE *CmdFile = fopen( "PS3CommandLine.txt", "wt" );
		if ( !OptFile || !CmdFile )
		{
			Dump( LogFile, "[TestbedPS3] Failed to open option file (\"" );
			Dump( LogFile, argv[1] );
			Dump( LogFile, "\") or create PS3CommandLine.txt!\n" );
			CloseHandle( LogFile );
			if( CmdFile != NULL )
			{
				fclose( CmdFile );
			}

			if( OptFile != NULL )
			{
				fclose( OptFile );
			}

			return PS3ERROR_LOGFILE;
		}
		fgets( ElfName, sizeof(ElfName), OptFile );
		StripTrailingWhitespace( ElfName );
		fgets( Buffer, BufferSize, OptFile );
		fprintf( CmdFile, Buffer );
		fgets( Buffer, BufferSize, OptFile );
		fclose( CmdFile );
		fclose( OptFile );
		int SpecifiedMaxSeconds = atoi( Buffer );
		if ( SpecifiedMaxSeconds >= 0 )
		{
			MaxSeconds = SpecifiedMaxSeconds;
		}
	}
	else
	{
		if ( argc >= 3 )
		{
			int SpecifiedMaxSeconds = atoi( argv[2] );
			if ( SpecifiedMaxSeconds >= 0 )
			{
				MaxSeconds = SpecifiedMaxSeconds;
			}
		}
		strcpy( ElfName, argv[1] );
	}

	// Deduce the *.xelf filename (for symbol lookups).
	{
		char Drive[ _MAX_DRIVE ], Dir[ _MAX_DIR ], Fname[ _MAX_FNAME ], Ext[ _MAX_EXT ];

		_splitpath( ElfName, Drive, Dir, Fname, Ext );
		sprintf( XElfName, "%s%s%s.xelf", Drive, Dir, Fname );
	}

	UCommandlet Addr2Line;
	UCommandlet PS3Run;
	sprintf( CmdLine, "ps3run -nd -p -r %s", ElfName );

	if ( !PS3Run.Create( CmdLine ) )
	{
		Dump( LogFile, "[TestbedPS3] Failed to create ps3run process\n" );
		CloseHandle( LogFile );
		return PS3ERROR_PS3;
	}

	bool bProcessComplete = false;
	bool bPS3Connected = false;
	DWORD ErrorCode = 0;
	clock_t TimeStart = clock();
	clock_t TimeThreshhold = TimeStart + MaxSeconds * CLOCKS_PER_SEC;

	// wait until the precompiler is finished
	while (!bProcessComplete )
	{
		DWORD Reason = PS3Run.Poll( 100 );
		const char *LineData = PS3Run.ReadLine();
		while ( LineData && !bProcessComplete )
		{
			const char *Str1 = strstr( LineData, "[PS3Callstack] " );
			const char *Str2 = strstr( LineData, "lv2(2): #   " );
			if ( Str1 || Str2 )
			{
				if ( !Addr2Line.IsCreated() )
				{
					char CmdLine2[1024];
					sprintf( CmdLine2, "ppu-lv2-addr2line -f -s -C -e %s", XElfName );
					if ( !Addr2Line.Create( CmdLine2 ) )
					{
						Dump( LogFile, "[TestbedPS3] Failed to create ppu-lv2-addr2line process\n" );
						CloseHandle( LogFile );
						return PS3ERROR_ASSERT;
					}
				}
				const char *Address = Str1 ? (Str1+15) : (Str2+12);
				Addr2Line.Write( Address );
				Addr2Line.Write( "\n" );
				const char *FunctionName = Addr2Line.ReadLine( 5000 );
				const char *FileRef = Addr2Line.ReadLine( 5000 );
				Dump( LogFile, LineData );
				Dump( LogFile, "  = " );
				Dump( LogFile, FunctionName ? FunctionName : "???" );
				Dump( LogFile, " [" );
				Dump( LogFile, FileRef ? FileRef : "??:0" );
				Dump( LogFile, "]\n" );
			} else
			{
				Dump( LogFile, LineData );
				Dump( LogFile, "\n" );

				if ( strstr( LineData, "invalid access address 0x3!" ) != NULL )
				{
					Dump( LogFile, "[TestbedPS3] Assertion failed!\n" );
					ErrorCode = PS3ERROR_ASSERT;
					bProcessComplete = TRUE;
				}
				else if ( strstr( LineData, "lv2(2): # Register Info." ) != NULL )
				{
					Dump( LogFile, "[TestbedPS3] Crash detected!\n" );
					ErrorCode = PS3ERROR_CRASH;
					bProcessComplete = TRUE;
				}
				else if ( MaxSeconds > 0 && strstr( LineData, "PS3Progress_Running" ) != NULL )
				{
					Dump( LogFile, "[TestbedPS3] Successful run detected!\n" );
					ErrorCode = 0;
					bProcessComplete = TRUE;
				}
				else if ( !bPS3Connected && strncmp( LineData, "lv2(", 4 ) == NULL )
				{
					bPS3Connected = TRUE;
				}
			}

			LineData = PS3Run.ReadLine();
		}

		if (Reason == WAIT_OBJECT_0)		// when the process signals, its done
		{
			if ( bPS3Connected )
			{
				Dump( LogFile, "[TestbedPS3] PS3RUN.EXE command line process terminated too early! The game wrote Ctrl-Z to abort PS3RUN?\n" );
			}
			else
			{
				Dump( LogFile, "[TestbedPS3] PS3RUN.EXE command line process failed! Couldn't connect to PS3?\n" );
				Dump( LogFile, "[TestbedPS3] Try: " );
				Dump( LogFile, CmdLine );
				Dump( LogFile, "\n" );
			}
			ErrorCode = PS3ERROR_PS3;
			bProcessComplete = TRUE;		// break out of the loop
		}
		else if ( !bProcessComplete && clock() > TimeThreshhold && MaxSeconds > 0 )
		{
			Dump( LogFile, "[TestbedPS3] Time's up! Shutting down...\n" );
			ErrorCode = PS3ERROR_TIMEOUT;
			bProcessComplete = TRUE;
		}
	}

	// Shut down the process
	if ( ErrorCode != PS3ERROR_PS3 && !PS3Run.Terminate() )
	{
		DWORD WinError = GetLastError();
		FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, WinError, 0, Buffer, BufferSize, NULL );
		Dump( LogFile, "[TestbedPS3] Windows error while shutting down PS3RUN.EXE: " );
		Dump( LogFile, Buffer );
	}

	// Close handles
	CloseHandle(LogFile);

	return ErrorCode;
}
