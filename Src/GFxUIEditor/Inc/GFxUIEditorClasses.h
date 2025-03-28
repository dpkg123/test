/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
    Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

#include "GFxUIEditorNames.h"

// Split enums from the rest of the header so they can be included earlier
// than the rest of the header file by including this file twice with different
// #define wrappers. See Engine.h and look at EngineClasses.h for an example.
#if !NO_ENUMS && !defined(NAMES_ONLY)

#ifndef INCLUDED_GFXUIEDITOR_ENUMS
#define INCLUDED_GFXUIEDITOR_ENUMS 1


#endif // !INCLUDED_GFXUIEDITOR_ENUMS
#endif // !NO_ENUMS

#if !ENUMS_ONLY

#ifndef NAMES_ONLY
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

#ifndef INCLUDED_GFXUIEDITOR_CLASSES
#define INCLUDED_GFXUIEDITOR_CLASSES 1
#define ENABLE_DECLARECLASS_MACRO 1
#include "UnObjBas.h"
#undef ENABLE_DECLARECLASS_MACRO

class UGenericBrowserType_GFxMovie : public UGenericBrowserType
{
public:
    //## BEGIN PROPS GenericBrowserType_GFxMovie
    //## END PROPS GenericBrowserType_GFxMovie

    DECLARE_CLASS(UGenericBrowserType_GFxMovie,UGenericBrowserType,0,GFxUIEditor)
	virtual void Init();

	virtual UBOOL ShowObjectEditor();
	virtual UBOOL ShowObjectEditor(UObject* InObject);

	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
	virtual void QuerySupportedCommands( USelection* InObjects, TArray<FObjectSupportedCommandType>& OutCommands ) const;
	virtual void DoubleClick();
};

class UGFxImportCommandlet : public UCommandlet
{
public:
    //## BEGIN PROPS GFxImportCommandlet
    //## END PROPS GFxImportCommandlet

    DECLARE_CLASS(UGFxImportCommandlet,UCommandlet,0|CLASS_Transient,GFxUIEditor)
	virtual INT Main(const FString& Params);
};

class UGFxMovieFactory : public UFactory, public FReimportHandler
{
public:
    //## BEGIN PROPS GFxMovieFactory
    BITFIELD bSetSRGBOnImportedTextures:1;
    BITFIELD bPackTextures:1;
    INT PackTextureSize;
    BYTE TextureRescale;
    FStringNoInit TextureFormat;
    //## END PROPS GFxMovieFactory

    DECLARE_CLASS(UGFxMovieFactory,UFactory,0|CLASS_Transient,GFxUIEditor)
	UObject* FactoryCreateBinary(UClass* InClass,
		UObject* InOuter,
		FName InName,
		EObjectFlags InFlags,
		UObject* Context,
		const TCHAR* Type,
		const BYTE*& Buffer,
		const BYTE* BufferEnd,
		FFeedbackContext* Warn);

	UBOOL Reimport(UObject* InObject);

#if WITH_GFx
	/**
	 * Parses the Swf data in the movie and locates all the import tags.
	 * Each import tag is translated to a "weak referece" (i.e. a UE3 fullpath pointing at a SwfMovie.)
	 * All weak references are added to the ReferencedSwfs property of the USwfMovie being imported.
	 *
	 * @param MovieInfo The SwfMovie being imported; its contents will be parsed for references to other Swfs.
	 * @param OutMissingRefs A list of references to Swfs that we failed to convert to full UE3 references (used for error reporting only)
	 */
	static void CheckImportTags(USwfMovie* SwfMovie, TArray<FString>* OutMissingRefs, UBOOL bAddFonts = FALSE);

	/**
	 * Utility class with lots of useful info for importing a SWF.
	 * It can be constructed given a SWF file path. See constructor for more details.
	 */
	struct SwfImportInfo
	{
		/** 
		 * Given a path, ensure that this path uses only the approved PATH_SEPARATOR.
		 * If this path is relative, the optional ./ at the beginning is removed.
		 *
		 * @param InPath Path to canonize
		 *
		 * @return The canonical copy of InPath.
		 */
		static FString EnforceCanonicalPath(const FString& InPath);

		/**
		 * Given a path to a SWF file (either an absolute path or a path relative to the game's Flash/ directory)
		 * fill out all the struct's members. See member description for details.
		 */
		SwfImportInfo( const FString& InSwfFile );

		/** The absolute path to swf including the filename */
		FFilename AbsoluteSwfFileLocation;

		/** The asset name of the SwfMovie. This will be the same as the filename but without the .SWF extension */
		FString AssetName;

