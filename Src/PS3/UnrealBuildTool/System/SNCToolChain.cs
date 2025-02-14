/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool
{
	class SNCToolChain
	{
		static string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			Result += " -c";
			Result += " -DPS3_SNC=1";

			// report warnings as errors (by returning a failed value)
			Result += " -Xquit=1";

			// disble specific warnings that are hopeless to fix :)
			Result += " --diag_suppress=237";   // suppress "controlling expression is constant"
			Result += " --diag_suppress=1700";  // suppress "'x' has virtual functions but non-virtual destructor" (could be hiding a bug, but for the time being...)
			Result += " --diag_suppress=1724";  // suppress "routine is both inline and noinline" (inline function with attribute noinline)
			Result += " --diag_suppress=613";   // suppress "partially overridden virtual" (missing using directive, but probably harmless)
			Result += " --diag_suppress=1011";  // suppress "'x' is hidden by 'y' -- virtual function override intended?"
			Result += " --diag_suppress=178";   // suppress "variable 'x' was declared but never referenced". P_GET_STRUCT_REF is causing lots of these. 
			Result += " --diag_suppress=187";   // suppress "pointless comparison of unsigned integer with zero". Harmless.
			Result += " --diag_suppress=552";   // suppress variable 'X' was set but never used
			Result += " --diag_suppress=186";   // suppress dynamic initialization in unreachable code
			Result += " --diag_suppress=112";   // suppress statement is unreachable
			if (PS3ToolChain.SNCompilerVersion >= 330)
			{
				Result += " --diag_suppress=1787";  // suppress 'x' is not a valid value for the enumeration type "Y"
			}

			// enable this again maybe when SNC has usefully named vars instead of stuff like 1777342|X
			Result += " --diag_suppress=1669"; // suppress "Potential uninitialized reference to XXX in function YYY"
			Result += " --diag_warning=1774";  // enable this warning:  constructor list order verification

            // remove all the TOC mumbo jumbo, requires --notocrestore on the linker
            Result += " -Xnotocrestore=2";

            // Optimize non- debug builds.
			if( CompileEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug )
			{
//				Result += " -Os";

				// reorder is an undocumented (as of 270) optimization. It should be ready for primetime in 280, 
				// however it currently does not work with -Os
				Result += " -O3 -Xreorder=1";

				// this makes assumptions about how signed and unsigned number play together (including pointers stored in INTs, etc)
				// we have seen no problems, but it is theoretically possible they can introduce bugs
				// NOTE: It actually causes a bug in the Min<DWORD> specialization!
				Result += " -Xassumecorrectsign=1";

				Result += " -Xassumecorrectalignment=1";

				// enable this flag for the Call Prof Tuner captures (like the Xbox Callcap stuff)
				//Result += " -Xcallprof";
				
				// disable RTTI
				Result += " -Xc-=rtti";

				// enable auto-VMXification of code, and other math speedups (which are slightly less precise)
				Result += " -Xfastmath=1";

				// 
//				Result += " --pch_messages --pch_verbose";
			}

			// Create GDB format debug info if wanted.
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -g";

				// This debug info will be enabled by default in SNC in the future.
				// It lets the debugger to identify the correct object type based on the vtable pointer.
				Result += " -Xdebugvtbl";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			return Result;
		}

		static string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			// Use a wrapper function for these symbols.
            Result += " -wrap=malloc";
            Result += " -wrap=free";
            Result += " -wrap=memalign";
            Result += " -wrap=calloc";
            Result += " -wrap=realloc";
            Result += " -wrap=reallocalign";

			// make sure all exception data is gone
			Result += " --no-exceptions";

			// strip unused code and data
			Result += " --strip-unused-data";

            // don't warn about Sony's libs not having the sources stripped (out of our control)
            Result += " --disable-warning=0134";
            
			// strip-duplicates will munge up the debug info so don't do it for debug builds
			if (LinkEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug)
			{
				Result += " --strip-duplicates";
			}

            Result += " --notocrestore --no-multi-toc";

            // use libcs - faster versions of libc functions, but bigger code
			// However, appMemzero() will sometimes crash.
//             Result += " --use-libcs";

			// write directly to encrypted elf format (requires update PS3Tools to be able to parse crallstacks)
			Result += " --oformat=fself";


			// work around the following linker error that some people get, seemingly randomly:
			//		error: L0170: Line number info has expanded whilst stripping unused function info in <...>
			Result += " --pad-debug-line=1";

			return Result;
		}

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment,List<FileItem> SourceFiles)
		{
			string Arguments = GetCompileArguments_Global(CompileEnvironment);

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				Arguments += string.Format(
					" -I\"{0}\"",
					Path.GetDirectoryName(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath)
					);
			}

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.IncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}

			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D\"{0}\"", Definition);
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action();
				string FileArguments = "";

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);
				foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
				{
					CompileAction.PrerequisiteItems.Add(IncludedFile);
				}

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
                    // use --create_pch/--use_pch for Unity build, --pch for non-Unity
                    if (BuildConfiguration.bUseUnityBuild)
                    {
                        // Generate a CPP File that just includes the precompiled header.
                        string PCHCPPFilename = Path.GetFileName(CompileEnvironment.PrecompiledHeaderIncludeFilename) + ".PCH.cpp";
                        string PCHCPPPath = Path.Combine(CompileEnvironment.OutputDirectory, PCHCPPFilename);
                        FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
                            PCHCPPPath,
                            string.Format("#include \"{0}\"\r\n", CompileEnvironment.PrecompiledHeaderIncludeFilename)
                            );

                        // Add the precompiled header file to the produced item list.
                        FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
                            Path.Combine(
                                CompileEnvironment.OutputDirectory,
                                Path.GetFileName(SourceFile.AbsolutePath) + ".pch"
                                )
                            );

                        CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
                        Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

                        // Add the parameters needed to compile the precompiled header file to the command-line.
                        FileArguments += string.Format(" --create_pch=\"{0}\"", PrecompiledHeaderFile.AbsolutePath);
                        FileArguments += string.Format(" \"{0}\"", PCHCPPFile.AbsolutePath);
                        FileArguments += string.Format(" --pch_verbose --pch_messages");
                    }
                    else if (CompileEnvironment.ShouldUsePCHs())
                    {
                        FileArguments += string.Format(" --pch");
                        FileArguments += string.Format(" --pch_verbose --pch_messages");
                    }
				}
				else
				{
                    // use --create_pch/--use_pch for Unity build, --pch for non-Unity
                    if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
                    {
                        if (BuildConfiguration.bUseUnityBuild)
                        {
                            CompileAction.bIsUsingPCH = true;
                            CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
                            FileArguments += string.Format(" --use_pch=\"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath);
                            FileArguments += string.Format(" --pch_verbose --pch_messages");
                        }
					}
                    else if (CompileEnvironment.ShouldUsePCHs())
                    {
                        FileArguments += string.Format(" --pch");
                        FileArguments += string.Format(" --pch_verbose --pch_messages");
					}
                    FileArguments += string.Format(" {0}", SourceFile.AbsolutePath);
				}

				// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByPath(
					Path.Combine(
						CompileEnvironment.OutputDirectory,
						Path.GetFileName(SourceFile.AbsolutePath) + ".o"
						)
					);
				CompileAction.ProducedItems.Add(ObjectFile);
				Result.ObjectFiles.Add(ObjectFile);
				FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath);

				if (Path.GetExtension(SourceFile.AbsolutePathUpperInvariant) == ".C")
				{
					// Compile the file as C code.
					FileArguments += GetCompileArguments_C();
				}
				else
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();
				}

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/sn/bin/ps3ppusnc.exe");
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
//Console.WriteLine("{0}", CompileAction.CommandArguments);
                CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
                CompileAction.StatusDetailedDescription = SourceFile.Description;
