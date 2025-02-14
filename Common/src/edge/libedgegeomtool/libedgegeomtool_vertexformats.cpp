/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/libedgegeomtool/libedgegeomtool.h"
#include "edge/libedgegeomtool/libedgegeomtool_internal.h"

// These are the same vertex formats supported by default in the Edge Geometry runtime
// (see edgegeom_decompress.cpp and edgegeom_compress.cpp).  So, obviously, these definitions
// must be kept in sync with the ones defined in the runtime!

//-------------------
// Macros for common SPU vertex attribute types
#define IATTR_U8(COUNT, ATTRIBUTEID)		{ kSpuAttr_U8, COUNT, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_U8N(COUNT, ATTRIBUTEID)		{ kSpuAttr_U8N, COUNT, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_I16(COUNT, ATTRIBUTEID)		{ kSpuAttr_I16, COUNT, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_I16N(COUNT, ATTRIBUTEID)		{ kSpuAttr_I16N, COUNT, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_F16(COUNT, ATTRIBUTEID)		{ kSpuAttr_F16, COUNT, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_F32(COUNT, ATTRIBUTEID)		{ kSpuAttr_F32, COUNT, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_X11Y11Z10N(ATTRIBUTEID)		{ kSpuAttr_X11Y11Z10N, 1, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_UNITVECTOR3(ATTRIBUTEID)		{ kSpuAttr_UnitVector, 3, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_UNITVECTOR4(ATTRIBUTEID)		{ kSpuAttr_UnitVector, 4, ATTRIBUTEID, 0, {0,0,0,0}, {0,0,0,0}, }
#define IATTR_FIXEDPOINT(COUNT, ATTRIBUTEID, XINT, XFRAC, YINT, YFRAC, ZINT, ZFRAC, WINT, WFRAC)	{ kSpuAttr_FixedPoint, COUNT, ATTRIBUTEID, 0, {XINT,YINT,ZINT,WINT}, {XFRAC,YFRAC,ZFRAC,WFRAC}, }

// These macros can be used with the ordinary types (U8..F32).  More complex types need to be spelled out.
#define IPOSITION(TYPE)  IATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_POSITION)
#define INORMAL(TYPE)    IATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_NORMAL)
#define ICOLOR(TYPE)     IATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_COLOR)
#define ITANGENT3(TYPE)  IATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
#define ITANGENT4(TYPE)  IATTR_##TYPE(4, EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
#define IUV(TYPE)        IATTR_##TYPE(2, EDGE_GEOM_ATTRIBUTE_ID_UV0)
#define IBINORMAL(TYPE)  IATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_BINORMAL)
#define ICOLOR4(TYPE)    IATTR_##TYPE(4, EDGE_GEOM_ATTRIBUTE_ID_COLOR)

#define IINSTANCEDCOLOR(TYPE) IATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_INSTANCED_COLOR)

// input attribute array definitions
#define ipos					{ IPOSITION(F32) }
#define iposNormTan				{ IPOSITION(F32), IATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), IATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT) }
#define iposNormTanFlip			{ IPOSITION(F32), IATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), ITANGENT4(I16N) }
#define iposNormTanBin			{ IPOSITION(F32), IATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), IATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT), IATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_BINORMAL) }
#define iposNormTanFlip2		{ IATTR_FIXEDPOINT(3, EDGE_GEOM_ATTRIBUTE_ID_POSITION, 4,12, 4,12, 4,12, 0,0), IATTR_UNITVECTOR3(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), IATTR_UNITVECTOR4(EDGE_GEOM_ATTRIBUTE_ID_TANGENT) }
#define iposNormTanFlipBin		{ IATTR_FIXEDPOINT(3, EDGE_GEOM_ATTRIBUTE_ID_POSITION, 4,12, 4,12, 4,12, 0,0), IATTR_UNITVECTOR3(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), IATTR_UNITVECTOR4(EDGE_GEOM_ATTRIBUTE_ID_TANGENT), IATTR_UNITVECTOR3(EDGE_GEOM_ATTRIBUTE_ID_BINORMAL) }

