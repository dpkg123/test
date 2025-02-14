/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#if !defined(LIBEDGEGEOMTOOL_H)
#define LIBEDGEGEOMTOOL_H

//-------------------

#include "edge/edge_version.h"
#define LIBEDGEGEOMTOOL_VERSION EDGE_SDK_VERSION

//-------------------

#include <string.h>
#include <stddef.h>

#ifdef _MSC_VER
#	pragma warning( push )
#	pragma warning( disable : 4200 )			// warning C4200: nonstandard extension used : zero-sized array in struct/union
#endif//_MSC_VER
#include	<edge/geom/edgegeom_structs.h>
#ifdef _MSC_VER
#	pragma warning( pop )
#endif//_MSC_VER

// Forward declaration required
namespace cell
{
	namespace Gcm
	{
		struct CellGcmContext;
	} // namespace Gcm
} // namespace cell

//-------------------
// Memory allocation functions
//-------------------

// Do not use these functions directly; use the edgeGeomAlloc and edgeGeomFree macros below,
// for accurate filename/line number information.
void *edgeGeomAllocEx(size_t allocSize, const char *filename, const uint32_t lineNumber);
void edgeGeomFreeEx(void *ptr, const char *filename, const uint32_t lineNumber);

// These wrappers are used internally throughout libedgegeomtool.  They use user-defined callbacks
// (by default, malloc and free) to dynamically allocate and deallocate memory.
#define edgeGeomAlloc(allocSize) edgeGeomAllocEx((allocSize), __FILE__, __LINE__)
#define edgeGeomFree(ptr) edgeGeomFreeEx((ptr), __FILE__, __LINE__)

typedef void* (*EdgeGeomAllocFunc)(size_t allocSize, const char *filename, const uint32_t lineNumber);
typedef void  (*EdgeGeomFreeFunc)(void *ptr, const char *filename, const uint32_t lineNumber);

// Use these functions to provide callbacks for edgeGeomAlloc() and edgeGeomFree().
// If the argument is NULL, the default system malloc/free will be used.
void EdgeGeomSetAllocFunc(EdgeGeomAllocFunc func);
void EdgeGeomSetFreeFunc(EdgeGeomFreeFunc func);


//-------------------
// Vertex format type definitions
//-------------------

// Don't change this value! It's only here to avoid sprinkling the code with hard-coded literal "4"s.
#define kEdgeGeomNumInputBonesPerVertex 4

//-------------------
/// This section lists the types of data the user can specify as inputs to the SPU
/// for vertex attributes.
enum EdgeGeomSpuVertexAttributeType ///< To SPU
{
	kSpuAttr_I16N       = 0x01,
	kSpuAttr_F32        = 0x02,
	kSpuAttr_F16        = 0x03,
	kSpuAttr_U8N        = 0x04,
	kSpuAttr_I16        = 0x05,
	kSpuAttr_X11Y11Z10N = 0x06, ///< requires a component count of 1 in the attribute definition
	kSpuAttr_U8         = 0x07,
	kSpuAttr_FixedPoint = 0x08, ///< requires integer/fractional bits per component
	kSpuAttr_UnitVector = 0x09, ///< requires a component count of 3 or 4 in the attribute definition
};

//-------------------

enum EdgeGeomRsxVertexAttributeType  ///< To RSX
{
	kRsxAttr_I16N       = 0x01,
	kRsxAttr_F32        = 0x02,
	kRsxAttr_F16        = 0x03,
	kRsxAttr_U8N        = 0x04,
	kRsxAttr_I16        = 0x05,
	kRsxAttr_X11Y11Z10N = 0x06, ///< requires a component count of 1 in the attribute definition
	kRsxAttr_U8         = 0x07,
};

//-------------------

enum EdgeGeomIndexesFlavor
{
	kIndexesU16TriangleListCW         = EDGE_GEOM_INDEXES_U16_TRIANGLE_LIST_CW,
	kIndexesU16TriangleListCCW		  = EDGE_GEOM_INDEXES_U16_TRIANGLE_LIST_CCW,
	kIndexesCompressedTriangleListCW  = EDGE_GEOM_INDEXES_COMPRESSED_TRIANGLE_LIST_CW,
	kIndexesCompressedTriangleListCCW = EDGE_GEOM_INDEXES_COMPRESSED_TRIANGLE_LIST_CCW,
};

//-------------------

