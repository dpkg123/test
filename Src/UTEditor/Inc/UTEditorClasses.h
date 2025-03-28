/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
    Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

#include "UTEditorNames.h"

// Split enums from the rest of the header so they can be included earlier
// than the rest of the header file by including this file twice with different
// #define wrappers. See Engine.h and look at EngineClasses.h for an example.
#if !NO_ENUMS && !defined(NAMES_ONLY)

#ifndef INCLUDED_UTEDITOR_ENUMS
#define INCLUDED_UTEDITOR_ENUMS 1


#endif // !INCLUDED_UTEDITOR_ENUMS
#endif // !NO_ENUMS

#if !ENUMS_ONLY

#ifndef NAMES_ONLY
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

#ifndef INCLUDED_UTEDITOR_CLASSES
#define INCLUDED_UTEDITOR_CLASSES 1
#define ENABLE_DECLARECLASS_MACRO 1
#include "UnObjBas.h"
#undef ENABLE_DECLARECLASS_MACRO

class UGenericBrowserType_UTMapMusicInfo : public UGenericBrowserType
{
public:
    //## BEGIN PROPS GenericBrowserType_UTMapMusicInfo
    //## END PROPS GenericBrowserType_UTMapMusicInfo

    DECLARE_CLASS(UGenericBrowserType_UTMapMusicInfo,UGenericBrowserType,0,UTEditor)
	/**
	* Initialize the supported classes for this browser type.
	*/
	virtual void Init();
};

class UUTUnrealEdEngine : public UUnrealEdEngine
{
public:
    //## BEGIN PROPS UTUnrealEdEngine
    //## END PROPS UTUnrealEdEngine

    DECLARE_CLASS(UUTUnrealEdEngine,UUnrealEdEngine,0|CLASS_Transient|CLASS_Config,UTEditor)
    NO_DEFAULT_CONSTRUCTOR(UUTUnrealEdEngine)
};

#undef DECLARE_CLASS
#undef DECLARE_CASTED_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef DECLARE_ABSTRACT_CASTED_CLASS
#endif // !INCLUDED_UTEDITOR_CLASSES
#endif // !NAMES_ONLY


#ifndef NAMES_ONLY
#undef AUTOGENERATE_FUNCTION
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef UTEDITOR_NATIVE_DEFS
#define UTEDITOR_NATIVE_DEFS

#define AUTO_INITIALIZE_REGISTRANTS_UTEDITOR \
	UGenericBrowserType_UTMapMusicInfo::StaticClass(); \
	UUTMapMusicInfoFactoryNew::StaticClass(); \
	UUTUnrealEdEngine::StaticClass(); \

#endif // UTEDITOR_NATIVE_DEFS

#ifdef NATIVES_ONLY
#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_SIZE_NODIE(UGenericBrowserType_UTMapMusicInfo)
VERIFY_CLASS_SIZE_NODIE(UUTUnrealEdEngine)
#endif // VERIFY_CLASS_SIZES
#endif // !ENUMS_ONLY

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