// Note, the vertex stride field is left zero here, because it is computed and overwritten 
// here by the Finalize*() functions below function, to reduce typing errors.
static EdgeGeomSpuVertexFormat g_spuInputVertexFormats[] = 
	{ 
		{1, 0, ipos},					// position only
		{3, 0, iposNormTan},			// position, normal, tangent
		{3, 0, iposNormTanFlip},		// position, normal, tangent/flip
		{4, 0, iposNormTanBin},			// position, normal, tangent, binormal
		{3, 0, iposNormTanFlip2},		// position, normal, tangent/flip
		{4, 0, iposNormTanFlipBin},		// position, normal, tangent/flip, binormal
		{0, 0, {}},                     // empty (no attributes = no stream)
	};
#define NUM_SPU_INPUT_FORMATS ( sizeof g_spuInputVertexFormats/sizeof g_spuInputVertexFormats[0] )

//-------------------
// Macros for common RSX vertex attribute types
#define OATTR_U8(COUNT, ATTRIBUTEID)		{ kRsxAttr_U8, COUNT, ATTRIBUTEID, 0 }
#define OATTR_U8N(COUNT, ATTRIBUTEID)		{ kRsxAttr_U8N, COUNT, ATTRIBUTEID, 0 }
#define OATTR_I16(COUNT, ATTRIBUTEID)		{ kRsxAttr_I16, COUNT, ATTRIBUTEID, 0 }
#define OATTR_I16N(COUNT, ATTRIBUTEID)		{ kRsxAttr_I16N, COUNT, ATTRIBUTEID, 0 }
#define OATTR_F16(COUNT, ATTRIBUTEID)		{ kRsxAttr_F16, COUNT, ATTRIBUTEID, 0 }
#define OATTR_F32(COUNT, ATTRIBUTEID)		{ kRsxAttr_F32, COUNT, ATTRIBUTEID, 0 }
#define OATTR_X11Y11Z10N(ATTRIBUTEID)		{ kRsxAttr_X11Y11Z10N, 1, ATTRIBUTEID, 0 }

// These macros can be used with the ordinary types (U8..F32).  More complex types need to be spelled out.
#define OPOSITION(TYPE)  OATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_POSITION)
#define ONORMAL(TYPE)    OATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_NORMAL)
#define OTANGENT3(TYPE)  OATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
#define OTANGENT4(TYPE)  OATTR_##TYPE(4, EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
#define OUV(TYPE)        OATTR_##TYPE(2, EDGE_GEOM_ATTRIBUTE_ID_UV0)
#define OBINORMAL(TYPE)  OATTR_##TYPE(3, EDGE_GEOM_ATTRIBUTE_ID_BINORMAL)
#define OCOLOR(TYPE)     OATTR_##TYPE(4, EDGE_GEOM_ATTRIBUTE_ID_COLOR)

#define opos				{ OPOSITION(F32) }
#define oposNormTan			{ OPOSITION(F32), OATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), OATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT) }
#define oposNormTanFlip		{ OPOSITION(F32), OATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), OTANGENT4(I16N) }
#define oposNormTanBin		{ OPOSITION(F32), OATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL), OATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT), OATTR_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_BINORMAL) }

// Note, the vertex stride field left zero here, because it is computed and overwritten 
// here by the Finalize*() functions below function, to reduce typing errors.
static EdgeGeomRsxVertexFormat g_spuOutputVertexFormats[] =
	{
		{1, 0, opos},				// position-only
		{3, 0, oposNormTan},		// position, normal, tangent
		{3, 0, oposNormTanFlip},	// position, normal, tangent/flip
		{4, 0, oposNormTanBin},		// position, normal, tangent, binormal
		{0, 0, {}},					// empty (no attributes = no output stream)
	};