enum EdgeGeomSkinningFlavor
{
	kSkinNone              = EDGE_GEOM_SKIN_NONE,
	kSkinNoScaling         = EDGE_GEOM_SKIN_NO_SCALING,
	kSkinUniformScaling    = EDGE_GEOM_SKIN_UNIFORM_SCALING,
	kSkinNonUniformScaling = EDGE_GEOM_SKIN_NON_UNIFORM_SCALING,
	kSkinSingleBoneNoScaling = EDGE_GEOM_SKIN_SINGLE_BONE_NO_SCALING,
	kSkinSingleBoneUniformScaling = EDGE_GEOM_SKIN_SINGLE_BONE_UNIFORM_SCALING,
	kSkinSingleBoneNonUniformScaling = EDGE_GEOM_SKIN_SINGLE_BONE_NON_UNIFORM_SCALING,
};

//-------------------

enum EdgeGeomMatrixFormat
{
	kMatrix3x4RowMajor = EDGE_GEOM_MATRIX_3x4_ROW_MAJOR, //! Edge's native format, and the most memory-efficient.
	kMatrix4x4RowMajor = EDGE_GEOM_MATRIX_4x4_ROW_MAJOR, //! "DirectX-style"
	kMatrix4x4ColumnMajor = EDGE_GEOM_MATRIX_4x4_COLUMN_MAJOR, //! "OpenGL-style"
};

//-------------------

enum EdgeGeomCullingFlavor
{
	kCullNone                 = EDGE_GEOM_CULL_NONE,
	kCullFrustum              = EDGE_GEOM_CULL_FRUSTUM,
	kCullBackFacesAndFrustum  = EDGE_GEOM_CULL_BACKFACES_AND_FRUSTUM,
	kCullFrontFacesAndFrustum = EDGE_GEOM_CULL_FRONTFACES_AND_FRUSTUM,
};

//-------------------

struct EdgeGeomSpuVertexAttributeDefinition
{
	/// This is the data type as input to either the SPU or RSX.  
	/// Certain types are legal only to the SPU.
	EdgeGeomSpuVertexAttributeType m_type;
	
	/// This is how many of EdgeSpuVertexAttributeType get encoded for 
	/// this attribute.  X11Y11Z10N will always be 1.  Vec3 will 
	/// be 3, Vec4 will be 4, Edge unit vectors will be 3 or 4, etc.
	uint32_t           m_count;

	/// This describes to the tools what the data in this attribute MEANS.
	/// This is how the tools know to put positions in a specific place, so 
	/// the backface cull routine can figure out the orientation of a face at runtime.
	EdgeGeomAttributeId	   m_attributeId;
	
	/// This is where in the vertex data block to start loading the attribute for decompression.
	/// Generally, it should be at the same place as the converted data was
	/// written, though that is not a requirement, in the case of odd or optimized packings.
	uint32_t           m_byteOffset;

	/// Each component gets its bit count specified explicitly.  Only used for kSpuAttr_FixedPoint.
	/// The sum for a single attribute MUST be modulo 8.  The user can control the number of bits per
	/// component which are fractional (representing [0..1]) and how many are integer.
	uint32_t           m_fixedPointBitDepthInteger[4];
	uint32_t           m_fixedPointBitDepthFractional[4];
};
	
struct EdgeGeomSpuVertexFormat
{
	/// This is the number of vertex attribute records in the array.
	uint32_t m_numAttributes;

	/// This is the number of bytes used by a single vertex in the input vertex stream.
	uint32_t m_vertexStride;	

	/// An array of attribute definitions
	EdgeGeomSpuVertexAttributeDefinition m_attributeDefinition[16];

	bool operator==(const EdgeGeomSpuVertexFormat &rhs) const
	{
		if (this == &rhs)
			return true;
		bool identical = (this->m_numAttributes == rhs.m_numAttributes);
		identical = identical && (this->m_vertexStride == rhs.m_vertexStride);
		identical = identical && (memcmp(this->m_attributeDefinition, rhs.m_attributeDefinition, m_numAttributes*sizeof(EdgeGeomSpuVertexAttributeDefinition)) == 0);
		return identical;
	}
};

//-------------------

struct EdgeGeomRsxVertexAttributeDefinition
{
	/// This is the data type as input to the RSX.
	EdgeGeomRsxVertexAttributeType m_type;

	/// This is how many of EdgeRsxVertexAttributeType get encoded for 
	/// this attribute.  X11Y11Z10N will always be 1.  Vec3 will 
	/// be 3, Vec4 will be 4, etc.
	uint32_t            m_count;

