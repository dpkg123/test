/*=============================================================================
	PS3DownloadableContent.cpp: PS3 implementation of platform DLC class
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "FFileManagerPS3.h"
#include "PS3DownloadableContent.h"

// are mods disabled?
UBOOL GNoInstallMods = FALSE;

// global objects
FDownloadableContent* GDownloadableContent = NULL;

FPlatformDownloadableContent* GPlatformDownloadableContent = NULL;


/**
 * This will add files that were downloaded (or similar mechanism) to the engine. The
 * files themselves will have been found in a platform-specific method.
 *
 * @param Name Friendly, displayable name of the DLC
 * @param PackageFiles The list of files that are game content packages (.upk etc)
 * @param NonPacakgeFiles The list of files in the DLC that are everything but packages (.ini, etc)
 * @param UserIndex Optional user index to associate with the content (used for removing)
 */
UBOOL FDownloadableContent::InstallDownloadableContent(const TCHAR* Name, const TArray<FString>& PackageFiles, const TArray<FString>& NonPackageFiles, INT UserIndex)
{

	// attempt to case the engine to a GameEngine
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	// temporarily allow the GConfigCache to perform file operations if they were off
	UBOOL bWereFileOpsDisabled = GConfig->AreFileOperationsDisabled();
	GConfig->EnableFileOperations();

	// get a list of all known config files
	TArray<FFilename> ConfigFiles;
	GConfig->GetConfigFilenames(ConfigFiles);

	// default to non-special DLC, but allow all DLC
	// @todo: If you need to support user mods, you will want to be able to handle
	// verification of mod inis before installing them, but allow a way for your
	// own DLC to still work (bIsInstallingSpecialDLC would be TRUE when DLC is
	// detected, vs a user mod)
	bIsInstallingSpecialDLC = FALSE;
	UBOOL bIsDLCValid = TRUE;

	// if installing mods is disal
	if (GNoInstallMods && !bIsInstallingSpecialDLC)
	{
		// add this DLC to the list of "installed" DLCs so that the UI can still delete the mods
		InstalledDLCs.AddItem(Name);
		return TRUE;
	}

	// only install it if it was valid
	if ((bIsDLCValid && !GNoInstallMods) || bIsInstallingSpecialDLC)
	{
		//debugf(TEXT("----- Mod/DLC %s is safe!"), Name );

		// tell the package cache about all the new packages
		FName GuidCacheName = NAME_None;
 		for (INT PackageIndex = 0; PackageIndex < PackageFiles.Num(); PackageIndex++)
		{
			// debugf(TEXT("-----!!!!!!!!!!! caching %s"), *PackageFiles(PackageIndex) );
			FFilename BaseName = FFilename(PackageFiles(PackageIndex)).GetBaseFilename();
			if (BaseName.StartsWith(TEXT("GuidCache_")))
			{
				GuidCacheName = FName(*BaseName);
			}
			GPackageFileCache->CacheDownloadedPackage(*PackageFiles(PackageIndex), UserIndex);
		}

		if (GuidCacheName != NAME_None)
		{
			TArray<FName> GuidCache;
			GuidCache.AddItem(GuidCacheName);
			GameEngine->AddPackagesToFullyLoad(FULLYLOAD_Always, TEXT(""), GuidCache, TRUE);
			debugf(TEXT("Loaded guid cache %s"), *GuidCacheName.ToString());
		}

		for (INT FileIndex = 0; FileIndex < NonPackageFiles.Num(); FileIndex++)
		{
			FFilename FullFilename = FFilename(NonPackageFiles(FileIndex));
			FFilename ContentFile = FullFilename.GetCleanFilename();
			//debugf(TEXT("----- Installing %s"), *ContentFile );

			// get filename extension
			FString Ext = ContentFile.GetExtension();

			if (Ext == TEXT("tfc"))
			{
				debugf(NAME_DevPatch, TEXT("Caching DLC texture package: %s/%s"),*FullFilename.GetBaseFilename(),*FullFilename);
				DLCTextureCaches.Set(FName(*FullFilename.GetBaseFilename()),FullFilename);
				continue;
			}
				
			if (Ext == TEXT("sha"))
			{
				// add any script sha hashes to the known set of hashes
				TArray<BYTE> HashContents;
				appLoadFileToArray(HashContents, *NonPackageFiles(FileIndex));
				FSHA1::InitializeFileHashesFromBuffer(HashContents.GetData(), HashContents.Num(), TRUE);

				debugf(NAME_DevPatch, TEXT("Added %s to the set of hashes"), *NonPackageFiles(FileIndex));
				continue;
			}

			// skip any non-ini/loc (for current language) files
			if (Ext != TEXT("ini") && Ext != UObject::GetLanguage())
			{
				continue;
			}

			// look for the optional special divider string;
			#define DividerString TEXT("__")
			INT Divider = ContentFile.InStr(DividerString);
			// if we found it, just use what's after the divider
			if (Divider != -1)
			{
				ContentFile = ContentFile.Right(ContentFile.Len() - (Divider + appStrlen(DividerString)));
			}

			FConfigFile* NewConfigFile = NULL;
			FString NewConfigFilename;

			// look for the filename in the config files
			UBOOL bWasCombined = FALSE;
			for (INT ConfigFileIndex = 0; ConfigFileIndex < ConfigFiles.Num() && !bWasCombined; ConfigFileIndex++)
			{
				// does the config file (without path) match the DLC file?
				if (ConfigFiles(ConfigFileIndex).GetCleanFilename() == ContentFile)
				{
					//debugf(TEXT("--------- Accepting %s"), *ConfigFiles(ConfigFileIndex) );
					// get the configfile object
					NewConfigFile = GConfig->FindConfigFile(*ConfigFiles(ConfigFileIndex));
					check(NewConfigFile);

					// merge our ini file into the existing one
					if (NewConfigFile->Combine(*NonPackageFiles(FileIndex)))
					{
						debugf(TEXT("Merged DLC config file '%s' into existing config '%s'"), *NonPackageFiles(FileIndex), *ConfigFiles(ConfigFileIndex));
					}
					else
					{
						debugf(TEXT("Failed to merged DLC config file '%s' into existing config '%s' (possible DRM error)"), *NonPackageFiles(FileIndex), *ConfigFiles(ConfigFileIndex));
					}

					// mark that we have attempted to combine
					bWasCombined = TRUE;

					// remember the filename
					NewConfigFilename = ConfigFiles(ConfigFileIndex);

					break;
				}
			}

			// if it wasn't combined, add a new file
			if (!bWasCombined)
			{
				// we need to create a usable pathname for the new ini/loc file
				if (Ext == TEXT("ini"))
				{
					NewConfigFilename = appGameConfigDir() + ContentFile;
				}
				else
				{
					// put this into any localization directory in the proper language sub-directory (..\ExampleGame\Localization\fra\DLCMap.fra)
					NewConfigFilename = GSys->LocalizationPaths(0) * Ext * ContentFile;
				}
				// first we set a value into the config for this filename (since we will be reading from a different
				// path than we want to store the config under, we can't use LoadFile)
				GConfig->SetBool(TEXT("DLCDummy"), TEXT("A"), FALSE, *NewConfigFilename);

				// now get the one we just made
				NewConfigFile = GConfig->FindConfigFile(*NewConfigFilename);
				
				// read in the file
				NewConfigFile->Combine(*NonPackageFiles(FileIndex));

				debugf(TEXT("Read new DLC config file '%s' into the config cache"), *NonPackageFiles(FileIndex));
			}
			
			check(NewConfigFile);

			if (GameEngine)
			{
				// look for packages to load for maps
				TMap<FName, TArray<FName> > MapMappings;
				GConfig->Parse1ToNSectionOfNames(TEXT("Engine.PackagesToFullyLoadForDLC"), TEXT("MapName"), TEXT("Package"), MapMappings, *NewConfigFilename);

				// tell the game engine about the parsed settings
				for(TMap<FName, TArray<FName> >::TIterator It(MapMappings); It; ++It)
				{
					GameEngine->AddPackagesToFullyLoad(FULLYLOAD_Map, It.Key().ToString(), It.Value(), TRUE);
				}

				// now get the per-gametype packages
				for (INT PrePost = 0; PrePost < 2; PrePost++)
				{
					TMap<FString, TArray<FString> > GameMappings;
					GConfig->Parse1ToNSectionOfStrings(TEXT("Engine.PackagesToFullyLoadForDLC"), PrePost ? TEXT("GameType_PostLoadClass") : TEXT("GameType_PreLoadClass"), TEXT("Package"), GameMappings, *NewConfigFilename);

					// tell the game engine about the parsed settings
					for(TMap<FString, TArray<FString> >::TIterator It(GameMappings); It; ++It)
					{
						// convert array of string package names to names
						TArray<FName> PackageNames;
						for (INT PackageIndex = 0; PackageIndex < It.Value().Num(); PackageIndex++)
						{
							PackageNames.AddItem(FName(*It.Value()(PackageIndex)));
						}
						GameEngine->AddPackagesToFullyLoad(PrePost ? FULLYLOAD_Game_PostLoadClass : FULLYLOAD_Game_PreLoadClass, It.Key(), PackageNames, TRUE);
					}
				}

				// Finally, register the "for all gametype" packages.
				const TCHAR* LoadForAllGameTypesIniSection = TEXT("LoadForAllGameTypes");
				const FConfigSection* AllGameTypeList = GConfig->GetSectionPrivate( LoadForAllGameTypesIniSection, FALSE, TRUE, *NewConfigFilename );
				if ( LoadForAllGameTypesIniSection && AllGameTypeList )
				{
					TArray<FName> PackageNames;
					for( FConfigSectionMap::TConstIterator It(*AllGameTypeList) ; It ; ++It )
					{
						const FString& PackageName = It.Value();
						PackageNames.AddItem(FName(*PackageName));
						//debugf(TEXT("----------- Adding %s"), *PackageName);
					}
					GameEngine->AddPackagesToFullyLoad( FULLYLOAD_Game_PostLoadClass, TEXT("LoadForAllGameTypes"), PackageNames, TRUE );
				}

				// allow game to handle extra ini sections
				HandleExtraIniSection(NewConfigFilename, NonPackageFiles(FileIndex));
			}
		}
	}
/*
	else
	{
		debugf(TEXT("----- Mod/DLC %s is not safe! It will not be installed!"), Name );
	}
*/


	// re-disable file ops if they were before
	if (bWereFileOpsDisabled)
	{
		GConfig->DisableFileOperations();
	}

	// update everything if it was installed
	if (bIsDLCValid)
	{
		if (!bIsInstallingSpecialDLC)
		{
			// add this DLC to the list of installed DLCs
			InstalledDLCs.AddItem(Name);
		}

		// let per-game native code run to update anything needed
//		if (!GSkipInstallNotify)
		{
			OnDownloadableContentChanged(TRUE);
		}
	}
	// if it was invalid, just delete it from the HD
	else
	{
		GPlatformDownloadableContent->DeleteDownloadableContent(Name);
	}

	return TRUE;
}

