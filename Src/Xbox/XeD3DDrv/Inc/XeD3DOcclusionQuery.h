/*=============================================================================
	XeD3DOcclusionQuery.h: XeD3D occlusion query RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_XeD3D_RHI

/*-----------------------------------------------------------------------------
RHI occlusion query types
-----------------------------------------------------------------------------*/

/** 
 * Occlusion query resource type. 
 * This wraps IDirect3DQuery9 and provides refcounting instead of using IDirect3DQuery9::AddRef and RemoveRef directly, since those use slow interlocked functions.
 */
class FXeOcclusionQuery : public FXeResource
{
public:

	/** constructor initialized w/ existing D3D resource */
	FXeOcclusionQuery(IDirect3DQuery9* InQuery)
	:	Query(InQuery)
	{
		checkSlow(InQuery);
	}

	virtual ~FXeOcclusionQuery()
	{
		FRefCountedObject::~FRefCountedObject();
		Query->Release();
	}

	/** Pointer to the actual D3D query */
	IDirect3DQuery9* Query;
};

typedef TXeGPUResourceRef<IDirect3DQuery9,FXeOcclusionQuery> FOcclusionQueryRHIRef;
typedef FXeOcclusionQuery* FOcclusionQueryRHIParamRef;

#endif