	/// This describes to the tools what the data in this attribute MEANS.
	/// This is how the tools know to put positions in a specific place, so 
	/// the backface cull routine can figure out the orientation of a face at runtime.
	EdgeGeomAttributeId	   m_attributeId;

	/// This is where in the vertex data block to start loading the attribute.
	uint32_t            m_byteOffset;
};
	
struct EdgeGeomRsxVertexFormat
{
	/// This is the number of attribute records in the array.
	uint32_t m_numAttributes;

	/// This cannot be figured out easily by the below structure alone, due to offsets.
	uint32_t m_vertexStride;

	/// This should be an array of attribute definitions from the SPU to RSX types.
	EdgeGeomRsxVertexAttributeDefinition m_attributeDefinition[16];

	bool operator==(const EdgeGeomRsxVertexFormat &rhs) const
	{
		if (this == &rhs)
			return true;
		bool identical = (this->m_numAttributes == rhs.m_numAttributes);
		identical = identical && (this->m_vertexStride == rhs.m_vertexStride);
		identical = identical && (memcmp(this->m_attributeDefinition, rhs.m_attributeDefinition, m_numAttributes*sizeof(EdgeGeomRsxVertexAttributeDefinition)) == 0);
		return identical;
	}
};


//-------------------
// Global Functions
//-------------------

//-------------------
/// Test a vertex format definition for validity (i.e. make sure its attributes don't overlap, that sort of thing)
bool edgeGeomSpuVertexFormatIsValid(const EdgeGeomSpuVertexFormat& vertexFormat);
bool edgeGeomRsxVertexFormatIsValid(const EdgeGeomRsxVertexFormat& vertexFormat);

//-------------------
/// These functions return pointers to the built-in vertex formats, given the format ID.
/// If the outFormat argument is non-NULL, the target object will be filled with the requested vertex format.
/// If outFormat is NULL, a new format structure will be allocated; it is the caller's responsibility to free
/// the returned pointer with edgeGeomFree().
/// Either way, a pointer to the requested format object is returned (or NULL, if formatId is invalid).
EdgeGeomSpuVertexFormat *edgeGeomGetSpuVertexFormat(uint32_t formatId, EdgeGeomSpuVertexFormat *outFormat = NULL);
EdgeGeomRsxVertexFormat *edgeGeomGetRsxVertexFormat(uint32_t formatId, EdgeGeomRsxVertexFormat *outFormat = NULL);

//-------------------
/// These functions are used to retrieve the numerical index of a vertex format.  This index
/// can be used by the Edge SPU runtime to look up a built-in vertex format (assuming, of course, that
/// the lists of built-in formats are kept identical between the runtime and the tools).
/// If the format does not correspond to one of the built-in formats, the special index 0xFF is
/// returned, which indicates that the format should be treated as a custom vertex format.
uint8_t edgeGeomGetSpuVertexFormatId(const EdgeGeomSpuVertexFormat &vertexFormat);
uint8_t edgeGeomGetRsxVertexFormatId(const EdgeGeomRsxVertexFormat &vertexFormat);

//-------------------
/// Returns the size of each instance of a vertex attribute in bytes, given its attribute
/// definition.  Useful when creating custom EdgeGeomSpuVertexFormat and EdgeGeomRsxVertexFormat
/// structures.
uint32_t edgeGeomGetSpuVertexAttributeSize(const EdgeGeomSpuVertexAttributeDefinition &attr);
uint32_t edgeGeomGetRsxVertexAttributeSize(const EdgeGeomRsxVertexAttributeDefinition &attr);

//-------------------

/// This reorders a triangle list so as to minimize cache misses.  It is not particularly sensitive to the 
/// values of triangle indexes, so remapping to lower-valued indexes is not necessary.  The fifoMiss and lruMiss
/// factors are a measurement of cost for each hardware unit to fetch the necessary data.  The RSX tends to be 4:1
/// FIFO:LRU.  The constants k1-k3 are weightings for different score metrics.  No single set of values is ideal for
/// all partitions, but a good default is (1.0, .25, .8).  Trying various other values may yield better results on
/// any particular dataset.
struct EdgeGeomKCacheOptimizerUserData
{
	float m_fifoMissCost; /// corresponds to the cost of running a vertex program.
	float m_lruMissCost; /// corresponds to the cost of loading a vertex in the FIFO into the mini-cache.
	float m_k1; // Reasonable default: 1.0
	float m_k2; // Reasonable default: 0.25
	float m_k3; // Reasonable default: 0.8
};
void edgeGeomKCacheOptimizer(const uint32_t *inTriangles, uint32_t numTriangles, void *userData, uint32_t *outTriangles);