		/** UE3 Path to the asset not including the actual asset name. E.g. Package.Group0.Group1 */
		FString PathToAsset;
		
		/** The name of the outermost package into which this asset should be imported */
		FString OutermostPackageName;

		/** The group only portion of the path. E.g. If the fullpath is Package.Group0.Group1.Asset, then this field is just Group0.Group1 */
		FString GroupOnlyPath;

		/** Is it OK to import this swf? */
		UBOOL bIsValidForImport;
	};

	/** Return the Flash/ directory for this game. E.g. d:/UE3/UDKGame/Flash/ */
	static FString GetGameFlashDir();

private:
	UObject* CreateMovieGFxExport(UObject* InParent, FName Name, const FString& OriginalSwfPath, EObjectFlags Flags, const BYTE* Buffer, const BYTE* BufferEnd,
		const FString& GFxExportCmdline, FFeedbackContext* Warn);
#endif

private:
	UBOOL RunGFXExport( const FString& strCmdLineParams, FString* OutGfxExportErrors );
	UObject* BuildPackage(const FString& strInputDataFolder, const FString& strSwfFile, const FString& OriginalSwfLocation,
		const FName& Name, UObject* InOuter, EObjectFlags Flags, FFeedbackContext* Warn);

	/**
	 * Attempt to locate the directory with original resources that were imported into the .FLA document.
	 * Given the SwfLocation, GfxExport will search for original files in the SWF's sibling directory
	 * with the same name as the SWF.
	 * E.g. If we have a c:\Art\SWFName.swf and it uses SomePicture.TGA, then we tell GfxExport to 
	 * look for c:\Art\SWFName\SomePicture.PNG.
	 *
	 * @param InSwfLocation The location of the SWF; original resources are searched relative to the SWF.
	 *
	 * @return A directory where the original images used in this SWF are found.
	 */
	static FString GetOriginalResourceDir( const FString& InSwfPath );

	/**
	 * Function that is used to modify textures on import. This should be extended
	 * where possible to set correct texture compression values, etc, for the different
	 * platforms based on the texture data.
	 *
	 * @param strTextureFileName - the file path of the texture being imported
	 * @param pTexture - the Texture object to be modified
	 */
	void FixupTextureImport(UTexture2D* pTexture, const FString& strTextureFileName);

	/**
	 * Deletes a folder and all of its contents.
	 */
	UBOOL DeleteFolder(const FString& strFolderPath);

	void GetAllFactories(TArray<UFactory*> &factories);
	UFactory *FindMatchingFactory(TArray<UFactory*> &factories, const FString &fileExtension);

};

class UGFxReimportCommandlet : public UCommandlet
{
public:
    //## BEGIN PROPS GFxReimportCommandlet
    //## END PROPS GFxReimportCommandlet

    DECLARE_CLASS(UGFxReimportCommandlet,UCommandlet,0|CLASS_Transient,GFxUIEditor)
	virtual INT Main(const FString& Params);
};

#undef DECLARE_CLASS
#undef DECLARE_CASTED_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef DECLARE_ABSTRACT_CASTED_CLASS
#endif // !INCLUDED_GFXUIEDITOR_CLASSES
#endif // !NAMES_ONLY


#ifndef NAMES_ONLY
#undef AUTOGENERATE_FUNCTION
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef GFXUIEDITOR_NATIVE_DEFS
#define GFXUIEDITOR_NATIVE_DEFS

#define AUTO_INITIALIZE_REGISTRANTS_GFXUIEDITOR \
	UGenericBrowserType_GFxMovie::StaticClass(); \
	UGFxImportCommandlet::StaticClass(); \
	UGFxMovieFactory::StaticClass(); \
	UGFxReimportCommandlet::StaticClass(); \

#endif // GFXUIEDITOR_NATIVE_DEFS

#ifdef NATIVES_ONLY
#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_SIZE_NODIE(UGenericBrowserType_GFxMovie)
VERIFY_CLASS_SIZE_NODIE(UGFxImportCommandlet)
VERIFY_CLASS_OFFSET_NODIE(UGFxMovieFactory,GFxMovieFactory,PackTextureSize)
VERIFY_CLASS_OFFSET_NODIE(UGFxMovieFactory,GFxMovieFactory,TextureFormat)
VERIFY_CLASS_SIZE_NODIE(UGFxMovieFactory)
VERIFY_CLASS_SIZE_NODIE(UGFxReimportCommandlet)
#endif // VERIFY_CLASS_SIZES
#endif // !ENUMS_ONLY

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