/**
 * Removes downloadable content from the system. Can use UserIndex's or not. Will always remove
 * content that did not have a user index specified
 * 
 * @param MaxUserIndex (Platform-dependent) max number of users to flush (this will iterate over all users from 0 to MaxNumUsers), as well as NO_USER 
 */
void FDownloadableContent::RemoveAllDownloadableContent(INT MaxUserIndex)
{
	// clear out all DLCs from the list
	InstalledDLCs.Empty();

	// remove content not associated with a user
	GPackageFileCache->ClearDownloadedPackages();
//	GConfig->RemoveDownloadedSections();

	// remove any existing content packages for all users
	for (INT UserIndex = 0; UserIndex < MaxUserIndex; UserIndex++)
	{
		GPackageFileCache->ClearDownloadedPackages(UserIndex);
//		GConfig->RemoveDownloadedSections(UserIndex);
	}

	// let per-game native code run to update anything needed
	OnDownloadableContentChanged(FALSE);

	// cleanup the list of packages to load for maps
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine)
	{
		// cleanup all of the fully loaded packages for maps (may not free memory until next GC)
		GameEngine->CleanupAllPackagesToFullyLoad();
	}
}

UBOOL FDownloadableContent::GetDLCTextureCachePath(FName TextureCacheName, FString& Path)
{
//	debugf(TEXT("Looking for DLC texture cache: %s"),TextureCacheName.GetName());
    FString *Result = DLCTextureCaches.Find(TextureCacheName);
	if (Result != NULL)
	{
		Path = *Result;
		return TRUE;
	}
	return FALSE;
}

