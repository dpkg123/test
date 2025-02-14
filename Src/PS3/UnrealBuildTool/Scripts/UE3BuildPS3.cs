/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	enum PS3GCMType
	{
		Release,
		Debug,
		HUD
	}

	partial class UE3BuildTarget
	{
		/** True if Bink SPU libs are present */
		private bool bHasBinkSPU;

        ///////////////////////////
		// NOTE:
		//	 The following SPUJobNames array is used to specify what SPU job projects you want to compile
		//   However, since the PPU needs to load the offsets for the jobs at runtime, you will need to
		//   modify the code in PS3LoadSPUJobs() that gets the offsets and sizes for the SPU jobs.
		//   
		//   The capitalization in _this_ list is very important, it will determine the name of the symbol
		//   that needs to be extern'd in C++ to load the start/size of the SPU program symbols
		//
		//   Since Bink is optional (depends on if you have access or not), we programmatically append 
		//   that to the end (so it will always be the last one after the end of this list)
		///////////////////////////

		/** List of SPU jobs to compile. See note above, as you must fix up PS3 runtime code */
		static string[] SPUJobNames =
		{
			"PixelshaderPatching",
			"WriteFence",
			"LandscapeSPU",
		};
		


		/** Returns a valid PS3 SDK root directory path, or throws a BuildException. */
		public static string GetSCEPS3Root()
		{
			string MoreInfoString =
				"See https://udn.epicgames.com/Three/GettingStartedPS3 for help setting up the UE3 PS3 compilation environment";

			// Read the root directory of the PS3 SDK from the environment.
			string SCEPS3RootEnvironmentVariable = Environment.GetEnvironmentVariable("SCE_PS3_ROOT");

			// Check that the environment variable is defined
			if (SCEPS3RootEnvironmentVariable == null)
			{
				throw new BuildException(
					"SCE_PS3_ROOT environment variable isn't set; you must properly install the PS3 SDK before building UE3 for PS3.\n" +
					MoreInfoString
					);
			}

			// Check that the environment variable references a valid directory.
			if (!Directory.Exists(SCEPS3RootEnvironmentVariable))
			{
				throw new BuildException(
					string.Format(
						"SCE_PS3_ROOT environment variable is set to a non-existant directory: {0}\n",
						SCEPS3RootEnvironmentVariable
						) +
					MoreInfoString
					);
			}

			return SCEPS3RootEnvironmentVariable;
		}



		bool PS3SupportEnabled()
		{
			return true;
		}

		private FileItem CompileSPUJob(string JobName, CPPEnvironment SPUCPPEnvironment, LinkEnvironment SPULinkEnvironment)
		{
			//////////////////////////////////////////////////////////////////////////
			// Compile the elf
			//////////////////////////////////////////////////////////////////////////

			// remove any previous linker's input
			SPULinkEnvironment.InputFiles.Clear();

			bool bIsSpecialBinkTask = JobName == "Bink";
			bool bIsSpursTask = false;

			// handle Bink lib specially, since it doesn't have a project with .cpp files
			if (bIsSpecialBinkTask)
			{
				// no .cpp, just need to link the Bink SPU libs
				SPULinkEnvironment.InputFiles.Add(FileItem.GetItemByPath("..\\External\\Bink\\lib\\PS3\\libBinkspu.a"));
				SPULinkEnvironment.InputFiles.Add(FileItem.GetItemByPath("..\\External\\Bink\\lib\\PS3\\libBinkspu_spurs.a"));
				bIsSpursTask = true;
			}
			else
			{
				// compile the job project, assumes that it's in PS3\SPU\<jobname>\<jobname>.vcproj
				List<UE3ProjectDesc> SPUJobProjects = new List<UE3ProjectDesc>();
				SPUJobProjects.Add(new UE3ProjectDesc(string.Format("PS3/SPU/{0}/{0}.vcproj", JobName)));
				SPULinkEnvironment.OutputFilePath = Path.Combine(SPUCPPEnvironment.OutputDirectory, string.Format("{0}.elf", JobName));
				Utils.CompileProjects(SPUCPPEnvironment, SPULinkEnvironment, SPUJobProjects);
			}

			SPULinkEnvironment.OutputFilePath = Path.Combine(SPUCPPEnvironment.OutputDirectory, string.Format("{0}.elf", JobName));

			// link the executable (bink doesn't need start, it's already in the lib)
			FileItem ElfFile = SPUToolChain.LinkFiles(SPULinkEnvironment, bIsSpursTask);

			//////////////////////////////////////////////////////////////////////////
			// elf -> obj
			//////////////////////////////////////////////////////////////////////////

			Action MakeObjAction = new Action();
			MakeObjAction.WorkingDirectory = SPUCPPEnvironment.OutputDirectory;
			MakeObjAction.CommandPath = Path.Combine(GetSCEPS3Root(), "host-win32/bin/spu_elf-to-ppu_obj.exe");
			MakeObjAction.StatusDescription = string.Format("MakeObj {0}", Path.GetFileName(SPULinkEnvironment.OutputFilePath));
			MakeObjAction.bCanExecuteRemotely = false;

			// params (SDK 310 and above needs normal strip mode specified, or Bink at least will fail)
            if (PS3ToolChain.SDKVersion < 310)
            {
                MakeObjAction.CommandArguments = string.Format("--symbol-name={0}", JobName);
            }
            else
            {
                MakeObjAction.CommandArguments = string.Format("--symbol-name={0} --strip-mode=normal", JobName);
            }

			// Specify the input file.
			MakeObjAction.CommandArguments += string.Format(" \"{0}\"", ElfFile.AbsolutePath);
			MakeObjAction.PrerequisiteItems.Add(ElfFile);

			// Specify the output object file.
			FileItem ObjFile = FileItem.GetItemByPath(Path.Combine(SPUCPPEnvironment.OutputDirectory, string.Format("{0}.ppu.obj", JobName)));
			MakeObjAction.CommandArguments += string.Format(" \"{0}\"", ObjFile.AbsolutePath);
			MakeObjAction.ProducedItems.Add(ObjFile);

			return ObjFile;
		}

		private void CompileSPUJobs()
		{
			// now we need to make sure the SPUJobs.sprx is up to date and compiled

			// A CPP environment for compiling the SPUJobs.sprx
			CPPEnvironment SPUCPPEnvironment = new CPPEnvironment();

			// A Link environment for linking the SPUJobs.sprx
			LinkEnvironment SPULinkEnvironment = new LinkEnvironment();
				
			SPUCPPEnvironment.TargetPlatform = CPPTargetPlatform.PS3_SPU;
			SPUCPPEnvironment.OutputDirectory = GlobalCPPEnvironment.OutputDirectory;
			SPUCPPEnvironment.TargetConfiguration = CPPTargetConfiguration.Release;
			SPUCPPEnvironment.Definitions.Add("SPU");
			SPUCPPEnvironment.IncludePaths.Add("PS3/SPU/Inc");

			SPULinkEnvironment.TargetPlatform = CPPTargetPlatform.PS3_SPU;
			SPULinkEnvironment.LibraryPaths.Add("$(SCE_PS3_ROOT)/target/spu/lib");
			SPULinkEnvironment.AdditionalLibraries.Add("spurs");
			SPULinkEnvironment.AdditionalLibraries.Add("gcm_spu");
			SPULinkEnvironment.AdditionalLibraries.Add("edgegeom");
			SPULinkEnvironment.AdditionalLibraries.Add("dma");

			// Validates current settings and updates if required.
			BuildConfiguration.ValidateConfiguration(SPUCPPEnvironment.TargetConfiguration, SPUCPPEnvironment.TargetPlatform);
			bool bSavedUseUnity = BuildConfiguration.bUseUnityBuild;
			BuildConfiguration.bUseUnityBuild = false;

			
			List<string> FinalSPUJobNames = new List<string>(SPUJobNames);

			// add Bink SPU job if the libs are there
			if (bHasBinkSPU)
			{
				FinalSPUJobNames.Add("Bink");
			}

			// collect the output objs (.cpp -> .elf -> .bin -> .obj)
			List<FileItem> SPUJobObjs = new List<FileItem>();

			// compile the files in each SPU project, and add the .obj file to the list of inputs to the PPU link step
			foreach (string JobName in FinalSPUJobNames)
			{
				FileItem PPUObjFile = CompileSPUJob(JobName, SPUCPPEnvironment, SPULinkEnvironment);
				FinalLinkEnvironment.InputFiles.Add(PPUObjFile);

			}
						
			BuildConfiguration.bUseUnityBuild = bSavedUseUnity;
		}

		void SetUpPS3Environment()
		{
			// look to see if we have the SPU version of bink
			bHasBinkSPU = File.Exists("../External/Bink/lib/PS3/libBinkPPU_Spurs.a");

			// needed for non-english machine so that parsing the SDK version below is correct
			System.Threading.Thread.CurrentThread.CurrentCulture = new System.Globalization.CultureInfo("en-US");

			// find the SN Compiler version
			FileVersionInfo SNVersion = FileVersionInfo.GetVersionInfo( Path.Combine( GetSCEPS3Root(), "host-win32\\sn\\bin\\ps3ppusnc.exe" ) );
			PS3ToolChain.SNCompilerVersion = SNVersion.FileMajorPart;

			if( PS3ToolChain.SNCompilerVersion == 330 )
			{
				Console.WriteLine( "Version 330 of the SN Compiler has known compile issues; using 310 is recommended" );
			}

			// find what SDK we are compiling with, and set it in the toolchain
            string SDKString = File.ReadAllText(Path.Combine(GetSCEPS3Root(), "version-SDK"));
            PS3ToolChain.SDKVersion = (int)float.Parse(SDKString);
 
            // disable Move if not supported in the current SDK
            if (PS3ToolChain.SDKVersion < 330 && UE3BuildConfiguration.bCompilePS3Move)
            {
                Console.WriteLine("Disabling Move support, SDK needs to be at least 330");

                UE3BuildConfiguration.bCompilePS3Move = false;
            }

            // tell user what SDK they compiled with
            Console.WriteLine("Compiling PS3 with SDK version {0}", PS3ToolChain.SDKVersion);

			// make sure that the SPU job .obj files are up to date
			CompileSPUJobs();
					
			GlobalCPPEnvironment.Definitions.Add("PS3_NO_TLS=0");
			GlobalCPPEnvironment.Definitions.Add("PS3=1");
			GlobalCPPEnvironment.Definitions.Add("_PS3=1");
			GlobalCPPEnvironment.Definitions.Add("REQUIRES_ALIGNED_ACCESS=1");
			GlobalCPPEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=1");

			GlobalCPPEnvironment.IncludePaths.Add("PS3");
			GlobalCPPEnvironment.IncludePaths.Add("PS3/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("PS3/SPU/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("PS3/RHI/Inc");

			GlobalCPPEnvironment.SystemIncludePaths.Add("$(SCE_PS3_ROOT)/target");
			GlobalCPPEnvironment.SystemIncludePaths.Add("$(SCE_PS3_ROOT)/target/common/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("$(SCE_PS3_ROOT)/target/ppu/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("$(SN_PS3_PATH)/include/sn");

			FinalLinkEnvironment.LibraryPaths.Add("../External/GamersSDK/4.2.1/lib/PS3");
			FinalLinkEnvironment.AdditionalLibraries.Add("VoiceCmds");

			// Link with the system libraries.
			FinalLinkEnvironment.LibraryPaths.Add("$(SCE_PS3_ROOT)/target/ppu/lib");

            FinalLinkEnvironment.AdditionalLibraries.Add("m");					// Math
			FinalLinkEnvironment.AdditionalLibraries.Add("mic_stub");			// Microphone
			FinalLinkEnvironment.AdditionalLibraries.Add("net_stub");			// Networking
			FinalLinkEnvironment.AdditionalLibraries.Add("netctl_stub");
			FinalLinkEnvironment.AdditionalLibraries.Add("padfilter");			// Controller
			FinalLinkEnvironment.AdditionalLibraries.Add("io_stub");
			FinalLinkEnvironment.AdditionalLibraries.Add("fios");
			FinalLinkEnvironment.AdditionalLibraries.Add("mstreamSPURSMP3");	// MP3 SPU support
			FinalLinkEnvironment.AdditionalLibraries.Add("fs_stub");			// Filesystem
			FinalLinkEnvironment.AdditionalLibraries.Add("sysutil_stub");		// System utils
			FinalLinkEnvironment.AdditionalLibraries.Add("edgezlib");			// SPU zlib support
			FinalLinkEnvironment.AdditionalLibraries.Add("sync_stub");
			FinalLinkEnvironment.AdditionalLibraries.Add("spurs_stub");			// SPURS
			FinalLinkEnvironment.AdditionalLibraries.Add("sysmodule_stub");
			FinalLinkEnvironment.AdditionalLibraries.Add("ovis_stub");
			FinalLinkEnvironment.AdditionalLibraries.Add("rtc_stub");			// Real-time clock
			FinalLinkEnvironment.AdditionalLibraries.Add("audio_stub");			// Audio
			FinalLinkEnvironment.AdditionalLibraries.Add("sysutil_np_stub");	// Network Platform
			FinalLinkEnvironment.AdditionalLibraries.Add("sysutil_np_trophy_stub");	// Trophy support
			FinalLinkEnvironment.AdditionalLibraries.Add("l10n_stub");			// Internationalization
			FinalLinkEnvironment.AdditionalLibraries.Add("pthread");			// POSIX threads
			FinalLinkEnvironment.AdditionalLibraries.Add("sysutil_game_stub");	// new game content utility
			FinalLinkEnvironment.AdditionalLibraries.Add("pngdec_stub");		// PNG decoder

            // compile/link in Move support if desired
            if (UE3BuildConfiguration.bCompilePS3Move)
            {
                GlobalCPPEnvironment.Definitions.Add("USE_PS3_MOVE=1");

                FinalLinkEnvironment.AdditionalLibraries.Add("camera_stub");	// PS eye (needed for Move)
                FinalLinkEnvironment.AdditionalLibraries.Add("gem_stub");		// Gem (PS Move) 
            }

            if (Configuration != UnrealTargetConfiguration.Shipping)
			{
				FinalLinkEnvironment.AdditionalLibraries.Add("lv2dbg_stub");	// Non-shipping exception-handling
				FinalLinkEnvironment.AdditionalLibraries.Add("rsxdebug");		// For cellRSXFifoFlattenToFile()

	            if (PS3ToolChain.SDKVersion < 310)
				{
					// Link in GPAD. In later SDK versions, this API will be part of standard Gcm.
					GlobalCPPEnvironment.SystemIncludePaths.Add("$(SCE_PS3_ROOT)/GPAD");
					GlobalCPPEnvironment.SystemIncludePaths.Add("$(SCE_PS3_ROOT)/Tools/GPAD");
					GlobalCPPEnvironment.SystemIncludePaths.Add("$(SCE_PS3_ROOT)/Tools/GPAD/GPAD");
					FinalLinkEnvironment.LibraryPaths.Add("$(SCE_PS3_ROOT)/GPAD");
					FinalLinkEnvironment.LibraryPaths.Add("$(SCE_PS3_ROOT)/Tools/GPAD");
					FinalLinkEnvironment.LibraryPaths.Add("$(SCE_PS3_ROOT)/Tools/GPAD/GPAD");
					FinalLinkEnvironment.AdditionalLibraries.Add("dbg_capture");
				}
			}

			SetUpPS3BinkEnvironment();

			FinalLinkEnvironment.LibraryPaths.Add("PS3/External/zlib/lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("z");

			FinalLinkEnvironment.LibraryPaths.Add("PS3/External/PhysX/SDKs/lib/PS3");
			FinalLinkEnvironment.AdditionalLibraries.Add("PhysX");
			FinalLinkEnvironment.AdditionalLibraries.Add("PhysXExtensions");

            FinalLinkEnvironment.LibraryPaths.Add("PS3/External/PhysX/APEX/lib/ps3-PhysX_2.8.4");
//            if (UE3BuildConfiguration.PS3GCMType == PS3GCMType.Debug)
//            {
//                FinalLinkEnvironment.AdditionalLibraries.Add("ApexCommonDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("ApexFrameworkDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("ApexSharedDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("APEX_BasicIOSDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("APEX_ClothingDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("APEX_DestructibleDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("APEX_EmitterDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("APEX_IOFXDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("APEX_ParticlesDEBUG");
//                FinalLinkEnvironment.AdditionalLibraries.Add("pxtaskDEBUG");
//            }
//            else
            {

                if (Configuration == UnrealTargetConfiguration.Shipping)
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("ApexCommonSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("ApexFrameworkSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("ApexSharedSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_BasicIOSSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_ClothingSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_DestructibleSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_EmitterSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_IOFXSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_ParticlesSHIPPING");
                    FinalLinkEnvironment.AdditionalLibraries.Add("pxtask");
                }
                else
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("ApexCommon");
                    FinalLinkEnvironment.AdditionalLibraries.Add("ApexFramework");
                    FinalLinkEnvironment.AdditionalLibraries.Add("ApexShared");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_BasicIOS");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_Clothing");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_Destructible");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_Emitter");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_IOFX");
                    FinalLinkEnvironment.AdditionalLibraries.Add("APEX_Particles");
                    FinalLinkEnvironment.AdditionalLibraries.Add("pxtask");
                }
            }

			// Compile and link with GCM.
			switch (UE3BuildConfiguration.PS3GCMType)
			{
				case PS3GCMType.Release:
					// Use standard release GCM libraries.
					FinalLinkEnvironment.AdditionalLibraries.Add("gcm_cmd");
					break;
				case PS3GCMType.Debug:
					// Use debuggable GCM libraries.
					GlobalCPPEnvironment.Definitions.Add("CELL_GCM_DEBUG=1");
					FinalLinkEnvironment.AdditionalLibraries.Add("gcm_cmddbg");
		            if (PS3ToolChain.SDKVersion >= 310)
					{
	                    FinalLinkEnvironment.AdditionalLibraries.Add("gcm_gpad_stub");
					}
                    break;
			};
			FinalLinkEnvironment.AdditionalLibraries.Add("gcm_sys_stub");

            SetUpSpeedTreeEnvironment();
			
			// currently not 100% supported on PS3
//			SetUpTriovizEnvironment();

			FinalLinkEnvironment.LibraryPaths.Add("PS3/External/FaceFx/lib");

			if (Configuration == UnrealTargetConfiguration.Debug)
			{
                FinalLinkEnvironment.AdditionalLibraries.Add("FaceFX");
            }
			else
			{
                FinalLinkEnvironment.AdditionalLibraries.Add("FaceFX_FinalRelease");
			}

            // Link with GFx
            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx=0"))
            {
				switch (Configuration)
				{
					case UnrealTargetConfiguration.Debug:
						FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/PS3/Debug");
						break;
					case UnrealTargetConfiguration.Shipping:
						FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/PS3/Shipping_NoRTTI");
						break;
					default:
						FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/PS3/Release_NoRTTI");
						break;
				}
				FinalLinkEnvironment.AdditionalLibraries.Add("gfx");
				FinalLinkEnvironment.AdditionalLibraries.Add("rtc_stub");
				FinalLinkEnvironment.AdditionalLibraries.Add("pthread");
			}

			// Add libNull as the last library path, so if any of the optional libraries from above don't exist, they will be found in libNull.
			FinalLinkEnvironment.LibraryPaths.Add("PS3/libNull");

			// Add the PS3-specific projects.
			NonGameProjects.Add(new UE3ProjectDesc("PS3/PS3.vcproj"));

			string DesiredOSS = Game.GetDesiredOnlineSubsystem( GlobalCPPEnvironment, Platform );

			switch( DesiredOSS )
			{
			case "PC":
				SetUpPCEnvironment();
				break;

			case "GameSpy":
				SetUpGameSpyEnvironment();
				break;

			default:
				throw new BuildException( "Requested OnlineSubsystem{0}, but that is not supported on PS3 (only OSSGameSpy or OSSPC)", DesiredOSS );
			}
		}

		void SetUpPS3PhysXEnvironment()
		{
			GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/SDKs/PhysXLoader/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/SDKs/Foundation/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/SDKs/Physics/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/SDKs/Cooking/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/SDKs/PhysXExtensions/include");
            GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/SDKs/PhysXExtensions/include");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/PxFoundation");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/PxTask");
            GlobalCPPEnvironment.SystemIncludePaths.Add("PS3/External/PhysX/PxFoundation/ps3");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/framework/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/NxParameterized/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/clothing/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/destructible/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/emitter/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/explosion/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/forcefield/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/iofx/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/basicios/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/particles/public");
		}

		void SetUpPS3BinkEnvironment()
		{
			FinalLinkEnvironment.LibraryPaths.Add("../External/Bink/lib/PS3");

			if (bHasBinkSPU)
			{
				// link with the BinkPPU_Spurs lib, and set a #define to use it
				GlobalCPPEnvironment.Definitions.Add("USE_BINK_SPU=1");
				FinalLinkEnvironment.AdditionalLibraries.Add("Binkppu_spurs");
			}

			// always add the basic lib
			FinalLinkEnvironment.AdditionalLibraries.Add("BinkPS3");
		}

		void PS3AddDataFileToExecutable(string InputDataFilePath/*,string SectionName*/)
		{
			// Add the object file as an input to the link environment.
			FinalLinkEnvironment.InputFiles.Add(FileItem.GetItemByPath(InputDataFilePath));
		}

		List<FileItem> GetPS3OutputItems()
		{
			// Verify that the user has specified the expected output extension.
			if (Path.GetExtension(OutputPath).ToUpperInvariant() != ".ELF")
			{
				throw new BuildException("Unexpected output extension: {0} instead of .ELF", Path.GetExtension(OutputPath));
			}

			// Link with the precompiled mstream object files.
			string SCE_PS3_ROOT = Environment.GetEnvironmentVariable("SCE_PS3_ROOT");
			PS3AddDataFileToExecutable(Path.Combine(SCE_PS3_ROOT, "target/spu/lib/pic/multistream/mstream_dsp_i3dl2.ppu.o"));
			PS3AddDataFileToExecutable(Path.Combine(SCE_PS3_ROOT, "target/spu/lib/pic/multistream/mstream_dsp_filter.ppu.o"));
			PS3AddDataFileToExecutable(Path.Combine(SCE_PS3_ROOT, "target/spu/lib/pic/multistream/mstream_dsp_para_eq.ppu.o"));

			// link in the hashes.sha file
			FinalLinkEnvironment.InputFiles.Add(
				PS3ToolChain.CreateObjectFileFromDataFile(
					FileItem.GetItemByPath("..\\..\\" + Game.GetGameName() + "\\Build\\hashes.sha"),
					Path.Combine(GlobalCPPEnvironment.OutputDirectory, "hashes.sha.o"), 
					"hashes"));

			// link directly to the final elf, unless we are shipping config, in which case, link to .symbols.elf, and then
			// we will strip the symbols to make the final shipping .elf
			string LinkTargetPath = OutputPath;
			if (Configuration == UnrealTargetConfiguration.Shipping)
			{
				LinkTargetPath = Path.ChangeExtension(LinkTargetPath, ".symbols.elf");
			}
			
			FinalLinkEnvironment.OutputFilePath = LinkTargetPath;
			FileItem ELFFile = FinalLinkEnvironment.LinkExecutable();

			if (Configuration == UnrealTargetConfiguration.Shipping)
			{
				// Strip the debug info from the .symbolslelf file and write the stripped executable to the
				// final destination
				ELFFile = PS3ToolChain.StripDebugInfoFromExecutable(ELFFile, OutputPath);
			}

			// Return a list of the output files.
			List<FileItem> OutputFiles = new List<FileItem>()
			{
				ELFFile
			};
			return OutputFiles;
		}
	}
}
