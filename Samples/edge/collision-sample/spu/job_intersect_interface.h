/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef JOB_INTERSECT_INTERFACE_H
#define JOB_INTERSECT_INTERFACE_H

#include <cell/spurs/job_descriptor.h>

#ifdef __cplusplus
extern "C" {
#endif

// Input DMA list elements - all items larger than 16KB are split in multiple DMA list elements
enum JobIntersectInputDma {
	kJobIntersectInputDmaStart, 
		kJobIntersectDmaOutputStreamDesc = kJobIntersectInputDmaStart,					// output stream descriptor
		kJobIntersectDmaIndexes,														// indices = up to 2 x 16KB = 32KB total
			kJobIntersectDmaIndexes_0 = kJobIntersectDmaIndexes,						// first list element
			kJobIntersectDmaIndexes_1,													// indices - second list element = up to 32KB total
		kJobIntersectDmaSkinMatrices,													// skinning matrices = up to 32KB total
			kJobIntersectDmaSkinMatrices_0 = kJobIntersectDmaSkinMatrices,				// skinning matrices - first list element
			kJobIntersectDmaSkinMatrices_1,												// skinning matrices - second list element
		kJobIntersectDmaSkinIndexesAndWeights,											// skinning info and weights  = up to 32KB total
			kJobIntersectDmaSkinIndexesAndWeights_0 = kJobIntersectDmaSkinIndexesAndWeights,	// skinning info and weights - first list element
			kJobIntersectDmaSkinIndexesAndWeights_1,									// skinning info and weights - second list element
		kJobIntersectDmaVertexes1,														// primary vertex stream = up to 3 x 16KB = 48KB
			kJobIntersectDmaVertexes1_0 = kJobIntersectDmaVertexes1,					// primary vertex stream - first list element
			kJobIntersectDmaVertexes1_1,												// primary vertex stream - second list element
			kJobIntersectDmaVertexes1_2,												// primary vertex stream - third list element
		kJobIntersectDmaVertexes2,														// secondary vertex stream = up to 3 x 16KB = 48KB
			kJobIntersectDmaVertexes2_0 = kJobIntersectDmaVertexes2,					// secondary vertex stream - first list element
			kJobIntersectDmaVertexes2_1,												// secondary vertex stream - second list element
			kJobIntersectDmaVertexes2_2,												// secondary vertex stream - third list element
		kJobIntersectDmaViewportInfo,													// viewport info - used for culling
		kJobIntersectDmaLocalToWorldMatrix,												// see EdgeGeomLocalToWorldMatrix - used for culling
		kJobIntersectDmaSpuConfigInfo,													// configuration info for this geometry segment
		kJobIntersectDmaFixedPointOffsets1,												// software fixed point format offsets - primary stream
		kJobIntersectDmaFixedPointOffsets2,												// software fixed point format offsets - secondary stream
		kJobIntersectDmaInputStreamDesc1,												// primary input stream descriptor
		kJobIntersectDmaInputStreamDesc2,												// secondary input stream descriptor
		kJobIntersectDmaRay,															// the ray to cast into the scene
	kJobIntersectInputDmaEnd
};

// The CellSpursJob256 for this geom job
typedef struct JobIntersect256 {
	// Spurs job header / 48 bytes
	CellSpursJobHeader	header;														// regular spurs job header

	// Input DMA list / 168 bytes 
	struct {
		uint32_t	size;         
		uint32_t	eal;          		
	} inputDmaList[kJobIntersectInputDmaEnd];											// input DMA list

	// User data / 16 bytes
	struct {
		uint32_t	eaBlendShapeInfo;												// effective address of blend shape data, if any
		uint32_t	blendShapeInfoCount;											// number of blendshape infos
		uint32_t	eaRayResults;													// effective address of the output buffer for the ray results
		uint32_t	segmentIndex;													// index into the ray results array to write the results for this segment
	} userData;

	// Pad to make it a CellSpursJob256 / 24 bytes
	uint32_t pad[6];
} __attribute__((aligned(128))) JobIntersect256;

#ifdef __cplusplus
} // extern "C" 
#endif

#endif//JOB_GEOM_INTERFACE_H

