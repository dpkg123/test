/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
    Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

#include "IpDrvNames.h"

// Split enums from the rest of the header so they can be included earlier
// than the rest of the header file by including this file twice with different
// #define wrappers. See Engine.h and look at EngineClasses.h for an example.
#if !NO_ENUMS && !defined(NAMES_ONLY)

#ifndef INCLUDED_IPDRV_UIPRIVATE_ENUMS
#define INCLUDED_IPDRV_UIPRIVATE_ENUMS 1


#endif // !INCLUDED_IPDRV_UIPRIVATE_ENUMS
#endif // !NO_ENUMS

#if !ENUMS_ONLY

#ifndef NAMES_ONLY
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

#ifndef INCLUDED_IPDRV_UIPRIVATE_CLASSES
#define INCLUDED_IPDRV_UIPRIVATE_CLASSES 1
#define ENABLE_DECLARECLASS_MACRO 1
#include "UnObjBas.h"
#undef ENABLE_DECLARECLASS_MACRO

class UOnlinePlaylistProvider : public UUIResourceDataProvider
{
public:
    //## BEGIN PROPS OnlinePlaylistProvider
    INT PlaylistId;
    TArrayNoInit<FName> PlaylistGameTypeNames;
    FStringNoInit DisplayName;
    INT Priority;
    //## END PROPS OnlinePlaylistProvider

    DECLARE_CLASS(UOnlinePlaylistProvider,UUIResourceDataProvider,0|CLASS_Transient|CLASS_Config,IpDrv)
    static const TCHAR* StaticConfigName() {return TEXT("Playlist");}

    NO_DEFAULT_CONSTRUCTOR(UOnlinePlaylistProvider)
};

#define UCONST_PRIVATEPROVIDERTAG TEXT("PlaylistsPrivate")
#define UCONST_RECMODEPROVIDERTAG TEXT("PlaylistsRecMode")
#define UCONST_UNRANKEDPROVIDERTAG TEXT("PlaylistsUnranked")
#define UCONST_RANKEDPROVIDERTAG TEXT("PlaylistsRanked")

struct UIDataStore_OnlinePlaylists_eventGetMatchTypeForPlaylistId_Parms
{
    INT PlaylistId;
    INT ReturnValue;
    UIDataStore_OnlinePlaylists_eventGetMatchTypeForPlaylistId_Parms(EEventParm)
    {
    }
};
struct UIDataStore_OnlinePlaylists_eventInit_Parms
{
    UIDataStore_OnlinePlaylists_eventInit_Parms(EEventParm)
    {
    }
};
class UUIDataStore_OnlinePlaylists : public UUIDataStore, public IUIListElementProvider
{
public:
    //## BEGIN PROPS UIDataStore_OnlinePlaylists
    FStringNoInit ProviderClassName;
    class UClass* ProviderClass;
    TArrayNoInit<class UUIResourceDataProvider*> RankedDataProviders;
    TArrayNoInit<class UUIResourceDataProvider*> UnrankedDataProviders;
    TArrayNoInit<class UUIResourceDataProvider*> RecModeDataProviders;
    TArrayNoInit<class UUIResourceDataProvider*> PrivateDataProviders;
    class UOnlinePlaylistManager* PlaylistMan;
    //## END PROPS UIDataStore_OnlinePlaylists