/**
 * Game-specific code to handle DLC being added or removed
 * 
 * @param bContentWasInstalled TRUE if DLC was installed, FALSE if it was removed
 */
void FDownloadableContent::OnDownloadableContentChanged(UBOOL bContentWasInstalled)
{
	// game-specific, nothing to do here
}

/**
 * @return the list of all installed DLC
 */
TArray<FString> FDownloadableContent::GetDownloadableContentList()
{
	return InstalledDLCs;
}

/** Default implementation is to simply call InstallDownloadableContent against each bundle. */
void FDownloadableContent::InstallDLCBundles(TArray<FDLCBundle>& Bundles)
{
	for ( INT BundleIndex = 0 ; BundleIndex < Bundles.Num() ; ++BundleIndex )
	{
		const FDLCBundle& Bundle = Bundles(BundleIndex);
		if (BundleIndex == Bundles.Num() - 1)
		{
		}
		InstallDownloadableContent( *Bundle.Directory, Bundle.PackageFiles, Bundle.NonPackageFiles );
	}

}

/**
 * Looks on disk for DLC and install it
 */
void FPlatformDownloadableContent::FindDownloadableContent()
{
	debugf(TEXT("Looking for DLC..."));

	// look for any DLC installed
	TArray<FString> DLCDirectories;
	FString DLCRoot = TEXT("..\\DLC\\");
	GFileManager->FindFiles(DLCDirectories, *(DLCRoot + TEXT("*")), FALSE, TRUE);

	// the DLCDirectories should now contain a list of all DLCs
	for (INT DLCIndex = 0; DLCIndex < DLCDirectories.Num(); DLCIndex++)
	{
		debugf(TEXT("  Found DLC dir %s..."), *DLCDirectories(DLCIndex));
		TArray<FString> PackageFiles;
		TArray<FString> NonPackageFiles;

		// find all the packages in the content
		appFindFilesInDirectory(PackageFiles, *(DLCRoot + DLCDirectories(DLCIndex)), TRUE, FALSE);

		// find all the non-packages in the content
		appFindFilesInDirectory(NonPackageFiles, *(DLCRoot + DLCDirectories(DLCIndex)), FALSE, TRUE);

		// tell engine about the DLC
		GDownloadableContent->InstallDownloadableContent(*DLCDirectories(DLCIndex), PackageFiles, NonPackageFiles);
	}
}

