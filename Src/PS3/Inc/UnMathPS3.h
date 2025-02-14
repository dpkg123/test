/*=============================================================================
	UnMathPS3.h: PS3-specific vector intrinsics

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef HEADER_UNMATHPS3
#define HEADER_UNMATHPS3


// Include the intrinsic functions and PS3 math SDK:
#include <altivec.h>
#include <vectormath/c/vectormath_aos.h>

// Force-inline work-around for the following:
#define my_perm( a1, a2, a3 )	((vector float) vec_vperm ((a1), (a2), (vector unsigned char) (a3)))
#define my_ld(a1, a2)			((vector float) vec_lvx((a1), (unsigned char*)(a2)))
#define my_lvsl( a1, a2 )		((vector unsigned char) vec_lvsl ((a1), (unsigned char*) (a2)))
#define my_sel( a1, a2, a3 )	((vector float) vec_vsel ((vector float) (a1), (vector float) (a2), (a3)))
#define my_st( a1, a2, a3 )		vec_st ((vector float) (a1), (a2), (a3))
#define my_add( a1, a2 )		((vector float) vec_vaddfp ((vector float) (a1), (vector float) (a2)))
#define my_sub( a1, a2 )		((vector float) vec_vsubfp ((vector float) (a1), (vector float) (a2)))
#define my_madd( a1, a2, a3 )	((vector float) vec_vmaddfp ((vector float) (a1), (vector float) (a2), (vector float) (a3)))
#define my_packsu8( a1, a2 )	((vector unsigned char) vec_vpkuhus ((vector unsigned short) (a1), (vector unsigned short) (a2)))
#define my_packsu16( a1, a2 )	((vector unsigned short) vec_vpkuwus ((vector unsigned int) (a1), (vector unsigned int) (a2)))
#define my_ste( a1, a2, a3 )	vec_ste ((vector float	) (a1), (a2), (a3))
#define my_xor( a1, a2 )		((vector float) __vxor ((vector signed int) a1, (vector signed int) (a2)))
#define my_spltisw( s5 )		((vector float) vec_vspltisw (s5))
#define my_min( a, b )			((vector float) vec_vminfp ((vector float) (a), (vector float) (b)))
#define my_max( a, b )			((vector float) vec_vmaxfp ((vector float) (a), (vector float) (b)))
#define my_slw( a, b )			((vector float) vec_vslw((vector unsigned int) (a), (vector unsigned int) (b)))

/*=============================================================================
 *	Helpers:
 *============================================================================*/

/** 16-byte vector register type */
typedef vector float			VectorRegister;

/**
 * Returns a bitwise equivalent vector based on 4 DWORDs.
 *
 * @param X		1st DWORD component
 * @param Y		2nd DWORD component
 * @param Z		3rd DWORD component
 * @param W		4th DWORD component
 * @return		Bitwise equivalent vector with 4 floats
 */
inline VectorRegister MakeVectorRegister( DWORD X, DWORD Y, DWORD Z, DWORD W ) __attribute__ ((always_inline));
inline VectorRegister MakeVectorRegister( DWORD X, DWORD Y, DWORD Z, DWORD W )
{
	union { VectorRegister v; DWORD f[4]; } Tmp;
	Tmp.f[0] = X;
	Tmp.f[1] = Y;
	Tmp.f[2] = Z;
	Tmp.f[3] = W;
	return Tmp.v;
}

/**
 * Returns a vector based on 4 FLOATs.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @param W		4th FLOAT component
 * @return		Vector of the 4 FLOATs
 */
inline VectorRegister MakeVectorRegister( FLOAT X, FLOAT Y, FLOAT Z, FLOAT W ) __attribute__ ((always_inline));
inline VectorRegister MakeVectorRegister( FLOAT X, FLOAT Y, FLOAT Z, FLOAT W )
{
	union { VectorRegister v; FLOAT f[4]; } Tmp;
	Tmp.f[0] = X;
	Tmp.f[1] = Y;
	Tmp.f[2] = Z;
	Tmp.f[3] = W;
	return Tmp.v;
}

#define USE_PRECOMPUTED_SWIZZLE_MASKS (1)

