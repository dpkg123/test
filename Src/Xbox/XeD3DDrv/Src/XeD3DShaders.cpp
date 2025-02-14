/*=============================================================================
	D3DShaders.cpp: D3D shader RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"

#if USE_XeD3D_RHI

// Pull the shader cache stats into this context
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_CompressedShaderMemory);
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_UncompressedShaderMemory);

FVertexShaderRHIRef RHICreateVertexShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	FVertexShaderRHIRef VertexShaderRHIRef( new FXeVertexShader( Code.GetData() ) );
	return VertexShaderRHIRef;
}

FPixelShaderRHIRef RHICreatePixelShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	FPixelShaderRHIRef PixelShaderRHIRef( new FXePixelShader( Code.GetData() ) );
	return PixelShaderRHIRef;
}

/**
 * Vertex shader GPU resource constructor.
 * 
 * @param	Code	Compiled code created by D3DXCompileShader
 */
FXeVertexShader::FXeVertexShader( const void* Code ) 
:	FXeGPUResource(RUF_Static)
{
	XGMICROCODESHADERPARTS MicroShaderParts;
	XGGetMicrocodeShaderParts( Code, &MicroShaderParts );

	IDirect3DVertexShader9* VertexShader = (IDirect3DVertexShader9*) new char[MicroShaderParts.cbCachedPartSize];
	XGSetVertexShaderHeader( VertexShader, MicroShaderParts.cbCachedPartSize, &MicroShaderParts );

	// Doing manual alignment, so allocate enough for the aligned pointer
	BaseAddress	= appPhysicalAlloc( MicroShaderParts.cbPhysicalPartSize + D3DSHADER_ALIGNMENT - 1, CACHE_WriteCombine );
	// Align the base address as required by D3D
	void* AlignedBaseAddress = Align(BaseAddress, D3DSHADER_ALIGNMENT);

	appMemcpy( AlignedBaseAddress, MicroShaderParts.pPhysicalPart, MicroShaderParts.cbPhysicalPartSize );
	XGRegisterVertexShader( VertexShader, AlignedBaseAddress );

	Resource = VertexShader;

	static UINT NextId = 0;
	check(NextId < UINT_MAX);
	Id = NextId++;

#if TRACK_GPU_RESOURCES || STATS_FAST
	PhysicalSize = MicroShaderParts.cbPhysicalPartSize;
	VirtualSize  = MicroShaderParts.cbCachedPartSize;
#endif

	INC_DWORD_STAT_BY(STAT_VertexShaderMemory,PhysicalSize+VirtualSize);
	INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, PhysicalSize+VirtualSize);
}

#if TRACK_GPU_RESOURCES || STATS_FAST
/** Destructor, used when tracking memory. */
FXeVertexShader::~FXeVertexShader()
{
	DEC_DWORD_STAT_BY(STAT_VertexShaderMemory,PhysicalSize+VirtualSize);
	DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, PhysicalSize+VirtualSize);
}
#endif

/**
 * Pixel shader GPU resource constructor.
 * 
 * @param	Code	Compiled code created by D3DXCompileShader
 */
FXePixelShader::FXePixelShader( const void* Code )
:	FXeGPUResource(RUF_Static)
{
	XGMICROCODESHADERPARTS MicroShaderParts;
	XGGetMicrocodeShaderParts( Code, &MicroShaderParts );

	IDirect3DPixelShader9* PixelShader = (IDirect3DPixelShader9*) new char[MicroShaderParts.cbCachedPartSize];
	XGSetPixelShaderHeader( PixelShader, MicroShaderParts.cbCachedPartSize, &MicroShaderParts );

	// Doing manual alignment, so allocate enough for the aligned pointer
	BaseAddress	= appPhysicalAlloc( MicroShaderParts.cbPhysicalPartSize + D3DSHADER_ALIGNMENT - 1, CACHE_WriteCombine );
	// Align the base address as required by D3D
	void* AlignedBaseAddress = Align(BaseAddress, D3DSHADER_ALIGNMENT);

	appMemcpy( AlignedBaseAddress, MicroShaderParts.pPhysicalPart, MicroShaderParts.cbPhysicalPartSize );
	XGRegisterPixelShader( PixelShader, AlignedBaseAddress );

	Resource = PixelShader;

	static UINT NextId = 0;
	check(NextId < UINT_MAX);
	Id = NextId++;

#if TRACK_GPU_RESOURCES || STATS_FAST
	PhysicalSize = MicroShaderParts.cbPhysicalPartSize;
	VirtualSize  = MicroShaderParts.cbCachedPartSize;
#endif

	INC_DWORD_STAT_BY(STAT_PixelShaderMemory,PhysicalSize+VirtualSize);
	INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, PhysicalSize+VirtualSize);
}