#define NUM_SPU_OUTPUT_FORMATS ( sizeof g_spuOutputVertexFormats/sizeof g_spuOutputVertexFormats[0] )

// This array stores the vertex program slot indexes associated with each attribute.
// Default values are provided for the most common attribute IDS (position, normal, tangent, etc.)
// based on what sce-cgc maps these attributes to by default.  These mappings are customizable
// using the edgeGeomSetAttributeSlotIndex() function.
static uint8_t g_slotIndexesByAttribute[256] = 
{
	0,  // invalid attribute ID
	0,  // EDGE_GEOM_ATTRIBUTE_ID_POSITION = 0x01,
	2,  // EDGE_GEOM_ATTRIBUTE_ID_NORMAL   = 0x02,
	14, // EDGE_GEOM_ATTRIBUTE_ID_TANGENT  = 0x03,
	15, // EDGE_GEOM_ATTRIBUTE_ID_BINORMAL = 0x04,
	8,  // EDGE_GEOM_ATTRIBUTE_ID_UV0      = 0x05,
	9,  // EDGE_GEOM_ATTRIBUTE_ID_UV1      = 0x06,
	10, // EDGE_GEOM_ATTRIBUTE_ID_UV2      = 0x07,
	11, // EDGE_GEOM_ATTRIBUTE_ID_UV3      = 0x08,
	3,  // EDGE_GEOM_ATTRIBUTE_ID_COLOR    = 0x09,
	0,
	0,
	0,
	0,
	0,
	0,
	3,  // EDGE_GEOM_ATTRIBUTE_ID_INSTANCED_COLOR = 0x10,
};

uint8_t edgeGeomGetAttributeSlotIndex(EdgeGeomAttributeId attrId)
{
	EDGEASSERT((uint32_t)attrId < 256);
	return g_slotIndexesByAttribute[(uint8_t)attrId];
}
void edgeGeomSetAttributeSlotIndex(EdgeGeomAttributeId attrId, uint8_t slotIndex)
{
	EDGEASSERT((uint32_t)attrId < 256);
	EDGEASSERT(slotIndex < 16);
	g_slotIndexesByAttribute[(uint8_t)attrId] = slotIndex;
}