#if !USE_PRECOMPUTED_SWIZZLE_MASKS
/** Makes a swizzle mask that selects each component from either Src1 (0-3) or Src2 (4-7). */
#define SWIZZLEFLOAT( C )	((DWORD)(((C)*4 + 0)<<24) | (((C)*4 + 1)<<16) | (((C)*4 + 2)<<8) | ((C)*4 + 3))
#define SWIZZLEMASK( X, Y, Z, W )		\
	((vector unsigned char) MakeVectorRegister( SWIZZLEFLOAT(X), SWIZZLEFLOAT(Y), SWIZZLEFLOAT(Z), SWIZZLEFLOAT(W) ) )
#else

#define SWIZZLEMASK_EXTERN_XYZW( X, Y, Z, W ) \
	extern const vector unsigned char SWIZZLEMASK_##X##Y##Z##W;

#define SWIZZLEMASK_EXTERN_W( X, Y, Z ) \
	SWIZZLEMASK_EXTERN_XYZW(X,Y,Z,0) \
	SWIZZLEMASK_EXTERN_XYZW(X,Y,Z,1) \
	SWIZZLEMASK_EXTERN_XYZW(X,Y,Z,2) \
	SWIZZLEMASK_EXTERN_XYZW(X,Y,Z,3)

#define SWIZZLEMASK_EXTERN_Z( X, Y ) \
	SWIZZLEMASK_EXTERN_W(X,Y,0) \
	SWIZZLEMASK_EXTERN_W(X,Y,1) \
	SWIZZLEMASK_EXTERN_W(X,Y,2) \
	SWIZZLEMASK_EXTERN_W(X,Y,3)

#define SWIZZLEMASK_EXTERN_Y( X ) \
	SWIZZLEMASK_EXTERN_Z(X,0) \
	SWIZZLEMASK_EXTERN_Z(X,1) \
	SWIZZLEMASK_EXTERN_Z(X,2) \
	SWIZZLEMASK_EXTERN_Z(X,3)

SWIZZLEMASK_EXTERN_Y(0)
SWIZZLEMASK_EXTERN_Y(1)
SWIZZLEMASK_EXTERN_Y(2)
SWIZZLEMASK_EXTERN_Y(3)

#define SWIZZLEMASK( X, Y, Z, W ) SWIZZLEMASK_##X##Y##Z##W

#endif


/*=============================================================================
 *	Constants:
 *============================================================================*/

#include "UnMathVectorConstants.h"

/** Vector that represents negative zero (-0,-0,-0,-0) */
extern const VectorRegister PS3ALTIVEC_NEGZERO;

/** Vector that represents (1,1,1,1) */
extern const VectorRegister PS3ALTIVEC_ONE;

/** Select mask that selects Src1.XYZ and Src2.W to make (X,Y,Z,W). */
extern const VectorRegister PS3ALTIVEC_SELECT_MASK;

/** Swizzle mask that selects each byte in X from Src1 and the last byte of W from Src2 to make (WWWX[0],WWWX[1],WWWX[2],WWWX[3]). */
extern const VectorRegister	PS3ALTIVEC_SWIZZLE_MASK_UNPACK;

/** Swizzle mask that selects each byte in X in reverse order from Src1 and the last byte of W from Src2 to make (WWWX[3],WWWX[2],WWWX[1],WWWX[0]). */
extern const VectorRegister	PS3ALTIVEC_SWIZZLE_MASK_UNPACK_REVERSE;

/** Swizzle mask to form an YZXW vector. */
extern const VectorRegister	PS3ALTIVEC_SWIZZLE_YZXW;

/** Swizzle mask to form a ZXYW vector. */
extern const VectorRegister	PS3ALTIVEC_SWIZZLE_ZXYW;

/**
 * Converts 4 INTs into 4 FLOATs and returns the result.
 *
 * @param Vec	Source INT vector
 * @return		VectorRegister( FLOAT(Vec.x), FLOAT(Vec.y), FLOAT(Vec.z), FLOAT(Vec.w) )
 */
#define VectorItof( Vec )						vec_ctf( (vector signed int) (Vec), 0 )

/**
 * Converts 4 UINTs into 4 FLOATs and returns the result.
 *
 * @param Vec	Source UINT vector
 * @return		VectorRegister( FLOAT(Vec.x), FLOAT(Vec.y), FLOAT(Vec.z), FLOAT(Vec.w) )
 */
#define VectorUitof( Vec )						vec_ctf( (vector unsigned int) (Vec), 0 )

