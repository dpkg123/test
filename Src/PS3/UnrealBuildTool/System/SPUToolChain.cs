/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool
{
	class SPUToolChain
	{
		static string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			// disable execptions
			Result += " -fno-exceptions";

			// position independent, for SPURS
			Result += " -fpic";

			Result += " -c";

			// Compiler warnings.
			Result += " -Wall";

			// Optimize non- debug builds.
			if (CompileEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug)
			{
				// full optimizations without inlining (this is what the projects use, we do want small code)
				Result += " -O2";
			}

			// Create GDB format debug info if wanted.
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -g";
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
			Result += " -x c";
			return Result;
		}

		static string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

//			Result += " -fpic";
//			Result += " -Wl,-q";
//			Result += " -nostartfiles";
//			Result += " -Ttext=0x0";
			Result += " -Wl,--gc-sections";

			return Result;
		}

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
		{
			string Arguments = GetCompileArguments_Global(CompileEnvironment);

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

				// Add the source file path to the command-line.
				FileArguments += string.Format(" {0}", SourceFile.AbsolutePath);

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/spu/bin/spu-lv2-g++.exe");
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.bIsGCCCompiler = true;
				CompileAction.bCanExecuteRemotely = false;
			}
			return Result;
		}

		public static FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bIsSpursTask)
		{
			// Create an action that invokes the linker.
			Action LinkAction = new Action();
			LinkAction.bIsLinker = true;
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = Path.Combine(UE3BuildTarget.GetSCEPS3Root(), "host-win32/spu/bin/spu-lv2-g++.exe");
			LinkAction.CommandArguments = GetLinkArguments_Global(LinkEnvironment);

			if (bIsSpursTask)
			{
				LinkAction.CommandArguments += " -mspurs-task";
			}
			else
			{
				LinkAction.CommandArguments += " -mspurs-job";
			}

			// Add the library paths to the argument list.
			foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
			{
				LinkAction.CommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
			}


			LinkAction.CommandArguments += " -Wl,--start-group";

			// Add the additional libraries to the argument list.
			foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				LinkAction.CommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
			}

			// Add the input files 
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				LinkAction.CommandArguments += string.Format(" \"{0}\"", InputFile.AbsolutePath);
				LinkAction.PrerequisiteItems.Add(InputFile);
			}
			LinkAction.CommandArguments += " -Wl,--end-group";

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));

			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}
	};
}
