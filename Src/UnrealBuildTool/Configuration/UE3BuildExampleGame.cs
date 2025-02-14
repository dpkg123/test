/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	class UE3BuildExampleGame : UE3BuildGame
	{
        /** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		public string GetGameName()
		{
			return "ExampleGame";
		}
		
		/** Returns a subplatform (e.g. dll) to disambiguate object files */
		public string GetSubPlatform()
		{
			return ( "" );
		}

		/** Get the desired OnlineSubsystem. */
		public string GetDesiredOnlineSubsystem( CPPEnvironment CPPEnv, UnrealTargetPlatform Platform )
		{
			string ForcedOSS = UE3BuildTarget.ForceOnlineSubsystem( Platform );
			if( ForcedOSS != null )
			{
				return ( ForcedOSS );
			}

			return ( "PC" );
		}

		/** Returns true if the game wants to have PC ES2 simulator (ie ES2 Dynamic RHI) enabled */
		public bool ShouldCompileES2()
		{
			return true;
		}

        /** Allows the game add any global environment settings before building */
        public void GetGameSpecificGlobalEnvironment(CPPEnvironment GlobalEnvironment)
        {

        }

        /** Returns the xex.xml file for the given game */
		public FileItem GetXEXConfigFile()
		{
			return FileItem.GetExistingItemByPath("ExampleGame/Live/xex.xml");
		}

        public void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects)
        {
            GameProjects.Add(new UE3ProjectDesc("UDKBase/UDKBase.vcproj"));
            GameCPPEnvironment.IncludePaths.Add("UDKBase/Inc");

            GameProjects.Add(new UE3ProjectDesc("UTGame/UTGame.vcproj"));

            if (UE3BuildConfiguration.bBuildEditor &&
                (GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64))
            {
                GameProjects.Add(new UE3ProjectDesc("UTEditor/UTEditor.vcproj"));
                GameCPPEnvironment.IncludePaths.Add("UTEditor/Inc");
            }

            GameCPPEnvironment.Definitions.Add("GAMENAME=UTGAME");
            GameCPPEnvironment.Definitions.Add("IS_UTGAME=1");
        }
	}
}