/**
 * Converts 4 FLOATs into 4 INTs (with saturation, rounding towards zero) and returns the result.
 *
 * @param Vec	Source FLOAT vector
 * @return		VectorRegister( INT(Vec.x), INT(Vec.y), INT(Vec.z), INT(Vec.w) )
 */
#define VectorFtoi( Vec )						((vector float ) vec_cts( (Vec), 0 ) )

/**
 * Converts 4 FLOATs into 4 UINTs (with saturation, rounding towards zero) and returns the result.
 *
 * @param Vec	Source FLOAT vector
 * @return		VectorRegister( UINT(Vec.x), UINT(Vec.y), UINT(Vec.z), UINT(Vec.w) )
 */
#define VectorFtoui( Vec )						((vector float ) vec_ctu( (Vec), 0 ) )

/**
 * Converts 16 WORDs from two vectors into 16 BYTEs in a single vector. Values are clamped to [0,255].
 *
 * @param Vec1		1st vector (converts into BYTE 0-7)
 * @param Vec2		2nd vector (converts into BYTE 8-15)
 * @return			A VectorRegister where BYTE 0-7 comes from the 8 WORDs in Vec1 and BYTE 8-15 comes from the 8 WORDs in Vec2.
 */
#define VectorU16ToU8( Vec1, Vec2 )				((vector float ) my_packsu8( (vector unsigned short) (Vec1), (vector unsigned short) (Vec2) ))

/**
 * Converts 8 DWORDs from two vectors into 8 WORDs in a single vector. Values are clamped to [0,65535].
 *
 * @param Vec1		1st vector (converts into WORD 0-3)
 * @param Vec2		2nd vector (converts into WORD 4-7)
 * @return			A VectorRegister where WORD 0-3 comes from the 4 DWORDs in Vec1 and WORD 4-7 comes from the 4 DWORDs in Vec2.
 */
#define VectorU32ToU16( Vec1, Vec2 )			((vector float ) my_packsu16( (vector unsigned int) (Vec1), (vector unsigned int) (Vec2) ))


/*=============================================================================
 *	Intrinsics:
 *============================================================================*/

/**
 * Returns a vector with all zeros.
 *
 * @return		VectorRegister(0.0f, 0.0f, 0.0f, 0.0f)
 */
#define VectorZero()					my_spltisw( 0 )

/**
 * Returns a vector with all ones.
 *
 * @return		VectorRegister(1.0f, 1.0f, 1.0f, 1.0f)
 */
#define VectorOne()						PS3ALTIVEC_ONE

/**
 * Loads 4 FLOATs from unaligned memory.
 *
 * @param Ptr	Unaligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
FORCEINLINE VectorRegister VectorLoad(const void* Ptr)
{
	return my_perm( my_ld(0,(float*)(Ptr)), my_ld(15,(float*)(Ptr)), my_lvsl(0,(float*)(Ptr)) );
}

/**
 * Loads 3 FLOATs from unaligned memory and leaves W undefined.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], undefined)
 */
FORCEINLINE VectorRegister VectorLoadFloat3(const void* Ptr)
{
	return my_perm( my_ld(0,(float*)(Ptr)), my_ld(11,(float*)(Ptr)), my_lvsl(0,(float*)(Ptr)) );
}

/**
 * Loads 3 FLOATs from unaligned memory and sets W=0.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 0.0f)
 */
#define VectorLoadFloat3_W0( Ptr )		my_sel( VectorLoadFloat3(Ptr), VectorZero(), (vector unsigned int)PS3ALTIVEC_SELECT_MASK )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=1.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 1.0f)
 */
#define VectorLoadFloat3_W1( Ptr )		my_sel( VectorLoadFloat3(Ptr), PS3ALTIVEC_ONE, (vector unsigned int)PS3ALTIVEC_SELECT_MASK )

/**
 * Loads 4 FLOATs from aligned memory.
 *
 * @param Ptr	Aligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoadAligned( Ptr )		my_ld( 0, Ptr )

/**
 * Loads 1 FLOAT from unaligned memory and replicates it to all 4 elements.
 *
 * @param Ptr	Unaligned memory pointer to the FLOAT
 * @return		VectorRegister(Ptr[0], Ptr[0], Ptr[0], Ptr[0])
 */
