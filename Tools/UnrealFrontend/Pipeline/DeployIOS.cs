﻿/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Color = System.Drawing.Color;
using System.IO;

namespace UnrealFrontend.Pipeline
{
	public class DeployIOS : Pipeline.CommandletStep
	{
		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Deploy"; } }

		public override String StepNameToolTip { get { return "Deploy app to iOS device."; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Deploy to iOS device"; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			string GameName = InProfile.SelectedGameName;
			string PlatformName = InProfile.TargetPlatformType.ToString();
			// The name of the directory needs to match where DevStudio outputs the executable - and it uses Shipping, not FR
			string ConfigurationName = Pipeline.PackageIOS.GetMobileConfigurationString( InProfile.LaunchConfiguration );

			String ExecutablePath = Path.GetFullPath(PlatformName + "\\iPhonePackager.exe");

			String CommandLine = String.Format("deploy {0} {1}", GameName, ConfigurationName);

			String CWD = Path.GetFullPath(PlatformName);
			
			return ProcessManager.StartProcess(ExecutablePath, CommandLine, CWD, InProfile.TargetPlatform);
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			Session.Current.SessionLog.AddLine(Color.DarkMagenta, "DeployIOS does not support Clean and Execute.");
			return true;
		}
	}

}