/**
 * Deletes a single DLC from the system (physically removes it, not just uninstalls it)
 *
 * @param Name Name of the DLC to delete
 */
void FPlatformDownloadableContent::DeleteDownloadableContent(const TCHAR* Name)
{
	// make path to DLC directory
	FString DLCRoot = TEXT("/../DLC/");
	DLCRoot += Name;

	// delete the entire directory, just like that :)
	GFileManager->DeleteDirectory(*DLCRoot, FALSE, TRUE);
}


FDLCBundle::FDLCBundle(const FString& InDLCRoot, const FString& InDirectory)
	:	Directory( InDirectory )
{
	// Find all the packages in the content.
	appFindFilesInDirectory(PackageFiles, *(InDLCRoot + Directory), TRUE, FALSE);
	// Find all the non-packages in the content.
	appFindFilesInDirectory(NonPackageFiles, *(InDLCRoot + Directory), FALSE, TRUE);
}

void FDLCBundle::FindDLCFiles(const FString& DLCRoot, TArray<FDLCBundle>& OutBundles)
{
	TArray<FString> DLCDirectories;
	GFileManager->FindFiles(DLCDirectories, *(DLCRoot + TEXT("*")), FALSE, TRUE);

	// Create bundles for any subdirectory of the root.  We don't load files from
	// the root itself because each bundle needs a name (ie the directory name).
	for (INT DLCIndex = 0; DLCIndex < DLCDirectories.Num(); DLCIndex++)
	{
		new(OutBundles) FDLCBundle( DLCRoot,  DLCDirectories(DLCIndex) );
	}
}

/**
 * Looks on disk for DLC and install it
 */
void FPS3DownloadableContent::FindDownloadableContent()
{
	debugf(TEXT("Looking for DLC..."));

	// look for any DLC and install it.
	const FString DLCRoot = FString(TEXT("/DLC/"));
	TArray<FDLCBundle> Bundles;
	FDLCBundle::FindDLCFiles( DLCRoot, Bundles );
	GDownloadableContent->InstallDLCBundles( Bundles );
}

/**
 * Deletes a single DLC from the system (physically removes it, not just uninstalls it)
 *
 * @param Name Name of the DLC to delete
 */
void FPS3DownloadableContent::DeleteDownloadableContent(const TCHAR* Name)
{
	// make path to DLC directory
	FString DLCRoot = FString(TEXT("/DLC/")) + Name;

	// delete the entire directory, just like that :)
	GFileManager->DeleteDirectory(*DLCRoot, FALSE, TRUE);
}