FORCEINLINE VectorRegister VectorLoadFloat1(const void* Ptr)
{
	return vec_splat( my_perm( my_ld(0,(float*)(Ptr)), my_ld(3,(float*)(Ptr)), my_lvsl(0,(float*)(Ptr)) ), 0 );
}

/**
 * Creates a vector out of three FLOATs and leaves W undefined.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @return		VectorRegister(X, Y, Z, undefined)
 */
inline VectorRegister VectorSetFloat3( FLOAT X, FLOAT Y, FLOAT Z ) __attribute__ ((always_inline));
inline VectorRegister VectorSetFloat3( FLOAT X, FLOAT Y, FLOAT Z )
{
	union { VectorRegister v; float f[4]; } Tmp;
	Tmp.f[0] = X;
	Tmp.f[1] = Y;
	Tmp.f[2] = Z;
	return Tmp.v;
}

/**
 * Creates a vector out of four FLOATs.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @param W		4th FLOAT component
 * @return		VectorRegister(X, Y, Z, W)
 */
#define VectorSet( X, Y, Z, W )				MakeVectorRegister( X, Y, Z, W )

/**
 * Stores a vector to aligned memory.
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
#define VectorStoreAligned( Vec, Ptr )		my_st( Vec, 0, (float*)(Ptr) )

/**
 * Stores a vector to memory (aligned or unaligned).
 *
 * @param Vec	Vector to store
 * @param Ptr	Memory pointer
 */
#define VectorStore( Vec, Ptr )				(vec_stvlx( Vec, 0, (float*)(Ptr) ), vec_stvrx(Vec, 16, (float*)(Ptr)))

/**
 * Stores the XYZ components of a vector to unaligned memory.
 *
 * @param Vec	Vector to store XYZ
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat3( Vec, Ptr )		(my_ste(vec_splat((Vec),0), 0, (float*)(Ptr)), my_ste(vec_splat((Vec),1), 4, (float*)(Ptr)), my_ste(vec_splat((Vec),2), 8, (float*)(Ptr)))

/**
 * Stores the X component of a vector to unaligned memory.
 *
 * @param Vec	Vector to store X
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat1( Vec, Ptr )		my_ste(vec_splat((vector float)(Vec),0), 0, (float*)(Ptr))

/**
 * Replicates one element into all four elements and returns the new vector.
 *
 * @param Vec			Source vector
 * @param ElementIndex	Index (0-3) of the element to replicate
 * @return				VectorRegister( Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex] )
 */
#define VectorReplicate( Vec, ElementIndex )	vec_splat( (Vec), (ElementIndex) )

/**
 * Returns the absolute value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( abs(Vec.x), abs(Vec.y), abs(Vec.z), abs(Vec.w) )
 */
FORCEINLINE VectorRegister VectorAbs( const VectorRegister &Vec )
{
	VectorRegister SignMask = my_spltisw(-1);	// SignMask =		(0xffffffff,0xffffffff,0xffffffff,0xffffffff)
    SignMask = my_slw(SignMask, SignMask);		// SignMask << 31	(0x80000000,0x80000000,0x80000000,0x80000000)
	return vec_andc(Vec, SignMask);				// Vec & ~SignMask	(AND with 0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff)
}

/**
 * Returns the negated value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( -Vec.x, -Vec.y, -Vec.z, -Vec.w )
 */
FORCEINLINE VectorRegister VectorNegate( const VectorRegister &Vec )
{
	VectorRegister SignMask = my_spltisw(-1);	// SignMask =		(0xffffffff,0xffffffff,0xffffffff,0xffffffff)
    SignMask = my_slw(SignMask, SignMask);		// SignMask << 31	(0x80000000,0x80000000,0x80000000,0x80000000)
    return vec_xor(Vec, SignMask);				// Vec ^ SignMask	(XOR with 0x80000000,0x80000000,0x80000000,0x80000000)
}

/**
 * Adds two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x+Vec2.x, Vec1.y+Vec2.y, Vec1.z+Vec2.z, Vec1.w+Vec2.w )
 */
#define VectorAdd( Vec1, Vec2 )				my_add( (Vec1), (Vec2) )

/**
 * Subtracts a vector from another (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x-Vec2.x, Vec1.y-Vec2.y, Vec1.z-Vec2.z, Vec1.w-Vec2.w )
 */
#define VectorSubtract( Vec1, Vec2 )		my_sub( (Vec1), (Vec2) )