    virtual INT GetProviderCount(FName ProviderTag) const;
    UBOOL GetResourceProviders(FName ProviderTag,TArray<class UUIResourceDataProvider*>& out_Providers) const;
    UBOOL GetResourceProviderFields(FName ProviderTag,TArray<FName>& ProviderFieldTags) const;
    UBOOL GetProviderFieldValue(FName ProviderTag,FName SearchField,INT ProviderIndex,struct FUIProviderScriptFieldValue& out_FieldValue) const;
    INT FindProviderIndexByFieldValue(FName ProviderTag,FName SearchField,const struct FUIProviderScriptFieldValue& ValueToSearchFor) const;
    UBOOL GetPlaylistProvider(FName ProviderTag,INT ProviderIndex,class UUIResourceDataProvider*& out_Provider);
    DECLARE_FUNCTION(execGetProviderCount)
    {
        P_GET_NAME(ProviderTag);
        P_FINISH;
        *(INT*)Result=this->GetProviderCount(ProviderTag);
    }
    DECLARE_FUNCTION(execGetResourceProviders)
    {
        P_GET_NAME(ProviderTag);
        P_GET_TARRAY_REF(class UUIResourceDataProvider*,out_Providers);
        P_FINISH;
        *(UBOOL*)Result=this->GetResourceProviders(ProviderTag,out_Providers);
    }
    DECLARE_FUNCTION(execGetResourceProviderFields)
    {
        P_GET_NAME(ProviderTag);
        P_GET_TARRAY_REF(FName,ProviderFieldTags);
        P_FINISH;
        *(UBOOL*)Result=this->GetResourceProviderFields(ProviderTag,ProviderFieldTags);
    }
    DECLARE_FUNCTION(execGetProviderFieldValue)
    {
        P_GET_NAME(ProviderTag);
        P_GET_NAME(SearchField);
        P_GET_INT(ProviderIndex);
        P_GET_STRUCT_INIT_REF(struct FUIProviderScriptFieldValue,out_FieldValue);
        P_FINISH;
        *(UBOOL*)Result=this->GetProviderFieldValue(ProviderTag,SearchField,ProviderIndex,out_FieldValue);
    }
    DECLARE_FUNCTION(execFindProviderIndexByFieldValue)
    {
        P_GET_NAME(ProviderTag);
        P_GET_NAME(SearchField);
        P_GET_STRUCT_INIT_REF(struct FUIProviderScriptFieldValue,ValueToSearchFor);
        P_FINISH;
        *(INT*)Result=this->FindProviderIndexByFieldValue(ProviderTag,SearchField,ValueToSearchFor);
    }
    DECLARE_FUNCTION(execGetPlaylistProvider)
    {
        P_GET_NAME(ProviderTag);
        P_GET_INT(ProviderIndex);
        P_GET_OBJECT_REF(UUIResourceDataProvider,out_Provider);
        P_FINISH;
        *(UBOOL*)Result=this->GetPlaylistProvider(ProviderTag,ProviderIndex,out_Provider);
    }
    INT eventGetMatchTypeForPlaylistId(INT PlaylistId)
    {
        UIDataStore_OnlinePlaylists_eventGetMatchTypeForPlaylistId_Parms Parms(EC_EventParm);
        Parms.ReturnValue=0;
        Parms.PlaylistId=PlaylistId;
        ProcessEvent(FindFunctionChecked(IPDRV_GetMatchTypeForPlaylistId),&Parms);
        return Parms.ReturnValue;
    }
    void eventInit()
    {
        ProcessEvent(FindFunctionChecked(IPDRV_Init),NULL);
    }
    DECLARE_CLASS(UUIDataStore_OnlinePlaylists,UUIDataStore,0|CLASS_Transient|CLASS_Config,IpDrv)
    static const TCHAR* StaticConfigName() {return TEXT("Game");}

    virtual UObject* GetUObjectInterfaceUIListElementProvider(){return this;}
	/* === UUIDataStore_GameResource interface === */
	/**
	 * Finds or creates the UIResourceDataProvider instances used by online playlists, and stores the result by ranked or unranked provider types
	 */
	virtual void InitializeListElementProviders();

	/**
	 * Finds the data provider associated with the tag specified.
	 *
	 * @param	ProviderTag		The tag of the provider to find.  Must match the ProviderTag value for one of elements
	 *							in the ElementProviderTypes array, though it can contain an array index (in which case
	 *							the array index will be removed from the ProviderTag value passed in).
	 * @param	InstanceIndex	If ProviderTag contains an array index, this will be set to the array index value that was parsed.
	 *
	 * @return	a data provider instance (or CDO if no array index was included in ProviderTag) for the element provider
	 *			type associated with ProviderTag.
	 */
	class UUIResourceDataProvider* ResolveProviderReference( FName& ProviderTag, INT* InstanceIndex=NULL ) const;

	/* === IUIListElementProvider interface === */
	/**
	 * Retrieves the list of all data tags contained by this element provider which correspond to list element data.
	 *
	 * @return	the list of tags supported by this element provider which correspond to list element data.
	 */
	virtual TArray<FName> GetElementProviderTags();

	/**
	 * Returns the number of list elements associated with the data tag specified.
	 *
	 * @param	FieldName	the name of the property to get the element count for.  guaranteed to be one of the values returned
	 *						from GetElementProviderTags.
	 *
	 * @return	the total number of elements that are required to fully represent the data specified.
	 */
	virtual INT GetElementCount( FName FieldName );