//-------------------
// Utility functions to finalize the (incomplete) built-in vertex format structures,
// calculating the proper byte offsets and the total vertex stride.
static void FinalizeSpuVertexFormat(EdgeGeomSpuVertexFormat *vertexFormat)
{
	// compute the vertex stride, poke the byte offset into the structure per attribute, etc.
	uint32_t byteOffset = 0;
	for (uint32_t iAttr=0; iAttr<vertexFormat->m_numAttributes; iAttr++)
	{
		vertexFormat->m_attributeDefinition[iAttr].m_byteOffset = byteOffset;
		switch (vertexFormat->m_attributeDefinition[iAttr].m_type)
		{
			case kSpuAttr_U8:
			case kSpuAttr_U8N:
				byteOffset += vertexFormat->m_attributeDefinition[iAttr].m_count * 1;
				break;
			case kSpuAttr_I16:
			case kSpuAttr_I16N:
			case kSpuAttr_F16:
				byteOffset += vertexFormat->m_attributeDefinition[iAttr].m_count * 2;
				break;
			case kSpuAttr_F32:
			case kSpuAttr_X11Y11Z10N:
				byteOffset += vertexFormat->m_attributeDefinition[iAttr].m_count * 4;
				break;
			case kSpuAttr_UnitVector:
				byteOffset += 3;
				break;
			case kSpuAttr_FixedPoint:
			{
				uint32_t totalBits = 0;
				for (uint32_t iComponent=0; iComponent<4; iComponent++)
				{
					totalBits += vertexFormat->m_attributeDefinition[iAttr].m_fixedPointBitDepthInteger[iComponent];
					totalBits += vertexFormat->m_attributeDefinition[iAttr].m_fixedPointBitDepthFractional[iComponent];
				}
				EDGEASSERT((totalBits % 8)==0);  // all fixed point attributes must be modulo 8 total bits (an even byte boundary)
				byteOffset += totalBits / 8;
				break;
			}
		}
	}
	
	// store the final offset as the vertex stride
	vertexFormat->m_vertexStride = byteOffset;		
}
static void FinalizeRsxVertexFormat(EdgeGeomRsxVertexFormat *vertexFormat)
{
	uint32_t byteOffset = 0;
	for (uint32_t iAttr=0; iAttr<vertexFormat->m_numAttributes; iAttr++)
	{
		vertexFormat->m_attributeDefinition[iAttr].m_byteOffset = byteOffset;
		switch (vertexFormat->m_attributeDefinition[iAttr].m_type)
		{
			case kRsxAttr_U8:
			case kRsxAttr_U8N:
				// RSX demands U8 to have component count of 4
				vertexFormat->m_attributeDefinition[iAttr].m_count = 4;
				byteOffset += 4;
				break;
			case kRsxAttr_I16:
			case kRsxAttr_I16N:
			case kRsxAttr_F16:
				byteOffset += vertexFormat->m_attributeDefinition[iAttr].m_count * 2;
				break;
			case kRsxAttr_F32:
			case kRsxAttr_X11Y11Z10N:
				byteOffset += vertexFormat->m_attributeDefinition[iAttr].m_count * 4;
				break;
			default:
				EDGEERROR_F();
		}
	}
	
	// store the final offset as the vertex stride
	vertexFormat->m_vertexStride = byteOffset;
}

EdgeGeomSpuVertexFormat *edgeGeomGetSpuVertexFormat(uint32_t formatId, EdgeGeomSpuVertexFormat *outFormat)
{
	if (formatId >= NUM_SPU_INPUT_FORMATS)
		return NULL;
	EdgeGeomSpuVertexFormat *format = outFormat;
	if (format == NULL)
		format = (EdgeGeomSpuVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomSpuVertexFormat));

	FinalizeSpuVertexFormat(&(g_spuInputVertexFormats[formatId]));
	memcpy(format, &(g_spuInputVertexFormats[formatId]), sizeof(EdgeGeomSpuVertexFormat));
	return format;
}
EdgeGeomRsxVertexFormat *edgeGeomGetRsxVertexFormat(uint32_t formatId, EdgeGeomRsxVertexFormat *outFormat)
{
	if (formatId >= NUM_SPU_OUTPUT_FORMATS)
		return NULL;
	EdgeGeomRsxVertexFormat *format = outFormat;
	if (format == NULL)
		format = (EdgeGeomRsxVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomRsxVertexFormat));
	
	FinalizeRsxVertexFormat(&(g_spuOutputVertexFormats[formatId]));
	memcpy(format, &(g_spuOutputVertexFormats[formatId]), sizeof(EdgeGeomRsxVertexFormat));
	return format;
}

uint8_t edgeGeomGetSpuVertexFormatId(const EdgeGeomSpuVertexFormat &vertexFormat)
{
	for(uint8_t iFormat=0; iFormat<NUM_SPU_INPUT_FORMATS; ++iFormat)
	{
		// Make sure the right-hand format has been finalized before we compare!
		if (g_spuInputVertexFormats[iFormat].m_vertexStride == 0)
			FinalizeSpuVertexFormat(&(g_spuInputVertexFormats[iFormat]));
		if (vertexFormat == g_spuInputVertexFormats[iFormat])
			return iFormat;
	}
	// Not found -- it's a custom flavor
	return 0xFF;
}
uint8_t edgeGeomGetRsxVertexFormatId(const EdgeGeomRsxVertexFormat &vertexFormat)
{
	for(uint8_t iFormat=0; iFormat<NUM_SPU_OUTPUT_FORMATS; ++iFormat)
	{
		// Make sure the right-hand format has been finalized before we compare!
		if (g_spuOutputVertexFormats[iFormat].m_vertexStride == 0)
			FinalizeRsxVertexFormat(&(g_spuOutputVertexFormats[iFormat]));
		if (vertexFormat == g_spuOutputVertexFormats[iFormat])
			return iFormat;
	}
	// Not found -- it's a custom flavor
	return 0xFF;
}

