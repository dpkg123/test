/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */



#ifndef COMMANDLET_H
#define COMMANDLET_H


class UCommandlet
{
public:
	UCommandlet();
	~UCommandlet();

	bool			IsCreated( );
	bool			Create( char *CmdLine );
	bool			Terminate( );
	DWORD			Poll( DWORD Timeout );			// Timeout in millisec

	const char *	ReadLine( DWORD Timeout=0 );	// Timeout in millisec
	const char *	ReadAll( );
	void			Write( const char *String );

protected:
	enum { BUFFERSIZE=4*1024 };
	void			Init();
	char *			GetNextLine( );
	const char *	ReadLineInternal();

	PROCESS_INFORMATION	ProcInfo;
	HANDLE			StdInRead, StdInWrite;
	HANDLE			StdOutRead, StdOutWrite;
	char			Buffer[BUFFERSIZE];
	DWORD			BufferStart;	// Index to beginning of un-read content
	DWORD			BufferEnd;		// Index to end of un-read content (usually pointing to a nul char).
};


#endif
