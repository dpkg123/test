/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#ifndef VARYING_HIGH
#define VARYING_HIGH varying highp 
#endif

#ifndef VARYING_MEDIUM
#define VARYING_MEDIUM varying mediump 
#endif

#ifndef VARYING_LOW
#define VARYING_LOW varying lowp 
#endif

#ifndef VARYING_DEFAULT
#define VARYING_DEFAULT VARYING_MEDIUM 
#endif


 
/**
 * Performance settings
 */
 
/** When enabled (slower), we'll renormalize unit vectors interpolated per pixel */
#define RENORMALIZE_INTERPOLATED_NORMALS 0

/** When enabled (slower), we'll compute the binormal in the pixel shader instead of transporting it from the vertex shader */
#define COMPUTE_BINORMAL_IN_PIXEL_SHADER 0

/** When enabled (faster), we'll use a much faster power function for vertex specular lighting, but the power is fixed */
#define USE_VERTEX_FIXED_SPECULAR_POWER_APPROXIMATION 0

/** When enabled (faster), we'll use a much faster power function for per-pixel specular lighting, but the power is fixed */
#define USE_PIXEL_FIXED_SPECULAR_POWER_APPROXIMATION 0

/** When enabled (faster) we use blinn-phone half-vector specular instead of reflection-based phong spec */
#define USE_HALF_VECTOR_SPECULAR_MODEL 1

/** When enabled (slower), we use reflection-based environment maps, otherwise we use view-direction based look ups */
// @todo: Expose this as a material option?
#define USE_REFLECTION_BASED_ENVIRONMENT_MAPS 1

/** When enabled (faster), we'll perform per-pixel lighting in tangent space, otherwise world space */
#define PER_PIXEL_LIGHTING_IN_TANGENT_SPACE 1

/** When enabled (slower), computes sky lighting per pixel */
// @todo: Expose this as a material option?
#define ALLOW_PER_PIXEL_DYNAMIC_SKY_LIGHT 0

/** When enabled (slower), computes environment mapping per pixel */
// @todo: Expose this as a material option?
#define ALLOW_PER_PIXEL_ENVIRONMENT_MAPPING 1

/** When enabled (slower), computes rim lighting per pixel */
// @todo: Expose this as a material option?
#define ALLOW_PER_PIXEL_RIM_LIGHTING 1

/** When enabled (faster), uses a simple sky lighting model with linear falloff */
#define USE_FASTER_SKY_LIGHT 1

// In the vertex or pixel shader, CameraWorldPosition is always equal to the local space origin as we
// pre-translate the view around the mesh's local space before rendering
#define CameraWorldPosition vec3( 0, 0, 0 )