/**
 * Multiplies two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x*Vec2.x, Vec1.y*Vec2.y, Vec1.z*Vec2.z, Vec1.w*Vec2.w )
 */
#define VectorMultiply( Vec1, Vec2 )		my_madd( (Vec1), (Vec2), PS3ALTIVEC_NEGZERO )

/**
 * Multiplies two vectors (component-wise), adds in the third vector and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @param Vec3	3rd vector
 * @return		VectorRegister( Vec1.x*Vec2.x + Vec3.x, Vec1.y*Vec2.y + Vec3.y, Vec1.z*Vec2.z + Vec3.z, Vec1.w*Vec2.w + Vec3.w )
 */
#define VectorMultiplyAdd( Vec1, Vec2, Vec3 )	my_madd( (Vec1), (Vec2), (Vec3) )


/**
 * Calculates the dot3 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot3(Vec1.xyz, Vec2.xyz), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot3( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Temp = VectorMultiply( Vec1, Vec2 );
	return VectorAdd( VectorReplicate(Temp,0), VectorAdd( VectorReplicate(Temp,1), VectorReplicate(Temp,2) ) );
}

/**
 * Calculates the dot4 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot4(Vec1.xyzw, Vec2.xyzw), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot4( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Temp1, Temp2;
	Temp1 = VectorMultiply( Vec1, Vec2 );
	Temp2 = vec_sld( Temp1, Temp1, 8 );	// Rotate left 8 bytes (Z,W,X,Y).
	Temp1 = VectorAdd( Temp1, Temp2 );	// (X*X + Z*Z, Y*Y + W*W, Z*Z + X*X, W*W + Y*Y)
	Temp2 = vec_sld( Temp1, Temp1, 4 );	// Rotate left 4 bytes (Y,Z,W,X).
	return VectorAdd( Temp1, Temp2 );	// (X*X + Z*Z + Y*Y + W*W, Y*Y + W*W + Z*Z + X*X, Z*Z + X*X + W*W + Y*Y, W*W + Y*Y + X*X + Z*Z)
}


/**
 * Creates a four-part mask based on component-wise == compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x == Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareEQ( Vec1, Vec2 )			(__vector float)vec_cmpeq( (Vec1), (Vec2) )

/**
 * Creates a four-part mask based on component-wise != compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x != Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareNE( Vec1, Vec2 )			(__vector float)vec_xor(VectorCompareEQ( (Vec1), (Vec2) ), my_spltisw(-1))

/**
 * Creates a four-part mask based on component-wise > compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x > Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareGT( Vec1, Vec2 )			(__vector float)vec_cmpgt( (Vec1), (Vec2) )

/**
 * Creates a four-part mask based on component-wise >= compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x >= Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareGE( Vec1, Vec2 )			(__vector float)vec_cmpge( (Vec1), (Vec2) )

/**
 * Does a bitwise vector selection based on a mask (e.g., created from VectorCompareXX)
 *
 * @param Mask  Mask (when 1: use the corresponding bit from Vec1 otherwise from Vec2)
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Mask[i] ? Vec1[i] : Vec2[i] )
 *
 */

#define VectorSelect( Mask, Vec1, Vec2 )		my_sel( (Vec2), (Vec1), (vector unsigned int)(Mask) )

/**
 * Combines two vectors using bitwise OR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] | Vec2[i] )
 */
#define VectorBitwiseOr(Vec1, Vec2)				vec_or((Vec1), (Vec2))

/**
 * Combines two vectors using bitwise AND (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] & Vec2[i] )
 */
#define VectorBitwiseAnd(Vec1, Vec2)			vec_and((Vec1), (Vec2))

/**
 * Combines two vectors using bitwise XOR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] ^ Vec2[i] )
 */
#define VectorBitwiseXor(Vec1, Vec2)			vec_xor((Vec1), (Vec2))

/**
 * Calculates the cross product of two vectors (XYZ components). W is set to 0.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		cross(Vec1.xyz, Vec2.xyz). W is set to 0.
 */
