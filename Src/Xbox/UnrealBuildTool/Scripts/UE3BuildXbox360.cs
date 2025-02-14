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
	partial class UE3BuildTarget
	{
		bool Xbox360SupportEnabled()
		{
			return true;
		}

		void SetUpXbox360Environment()
		{
			GlobalCPPEnvironment.Definitions.Add("_XBOX=1");
			GlobalCPPEnvironment.Definitions.Add("XBOX=1");
			GlobalCPPEnvironment.Definitions.Add("__STATIC_LINK=1");
			GlobalCPPEnvironment.Definitions.Add("_MBCS");

			// Add the Xbox 360 public project include paths.
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeAudio/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeCore/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeD3DDrv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeD3DDrv/Src");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeDrv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeEngine/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeIpDrv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Xenon/XeLaunch/Src");
            GlobalCPPEnvironment.IncludePaths.Add("XAudio2/Inc");
            GlobalCPPEnvironment.IncludePaths.Add("OnlineSubsystemLive/Inc");

			// Include lzo and zlib headers.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/lzopro/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/zlib/inc");

			// Compile and link with the Xbox 360 runtime libraries.
			GlobalCPPEnvironment.SystemIncludePaths.Add("$(XEDK)/include/xbox");
			FinalLinkEnvironment.LibraryPaths.Add("$(XEDK)/lib/xbox");
			if (Configuration == UnrealTargetConfiguration.Debug)
			{
                FinalLinkEnvironment.AdditionalLibraries.Add("xmcored.lib");
                FinalLinkEnvironment.AdditionalLibraries.Add("xgraphicsd.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xboxkrnl.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xnetd.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("x3daudio.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xonlined.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xmpd.lib");

				// Debug builds use LIBCMTD, so suppress any references to the other versions of the library.
				FinalLinkEnvironment.ExcludedLibraries.Add("LIBCMT");

				// Debug builds use XAPILIBD, so suppress any references to the other versions of the library.
				FinalLinkEnvironment.ExcludedLibraries.Add("xapilib");
				FinalLinkEnvironment.ExcludedLibraries.Add("xapilibi");

                // Compile and link with XAudio2
                FinalLinkEnvironment.AdditionalLibraries.Add("xaudio2.lib");
			}
			else
			{
				if (Configuration == UnrealTargetConfiguration.Release)
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("x3daudio.lib");
					// Release builds use xapilibi, so suppress any references to the other versions of the library.
					FinalLinkEnvironment.ExcludedLibraries.Add("xapilib");
					FinalLinkEnvironment.ExcludedLibraries.Add("xapilibd");
				}
				else
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("x3daudioltcg.lib");
					// Shipping builds use xapilib, so suppress any references to the other versions of the library.
					FinalLinkEnvironment.ExcludedLibraries.Add("xapilibi");
					FinalLinkEnvironment.ExcludedLibraries.Add("xapilibd");
				}

                FinalLinkEnvironment.AdditionalLibraries.Add("xmcore.lib");
                FinalLinkEnvironment.AdditionalLibraries.Add("xgraphics.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xboxkrnl.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xnet.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xonline.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("xmp.lib");

                // Compile and link with XAudio2
                FinalLinkEnvironment.AdditionalLibraries.Add("xaudio2.lib");

				// Release and Shipping builds use LIBCMT, so suppress any references to the other versions of the library.
				FinalLinkEnvironment.ExcludedLibraries.Add("LIBCMTD");
			}

            FinalLinkEnvironment.AdditionalLibraries.Add("XAPOFX.lib");

            // Link with GFx
            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx=0"))
            {
				UnrealTargetConfiguration ForcedConfiguration = Configuration;
                if (UE3BuildConfiguration.bForceScaleformRelease && Configuration == UnrealTargetConfiguration.Debug)
                {
                    ForcedConfiguration = UnrealTargetConfiguration.Release;
                }
            	switch(ForcedConfiguration)
                {
                  case UnrealTargetConfiguration.Debug:
                        FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/Xbox360/Msvc90/Debug");
                        break;
					case UnrealTargetConfiguration.Shipping:
						FinalLinkEnvironment.LibraryPaths.Add( GFxDir + "/Lib/Xbox360/Msvc90/Shipping" );
						break;
					default:
                        FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/Xbox360/Msvc90/Release");
                        break;
                }
                FinalLinkEnvironment.AdditionalLibraries.Add("libgfx.lib");
            }

			// Link with either the debug or release D3DX9 library.
			if (Configuration == UnrealTargetConfiguration.Debug)
			{
				FinalLinkEnvironment.AdditionalLibraries.Add("d3dx9d.lib");
			}
			else
			{
				FinalLinkEnvironment.AdditionalLibraries.Add("d3dx9.lib");
			}

			// Add the Xbox360-specific projects.
			NonGameProjects.Add( new UE3ProjectDesc("Xenon/Xenon.vcproj") );
            NonGameProjects.Add( new UE3ProjectDesc("Xenon/XeCore/XenonCore.vcproj") );
            NonGameProjects.Add( new UE3ProjectDesc("Xenon/XeD3DDrv/XenonD3DDrv.vcproj") );
            NonGameProjects.Add( new UE3ProjectDesc("Xenon/XeDrv/XenonDrv.vcproj") );
            NonGameProjects.Add( new UE3ProjectDesc("Xenon/XeEngine/XenonEngine.vcproj") );
            NonGameProjects.Add( new UE3ProjectDesc("Xenon/XeLaunch/XenonLaunch.vcproj") );
			NonGameProjects.Add( new UE3ProjectDesc("OnlineSubsystemLive/OnlineSubsystemLive.vcproj") );
            NonGameProjects.Add( new UE3ProjectDesc("XAudio2/XAudio2.vcproj") );

			// Add library search paths for libraries included via pragma comment (lib)
			FinalLinkEnvironment.LibraryPaths.Add("../External/Bink/lib/Xenon/");
			FinalLinkEnvironment.LibraryPaths.Add("../External/GamersSDK/4.2.1/lib/Xenon/");
            SetUpSpeedTreeEnvironment();
//			SetUpTriovizEnvironment();
			FinalLinkEnvironment.LibraryPaths.Add("../External/DECtalk464/lib/Xenon/");
			FinalLinkEnvironment.LibraryPaths.Add("Xenon/External/FaceFX/FxSDK/lib/xbox360/vs9/");
			FinalLinkEnvironment.LibraryPaths.Add("Xenon/External/PhysX/SDKs/lib/xbox360/");
			FinalLinkEnvironment.LibraryPaths.Add("../External/XNAContentServer/lib/xbox/");
            FinalLinkEnvironment.LibraryPaths.Add("Xenon/External/PhysX/APEX/lib/xbox360-PhysX_2.8.4");
		}

		List<FileItem> GetXbox360OutputItems()
		{
			// Verify that the user has specified the expected output extension.
			if (Path.GetExtension(OutputPath).ToUpperInvariant() != ".XEX")
			{
				throw new BuildException("Unexpected output extension: {0} instead of .XEX", Path.GetExtension(OutputPath));
			}

			// Put the non-executable output files (PDB, import library, etc) in the same directory as the executable
			FinalLinkEnvironment.OutputDirectory = Path.GetDirectoryName(OutputPath);

			if( AdditionalDefinitions.Contains( "USE_MALLOC_PROFILER=1" ) )
			{
				if( !OutputPath.ToLower().Contains( ".mprof.xex" ) )
				{
					OutputPath = Path.ChangeExtension( OutputPath, ".MProf.xex" );
				}
			}

			// Link the XEX file.
			FinalLinkEnvironment.OutputFilePath = OutputPath;			
			if( Game.GetXEXConfigFile() != null )
			{
				// Pass XEX.XML to linker to invoke imageXEX to apply it.
				FinalLinkEnvironment.AdditionalArguments += string.Format(" /XEXCONFIG:\"{0}\"", Game.GetXEXConfigFile().AbsolutePath);
			}
			FileItem XEXFile = FinalLinkEnvironment.LinkExecutable();
			FileItem OutputFile = XEXFile;
			
			if (BuildConfiguration.bDeployAfterCompile)
			{
				// Run CookerSync to copy the XEX to the console.
				Action CookerSyncAction = new Action();
				CookerSyncAction.CommandPath = "../../Binaries/CookerSync.exe";
				CookerSyncAction.CommandArguments = string.Format("{0} -p Xbox360", Game.GetGameName().Replace("Game",""));
				CookerSyncAction.bCanExecuteRemotely = false;
				CookerSyncAction.StatusDescription = "Deploying files to devkit...";
				CookerSyncAction.WorkingDirectory = "../../Binaries";
				CookerSyncAction.PrerequisiteItems.Add(OutputFile);

				// Use the -AllowNoTarget to disable the error CookerSync would otherwise throw when no default target to copy to is configured.
				CookerSyncAction.CommandArguments += " -AllowNoTarget";

				// Use non-interactive mode so CookerSync just fails if the target devkit isn't turned on.
				CookerSyncAction.CommandArguments += " -NonInter";

				// Don't regenerate the TOC to prevent conflicts when compiling at the same time as cooking.
				CookerSyncAction.CommandArguments += " -notoc";

				// Use the UnrealBuildTool tagset to only copy the executables.
				CookerSyncAction.CommandArguments += " -x UnrealBuildTool";

				// Pass in the deployment root so the deployment root does not have to match the automatically generated one
				CookerSyncAction.CommandArguments += " -b " + RemoteRoot;

				// use the default target as we aren't supplying a target
				CookerSyncAction.CommandArguments += " -deftarget";

				// reboot targets
				CookerSyncAction.CommandArguments += " -o";

				// Give the CookerSync action a produced file that it doesn't write to, ensuring that it will always run.
				OutputFile = FileItem.GetItemByPath("Dummy.txt");
				CookerSyncAction.ProducedItems.Add(OutputFile);
			}
			
			// Return a list of the output files.
			List<FileItem> OutputFiles = new List<FileItem>()
			{
				OutputFile
			};
			return OutputFiles;
		}
	}
}