/// This function will start at a particular place in the 3 dimensional parametric space that controls the k-cache optimizer,
/// and will re-run it a given number of iterations, altering the constants slightly as it goes to find the best configuration
/// for the given triangle list.  This can improve performance by about 1/4% to 1% over about 100 iterations.  Passing in 0 will
/// just run the cache optimizer without trying to randomize the control parameters.
struct EdgeGeomKCacheOptimizerHillclimberUserData
{
	uint32_t m_numIterations;
	EdgeGeomIndexesFlavor m_indexesType;
	uint32_t m_numRsxInputAttributes;
	uint32_t m_numRsxOutputAttributes;
};
void edgeGeomKCacheOptimizerHillclimber(const uint32_t *inTriangles, const uint32_t numTriangles, void *userData, uint32_t *outTriangles);

typedef void (*EdgeGeomVertexCacheOptimizerFunc)(const uint32_t *triangles, uint32_t numTriangles, void *userData, uint32_t *outTriangles);

//-----------------------
// Advanced users may wish to include additional data with their Edge jobs in order to perform custom processing
// (beyond the standard Edge operations like skinning and triangle culling).  Edge needs to be made aware of
// this data (and its size) when building a scene's segments, or else the SPU runtime could easily overrun its
// available memory.
//
// Edge provides the EdgeGeomCustomPartitionDataSizeFunc callback function (and the associated EdgeGeomPartitionElementCounts structure)
// solely for this reason.  When creating segments that require extra data, you must provide an implementation of this callback
// that returns the size of the extra data for a partition, given its current contents.
struct EdgeGeomPartitionElementCounts
{
	uint32_t m_numTriangles;
	uint32_t m_numVertexes;
	uint32_t m_numMatrices;
	uint32_t m_numBlendedVertexes; // if >0, this partition will use blend shapes.
};

// The Edge partitioner calls this function when choosing which triangle to add to a partition.
// The function takes as input a description of the partition currently under construction,
// and must return the size (in bytes) of any extra data (beyond the standard Edge buffers) that
// would be required by the partition.
// Note that this function may be called *extremely* often! It should therefore perform as
// simple a calculation as possible, or else it will quickly dominate the tool's running time.
typedef uint32_t (*EdgeGeomCustomPartitionDataSizeFunc)(const EdgeGeomPartitionElementCounts &partitionContents);

// The Edge partitioner calls this function when computing command buffer hole size.
// The function takes as input a GCM context already containing space for the standard EDGE runtime
// render state calls.  This function should call any additional GCM functions (in their Measure form)
// to include the necessary space for the equivalent run time calls.  An example would be 
// calling a texture cache invalidation for a job that patches fragment programs.
typedef void (*EdgeGeomCustomCommandBufferHoleSizeFunc)(cell::Gcm::CellGcmContext *fakeContext);

//-------------------
/// This is filled out by the user and passed in to control how the geometry partitioner works.
struct EdgeGeomPartitionerInput
{
	uint32_t m_numTriangles; ///< Number of triangles in the triangle list.
	uint32_t *m_triangleList; ///< Flat list of vertex indexes (3 indexes per triangle).
	uint32_t m_numInputAttributes; ///< Total number of input attributes (including both input SPU vertex streams)
	uint32_t m_numOutputAttributes; ///< Number of attributes in the SPU's output vertex format
	uint32_t m_inputVertexStride[2]; ///< size (in bytes) of a vertex in each of the two (primary and secondary/instanced) vertex streams
	uint32_t m_outputVertexStride; ///< in bytes
	EdgeGeomSkinningFlavor m_skinningFlavor;   ///< What type of skinning will geometry be using? This is a worst-case only; if a partition ends up with no skinning matrices influencing it, it will not be skinned.
	EdgeGeomIndexesFlavor m_indexListFlavor;
	EdgeGeomMatrixFormat m_skinningMatrixFormat;

	//
	// Everything below this point is optional, and can be set to zero if not used.
	//