FORCEINLINE VectorRegister VectorCross( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister A_YZXW = my_perm( Vec1, Vec1, PS3ALTIVEC_SWIZZLE_YZXW );
	VectorRegister B_ZXYW = my_perm( Vec2, Vec2, PS3ALTIVEC_SWIZZLE_ZXYW );
	VectorRegister A_ZXYW = my_perm( Vec1, Vec1, PS3ALTIVEC_SWIZZLE_ZXYW );
	VectorRegister B_YZXW = my_perm( Vec2, Vec2, PS3ALTIVEC_SWIZZLE_YZXW );
	return vec_nmsub( A_ZXYW, B_YZXW, VectorMultiply(A_YZXW,B_ZXYW) );
}

/**
 * Calculates x raised to the power of y (component-wise).
 *
 * @param Base		Base vector
 * @param Exponent	Exponent vector
 * @return			VectorRegister( Base.x^Exponent.x, Base.y^Exponent.y, Base.z^Exponent.z, Base.w^Exponent.w )
 */
FORCEINLINE VectorRegister VectorPow( const VectorRegister& Base, const VectorRegister& Exponent )
{
	//@TODO: Optimize this
	union { VectorRegister v; FLOAT f[4]; } B, E;
	B.v = Base;
	E.v = Exponent;
	return MakeVectorRegister( powf(B.f[0], E.f[0]), powf(B.f[1], E.f[1]), powf(B.f[2], E.f[2]), powf(B.f[3], E.f[3]) );
}

/**
* Returns an estimate of 1/sqrt(c) for each component of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(t), 1/sqrt(t), 1/sqrt(t), 1/sqrt(t))
*/
#define VectorReciprocalSqrt(Vec)					vec_rsqrte(Vec)

/**
 * Computes an estimate of the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( (Estimate) 1.0f / Vec.x, (Estimate) 1.0f / Vec.y, (Estimate) 1.0f / Vec.z, (Estimate) 1.0f / Vec.w )
 */
#define VectorReciprocal(Vec)						vec_re(Vec)

/**
* Return Reciprocal Length of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(rlen, rlen, rlen, rlen) when rlen = 1/sqrt(dot4(V))
*/
FORCEINLINE VectorRegister VectorReciprocalLen( const VectorRegister& Vector )
{
	VectorRegister RecipLen = VectorDot4( Vector, Vector );
	return VectorReciprocalSqrt( RecipLen );
}

/**
* Return the reciprocal of the square root of each component
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(Vec.X), 1/sqrt(Vec.Y), 1/sqrt(Vec.Z), 1/sqrt(Vec.W))
*/
FORCEINLINE VectorRegister VectorReciprocalSqrtAccurate(const VectorRegister& Vec)
{
	// Perform a single pass of Newton-Raphson iteration on the hardware estimate
	//    v^-0.5 = x
	// => x^2 = v^-1
	// => 1/(x^2) = v
	// => F(x) = x^-2 - v
	//    F'(x) = -2x^-3

	//    x1 = x0 - F(x0)/F'(x0)
	// => x1 = x0 + 0.5 * (x0^-2 - Vec) * x0^3
	// => x1 = x0 + 0.5 * (x0 - Vec * x0^3)
	// => x1 = x0 + x0 * (0.5 - 0.5 * Vec * x0^2)

	const VectorRegister OneHalf = GlobalVectorConstants::FloatOneHalf;
	const VectorRegister VecDivBy2 = VectorMultiply(Vec, OneHalf);

	// Initial estimate
	const VectorRegister x0 = VectorReciprocalSqrt(Vec);

	// First iteration
	const VectorRegister x0Squared = VectorMultiply(x0, x0);
	const VectorRegister InnerTerm0 = VectorSubtract(OneHalf, VectorMultiply(VecDivBy2, x0Squared));
	const VectorRegister x1 = VectorMultiplyAdd(x0, InnerTerm0, x0);

	return x1;
}

/**
 * Computes the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( 1.0f / Vec.x, 1.0f / Vec.y, 1.0f / Vec.z, 1.0f / Vec.w )
 */