//-------------------

uint32_t edgeGeomGetSpuVertexAttributeSize(const EdgeGeomSpuVertexAttributeDefinition &attr)
{
	bool isValidAttrType = true;
	switch(attr.m_type)
	{
	case kSpuAttr_F32:
		return attr.m_count * 4;
	case kSpuAttr_I16N:
	case kSpuAttr_I16:
	case kSpuAttr_F16:
		return attr.m_count * 2;
	case kSpuAttr_U8N:
	case kSpuAttr_U8:
		return attr.m_count * 1;
	case kSpuAttr_X11Y11Z10N:
		return 4;
	case kSpuAttr_UnitVector:
		return 3;
	case kSpuAttr_FixedPoint:
		{
			uint32_t sizeInBits = 0;
			for(uint32_t iComponent=0; iComponent<attr.m_count; ++iComponent)
			{
				sizeInBits += attr.m_fixedPointBitDepthInteger[iComponent] + attr.m_fixedPointBitDepthFractional[iComponent];
			}
			EDGEASSERT(sizeInBits % 8 == 0); // test for invalid sizes
			return sizeInBits / 8;
		}
	default:
		isValidAttrType = false;
		break;
	}
	if (!isValidAttrType)
		EDGEERROR_F(); // unknown attribute type
	return 0;
}

//-------------------

uint32_t edgeGeomGetRsxVertexAttributeSize(const EdgeGeomRsxVertexAttributeDefinition &attr)
{
	bool isValidAttrType = true;
	switch(attr.m_type)
	{
	case kRsxAttr_F32:
		return attr.m_count * 4;
	case kRsxAttr_I16N:
	case kRsxAttr_I16:
	case kRsxAttr_F16:
		return attr.m_count * 2;
	case kRsxAttr_U8N:
	case kRsxAttr_U8:
		return attr.m_count * 1;
	case kRsxAttr_X11Y11Z10N:
		return 4;
	default:
		isValidAttrType = false;
		break;
	}
	if (!isValidAttrType)
		EDGEERROR_F(); // unknown attribute type
	return 0;
}

//-------------------

