/*=============================================================================
	PS3ResourceArray.h: PS3 Resource array definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3RESOURCEARRAY_H__
#define __PS3RESOURCEARRAY_H__

/** alignment for supported resource types */
enum EResourceAlignment
{
	VERTEXBUFFER_ALIGNMENT	=16,
	INDEXBUFFER_ALIGNMENT	=16
};

// @todo: If this was file was in included from Engine, not Core, we could use GPS3Gcm directly without this extern
extern void* PS3GPUAllocate(INT InSize, INT InAlignment, UBOOL bAllocateInSystemMemory);
extern void PS3GPUFree(void* InPtr, UBOOL bFreeFromSystemMemory);

#define TResourceArray TPS3ResourceArray

/** The PS3 GPU resource allocation policy always allocates the elements indirectly. */
template<DWORD Alignment = DEFAULT_ALIGNMENT>
class TPS3GPUResourceAllocator
{
public:

	enum { NeedsElementType = TRUE };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType():
			Data(NULL)
		{}

		/** ENoInit constructor. */
		ForElementType(ENoInit)
		{}

		/** Destructor. */
		~ForElementType()
		{
			if( Data )
			{
				PS3GPUFree(Data, bNeedsCPUAccess);
				Data = NULL;
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(
			INT PreviousNumElements,
			INT NumElements,
			INT NumBytesPerElement
			)
		{
			if( Data )
			{
				PS3GPUFree(Data, bNeedsCPUAccess);
				Data = NULL;
			}

			if (NumElements > 0)
			{
				Data = (ElementType*)PS3GPUAllocate(NumElements * NumBytesPerElement, Alignment, bNeedsCPUAccess);
			}
		}
		INT CalculateSlack(
			INT NumElements,
			INT CurrentNumSlackElements,
			INT NumBytesPerElement
			) const
		{
			// Don't allocate slack for GPU resources.
			return NumElements;
		}

		INT GetAllocatedSize(INT NumAllocatedElements, INT NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

		UBOOL GetAllowCPUAccess() const
		{
			return bNeedsCPUAccess;
		}

		void SetAllowCPUAccess(UBOOL bInNeedsCPUAccess)
		{
			bNeedsCPUAccess = bInNeedsCPUAccess;
		}

	private:

		/** A pointer to the container's elements. */
		ElementType* Data;

		/** Whether the elements need to be accessed by the CPU. */
		UBOOL bNeedsCPUAccess;
	};
	
	typedef ForElementType<FScriptContainerElement> ForAnyElementType;
};

/**
 * A array which allocates memory which can be used for rendering resources.
 * This is currently just a placeholder.
 */
template< typename ElementType, DWORD Alignment=DEFAULT_ALIGNMENT >
class TPS3ResourceArray
:	public FResourceArrayInterface
,	public TArray<ElementType,TPS3GPUResourceAllocator<Alignment> >
{
public:
	typedef TPS3GPUResourceAllocator<Alignment> Allocator;
	typedef TArray<ElementType,Allocator> Super;

	// FResourceArrayInterface

	/** 
	* Constructor 
	*/
	TPS3ResourceArray(UBOOL bInNeedsCPUAccess=FALSE)
		:	Super()
	{
		this->AllocatorInstance.SetAllowCPUAccess(bInNeedsCPUAccess);
	}

	// FResourceArrayInterface

	/**
	* Access the resource data
	* @return ptr to start of resource data array
	*/
	virtual const void* GetResourceData() const 
	{ 
		return this->GetData(); 
	}

	/**
	 * @return size of resource data allocation
	 */
	virtual DWORD GetResourceDataSize() const
	{
		return Super::Num() * sizeof(ElementType);
	}
	
	/**
	 * Called after resource for this array has been created.
	 * Always keep resource array data on PS3 since it is used directly as a GPU resource (either
	 * in main of GPU memory)
	 */
	virtual void Discard()
	{
	}

	/**
	 * @return TRUE if the resource array is static and shouldn't be modified
	 */
	virtual UBOOL IsStatic() const
	{
		// resource arrays are always static
		return TRUE;
	}

	/**
	* @return TRUE if the resource keeps a copy of its resource data after the RHI resource has been created
	*/
	virtual UBOOL GetAllowCPUAccess() const
	{
		return this->AllocatorInstance.GetAllowCPUAccess();
	}

	// FScriptArray interface - don't allow array modification

	void Insert( INT /*Index*/, INT /*Count*/, INT /*ElementSize*/, DWORD /*Alignment*/ )
	{
		check(0);
	}
	INT Add( INT /*Count*/, INT /*ElementSize*/, DWORD /*Alignment*/ )
	{
		check(0); return 0;
	}
	void Empty( INT /*ElementSize*/, DWORD /*Alignment*/, INT Slack=0 )
	{
		check(0);
	}
	void Remove( INT /*Index*/, INT /*Count*/, INT /*ElementSize*/, DWORD /*Alignment*/ )
	{
		check(0);
	}

	// TArray interface - don't allow array modification

	INT Add( INT Count=1 )
	{
		check(0);
		return 0;
	}
	INT AddItem( const ElementType& /*Item*/ )
	{
		check(0);
		return 0;
	}
	void Insert( INT /*Index*/, INT Count=1 )
	{
		check(0);
	}
	INT InsertItem( const ElementType& /*Item*/, INT /*Index*/ )
	{
		check(0);
		return 0;
	}
	void Remove( INT /*Index*/, INT Count=1 )
	{
		check(0);
	}
	void Empty( INT Slack=0 )
	{
		check(0);
	}
	void Append(const Super& /*Source*/)
	{
		check(0);
	}	

	// Assignment operators.
	
	/**
	 * Assignment operator. This is currently the only method which allows for 
	 * modifying an existing resource array. These are very slow however for GPU arrays
	 * @todo: Put a printout/breakpoint so we know when this happens?
	 */
	template<typename Allocator>
	TPS3ResourceArray& operator=(const TArray<ElementType,Allocator>& Other)
	{
		Super::Empty(Other.Num());
		Super::Add(Other.Num());
		appMemcpy(this->GetData(), Other.GetData(), Other.Num() * sizeof(ElementType));
		return *this;
	}

	TPS3ResourceArray& operator=(const TPS3ResourceArray<ElementType,Alignment>& Other)
	{
		Super::Empty(Other.Num());
		Super::Add(Other.Num());
		appMemcpy(this->GetData(), Other.GetData(), Other.Num() * sizeof(ElementType));
		return *this;
	}



	/**
	 * Serializer for this class
	 * @param Ar - archive to serialize to
 	 * @param ResourceArray - resource array data to serialize 
	 */
	friend FArchive& operator<<(FArchive& Ar,TPS3ResourceArray& ResourceArray)
	{
		ResourceArray.CountBytes( Ar );
		if( sizeof(ElementType)==1 )
		{
			// Serialize simple bytes which require no construction or destruction.
			Ar << ResourceArray.ArrayNum;
			check( ResourceArray.ArrayNum >= 0 );
			if( Ar.IsLoading() )
			{
				ResourceArray.Super::Empty(ResourceArray.ArrayNum);
				ResourceArray.Super::Add(ResourceArray.ArrayNum);
			}
			// @todo: DMA this with SPU, possibly
			Ar.Serialize(ResourceArray.GetData(), ResourceArray.Num());
		}
		else if(Ar.IsLoading())
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			ResourceArray.Super::Empty(NewNum);
			ResourceArray.Super::Add(NewNum);
			for (INT i=0; i<NewNum; i++)
			{
				Ar << ResourceArray(i);
			}
		}
		else if( Ar.IsSaving() )
		{
			// no saving allowed/needed for Xe resource arrays
			check(0);
		}
		else
		{			
			Ar << ResourceArray.ArrayNum;
			for( INT i=0; i<ResourceArray.ArrayNum; i++ )
			{
				Ar << ResourceArray(i);
			}
		}
		return Ar;
	}

	/**
	 * Serialize data as a single block. See TArray::BulkSerialize for more info.
	 *
	 * IMPORTANT:
	 *   - This is Overridden from UnTemplate.h TArray::BulkSerialize  Please make certain changes are propogated accordingly
	 *
	 * @param Ar	FArchive to bulk serialize this TArray to/from
	 */
	void BulkSerialize(FArchive& Ar)
	{
		// Serialize element size to detect mismatch across platforms.
		INT SerializedElementSize = 0;
		Ar << SerializedElementSize;

		if( Ar.IsSaving() || Ar.Ver() < GPackageFileVersion || Ar.LicenseeVer() < GPackageFileLicenseeVersion )
		{
			Ar << *this;
		}
		else 
		{
			this->CountBytes(Ar); 
			if( Ar.IsLoading() )
			{
				// Basic sanity checking to ensure that sizes match.
				checkf(SerializedElementSize==0 || SerializedElementSize==sizeof(ElementType),TEXT("Expected %i, Got: %i"),sizeof(ElementType),SerializedElementSize);
				INT NewArrayNum;
				Ar << NewArrayNum;
				Super::Empty(NewArrayNum);
				Super::Add(NewArrayNum);
				Ar.Serialize(this->GetData(), NewArrayNum * sizeof(ElementType));
			}
		}
	}
};

#endif