	/**
	 * Retrieves the list elements associated with the data tag specified.
	 *
	 * @param	FieldName		the name of the property to get the element count for.  guaranteed to be one of the values returned
	 *							from GetElementProviderTags.
	 * @param	out_Elements	will be filled with the elements associated with the data specified by DataTag.
	 *
	 * @return	TRUE if this data store contains a list element data provider matching the tag specified.
	 */
	virtual UBOOL GetListElements( FName FieldName, TArray<INT>& out_Elements );

	/**
	 * Determines whether a member of a collection should be considered "enabled" by subscribed lists.  Disabled elements will still be displayed in the list
	 * but will be drawn using the disabled state.
	 *
	 * @param	FieldName			the name of the collection data field that CollectionIndex indexes into.
	 * @param	CollectionIndex		the index into the data field collection indicated by FieldName to check
	 *
	 * @return	TRUE if FieldName doesn't correspond to a valid collection data field, CollectionIndex is an invalid index for that collection,
	 *			or the item is actually enabled; FALSE only if the item was successfully resolved into a data field value, but should be considered disabled.
	 */
	virtual UBOOL IsElementEnabled( FName FieldName, INT CollectionIndex );

	/**
	 * Retrieves a list element for the specified data tag that can provide the list with the available cells for this list element.
	 * Used by the UI editor to know which cells are available for binding to individual list cells.
	 *
	 * @param	FieldName		the tag of the list element data provider that we want the schema for.
	 *
	 * @return	a pointer to some instance of the data provider for the tag specified.  only used for enumerating the available
	 *			cell bindings, so doesn't need to actually contain any data (i.e. can be the CDO for the data provider class, for example)
	 */
	virtual TScriptInterface<class IUIListElementCellProvider> GetElementCellSchemaProvider( FName FieldName );

	/**
	 * Retrieves a UIListElementCellProvider for the specified data tag that can provide the list with the values for the cells
	 * of the list element indicated by CellValueProvider.DataSourceIndex
	 *
	 * @param	FieldName		the tag of the list element data field that we want the values for
	 * @param	ListIndex		the list index for the element to get values for
	 *
	 * @return	a pointer to an instance of the data provider that contains the value for the data field and list index specified
	 */
	virtual TScriptInterface<class IUIListElementCellProvider> GetElementCellValueProvider( FName FieldName, INT ListIndex );

	/* === UIDataStore interface === */
	/**
	 * Calls the script event so that it can access the playlist manager
	 */
	virtual void InitializeDataStore()
	{
		Super::InitializeDataStore();
		eventInit();
	}

	/**
	 * Loads the classes referenced by the ElementProviderTypes array.
	 */
	virtual void LoadDependentClasses();

	/**
	 * Called when this data store is added to the data store manager's list of active data stores.
	 *
	 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
	 *							associated with a particular player; NULL if this is a global data store.
	 */
	virtual void OnRegister( ULocalPlayer* PlayerOwner );

	/**
	 * Resolves PropertyName into a list element provider that provides list elements for the property specified.
	 *
	 * @param	PropertyName	the name of the property that corresponds to a list element provider supported by this data store
	 *
	 * @return	a pointer to an interface for retrieving list elements associated with the data specified, or NULL if
	 *			there is no list element provider associated with the specified property.
	 */
	virtual TScriptInterface<class IUIListElementProvider> ResolveListElementProvider( const FString& PropertyName );

	/* === UIDataProvider interface === */
	/**
	 * Resolves the value of the data field specified and stores it in the output parameter.
	 *
	 * @param	FieldName		the data field to resolve the value for;  guaranteed to correspond to a property that this provider
	 *							can resolve the value for (i.e. not a tag corresponding to an internal provider, etc.)
	 * @param	out_FieldValue	receives the resolved value for the property specified.
	 *							@see GetDataStoreValue for additional notes
	 * @param	ArrayIndex		optional array index for use with data collections
	 *
	 * @todo - not yet implemented
	 */
	virtual UBOOL GetFieldValue( const FString& FieldName, FUIProviderFieldValue& out_FieldValue, INT ArrayIndex=INDEX_NONE );

	/**
	 * Generates filler data for a given tag.  This is used by the editor to generate a preview that gives the
	 * user an idea as to what a bound datastore will look like in-game.
	 *
 	 * @param		DataTag		the tag corresponding to the data field that we want filler data for
 	 *
	 * @return		a string of made-up data which is indicative of the typical [resolved] value for the specified field.
	 */
	virtual FString GenerateFillerData( const FString& DataTag );