bool edgeGeomSpuVertexFormatIsValid(const EdgeGeomSpuVertexFormat& vertexFormat)
{
	// Cache the ending byte offset of each attribute
	uint32_t attributeEnds[16];
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		attributeEnds[iAttr] = vertexFormat.m_attributeDefinition[iAttr].m_byteOffset
			+ edgeGeomGetSpuVertexAttributeSize(vertexFormat.m_attributeDefinition[iAttr]);
	}
	// Make sure attributes don't overlap, or extend past the stride.
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		uint32_t attrStart1 = vertexFormat.m_attributeDefinition[iAttr].m_byteOffset;
		uint32_t attrEnd1 = attributeEnds[iAttr];
		if (attrEnd1 > vertexFormat.m_vertexStride)
			return false; // attribute extends past the end of the vertex
		for(uint32_t jAttr=0; jAttr<iAttr; ++jAttr)
		{
			uint32_t attrStart2 = vertexFormat.m_attributeDefinition[jAttr].m_byteOffset;
			uint32_t attrEnd2 = attributeEnds[jAttr];
			if (attrStart1 > attrStart2 && attrStart1 < attrEnd2)
				return false; // attributes overlap
			if (attrStart2 > attrStart1 && attrStart2 < attrEnd1)
				return false; // attributes overlap
		}
	}
	return true;
}
bool edgeGeomRsxVertexFormatIsValid(const EdgeGeomRsxVertexFormat& vertexFormat)
{
	// Cache the ending byte offset of each attribute
	uint32_t attributeEnds[16];
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		attributeEnds[iAttr] = vertexFormat.m_attributeDefinition[iAttr].m_byteOffset
			+ edgeGeomGetRsxVertexAttributeSize(vertexFormat.m_attributeDefinition[iAttr]);
	}
	// Make sure attributes don't overlap, or extend past the stride.
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		uint32_t attrStart1 = vertexFormat.m_attributeDefinition[iAttr].m_byteOffset;
		uint32_t attrEnd1 = attributeEnds[iAttr];
		if (attrEnd1 > vertexFormat.m_vertexStride)
			return false; // attribute extends past the end of the vertex
		for(uint32_t jAttr=0; jAttr<iAttr; ++jAttr)
		{
			uint32_t attrStart2 = vertexFormat.m_attributeDefinition[jAttr].m_byteOffset;
			uint32_t attrEnd2 = attributeEnds[jAttr];
			if (attrStart1 > attrStart2 && attrStart1 < attrEnd2)
				return false; // attributes overlap
			if (attrStart2 > attrStart1 && attrStart2 < attrEnd1)
				return false; // attributes overlap
		}
	}
	return true;
}

//-------------------

void edgeGeomMakeSpuStreamDescription(const EdgeGeomSpuVertexFormat &vertexFormat, uint8_t **outStreamDescription, uint16_t *outStreamDescriptionSize)
{
	// count the number of 8-byte blocks to allocate. We need
	// one per attribute, and an additional one per fixed-point attribute.
	uint8_t attrCount = 0;
	uint8_t fixedBlockCount = 0;
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		++attrCount;
		if (vertexFormat.m_attributeDefinition[iAttr].m_type == kSpuAttr_FixedPoint)
		{
			++fixedBlockCount;
		}
	}
	if (attrCount == 0)
	{
		*outStreamDescription = 0;
		*outStreamDescriptionSize = 0;
		return;			
	}
	*outStreamDescriptionSize = (attrCount+fixedBlockCount)*8 + 8; // include 8 bytes for stream metadata
	*outStreamDescriptionSize = (*outStreamDescriptionSize + 0xF) & ~0xF; // round size up to 16 bytes
	*outStreamDescription = (uint8_t*)edgeGeomAlloc(*outStreamDescriptionSize);
	memset(*outStreamDescription, 0, *outStreamDescriptionSize);
	EdgeGeomVertexStreamDescription *streamDesc = (EdgeGeomVertexStreamDescription*)(*outStreamDescription);
	
	// Fill in the stream metadata
	streamDesc->numAttributes = (uint8_t)vertexFormat.m_numAttributes;
	streamDesc->stride        = (uint8_t)vertexFormat.m_vertexStride;
	streamDesc->numBlocks     = (uint8_t)(attrCount+fixedBlockCount);

	// Fill in the attribute blocks
	uint8_t nextFixedBlockOffset = attrCount*8; // starts immediately after the last attribute block
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		const EdgeGeomSpuVertexAttributeDefinition &src = vertexFormat.m_attributeDefinition[iAttr];
		EdgeGeomAttributeBlock &attr = streamDesc->blocks[iAttr].attributeBlock;

		attr.offset                 = (uint8_t)src.m_byteOffset;
		attr.format                 = (uint8_t)src.m_type;
		attr.componentCount         = (uint8_t)src.m_count;
		attr.edgeAttributeId        = (uint8_t)src.m_attributeId;
		attr.size                   = (uint8_t)edgeGeomGetSpuVertexAttributeSize(src);
		attr.vertexProgramSlotIndex = (uint8_t)edgeGeomGetAttributeSlotIndex(src.m_attributeId);
		if (attr.format == kSpuAttr_FixedPoint)
		{
			attr.fixedBlockOffset = nextFixedBlockOffset;
			nextFixedBlockOffset += 8;
		}
		else
		{
			attr.fixedBlockOffset = 0;
		}
		attr.padding = 0;
	}

	// Fill in the fixed point blocks
	EdgeGeomGenericBlock *fixedBlocks = streamDesc->blocks + streamDesc->numAttributes;
	uint32_t fixedIndex = 0;
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		// Skip attributes that aren't fixed point
		const EdgeGeomSpuVertexAttributeDefinition &src = vertexFormat.m_attributeDefinition[iAttr];
		if (src.m_type != kSpuAttr_FixedPoint)
			continue;

		EdgeGeomAttributeFixedBlock &fixed = fixedBlocks[fixedIndex++].fixedBlock;
		fixed.integer0  = (uint8_t)src.m_fixedPointBitDepthInteger[0];
		fixed.mantissa0 = (uint8_t)src.m_fixedPointBitDepthFractional[0];
		fixed.integer1  = (uint8_t)src.m_fixedPointBitDepthInteger[1];
		fixed.mantissa1 = (uint8_t)src.m_fixedPointBitDepthFractional[1];
		fixed.integer2  = (uint8_t)src.m_fixedPointBitDepthInteger[2];
		fixed.mantissa2 = (uint8_t)src.m_fixedPointBitDepthFractional[2];
		fixed.integer3  = (uint8_t)src.m_fixedPointBitDepthInteger[3];
		fixed.mantissa3 = (uint8_t)src.m_fixedPointBitDepthFractional[3];
	}
}

