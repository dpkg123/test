/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	class Xbox360ToolChain
	{
		/** Checks that the Xbox 360 SDK is installed, and if so returns the path to its binaries directory. */
		public static string GetBinDirectory( CPPTargetConfiguration Configuration )
		{
			string MoreInfoString =
				"See https://udn.epicgames.com/Three/GettingStartedXbox360 for help setting up the UE3 Xbox 360 compilation environment";

			// Read the root directory of the Xbox 360 SDK from the environment.
			string XEDKEnvironmentVariable = Environment.GetEnvironmentVariable( "XEDK" );

			// Check that the environment variable is defined
			if( XEDKEnvironmentVariable == null )
			{
				throw new BuildException(
					"XEDK environment variable isn't set; you must properly install the Xbox 360 SDK before building UE3 for Xbox 360.\n" +
					MoreInfoString
					);
			}

			// Check that the environment variable references a valid directory.
			if( !Directory.Exists( XEDKEnvironmentVariable ) )
			{
				throw new BuildException(
					string.Format(
						"XEDK environment variable is set to a non-existent directory: {0}\n",
						XEDKEnvironmentVariable
						) +
					MoreInfoString
					);
			}

			return Path.Combine( XEDKEnvironmentVariable, "bin/win32" );
		}
	}
}