FORCEINLINE VectorRegister VectorReciprocalAccurate(const VectorRegister& Vec)
{
	// Perform two passes of Newton-Raphson iteration on the hardware estimate
	//   x1 = x0 - f(x0) / f'(x0)
	//
	//    1 / Vec = x
	// => x * Vec = 1 
	// => F(x) = x * Vec - 1
	//    F'(x) = Vec
	// => x1 = x0 - (x0 * Vec - 1) / Vec
	//
	// Since 1/Vec is what we're trying to solve, use an estimate for it, x0
	// => x1 = x0 - (x0 * Vec - 1) * x0 = 2 * x0 - Vec * x0^2 

	// Initial estimate
	const VectorRegister x0 = VectorReciprocal(Vec);

	// First iteration
	const VectorRegister x0Squared = VectorMultiply(x0, x0);
	const VectorRegister x0Times2 = VectorAdd(x0, x0);
	const VectorRegister x1 = VectorSubtract(x0Times2, VectorMultiply(Vec, x0Squared));

	// Second iteration
	const VectorRegister x1Squared = VectorMultiply(x1, x1);
	const VectorRegister x1Times2 = VectorAdd(x1, x1);
	const VectorRegister x2 = VectorSubtract(x1Times2, VectorMultiply(Vec, x1Squared));

	return x2;
}


/**
* Normalize vector
*
* @param Vector		Vector to normalize
* @return			Normalized VectorRegister
*/
FORCEINLINE VectorRegister VectorNormalize( const VectorRegister& Vector )
{
	return VectorMultiply( Vector, VectorReciprocalLen( Vector ) );
}

/**
* Loads XYZ and sets W=0
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 0.0f)
*/
#define VectorSet_W0( Vec )		my_sel( (Vec), VectorZero(), (vector unsigned int)PS3ALTIVEC_SELECT_MASK )

/**
* Loads XYZ and sets W=1.
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 1.0f)
*/
#define VectorSet_W1( Vec )		my_sel( (Vec), VectorOne(), (vector unsigned int)PS3ALTIVEC_SELECT_MASK )

/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
FORCEINLINE void VectorMatrixMultiply( void *Result, const void* Matrix1, const void* Matrix2 )
{
	const VectorRegister *A	= (const VectorRegister *) Matrix1;
	const VectorRegister *B	= (const VectorRegister *) Matrix2;
	VectorRegister *R		= (VectorRegister *) Result;
	VectorRegister Temp, R0, R1, R2, R3;

	// First row of result (Matrix1[0] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[0], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[0], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[0], 2 ), B[2], Temp );
	R0		= VectorMultiplyAdd( VectorReplicate( A[0], 3 ), B[3], Temp );

	// Second row of result (Matrix1[1] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[1], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[1], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[1], 2 ), B[2], Temp );
	R1		= VectorMultiplyAdd( VectorReplicate( A[1], 3 ), B[3], Temp );

	// Third row of result (Matrix1[2] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[2], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[2], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[2], 2 ), B[2], Temp );
	R2		= VectorMultiplyAdd( VectorReplicate( A[2], 3 ), B[3], Temp );

	// Fourth row of result (Matrix1[3] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[3], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[3], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[3], 2 ), B[2], Temp );
	R3		= VectorMultiplyAdd( VectorReplicate( A[3], 3 ), B[3], Temp );

	// Store result
	R[0] = R0;
	R[1] = R1;
	R[2] = R2;
	R[3] = R3;
}

/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
#define VectorMatrixInverse( DstMatrix, SrcMatrix ) vmathM4Inverse( (VmathMatrix4*) DstMatrix, (const VmathMatrix4*) SrcMatrix )

/**
 * Returns the minimum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( min(Vec1.x,Vec2.x), min(Vec1.y,Vec2.y), min(Vec1.z,Vec2.z), min(Vec1.w,Vec2.w) )
 */
#define VectorMin( Vec1, Vec2 )				my_min( (Vec1), (Vec2) )

/**
 * Returns the maximum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( max(Vec1.x,Vec2.x), max(Vec1.y,Vec2.y), max(Vec1.z,Vec2.z), max(Vec1.w,Vec2.w) )
 */
#define VectorMax( Vec1, Vec2 )				my_max( (Vec1), (Vec2) )

/**
 * Swizzles the 4 components of a vector and returns the result.
 *
 * @param Vec		Source vector
 * @param X			Index for which component to use for X (literal 0-3)
 * @param Y			Index for which component to use for Y (literal 0-3)
 * @param Z			Index for which component to use for Z (literal 0-3)
 * @param W			Index for which component to use for W (literal 0-3)
 * @return			The swizzled vector
 */
#define VectorSwizzle( Vec, X, Y, Z, W )	my_perm( (Vec), (Vec), SWIZZLEMASK(X,Y,Z,W) )