#if TRACK_GPU_RESOURCES || STATS_FAST
/** Destructor, used when tracking memory. */
FXePixelShader::~FXePixelShader()
{
	DEC_DWORD_STAT_BY(STAT_PixelShaderMemory,PhysicalSize+VirtualSize);
	DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, PhysicalSize+VirtualSize);
}
#endif

/**
 * Key used to map a set of unique decl/vs/ps combinations to
 * a vertex shader resource
 */
class FShaderCombinationKey
{
public:
	/**
	 * Constructor (Init)
	 * @param InVertexDeclaration - existing decl
	 * @param InStreamStrides - array of vertex stream strides
	 * @param InVertexShaderId - existing vs
	 * @param InPixelShader - existing ps
	 */
	FShaderCombinationKey(
		void* InVertexDeclaration, 
		DWORD* InStreamStrides, 
		UINT InVertexShaderId, 
		UINT InPixelShaderId
		)
		: VertexDeclaration(InVertexDeclaration)
		, VertexShaderId(InVertexShaderId)
		, PixelShaderId(InPixelShaderId)
	{
		for( UINT Idx=0; Idx < MaxVertexElementCount; ++Idx )
		{
			StreamStrides[Idx] = (BYTE)InStreamStrides[Idx];
		}
	}

	/**
	 * Equality is based on decl, vertex shader and pixel shader 
	 * @param Other - instance to compare against
	 * @return TRUE if equal
	 */
	UBOOL operator == (const FShaderCombinationKey& Other) const
	{
		return( 
			VertexDeclaration == Other.VertexDeclaration && 
			VertexShaderId == Other.VertexShaderId && 
			PixelShaderId == Other.PixelShaderId && 
			(0 == appMemcmp(StreamStrides, Other.StreamStrides, sizeof(StreamStrides)))
			);
	}

	/**
	 * Get the hash for this type. 
	 * @param Key - struct to hash
	 * @return dword hash based on type
	 */
	friend DWORD GetTypeHash(const FShaderCombinationKey& Key)
	{
		return PointerHash(
			Key.VertexDeclaration, 
			PointerHash(
				(void*)Key.VertexShaderId, 
				PointerHash(
					(void*)Key.PixelShaderId, 
					appMemCrc(Key.StreamStrides, sizeof(Key.StreamStrides))
					)
				)
			);
	}

private:
	/** Note: we intentionally do not AddRef() these shaders
	  *  because we only need their pointer values for hashing purposes.
	  *  The object which owns the corresponding RHIBoundShaderState 
	  *  should have references to these objects in its material
	  *  and vertex factory.
	  */

	/** vertex decl for this combination */
	void* VertexDeclaration;
	/** vs for this combination */
	UINT VertexShaderId;
	/** ps for this combination */
	UINT PixelShaderId;
	/** assuming stides are always smaller than 8-bits */
	BYTE StreamStrides[MaxVertexElementCount];
};

/**
 * Set of bound shaders
 */
class FBoundShaderCache
{
public:
	/**
	 * Constructor
	 */
	FBoundShaderCache()
	{
		// We are assuming that strides can fit into 8-bits
        check((BYTE)GPU_MAX_VERTEX_STRIDE == GPU_MAX_VERTEX_STRIDE);
		PendingDeletes = 0;
	}

	/**
	 * Destructor
	 */
	~FBoundShaderCache()
	{
		PurgeAndRelax(TRUE);

		check(0 == BoundVertexShaderMap.Num());
		SET_MEMORY_STAT(STAT_VertexDeclarationCacheBytes, 0);
	}

	/**
	 * Create a new vertex shader using the given vs decl,vs,ps combination. Tries to find an existing entry in the set first.
	 * 
	 * @param InVertexDeclaration - existing vertex decl
	 * @param InStreamStrides - optional stream strides
	 * @param InVertexShader - existing vertex shader
	 * @param InPixelShader - existing pixel shader
	 * @return bound vertex shader entry
	 */
	FCachedVertexShader* GetBoundShader(
		FVertexDeclarationRHIParamRef InVertexDeclaration, 
		DWORD *InStreamStrides, 
		FVertexShaderRHIParamRef InVertexShader, 
		FPixelShaderRHIParamRef InPixelShader
		)
	{
		PurgeAndRelax(FALSE);

		FShaderCombinationKey Key(InVertexDeclaration, InStreamStrides, InVertexShader->GetId(), InPixelShader ? InPixelShader->GetId() : UINT_MAX);
		FCachedVertexShader **Value = BoundVertexShaderMap.Find(Key);

		if (!Value)
		{
			FCachedVertexShader *NewCachedShader = new FCachedVertexShader;
			NewCachedShader->VertexShader = CloneVertexShader(InVertexShader);
			NewCachedShader->CacheRefCount = 1;
			
			// Workaround for bind
			if (!InPixelShader)
			{
				TShaderMapRef<FNULLPixelShader> NullPixelShader(GetGlobalShaderMap());
				InPixelShader = NullPixelShader->GetPixelShader();
			}
            
			IDirect3DVertexShader9* XeD3DVertexShader = NewCachedShader->VertexShader;
			VERIFYD3DRESULT(XeD3DVertexShader->Bind(0, InVertexDeclaration, InStreamStrides, (IDirect3DPixelShader9*) InPixelShader->Resource ));
			BoundVertexShaderMap.Set(Key, NewCachedShader);
			SET_MEMORY_STAT(STAT_VertexDeclarationCacheBytes, BoundVertexShaderMap.GetAllocatedSize() + TempVertexShaderCode.GetAllocatedSize());

			return NewCachedShader;
		}
		else if ((*Value)->CacheRefCount == 0)
		{
			// This one was formerly marked for deletion, but we can re-use it.  Drop the pending delete count by 1
			check(PendingDeletes > 0);
			-- PendingDeletes;
		}
		
		++ (*Value)->CacheRefCount;
		return *Value;
	}