	EdgeGeomVertexCacheOptimizerFunc m_cacheOptimizerCallback; ///< callback function used to optimize vertex cache performance on the partitioned triangle lists. If NULL, no vertex cache optimization will be performed.
	void *m_cacheOptimizerUserData; ///< This will be passed as the third argument to the vertex cache optimizer callback function.  Its usage is callback-specific.
	EdgeGeomCustomPartitionDataSizeFunc m_customDataSizeCallback; ///< If your segments will include extra data (beyond the standard Edge buffers), you must provide an implementation of this callback which returns the size of the extra data!
	float *m_triangleCentroids; ///< NULL to ignore spatial locality in partitions
	int32_t *m_skinningMatrixIndexesPerVertex; ///< NULL if not skinning
	uint32_t m_deltaStreamVertexStride; ///< zero if no blend shapes
	uint32_t *m_blendedVertexIndexes; ///< Flat list of vertexes which are affected by blend shapes.  NULL if no blend shapes
	uint32_t m_numBlendedVertexes; ///< number of elements in blendedVertexIndexes
	EdgeGeomCustomCommandBufferHoleSizeFunc m_customCommandBufferHoleSizeCallback; ///< If your jobs will call additional libgcm commands, you must provide an implementation of this callback which calls the Measure version of the GCM functions!
};

//-------------------
/// This is returned to the caller as the result of geometry partitioning.
/// The caller is responsible for freeing the buffers in this structure using edgeGeomFree().
struct EdgeGeomPartitionerOutput
{
	uint32_t  m_numPartitions;
	uint32_t *m_numTrianglesPerPartition;  ///< The number of triangles in each partition.
	uint32_t *m_triangleListOut;           ///< The actual triangle index list per partition.
	uint32_t *m_originalVertexIndexesPerPartition;     ///< Mapping table from each partition-local vertex to its original source vertex.
	uint32_t *m_numUniqueVertexesPerPartition; ///< contains m_numPartitions vertex counts
	uint32_t *m_ioBufferSizePerPartition;   ///< The total size (in bytes) of each partition's job data that must be uploaded to the SPU.
};

/// This function simply takes in a structure and writes out a
/// different one containing all the partitioned geometry data.
void edgeGeomPartitioner(const EdgeGeomPartitionerInput &dataIn, EdgeGeomPartitionerOutput *dataOut);

//-------------------

// Determines the average 3D position of each triangle in the scene.  The partitioner uses this data
// to attempt to create partitions with better spatial locality.  The caller is responsible for deleting
// the facePositions array, using edgeGeomFree().
void edgeGeomComputeTriangleCentroids(const float *vertexes, uint32_t numFloatsPerVertex, uint16_t positionAttributeIndex,
								  const uint32_t *triangles, uint32_t numTriangles, float **outTriangleCentroids);

// Determines which vertexes are affected by blend shapes.  Generates an array of all vertex indexes
// which have non-zero delta data in at least one blend shape.
// The partitioner uses this data to make more informed decision about which partition a vertex should
// belong to, preferring to place as many non-blended vertexes as possible in their own partition.
void edgeGeomGetBlendedVertexes(const float *vertexDeltas, uint32_t numVertexes, uint32_t numFloatsPerDelta, const EdgeGeomSpuVertexFormat& deltaFormat,
									  const uint16_t *blendedAttributeIndexes, const EdgeGeomAttributeId *blendedAttributeIds, uint8_t numBlendedAttributes, 
									  uint32_t numBlendShapes, const uint32_t *triangles, uint32_t numTriangles,
									  uint32_t **outBlendedVertexIndexes, uint32_t *outNumBlendedVertexes);

/// This function determines if a specific blend shape has any delta data for vertexes in a given geometry segment.
bool edgeGeomBlendShapeAffectsSegment(const float *shapeDeltas, uint32_t numFloatsPerDelta, const EdgeGeomSpuVertexFormat& deltaFormat,
										const uint16_t *blendedAttributeIndexes, const EdgeGeomAttributeId *blendedAttributeIds, uint8_t numBlendedAttributes, 
										const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes);

/// These functions retrieve and set the vertex program slot indexes for vertex attributes.  Valid slot indexes
/// are 0-15. These must correspond with the mapping used by the vertex program to retrieve these attributes.
/// Reasonable default values are provided for the most common vertex attributes (position, normal, tangent, etc.)
/// in accordance with the output of the stock Cg compiler.
///
/// Note that if you change the slot index for any of Edge's built-in vertex attributes (e.g. position, normal, tangent, etc.),
/// you must also update the corresponding hard-coded values in the Edge SPU runtime code.  Otherwise, Edge's built-in vertex
/// formats will not function properly.  See edgeGeomSetVertexDataArrays() in target/src/edge/geom/edgegeom.cpp.
uint8_t edgeGeomGetAttributeSlotIndex(EdgeGeomAttributeId attrId);
void edgeGeomSetAttributeSlotIndex(EdgeGeomAttributeId attrId, uint8_t slotIndex);

