/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool
{
	class PS3ToolChain
	{
		/** The version of the SN Compiler */
		public static int SNCompilerVersion;

        /** The version of the PS3 SDK, as read from version-SDK */
        public static int SDKVersion;

        static string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			Result += " -fmessage-length=0";
			Result += " -fshort-wchar";
			Result += " -fno-exceptions";
			Result += " -pipe";
			Result += " -c";

			// Compiler warnings.
			Result += " -Wcomment";
			Result += " -Wmissing-braces";
			Result += " -Wparentheses";
			Result += " -Wredundant-decls";
			Result += " -Werror";
			Result += " -Winvalid-pch";
			Result += " -Wno-redundant-decls";
			Result += " -Wreturn-type";
			Result += " -Winit-self";

			// Optimize non- debug builds.
			if (CompileEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug)
			{
				Result += " -fno-strict-aliasing";
				Result += " -O3";
				Result += " -fforce-addr";
				Result += " -ffast-math";
				Result += " -funroll-loops";
				Result += " -Wuninitialized";
			}

			// Create GDB format debug info if wanted.
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -g";
				Result += " -ggdb";
			}

			// enable if you want callprof support in Tuner
			//Result += " -mcallprof";

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -Wreorder";
			Result += " -Wno-non-virtual-dtor";
			Result += " -fcheck-new";
			Result += " -fno-rtti";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			Result += " -ffunction-sections";
			Result += " -fdata-sections";
			return Result;
		}

		static string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			// Use a wrapper function for these symbols.
			Result += " -Wl,--wrap,malloc";
			Result += " -Wl,--wrap,free";
			Result += " -Wl,--wrap,memalign";
			Result += " -Wl,--wrap,calloc";
			Result += " -Wl,--wrap,realloc";
			Result += " -Wl,--wrap,reallocalign";

			Result += " -Wl,--strip-unused-data";

			// strip-duplicates will munge up the debug info so don't do it for debug builds
			if (LinkEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug)
			{
				Result += " -Wl,--strip-duplicates";
			}

			// write directly to encrypted elf format (requires update PS3Tools to be able to parse callstacks)
			Result += " -Wl,--oformat=fself";

            // use libcs - faster versions of libc functions, but bigger code
			// However, appMemzero() will sometimes crash.
//            Result += " -mfast-libc";

			return Result;
		}

		static string GetObjCopyArguments_Global()
		{
			string Result = "";

			Result += " -I binary";
			Result += " -O elf64-powerpc-celloslv2";
			Result += " -B powerpc";
			Result += " --set-section-align .data=7";
			Result += " --set-section-pad .data=128";

			return Result;
		}

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
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
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ".gch"
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", PrecompiledHeaderFile.AbsolutePath);
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
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
				}

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

				// Add the source file path to the command-line.
				FileArguments += string.Format(" {0}", SourceFile.AbsolutePath);

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/ppu/bin/ppu-lv2-g++.exe");
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.bIsGCCCompiler = true;

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
			LinkAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/ppu/bin/ppu-lv2-g++.exe");			
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

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		public static FileItem CreateObjectFileFromDataFile(FileItem InputDataFile, string OutputObjectFilePath, string SectionName)
		{
			// Create an action that invokes ppu-lv2-objcopy.
			// The action's working directory is the directory containing the input data file, so the directory path isn't included
			// in the mangled symbol name that is used to refer to the data.
			Action ObjCopyAction = new Action();
			ObjCopyAction.WorkingDirectory = Path.GetDirectoryName(InputDataFile.AbsolutePath);
			ObjCopyAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/ppu/bin/ppu-lv2-objcopy.exe");
			ObjCopyAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputObjectFilePath));
			ObjCopyAction.bCanExecuteRemotely = false;

			ObjCopyAction.CommandArguments = GetObjCopyArguments_Global();

			// Rename the data section to the specified name.
			ObjCopyAction.CommandArguments += string.Format(" --rename-section .data={0}", SectionName);

			// Specify the input file.
			ObjCopyAction.CommandArguments += string.Format(" {0}", Path.GetFileName(InputDataFile.AbsolutePath));
			ObjCopyAction.PrerequisiteItems.Add(InputDataFile);

			// Specify the output object file.
			FileItem OutputObjectFile = FileItem.GetItemByPath(OutputObjectFilePath);
			ObjCopyAction.CommandArguments += string.Format(" {0}", OutputObjectFile.AbsolutePath);
			ObjCopyAction.ProducedItems.Add(OutputObjectFile);

			return OutputObjectFile;
		}

		public static FileItem StripDebugInfoFromExecutable(FileItem InputFile, string OutputFilePath)
		{
			// Create an action that invokes ps3bin -sa
			Action StripAction = new Action();
			StripAction.WorkingDirectory = Path.GetFullPath(".");
			StripAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/sn/bin/ps3bin");
			StripAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFilePath));
			StripAction.bCanExecuteRemotely = false;

			// Strip all debug info and symbols.
			StripAction.CommandArguments = " -sa";

			// Specify the input file.
			StripAction.CommandArguments += string.Format(" -i {0}", InputFile.AbsolutePath);
			StripAction.PrerequisiteItems.Add(InputFile);

			// Specify the output object file.
			FileItem TmpFile = FileItem.GetItemByPath(OutputFilePath + ".tmp");
			StripAction.CommandArguments += string.Format(" -o {0}", TmpFile.AbsolutePath);
			StripAction.ProducedItems.Add(TmpFile);
			
			return EncryptExecutable(TmpFile, OutputFilePath, false, true);
		}

		public static FileItem EncryptExecutable(FileItem InputFile, string OutputFilePath, bool bUseNPDRM, bool bShouldDeleteInput)
		{
			// Create an action that invokes make_fself.
			Action EncryptAction = new Action();
			EncryptAction.WorkingDirectory = Path.GetFullPath(".");
			EncryptAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), bUseNPDRM ? "host-win32/bin/make_fself_npdrm.exe" : "host-win32/bin/make_fself.exe");
			EncryptAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFilePath));
			EncryptAction.bCanExecuteRemotely = false;

			// Specify the input file.
			EncryptAction.CommandArguments += string.Format(" {0}", InputFile.AbsolutePath);
			EncryptAction.PrerequisiteItems.Add(InputFile);
			EncryptAction.bShouldDeletePrereqs = bShouldDeleteInput;

			// Specify the output object file.
			FileItem OutputFile = FileItem.GetItemByPath(OutputFilePath);
			EncryptAction.CommandArguments += string.Format(" {0}", OutputFile.AbsolutePath);
			EncryptAction.ProducedItems.Add(OutputFile);

			return OutputFile;
		}
	};
}
