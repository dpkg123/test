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
// Main plugin entry point implementation -- exports from the 
// plugin library
//
#include "npplat.h"
#include "pluginbase.h"

NPNetscapeFuncs NPNFuncs;

NPError OSCALL NP_Shutdown( void )
{
	NS_PluginShutdown();
	return( NPERR_NO_ERROR );
}

static NPError fillPluginFunctionTable( NPPluginFuncs* aNPPFuncs )
{
	if( aNPPFuncs == NULL )
	{
		return( NPERR_INVALID_FUNCTABLE_ERROR );
	}

	// Set up the plugin function table that Netscape will use to call us. Netscape needs to know about our version and size   
	// and have a UniversalProcPointer for every function we implement.
	aNPPFuncs->version       = ( NP_VERSION_MAJOR << 8 ) | NP_VERSION_MINOR;
	aNPPFuncs->newp          = NPP_New;
	aNPPFuncs->destroy       = NPP_Destroy;
	aNPPFuncs->setwindow     = NPP_SetWindow;
	aNPPFuncs->newstream     = NPP_NewStream;
	aNPPFuncs->destroystream = NPP_DestroyStream;
	aNPPFuncs->asfile        = NPP_StreamAsFile;
	aNPPFuncs->writeready    = NPP_WriteReady;
	aNPPFuncs->write         = NPP_Write;
	aNPPFuncs->print         = NPP_Print;
	aNPPFuncs->event         = NPP_HandleEvent;
	aNPPFuncs->urlnotify     = NPP_URLNotify;
	aNPPFuncs->getvalue      = NPP_GetValue;
	aNPPFuncs->setvalue      = NPP_SetValue;
#ifdef OJI
	aNPPFuncs->javaClass     = NULL;
#endif

	return( NPERR_NO_ERROR );
}

static NPError fillNetscapeFunctionTable( NPNetscapeFuncs* aNPNFuncs )
{
	if( aNPNFuncs == NULL )
	{
		return( NPERR_INVALID_FUNCTABLE_ERROR );
	}

	if( HIBYTE( aNPNFuncs->version ) > NP_VERSION_MAJOR )
	{
		return( NPERR_INCOMPATIBLE_VERSION_ERROR );
	}

	if( aNPNFuncs->size < sizeof( NPNetscapeFuncs ) )
	{
		return( NPERR_INVALID_FUNCTABLE_ERROR );
	}

	NPNFuncs.size             = aNPNFuncs->size;
	NPNFuncs.version          = aNPNFuncs->version;
	NPNFuncs.geturlnotify     = aNPNFuncs->geturlnotify;
	NPNFuncs.geturl           = aNPNFuncs->geturl;
	NPNFuncs.posturlnotify    = aNPNFuncs->posturlnotify;
	NPNFuncs.posturl          = aNPNFuncs->posturl;
	NPNFuncs.requestread      = aNPNFuncs->requestread;
	NPNFuncs.newstream        = aNPNFuncs->newstream;
	NPNFuncs.write            = aNPNFuncs->write;
	NPNFuncs.destroystream    = aNPNFuncs->destroystream;
	NPNFuncs.status           = aNPNFuncs->status;
	NPNFuncs.uagent           = aNPNFuncs->uagent;
	NPNFuncs.memalloc         = aNPNFuncs->memalloc;
	NPNFuncs.memfree          = aNPNFuncs->memfree;
	NPNFuncs.memflush         = aNPNFuncs->memflush;
	NPNFuncs.reloadplugins    = aNPNFuncs->reloadplugins;
#ifdef OJI
	NPNFuncs.getJavaEnv       = aNPNFuncs->getJavaEnv;
	NPNFuncs.getJavaPeer      = aNPNFuncs->getJavaPeer;
#endif
	NPNFuncs.getvalue         = aNPNFuncs->getvalue;
	NPNFuncs.setvalue         = aNPNFuncs->setvalue;
	NPNFuncs.invalidaterect   = aNPNFuncs->invalidaterect;
	NPNFuncs.invalidateregion = aNPNFuncs->invalidateregion;
	NPNFuncs.forceredraw      = aNPNFuncs->forceredraw;

	return( NPERR_NO_ERROR );
}

NPError OSCALL NP_Initialize( NPNetscapeFuncs* aNPNFuncs )
{
	NPError rv = fillNetscapeFunctionTable( aNPNFuncs );
	if( rv != NPERR_NO_ERROR )
	{
		return( rv );
	}

	return( NS_PluginInitialize() );
}

NPError OSCALL NP_GetEntryPoints( NPPluginFuncs* aNPPFuncs )
{
	return( fillPluginFunctionTable( aNPPFuncs ) );
}