//-------------------

void edgeGeomMakeRsxStreamDescription(const EdgeGeomRsxVertexFormat &vertexFormat, uint8_t **outStreamDescription, uint16_t *outStreamDescriptionSize)
{
	// count the number of 8-byte blocks to allocate. We need
	// one per attribute, and an additional one per fixed-point attribute.
	uint8_t attrCount = (uint8_t)(vertexFormat.m_numAttributes);
	uint8_t fixedBlockCount = 0;
	*outStreamDescriptionSize = (attrCount+fixedBlockCount)*8 + 8; // include 8 bytes for stream metadata
	*outStreamDescriptionSize = (*outStreamDescriptionSize + 0xF) & ~0xF; // round size up to 16 bytes
	*outStreamDescription = (uint8_t*)edgeGeomAlloc(*outStreamDescriptionSize);
	memset(*outStreamDescription, 0, *outStreamDescriptionSize);
	EdgeGeomVertexStreamDescription *streamDesc = (EdgeGeomVertexStreamDescription*)(*outStreamDescription);
	
	// Fill in the stream metadata
	streamDesc->numAttributes = (uint8_t)vertexFormat.m_numAttributes;
	streamDesc->stride        = (uint8_t)vertexFormat.m_vertexStride;
	streamDesc->numBlocks     = (uint8_t)(attrCount+fixedBlockCount);

	// Fill in the attribute blocks
	for(uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; ++iAttr)
	{
		const EdgeGeomRsxVertexAttributeDefinition &src = vertexFormat.m_attributeDefinition[iAttr];
		EdgeGeomAttributeBlock &attr = streamDesc->blocks[iAttr].attributeBlock;

		attr.offset                 = (uint8_t)src.m_byteOffset;
		attr.format                 = (uint8_t)src.m_type;
		attr.componentCount         = (uint8_t)src.m_count;
		attr.edgeAttributeId        = (uint8_t)src.m_attributeId;
		attr.size                   = (uint8_t)edgeGeomGetRsxVertexAttributeSize(src);
		attr.vertexProgramSlotIndex = (uint8_t)edgeGeomGetAttributeSlotIndex(src.m_attributeId);
		attr.fixedBlockOffset = 0; // output streams can't have fixed point attributes
		attr.padding = 0;
	}
}