//				CompileAction.bIsGCCCompiler = true;

				// Allow compiles to execute remotely unless they create a precompiled header.  Force those to run locally to avoid the
				// need to copy the PCH back to the build instigator before distribution to other remote build agents.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create;
			}
			return Result;
		}

		public static FileItem LinkFiles(LinkEnvironment LinkEnvironment)
		{
			// Create an action that invokes the linker.
			Action LinkAction = new Action();
			LinkAction.bIsLinker = true;
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/sn/bin/ps3ppuld.exe");
			LinkAction.CommandArguments = GetLinkArguments_Global(LinkEnvironment);

			// Add the library paths to the argument list.
			foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
			{
				LinkAction.CommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
			}

			// Add the additional libraries to the argument list.
			foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				LinkAction.CommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file.
			string ResponseFileName = Path.Combine( LinkEnvironment.OutputDirectory, Path.GetFileName( LinkEnvironment.OutputFilePath ) + ".response" );
			LinkAction.CommandArguments += string.Format( " @\"{0}\"", ResponseFile.Create( ResponseFileName, InputFileNames ) );

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// generate the .map file for Shipping, since it's needed for submission
			if( LinkEnvironment.TargetConfiguration == CPPTargetConfiguration.Shipping )
			{
				LinkAction.CommandArguments += string.Format(" --Map=\"{0}\".map", OutputFile.AbsolutePath);
			}

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}
	};
}
