/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
 #pragma once

#include "SymbolParser.h"
#include "CommandLineWrapper.h"

namespace StandaloneSymbolParser
{
	/**
	 * Class for loading Android symbols.
	 */
	public ref class AndroidSymbolParser : public SymbolParser
	{
	private:
		FCommandLineWrapper *CmdLineProcess;

		String^ ExecutablePath;

	protected:
		/**
		 * Finalizer frees native memory.
		 */
		!AndroidSymbolParser(); // finalizer

	public:
		/**
		 * Constructor.
		 */
		AndroidSymbolParser();

		/**
		 * Converted to virtual void Dispose().
		 */
		virtual ~AndroidSymbolParser(); // Dispose()

		/**
		 * Loads symbols for an executable.
		 *
		 * @param	ExePath		The path to the executable whose symbols are going to be loaded.
		 * @param	Modules		The list of modules loaded by the process.
		 */
		virtual bool LoadSymbols(String ^ExePath, array<ModuleInfo^> ^/*Modules*/) override;

		/**
		 * Unloads any currently loaded symbols.
		 */
		virtual void UnloadSymbols() override;

		/**
		 * Retrieves the symbol info for an address.
		 *
		 * @param	Address			The address to retrieve the symbol information for.
		 * @param	OutFileName		The file that the instruction at Address belongs to.
		 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
		 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
		 * @return	True if the function succeeds.
		 */
		virtual bool ResolveAddressToSymboInfo(unsigned __int64 Address, String ^%OutFileName, String ^%OutFunction, int %OutLineNumber) override;

		/**
		 * Creates a formatted line of text for a callstack.
		 *
		 * @param	FileName		The file that the function call was made in.
		 * @param	Function		The name of the function that was called.
		 * @param	LineNumber		The line number the function call occured on.
		 *
		 * @return	A string formatted using "{0} [{1}:{2}]\r\n" as the template.
		 */
		virtual String^ FormatCallstackLine(String ^FileName, String ^Function, int LineNumber) override;
	};
}