	/**
	 * Deferred cleanup only when we hit the threshold of shaders to delete. 
	 * Iterates over the map of bound shaders and deletes unreferenced entries.
	 * @param bForce - ignore threshold and just delete unreferenced shaders
	 */
	void PurgeAndRelax(UBOOL bForce)
	{
		INT DeletedSoFar = 0;
		if (PendingDeletes > PendingDeleteThreshold || bForce)
		{
			TMap<FShaderCombinationKey, FCachedVertexShader *>::TIterator ShaderMapIt(BoundVertexShaderMap);
			while (ShaderMapIt)
			{
				const FCachedVertexShader* Shader = ShaderMapIt.Value();

				if (0 == Shader->CacheRefCount)
				{
					delete Shader;
					ShaderMapIt.RemoveCurrent();
					++ DeletedSoFar;
				}
				++ ShaderMapIt;
			}
			check(PendingDeletes == DeletedSoFar);
			PendingDeletes = 0;
		}
		SET_MEMORY_STAT(STAT_VertexDeclarationCacheBytes, BoundVertexShaderMap.GetAllocatedSize() + TempVertexShaderCode.GetAllocatedSize());
	}

	/**
	 * Increment a new shader delete request
	 */
	void IncrementDeletedShaderCount()
	{
		++ PendingDeletes;
	}

private:

	/**
	 * Create a unique copy of the given vs
	 * @param InVertexShader - instance to copy from
	 * @return newly created vertex shader 
	 */
	FVertexShaderRHIParamRef CloneVertexShader(FVertexShaderRHIParamRef InVertexShader)
	{
		UINT					ShaderCodeSize;
		IDirect3DVertexShader9* XeD3DVertexShader = (IDirect3DVertexShader9*) InVertexShader->Resource;

		VERIFYD3DRESULT(XeD3DVertexShader->GetFunction(NULL, &ShaderCodeSize));
		TempVertexShaderCode.Reserve(ShaderCodeSize);
		VERIFYD3DRESULT(XeD3DVertexShader->GetFunction(TempVertexShaderCode.GetTypedData(), &ShaderCodeSize));

		return new FXeVertexShader( TempVertexShaderCode.GetData() );
	}

	/** map decl/vs/ps combination to bound vertex shader resource */
	TMap<FShaderCombinationKey, FCachedVertexShader *> BoundVertexShaderMap;
	/** stores vertex shader code in CloneVertexShader */
	TArray<BYTE> TempVertexShaderCode;

	/** Cleaning the map gets expensive; best not to do it on every delete */
	UINT PendingDeletes;
	static const UINT PendingDeleteThreshold = 200;
};

FBoundShaderCache GBoundShaderCache;

/**
 * Keep track of the number of deleted shader requests
 */
void MarkShaderForDeletion()
{
	GBoundShaderCache.IncrementDeletedShaderCount();
}

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param PixelShader - existing pixel shader
 */
FBoundShaderStateRHIRef RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclaration, 
	DWORD *StreamStrides, 
	FVertexShaderRHIParamRef VertexShader, 
	FPixelShaderRHIParamRef PixelShader
	)
{
	FBoundShaderStateRHIRef NewBoundShaderState;

	NewBoundShaderState.VertexDeclaration = VertexDeclaration;
	NewBoundShaderState.BoundVertexShader = GBoundShaderCache.GetBoundShader(VertexDeclaration, StreamStrides, VertexShader, PixelShader);
	NewBoundShaderState.OriginalVertexShader = VertexShader;
	NewBoundShaderState.PixelShader = PixelShader;

	return NewBoundShaderState;
}

#endif