/**
 * Merges the XYZ components of one vector with the W component of another vector and returns the result.
 *
 * @param VecXYZ	Source vector for XYZ_
 * @param VecW		Source register for ___W (note: the fourth component is used, not the first)
 * @return			VectorRegister(VecXYZ.x, VecXYZ.y, VecXYZ.z, VecW.w)
 */
#define VectorMergeVecXYZ_VecW( VecXYZ, VecW )	VectorSelect( PS3ALTIVEC_SELECT_MASK, VecW, VecXYZ )

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( FLOAT(Ptr[0]), FLOAT(Ptr[1]), FLOAT(Ptr[2]), FLOAT(Ptr[3]) )
 */
#define VectorLoadByte4( Ptr )				VectorUitof( my_perm(VectorLoadFloat1(Ptr), VectorZero(), PS3ALTIVEC_SWIZZLE_MASK_UNPACK) )

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs in reversed order.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( FLOAT(Ptr[3]), FLOAT(Ptr[2]), FLOAT(Ptr[1]), FLOAT(Ptr[0]) )
 */
#define VectorLoadByte4Reverse( Ptr )		VectorUitof( my_perm(VectorLoadFloat1(Ptr), VectorZero(), PS3ALTIVEC_SWIZZLE_MASK_UNPACK_REVERSE) )

/**
 * Converts the 4 FLOATs in the vector to 4 BYTEs, clamped to [0,255], and stores to unaligned memory.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Vec			Vector containing 4 FLOATs
 * @param Ptr			Unaligned memory pointer to store the 4 BYTEs.
 */
#define VectorStoreByte4( Vec, Ptr )		\
 	VectorStoreFloat1( \
		VectorU16ToU8( \
			VectorU32ToU16( \
				VectorFtoui(Vec), \
				VectorZero()), \
			VectorZero()),  \
		(Ptr) )

/**
 * Returns non-zero if any element in Vec1 is greater than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) || (Vec1.y > Vec2.y) || (Vec1.z > Vec2.z) || (Vec1.w > Vec2.w)
 */
#define VectorAnyGreaterThan(Vec1, Vec2)	vec_any_gt( (Vec1), (Vec2) )

/**
 * Resets the floating point registers so that they can be used again.
 * Some intrinsics use these for MMX purposes (e.g. VectorLoadByte4 and VectorStoreByte4).
 */
#define VectorResetFloatRegisters()

/**
 * Returns the control register.
 *
 * @return			The DWORD control register
 */
#define VectorGetControlRegister()		0

/**
 * Sets the control register.
 *
 * @param ControlStatus		The DWORD control status value to set
 */
#define	VectorSetControlRegister(ControlStatus)

/**
 * Control status bit to round all floating point math results towards zero.
 */
#define VECTOR_ROUND_TOWARD_ZERO		0

/**
* Multiplies two quaternions: The order matters.
*
* @param Result	Returns Quat1 * Quat2
* @param Quat1	First quaternion
* @param Quat2	Second quaternion
*/
FORCEINLINE VectorRegister VectorQuaternionMultiply2( const VectorRegister& Quat1, const VectorRegister& Quat2 )
{
	VectorRegister Result = VectorMultiply(VectorReplicate(Quat1, 3), Quat2);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 0), VectorSwizzle(Quat2, 3,2,1,0)), GlobalVectorConstants::QMULTI_SIGN_MASK0, Result);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 1), VectorSwizzle(Quat2, 2,3,0,1)), GlobalVectorConstants::QMULTI_SIGN_MASK1, Result);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 2), VectorSwizzle(Quat2, 1,0,3,2)), GlobalVectorConstants::QMULTI_SIGN_MASK2, Result);

	return Result;
}

/**
* Multiplies two quaternions: The order matters
*
* @param Result	Pointer to where the result should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
FORCEINLINE void VectorQuaternionMultiply( void* RESTRICT Result, const void* RESTRICT Quat1, const void* RESTRICT Quat2)
{
	*((VectorRegister *)Result) = VectorQuaternionMultiply2(*((const VectorRegister *)Quat1), *((const VectorRegister *)Quat2));
}

// Returns true if the vector contains a component that is either NAN or +/-infinite.
inline UBOOL VectorContainsNaNOrInfinite(const VectorRegister& Vec)
{
	//@TODO: Only checks for NAN, not for infinites
	return !vec_all_numeric(Vec);
}

// To be continued...


#endif
