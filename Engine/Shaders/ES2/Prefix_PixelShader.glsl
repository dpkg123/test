/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

//////////////////////////
// SHARED ALPHA CODE
//////////////////////////

#if USE_ALPHAKILL

// kill the pixel if the alpha fails the test
#define ALPHAKILL(Alpha) if (Alpha <= AlphaTestRef) { discard; }

// parameter to get alpha test value
uniform float AlphaTestRef;

#else

// noop when no alpha kill is used
#define ALPHAKILL(Alpha) ;

#endif