	/**
	 * Gets the list of data fields exposed by this data provider.
	 *
	 * @param	out_Fields	will be filled in with the list of tags which can be used to access data in this data provider.
	 *						Will call GetScriptDataTags to allow script-only child classes to add to this list.
	 */
	virtual void GetSupportedDataFields( TArray<FUIDataProviderField>& out_Fields );

	/* === UObject interface === */
	/** Required since maps are not yet supported by script serialization */
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	virtual void Serialize( FArchive& Ar );

	/**
	 * Called from ReloadConfig after the object has reloaded its configuration data.  Reinitializes the collection of list element providers.
	 */
	virtual void PostReloadConfig( UProperty* PropertyThatWasLoaded );

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	return TRUE if property values were added to the map.
	 */
	virtual UBOOL GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags=0 ) const;
};

#undef DECLARE_CLASS
#undef DECLARE_CASTED_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef DECLARE_ABSTRACT_CASTED_CLASS
#endif // !INCLUDED_IPDRV_UIPRIVATE_CLASSES
#endif // !NAMES_ONLY

AUTOGENERATE_FUNCTION(UUIDataStore_OnlinePlaylists,-1,execGetPlaylistProvider);
AUTOGENERATE_FUNCTION(UUIDataStore_OnlinePlaylists,-1,execFindProviderIndexByFieldValue);
AUTOGENERATE_FUNCTION(UUIDataStore_OnlinePlaylists,-1,execGetProviderFieldValue);
AUTOGENERATE_FUNCTION(UUIDataStore_OnlinePlaylists,-1,execGetResourceProviderFields);
AUTOGENERATE_FUNCTION(UUIDataStore_OnlinePlaylists,-1,execGetResourceProviders);
AUTOGENERATE_FUNCTION(UUIDataStore_OnlinePlaylists,-1,execGetProviderCount);

#ifndef NAMES_ONLY
#undef AUTOGENERATE_FUNCTION
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef IPDRV_UIPRIVATE_NATIVE_DEFS
#define IPDRV_UIPRIVATE_NATIVE_DEFS

#define AUTO_INITIALIZE_REGISTRANTS_IPDRV_UIPRIVATE \
	UOnlinePlaylistProvider::StaticClass(); \
	UUIDataStore_OnlinePlaylists::StaticClass(); \
	GNativeLookupFuncs.Set(FName("UIDataStore_OnlinePlaylists"), GIpDrvUUIDataStore_OnlinePlaylistsNatives); \

#endif // IPDRV_UIPRIVATE_NATIVE_DEFS

#ifdef NATIVES_ONLY
FNativeFunctionLookup GIpDrvUUIDataStore_OnlinePlaylistsNatives[] = 
{ 
	MAP_NATIVE(UUIDataStore_OnlinePlaylists, execGetPlaylistProvider)
	MAP_NATIVE(UUIDataStore_OnlinePlaylists, execFindProviderIndexByFieldValue)
	MAP_NATIVE(UUIDataStore_OnlinePlaylists, execGetProviderFieldValue)
	MAP_NATIVE(UUIDataStore_OnlinePlaylists, execGetResourceProviderFields)
	MAP_NATIVE(UUIDataStore_OnlinePlaylists, execGetResourceProviders)
	MAP_NATIVE(UUIDataStore_OnlinePlaylists, execGetProviderCount)
	{NULL, NULL}
};

#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_OFFSET_NODIE(UOnlinePlaylistProvider,OnlinePlaylistProvider,PlaylistId)
VERIFY_CLASS_OFFSET_NODIE(UOnlinePlaylistProvider,OnlinePlaylistProvider,Priority)
VERIFY_CLASS_SIZE_NODIE(UOnlinePlaylistProvider)
VERIFY_CLASS_OFFSET_NODIE(UUIDataStore_OnlinePlaylists,UIDataStore_OnlinePlaylists,ProviderClassName)
VERIFY_CLASS_OFFSET_NODIE(UUIDataStore_OnlinePlaylists,UIDataStore_OnlinePlaylists,PlaylistMan)
VERIFY_CLASS_SIZE_NODIE(UUIDataStore_OnlinePlaylists)
#endif // VERIFY_CLASS_SIZES
#endif // !ENUMS_ONLY

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