//-------------------
/// Determines the maximum space required in the RSX command buffer by a given Edge job.
uint32_t edgeGeomGetCommandBufferHoleSize(uint32_t numOutputAttributes, uint32_t numIndexes, EdgeGeomCustomCommandBufferHoleSizeFunc customHoleSizeCallback = 0);

//-------------------
/// Compute the scratch space required on the SPU by a segment, given its input attribute count and vertex count.
/// Note that the scratch buffer size is given in 16-byte qwords, NOT in bytes!
uint32_t edgeGeomGetScratchBufferSizeInQwords(uint32_t numInputAttributes, uint32_t numUniqueVertexes);

//--------------------
// Output buffer generation functions.  Caller is responsible for calling edgeGeomFree() on all 
// output buffers!
void edgeGeomMakeSpuConfigInfo(uint32_t numUniqueVertexes, uint32_t numTriangles, uint32_t numInputAttributes, uint8_t segmentFlags,
							   EdgeGeomIndexesFlavor indexListType, EdgeGeomSkinningFlavor skinType, EdgeGeomMatrixFormat skinMatrixFormat,
							   uint8_t inputVertexFormatId, uint8_t secondaryInputVertexFormatId, uint8_t outputVertexFormatId, uint8_t vertexDeltaFormatId,
							   uint32_t commandBufferHoleSize, uint8_t **outSpuConfigInfo);

void edgeGeomMakeIndexBuffer(const uint32_t *triangles, uint32_t numTriangles, EdgeGeomIndexesFlavor indexListType, uint8_t **outIndexBuffer, uint16_t outIndexDmaSizes[2]);


void edgeGeomMakeSpuVertexBuffer(const float *sourceVertexes, uint32_t numFloatsPerSourceVertex,
							  const uint16_t *sourceAttributeIndexes, const EdgeGeomAttributeId *sourceAttributeIds, uint8_t numSourceAttributes, 
							  const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes, const EdgeGeomSpuVertexFormat& vertexFormat,
							  uint8_t **outVertexBuffer, uint16_t outVertexDmaSizes[3], uint32_t **outFixedPointOffsets, uint32_t *outFixedOffsetsSize);
void edgeGeomMakeRsxVertexBuffer(const float *sourceVertexes, uint32_t numFloatsPerSourceVertex,
							  const uint16_t *sourceAttributeIndexes, const EdgeGeomAttributeId *sourceAttributeIds, uint8_t numSourceAttributes,
							  const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes, const EdgeGeomRsxVertexFormat& vertexFormat, bool writeBigEndian,
							  uint8_t **outVertexBuffer, uint32_t *outVertexBufferSize);

void edgeGeomMakeSkinningBuffer(const int32_t *matrixIndexes, const float *skinningWeights, EdgeGeomSkinningFlavor skinType, EdgeGeomMatrixFormat skinMatrixFormat,
								 const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes, 
								 uint8_t **outSkinIndexesAndWeights, uint16_t outSkinIndexesAndWeightsDmaSizes[2],
								 uint16_t outSkinMatricesByteOffsets[2], uint16_t outSkinMatricesSizes[2]);

void edgeGeomMakeBlendShapeBuffer(const float *shapeDeltas, uint32_t numFloatsPerDelta, const EdgeGeomSpuVertexFormat& deltaFormat,
								  const uint16_t *blendedAttributeIndexes, const EdgeGeomAttributeId *blendedAttributeIds, uint8_t numBlendedAttributes, 
								  const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes,
								  uint8_t **outShapeBuffer, uint16_t *outShapeBufferSize);

void edgeGeomMakeSpuStreamDescription(const EdgeGeomSpuVertexFormat &vertexFormat, uint8_t **outStreamDescription, uint16_t *outStreamDescriptionSize);
void edgeGeomMakeRsxStreamDescription(const EdgeGeomRsxVertexFormat &vertexFormat, uint8_t **outStreamDescription, uint16_t *outStreamDescriptionSize);

#endif // !defined(LIBEDGEGEOMTOOL_H)
