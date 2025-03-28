/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

//
// Implementation of plugin entry points (NPP_*)
//
#include "pluginbase.h"

// here the plugin creates a plugin instance object which  will be associated with this newly created NPP instance and will do all the necessary job
NPError NPP_New( NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved )
{   
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	NPError rv = NPERR_NO_ERROR;

	// create a new plugin instance object
	// initialization will be done when the associated window is ready
	nsPluginCreateData ds;

	ds.instance = instance;
	ds.type = pluginType; 
	ds.mode = mode; 
	ds.argc = argc; 
	ds.argn = argn; 
	ds.argv = argv; 
	ds.saved = saved;

	nsPluginInstanceBase* plugin = NS_NewPluginInstance( &ds );
	if( plugin == NULL )
	{
		return( NPERR_OUT_OF_MEMORY_ERROR );
	}

	// associate the plugin instance object with NPP instance
	instance->pdata = ( void* )plugin;
	return( rv );
}

// here is the place to clean up and destroy the nsUE3PluginInstance object
NPError NPP_Destroy( NPP instance, NPSavedData** save )
{
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	NPError rv = NPERR_NO_ERROR;

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin != NULL ) 
	{
		plugin->shut();
		NS_DestroyPluginInstance( plugin );
	}

	return( rv ); 
}

// during this call we know when the plugin window is ready or
// is about to be destroyed so we can do some gui specific
// initialization and shutdown
NPError NPP_SetWindow( NPP instance, NPWindow* pNPWindow )
{    
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	NPError rv = NPERR_NO_ERROR;

	if( pNPWindow == NULL )
	{
		return( NPERR_GENERIC_ERROR );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL )
	{
		return( NPERR_GENERIC_ERROR );
	}

	// window just created
	if( !plugin->isInitialized() && ( pNPWindow->window != NULL ) ) 
	{ 
		if( !plugin->init( pNPWindow ) ) 
		{
			NS_DestroyPluginInstance( plugin );
			return( NPERR_MODULE_LOAD_FAILED_ERROR );
		}
	}

	// window goes away
	if( ( pNPWindow->window == NULL ) && plugin->isInitialized() )
	{
		return( plugin->SetWindow( pNPWindow ) );
	}

	// window resized?
	if( plugin->isInitialized() && ( pNPWindow->window != NULL ) )
	{
		return( plugin->SetWindow( pNPWindow ) );
	}

	// this should not happen, nothing to do
	if( ( pNPWindow->window == NULL ) && !plugin->isInitialized() )
	{
		return( plugin->SetWindow( pNPWindow ) );
	}

	return( rv );
}

NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype )
{
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( NPERR_GENERIC_ERROR );
	}

	NPError rv = plugin->NewStream( type, stream, seekable, stype );
	return( rv );
}

int32 NPP_WriteReady( NPP instance, NPStream* stream )
{
	if( instance == NULL )
	{
		return( 0x0fffffff );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( 0x0fffffff );
	}

	int32 rv = plugin->WriteReady( stream );
	return( rv );
}

int32 NPP_Write( NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer )
{   
	if( instance == NULL )
	{
		return( len );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( len );
	}

	int32 rv = plugin->Write( stream, offset, len, buffer );
	return( rv );
}

NPError NPP_DestroyStream( NPP instance, NPStream* stream, NPError reason )
{
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( NPERR_GENERIC_ERROR );
	}

	NPError rv = plugin->DestroyStream( stream, reason );
	return( rv );
}

void NPP_StreamAsFile( NPP instance, NPStream* stream, const char* fname )
{
	if( instance == NULL )
	{
		return;
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return;
	}

	plugin->StreamAsFile( stream, fname );
}

void NPP_Print( NPP instance, NPPrint* printInfo )
{
	if( instance == NULL )
	{
		return;
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return;
	}

	plugin->Print( printInfo );
}

void NPP_URLNotify( NPP instance, const char* url, NPReason reason, void* notifyData )
{
	if( instance == NULL )
	{
		return;
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return;
	}

	plugin->URLNotify( url, reason, notifyData );
}

NPError	NPP_GetValue( NPP instance, NPPVariable variable, void* value )
{
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( NPERR_GENERIC_ERROR );
	}

	NPError rv = plugin->GetValue( variable, value );
	return( rv );
}

NPError NPP_SetValue( NPP instance, NPNVariable variable, void* value )
{
	if( instance == NULL )
	{
		return( NPERR_INVALID_INSTANCE_ERROR );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( NPERR_GENERIC_ERROR );
	}

	NPError rv = plugin->SetValue( variable, value );
	return( rv );
}

int16 NPP_HandleEvent( NPP instance, void* event )
{
	if( instance == NULL )
	{
		return( 0 );
	}

	nsPluginInstanceBase* plugin = ( nsPluginInstanceBase* )instance->pdata;
	if( plugin == NULL ) 
	{
		return( 0 );
	}

	uint16 rv = plugin->HandleEvent( event );
	return( rv );
}

#ifdef OJI
jref NPP_GetJavaClass( void )
{
	return( NULL );
}
#endif


