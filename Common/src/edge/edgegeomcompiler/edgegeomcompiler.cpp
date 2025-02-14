/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include	"edgegeomcompiler.h"
#include	"../shared/joints.h"

#include	<edge/libedgegeomtool/libedgegeomtool_wrap.h>

#include	<FCollada/FCollada.h>
#include	<FCDocument/FCDocument.h>
#include	<FCDocument/FCDController.h>
#include	<FCDocument/FCDControllerInstance.h>
#include	<FCDocument/FCDGeometry.h>
#include	<FCDocument/FCDGeometryInstance.h>
#include	<FCDocument/FCDGeometryMesh.h>
#include	<FCDocument/FCDGeometryPolygons.h>
#include	<FCDocument/FCDGeometryPolygonsInput.h>
#include	<FCDocument/FCDGeometrySource.h>
#include	<FCDocument/FCDLibrary.h>
#include	<FCDocument/FCDMaterial.h>
#include	<FCDocument/FCDMaterialInstance.h>
#include	<FCDocument/FCDMorphController.h>
#include	<FCDocument/FCDSceneNode.h>
#include	<FCDocument/FCDSkinController.h>

#include <cassert>
#include <string>
#include <vector>
#include	<iostream>
#pragma warning( push )
#pragma warning( disable : 4702 )		// unreachable code
#include	<map>
#include	<vector>
#include	<set>
#include	<algorithm>
#pragma warning( pop )

using namespace Edge::Tools;

//--------------------------------------------------------------------------------------------------

// helper struct for collecting all the polygonal data in the scene
struct PolygonsInstance
{
	const FCDGeometryInstance*	m_geometryInstance;
	const FCDGeometryPolygons*	m_polygons;
	const FCDMaterial*			m_material;
};

//--------------------------------------------------------------------------------------------------

// helper structure for determining the set of topological vertexes
typedef std::vector<uint32_t> UniqueVert;
struct	UniqueVertCompare
{
	bool operator()( const UniqueVert& c1, const UniqueVert& c2 ) const
	{
		uint32_t size = (uint32_t)c1.size();
		for ( uint32_t i=0; i<size; ++i )
		{
			uint32_t v1 = c1[i];
			uint32_t v2 = c2[i];
			if ( v1 < v2 )
				return true;
			if ( v1 > v2 )
				return false;
		}
		return false;
	}
};

static FUDaeGeometryInput::Semantic EdgeAttributeIdToColladaSemantic(const EdgeGeomAttributeId edgeAttrId)
{
	switch(edgeAttrId)
	{
	case EDGE_GEOM_ATTRIBUTE_ID_POSITION:
		return FUDaeGeometryInput::POSITION;
	case EDGE_GEOM_ATTRIBUTE_ID_NORMAL:
		return FUDaeGeometryInput::NORMAL;
	case EDGE_GEOM_ATTRIBUTE_ID_TANGENT:
		return FUDaeGeometryInput::TEXTANGENT;
	case EDGE_GEOM_ATTRIBUTE_ID_BINORMAL:
		return FUDaeGeometryInput::TEXBINORMAL;
	}
	// No match
	return FUDaeGeometryInput::UNKNOWN;
}

static EdgeGeomAttributeId ColladaSemanticToEdgeAttributeId(const FUDaeGeometryInput::Semantic semantic)
{
	switch(semantic)
	{
	case FUDaeGeometryInput::POSITION:
		return EDGE_GEOM_ATTRIBUTE_ID_POSITION;
	case FUDaeGeometryInput::NORMAL:
		return EDGE_GEOM_ATTRIBUTE_ID_NORMAL;
	case FUDaeGeometryInput::TEXTANGENT:
		return EDGE_GEOM_ATTRIBUTE_ID_TANGENT;
	case FUDaeGeometryInput::TEXBINORMAL:
		return EDGE_GEOM_ATTRIBUTE_ID_BINORMAL;
	case FUDaeGeometryInput::TEXCOORD:
		return EDGE_GEOM_ATTRIBUTE_ID_UV0; // NOTE: this may need to be offset later if we have multiple texture coordinates
	}
	// No match
	return EDGE_GEOM_ATTRIBUTE_ID_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
/**
	Find blend shape targets.
**/
//--------------------------------------------------------------------------------------------------

static void	FindBlendShapeTargets( const FCDocument* document, std::set<const FCDGeometry*>& blendShapeTargets )
{
	// iterate over the controllers in the document
	const FCDControllerLibrary* lib = document->GetControllerLibrary();
	for ( uint32_t i=0; i<lib->GetEntityCount(); ++i )
	{
		const FCDController* controller = lib->GetEntity( i );
		if ( controller->IsMorph() )
		{
			// if it's a morph controller, add the targets to the list
			const FCDMorphController* morph = controller->GetMorphController();
			for ( uint32_t j=0; j<morph->GetTargetCount(); ++j )
				blendShapeTargets.insert( morph->GetTarget(j)->GetGeometry() );
		}
	}
}

//--------------------------------------------------------------------------------------------------
/**
	Retrieves mesh polygon objects and their associated shaders from a scene node hierarchy.
**/
//--------------------------------------------------------------------------------------------------

static void	GetPolygons( const FCDSceneNode* sceneNode,
						 std::vector< PolygonsInstance >& instances,
						 const std::set<const FCDGeometry*>& blendShapeTargets )
{
	// loop over node's children
	for ( uint32_t i=0; i<sceneNode->GetChildrenCount(); ++i )
	{
		const FCDSceneNode* child = sceneNode->GetChild( i );
		// look for instances
		for ( uint32_t j=0; j<child->GetInstanceCount(); ++j )
		{
			// get the instanced entity
			const FCDEntityInstance* entityInstance = child->GetInstance( j );
			const FCDEntity* entity = entityInstance->GetEntity();
			// per-instance type behaviour
			switch ( entityInstance->GetType() )
			{
			case FCDEntityInstance::CONTROLLER:
			case FCDEntityInstance::GEOMETRY:
				{
					// geometry instances set the material on the instance
					FCDGeometryInstance* geometryInstance = (FCDGeometryInstance*)entityInstance;

					// make a new instance structure
					PolygonsInstance instance;
					instance.m_geometryInstance = geometryInstance;

					// get geometry that we're directly or indirectly instancing
					const FCDGeometry* geometry;
					if ( entityInstance->HasType( FCDControllerInstance::GetClassType() ) )
					{
						const FCDController* controller = (const FCDController*)entity;
						geometry = controller->GetBaseGeometry();
					}
					else
					{
                        geometry = (const FCDGeometry*)entity;
					}
					// discount blend shape targets
					if ( blendShapeTargets.find( geometry ) != blendShapeTargets.end() )
						break;
					// is it a mesh?
					if ( geometry->IsMesh() )
					{
						const FCDGeometryMesh* mesh = geometry->GetMesh();
						// get polygons
						for ( uint32_t k=0; k<mesh->GetPolygonsCount(); ++k )
						{
							const FCDGeometryPolygons* polys = mesh->GetPolygons( k );
							// find the material for this instance of this object
							const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance( polys->GetMaterialSemantic() );
							if ( materialInstance == NULL)
								throw std::runtime_error( "Could not find material instance in Collada scene" );
							const FCDMaterial* material = materialInstance->GetMaterial();
							if ( material == NULL)
								throw std::runtime_error( "Could not find material in Collada scene" );
							// add to list
							instance.m_polygons = polys;
							instance.m_material = material;
							instances.push_back( instance );
						}
					}
				}
				break;
			case FCDEntityInstance::SIMPLE:
			default:
				break;
			}
		}
		// recurse
		GetPolygons( child, instances, blendShapeTargets );
	}
}

//--------------------------------------------------------------------------------------------------
/**
	Retrieves mesh polygon objects and their associated shaders from the document.
**/
//--------------------------------------------------------------------------------------------------

static void	GetPolygons( const FCDocument* document,
						 std::vector< PolygonsInstance >& instances,
						 const std::set<const FCDGeometry*>& blendShapeTargets )
{
	// loop over visual scenes in the document
	const FCDVisualSceneNodeLibrary* library = document->GetVisualSceneLibrary();
	for ( uint32_t i=0; i<library->GetEntityCount(); ++i )
	{
		// get scene node
		const FCDSceneNode* sceneNode = library->GetEntity( i );
		// recursive
		GetPolygons( sceneNode, instances, blendShapeTargets );
	}
}

//--------------------------------------------------------------------------------------------------
/**
	assigns an index to each material
**/
//--------------------------------------------------------------------------------------------------

static void	MakeMaterialIndexes( const std::vector< PolygonsInstance >& instances,
								 std::map< const FCDMaterial*, int >& indexes )
{
	int index = 0;
	for ( uint32_t i=0; i<instances.size(); ++i )
	{
		if ( indexes.find( instances[i].m_material ) == indexes.end() )
			indexes[ instances[i].m_material ] = index++;
	}
}

static void GetVertexesAndPolygons(const FCDGeometryPolygons *polys,
								   std::map<UniqueVert, uint32_t, UniqueVertCompare> *outUniqueVerts,
								   std::vector<int32_t> *outPolyList)
{
	const uint32_t numInputs = (uint32_t)polys->GetInputCount();

	std::vector<const uint32*> indexesPerInput;
	for(uint32_t iInput=0; iInput<numInputs; ++iInput)
	{
		const FCDGeometryPolygonsInput *pInput = polys->GetInput(iInput);
		indexesPerInput.push_back(pInput->GetIndices()); // store this input's vertex indexes for future reference
	}

	// Iterate over all the polygons and build a list of unique vertexes,
	// plus an indexed polygon list.
	outUniqueVerts->clear();
	outPolyList->clear();
	uint32_t numUniqueVerts = 0;
	UniqueVert vert( numInputs );
	const uint32_t numFaces = (uint32_t)polys->GetFaceCount();
	uint32_t vertexIndex = 0;
	for ( uint32_t i=0; i<numFaces; ++i )
	{
		// loop over corners of the face
		const uint32_t numCorners = (uint32_t)polys->GetFaceVertexCount( i );
		for ( uint32_t j=0; j<numCorners; ++j )
		{
			// make the unique vert structure, using only the useful Edge attributes
			for ( uint32_t iAttr=0; iAttr<numInputs; ++iAttr )
			{
				vert[iAttr] = indexesPerInput[iAttr][vertexIndex];
			}
			++vertexIndex;
			// look for the vertex
			uint32_t index;
			std::map<UniqueVert, uint32_t, UniqueVertCompare>::const_iterator it = outUniqueVerts->find( vert );
			if ( it == outUniqueVerts->end() )
			{
				// not found - it's a new vertex
				index = numUniqueVerts;
				(*outUniqueVerts)[ vert ] = numUniqueVerts++;
			}
			else
			{
				// found the vertex - use its stored index
				index = it->second;
			}
			// add to poly list
			outPolyList->push_back( index );
		}
		// end face with -1
		outPolyList->push_back( -1 );
	}
	// terminate with final -1
	outPolyList->push_back( -1 );
}

//--------------------------------------------------------------------------------------------------
/**
	Copy skinning data into the geometry unit structure.
**/
//--------------------------------------------------------------------------------------------------

static void	CopySkinData( const FCDSkinController* skin, 
						  const std::vector<const FCDSceneNode*>& joints,
						  const std::map<UniqueVert, uint32_t, UniqueVertCompare>& uniqueVerts,
						  int32_t positionInputIndex,
						  EdgeGeomScene *edgeScene)
{
	// allocate the storage in the Edge scene.
	edgeScene->m_skinningWeightsPerVertex = (float*)edgeGeomAlloc(kEdgeGeomNumInputBonesPerVertex * edgeScene->m_numVertexes * sizeof(float));
	edgeScene->m_matrixIndexesPerVertex = (int32_t*)edgeGeomAlloc(kEdgeGeomNumInputBonesPerVertex * edgeScene->m_numVertexes * sizeof(int32_t));
	memset(edgeScene->m_skinningWeightsPerVertex, 0x00, kEdgeGeomNumInputBonesPerVertex*edgeScene->m_numVertexes * sizeof(float)); // initialize to 0
	memset(edgeScene->m_matrixIndexesPerVertex, 0xFF, kEdgeGeomNumInputBonesPerVertex*edgeScene->m_numVertexes * sizeof(int32_t)); // initialize to -1

	// find the remap between skeleton indexes and indexes within this skin
    const FCDSkinControllerJoint* skinJoints = skin->GetJoints();
    uint32_t skinJointsCount = (uint32_t)skin->GetJointCount();

	// ColladaMaya for one will export skinning data with no joints, which seems wrong
	if ( skinJointsCount == 0)
		throw std::runtime_error( "Found skinning data, but no joints (known ColladaMaya problem)" );
	std::vector< unsigned > jointRemap( skinJointsCount );
	for ( uint32_t i=0; i<skinJointsCount; ++i )
	{
		int32_t index = -1;
        for ( uint32_t j=0; j<joints.size(); ++j )
		{
            if ( skinJoints[i].GetId() == joints[j]->GetSubId() )
			{
				index = j;
				break;
			}
		}
		if ( index < 0 )
			throw std::runtime_error( "skin joint was not found in skeleton joint list" );
		jointRemap[i] = index;
	}

	std::vector< std::pair<float, int32_t> > jointWeights;

	// loop over the vertexes and copy the data
	for ( std::map<UniqueVert, uint32_t, UniqueVertCompare>::const_iterator it = uniqueVerts.begin(); it != uniqueVerts.end(); ++it )
	{
		const UniqueVert& vert = it->first;
		const uint32_t index = it->second;

		// skip to this vertex's skin data
		int32_t* indexes = edgeScene->m_matrixIndexesPerVertex + index*kEdgeGeomNumInputBonesPerVertex;
		float* weights = edgeScene->m_skinningWeightsPerVertex + index*kEdgeGeomNumInputBonesPerVertex;

		// get the influence index - this corresponds to the positional index in the vertex
		const uint32_t influenceIndex = vert[ positionInputIndex ];
        const FCDSkinControllerVertex* skinVertex = skin->GetVertexInfluence( influenceIndex );

		// get the weight/index pairs
        const uint32_t influencePairCount = (uint32_t)skinVertex->GetPairCount();
		jointWeights.resize( influencePairCount );
		for ( uint32_t i=0; i<influencePairCount; ++i )
		{
            const FCDJointWeightPair* pair = skinVertex->GetPair( i );
			if( pair->jointIndex >= 0 && (uint32_t) pair->jointIndex < jointRemap.size() )
			{
				jointWeights[i].first = pair->weight;
				jointWeights[i].second = jointRemap[ pair->jointIndex ];
			}
			else
			{
				jointWeights[i] = std::make_pair( 0.0f, -1 );
			}
		}
		
		// now, take the top four influences
		std::sort( jointWeights.begin(), jointWeights.end(), std::greater<std::pair<float,int32_t> >() );

		// pad the array, to simplify the logic below
		while ( jointWeights.size() < kEdgeGeomNumInputBonesPerVertex )
			jointWeights.push_back( std::make_pair( 0.0f, -1 ) );

		// normalize the top weights
		float weightSum = 0.0f;
		for ( uint32_t i=0; i<kEdgeGeomNumInputBonesPerVertex; ++i )
		{
			// if there is NO WAY that a weight can ever be rounded up to a whole value, it's better to mark 
			// the matrix as unused, so it does not pull in unnecessary matrices during partitioning
			if ( jointWeights[i].first < (1.0f/255.0f)*0.75f )
			{
				jointWeights[i] = std::make_pair( 0.0f, -1 );
			}
			else
			{
				weightSum += jointWeights[i].first;
			}
		}
		const float invWeightSum = ( weightSum != 0.0f ) ? ( 1.0f/weightSum ) : 0.0f;

		// dump out the weights, renormalizing in the process
		float scaledSum = 0.0f;  // accumulate the scaled sums, so we can set the highest weight precisely to make them equal 1.0
		for ( uint32_t i=0; i<kEdgeGeomNumInputBonesPerVertex; ++i )
		{
			indexes[i] = jointWeights[i].second;
			if ( i != 0 )  // skip the first weight, so we know what the remainder should be
			{
				const float scaledWeight = jointWeights[i].first * invWeightSum;
				scaledSum += scaledWeight;
				weights[i] = scaledWeight;
			}
		}
		
		// finally, rewrite the top weight to be exactly the difference between the sum of 1,2,3's weights and 1.0
		weights[0] = 1.0f - scaledSum;
	}
}

//--------------------------------------------------------------------------------------------------
/**
	Copy blend shape data into the geometry unit structure.
**/
//--------------------------------------------------------------------------------------------------
static void	CopyBlendShapeData( const FCDMorphController* morph, 
								EdgeGeomScene *edgeScene )
{
	// Assume (until it's proven otherwise) that the Collada scene's blend shape targets have all
	// the same vertex attribute sources as the main mesh.  At the end of the function, we see which
	// blended attributes we *really* found, and adjust these values and arrays accordingly.
	edgeScene->m_numBlendShapes = (uint32_t)morph->GetTargetCount();
	edgeScene->m_numFloatsPerDelta = edgeScene->m_numFloatsPerVertex;
	edgeScene->m_numBlendedAttributes = edgeScene->m_numVertexAttributes;
	edgeScene->m_vertexDeltas = (float*)edgeGeomAlloc(edgeScene->m_numBlendShapes * edgeScene->m_numVertexes * edgeScene->m_numFloatsPerDelta * sizeof(float));
	edgeScene->m_blendedAttributeIds = (EdgeGeomAttributeId*)edgeGeomAlloc(edgeScene->m_numBlendedAttributes * sizeof(EdgeGeomAttributeId));
	edgeScene->m_blendedAttributeIndexes = (uint16_t*)edgeGeomAlloc(edgeScene->m_numBlendedAttributes * sizeof(uint16_t));
	
	memset(edgeScene->m_vertexDeltas, 0, edgeScene->m_numBlendShapes * edgeScene->m_numVertexes * edgeScene->m_numFloatsPerDelta * sizeof(float));
	memcpy(edgeScene->m_blendedAttributeIds, edgeScene->m_vertexAttributeIds,
		edgeScene->m_numBlendedAttributes * sizeof(EdgeGeomAttributeId));
	memcpy(edgeScene->m_blendedAttributeIndexes, edgeScene->m_vertexAttributeIndexes,
		edgeScene->m_numBlendedAttributes * sizeof(uint16_t));

	// As we're building the delta streams, we keep track of which attributes have non-zero deltas.
	// Attributes with no non-zero deltas will be removed from the scene's list of blended attributes
	// at the end of this function.
	bool *hasNonZeroDeltas = new bool[edgeScene->m_numBlendedAttributes];
	memset(hasNonZeroDeltas, 0, edgeScene->m_numBlendedAttributes * sizeof(bool));

	// loop over targets, to copy the morphed data
	for ( uint32_t iShape = 0; iShape<edgeScene->m_numBlendShapes; ++iShape )
	{
		float *destDeltas = edgeScene->m_vertexDeltas + iShape * edgeScene->m_numVertexes * edgeScene->m_numFloatsPerDelta;

		const FCDMorphTarget* morphTarget = morph->GetTarget( iShape );
		const FCDGeometry* targetGeometry = morphTarget->GetGeometry();
		if ( !targetGeometry->IsMesh() )
			throw std::runtime_error( "blend shape target is not a mesh" );
		const FCDGeometryMesh* targetMesh = targetGeometry->GetMesh();
		assert(targetMesh->GetPolygonsCount() == 1); // we only support one polygon group per mesh
		const FCDGeometryPolygons *targetPolys = targetMesh->GetPolygons(0);

		// check there are no holes
		if ( targetPolys->GetHoleCount() != 0 )
			throw std::runtime_error( "no support for polygons with holes" );

		// Iterate over the polygon set's inputs (basically, its vertex attributes), to determine
		// which attributes are present and how many components each has.
		const uint32_t numInputs = (uint32_t)targetPolys->GetInputCount();
		std::vector<uint32_t> attributeToInput; // maps Edge attributes in the scene to Collada inputs
		for(uint32_t iInput=0; iInput<numInputs; ++iInput)
		{
			const FCDGeometryPolygonsInput *pInput = targetPolys->GetInput(iInput);
			const FCDGeometrySource *pSource = pInput->GetSource();
			const FUDaeGeometryInput::Semantic semantic = pSource->GetType();
			EdgeGeomAttributeId edgeAttrId = ColladaSemanticToEdgeAttributeId(semantic);
			if (edgeAttrId == EDGE_GEOM_ATTRIBUTE_ID_UNKNOWN)
				continue;

			attributeToInput.push_back(iInput);
		}

		std::vector<int32_t> polyList; // We don't really need a poly list, but we need to pass one anyway
		std::map<UniqueVert, uint32_t, UniqueVertCompare> uniqueVerts;
		GetVertexesAndPolygons(targetPolys, &uniqueVerts, &polyList);

		// @note we have no good way to match up geometry - we must assume that the target mesh has the same structure as the base mesh.

		// For every vertex attribute in the edge scene, we look for a corresponding attribute
		// in the morph target.  The order & presence of attributes can theoretically vary from
		// shape to shape.
		for(uint32_t iAttr=0; iAttr<edgeScene->m_numBlendedAttributes; ++iAttr)
		{
			// RISKY ASSUMPTION ALERT: we assume, here, that the order of the inputs in the main and target
			// meshes are the same.
			uint32_t iInput = attributeToInput[iAttr];

			// Find the Collada attribute semantic for this attribute.
			EdgeGeomAttributeId edgeAttrId = edgeScene->m_blendedAttributeIds[iAttr];
			FUDaeGeometryInput::Semantic attrSemantic = EdgeAttributeIdToColladaSemantic(edgeAttrId);
			if (attrSemantic == FUDaeGeometryInput::UNKNOWN) // this attribute is not present in the target mesh
				continue;

			// Cache the indexes (within each vertex) of this attribute in both the destination delta
			// stream, and the Edge scene's main vertex stream (so we can subtract the two and get the final delta).
			uint16_t destAttrIndex = edgeScene->m_blendedAttributeIndexes[iAttr];
			uint16_t mainAttrIndex = 0;
			bool foundInMain = false;
			for(uint32_t iMainAttr=0; iMainAttr<edgeScene->m_numVertexAttributes; ++iMainAttr)
			{
				if (edgeScene->m_vertexAttributeIds[iMainAttr] == edgeAttrId)
				{
					mainAttrIndex = edgeScene->m_vertexAttributeIndexes[iMainAttr];
					foundInMain = true;
					break;
				}
			}
			assert(foundInMain); // If this fails, the morph target mesh contains attributes not present in the main mesh!

			// search the target mesh for this attribute, and get a pointer to its raw float data.
			const FCDGeometrySource *targetAttrSource = targetPolys->GetInput(iInput)->GetSource();
			if (targetAttrSource == NULL)
				throw std::runtime_error( "failed to find matching blend shape target source for attribute" );
			const float *targetAttrData = targetAttrSource->GetData();
			uint32_t attrComponentCount = targetAttrSource->GetStride();

			// loop over the target vertexes and copy the data
			for ( std::map<UniqueVert, uint32_t, UniqueVertCompare>::const_iterator it = uniqueVerts.begin(); it != uniqueVerts.end(); ++it )
			{
				const UniqueVert& vert = it->first;
				const uint32_t index = it->second;
				// RISKY ASSUMPTION ALERT: we assume that the vertex indexes are the same between the main and target mesh.
				// otherwise, I think we'd be doing a linear search through the mainVerts array for the element whose ->second
				// value matches index.

				// skip to the proper vertex/attribute's data
				const float *mainAttr = edgeScene->m_vertexes + index*edgeScene->m_numFloatsPerVertex + mainAttrIndex;
				const float *targetAttr = targetAttrData + attrComponentCount * vert[iInput];
				float* destAttr = destDeltas + index * edgeScene->m_numFloatsPerDelta + destAttrIndex;
				
				for(uint32_t iComponent=0; iComponent<attrComponentCount; ++iComponent)
				{
					destAttr[iComponent] = targetAttr[iComponent] - mainAttr[iComponent];
					if (destAttr[iComponent] != 0)
					{
						hasNonZeroDeltas[iAttr] = true;
					}
				}
			}
		}
	}

	// Strip out attributes that didn't have any non-zero deltas.
	// Actually freeing all the zero deltas would be too much of a pain, but at least
	// the scene won't report that they're there.
	uint8_t finalNumBlendedAttributes = 0;
	for(uint8_t iAttr=0; iAttr<edgeScene->m_numBlendedAttributes; ++iAttr)
	{
		if (hasNonZeroDeltas[iAttr])
		{
			edgeScene->m_blendedAttributeIds[finalNumBlendedAttributes] = edgeScene->m_blendedAttributeIds[iAttr];
			edgeScene->m_blendedAttributeIndexes[finalNumBlendedAttributes] = edgeScene->m_blendedAttributeIndexes[iAttr];
			finalNumBlendedAttributes++;
		}
	}
	edgeScene->m_numBlendedAttributes = finalNumBlendedAttributes;
	delete [] hasNonZeroDeltas;
}

//--------------------------------------------------------------------------------------------------
/**
	Select appropriate input/output/shape flavors for an Edge Scene, based on which vertex
	attributes are available.  If a match cannot be found (or if the forceCustomFlavors flag is set),
	a custom flavor will be generated.
**/
//--------------------------------------------------------------------------------------------------

static void FindAttributesInScene(const EdgeGeomAttributeId *attributeIds, uint8_t numAttributes,
								  bool *outHasPosition, bool *outHasNormal, bool *outHasTangent, bool *outHasBinormal, uint8_t *outNumUVs)
{
	*outHasPosition = *outHasNormal = *outHasTangent = *outHasBinormal = false;
	*outNumUVs = 0;
	for(uint32_t iAttr = 0; iAttr < numAttributes; ++iAttr)
	{
		switch(attributeIds[iAttr])
		{
		case EDGE_GEOM_ATTRIBUTE_ID_POSITION:
			*outHasPosition = true;
			break;
		case EDGE_GEOM_ATTRIBUTE_ID_NORMAL:
			*outHasNormal = true;
			break;
		case EDGE_GEOM_ATTRIBUTE_ID_TANGENT:
			*outHasTangent = true;
			break;
		case EDGE_GEOM_ATTRIBUTE_ID_BINORMAL:
			*outHasBinormal = true;
			break;
		case EDGE_GEOM_ATTRIBUTE_ID_UV0:
		case EDGE_GEOM_ATTRIBUTE_ID_UV1:
		case EDGE_GEOM_ATTRIBUTE_ID_UV2:
		case EDGE_GEOM_ATTRIBUTE_ID_UV3:
			*outNumUVs += 1;
			break;
		}
	}
}
void ChooseVertexFormatsForScene(const EdgeGeomScene &edgeScene, EdgeGeomSegmentFormat &edgeFormat)
{
	// Determine which input attributes are present
	bool hasPosition = false; // this had better be true!
	bool hasNormal = false;
	bool hasTangent = false;
	bool hasBinormal = false;
	uint8_t numUVs = 0;
	FindAttributesInScene(edgeScene.m_vertexAttributeIds, edgeScene.m_numVertexAttributes,
		&hasPosition, &hasNormal, &hasTangent, &hasBinormal, &numUVs);
	assert(hasPosition); // We've checked this before, but just to make sure...

	// The Edge samples recreate the binormal at runtime in the vertex shader.  Therefore,
	// we explicitly exclude it when building vertex formats for these scenes.
	uint32_t numUsefulAttributes = edgeScene.m_numVertexAttributes;
	if (hasBinormal)
	{
		hasBinormal = false;
		--numUsefulAttributes;
	}

	// Determine whether the scene is static (that is, unanimated and unaffected by blend shapes).
	// If so, we can make use of an optimization in Edge for efficient static geometry processing.
	bool isStaticScene = edgeScene.m_skinningWeightsPerVertex == NULL &&
		edgeScene.m_matrixIndexesPerVertex == NULL &&
		edgeScene.m_numBlendShapes == 0;

	// The first stream will always be position-only, F32x3. Decompressing this format is
	// extremely efficient, and it facilitates an easy depth-only pass
	edgeFormat.m_spuInputVertexFormats[0] = edgeGeomGetSpuVertexFormat(EDGE_GEOM_SPU_VERTEX_FORMAT_F32c3);

	// Choose SPU secondary-input and output formats based on the number of attributes.
	edgeFormat.m_spuInputVertexFormats[1] = edgeFormat.m_spuInputVertexDeltaFormat = NULL;
	edgeFormat.m_spuOutputVertexFormat = NULL;
	edgeFormat.m_rsxOnlyVertexFormat = NULL;

	// Generate secondary SPU input vertex format, if necessary.  
	// The secondary SPU input stream will only contain normals and tangents
	// (i.e. attributes that can be processed by Edge).
	// EXCEPTION: For static scenes, these attributes not needed on the RSX,
	// and are included in the RSX-only stream below.
	if (!isStaticScene && (hasNormal || hasTangent))
	{
		EdgeGeomSpuVertexFormat *format = (EdgeGeomSpuVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomSpuVertexFormat));
		edgeFormat.m_spuInputVertexFormats[1] = format;
		memset(format, 0, sizeof(EdgeGeomSpuVertexFormat));
		format->m_numAttributes = 0;
		for(uint8_t iAttr=0; iAttr<edgeScene.m_numVertexAttributes; ++iAttr)
		{
			if (edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_NORMAL &&
				edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
				continue;

			format->m_attributeDefinition[format->m_numAttributes].m_attributeId = edgeScene.m_vertexAttributeIds[iAttr];
			format->m_attributeDefinition[format->m_numAttributes].m_byteOffset = format->m_vertexStride;

			// Choose an appropriate type for this attribute, based on its ID.
			// This is difficult to generalize, so we err on the side of safety.
			switch(format->m_attributeDefinition[format->m_numAttributes].m_attributeId)
			{
			case EDGE_GEOM_ATTRIBUTE_ID_NORMAL:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kSpuAttr_X11Y11Z10N;//kSpuAttr_UnitVector;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 1; //3;
				format->m_vertexStride += 4;//3;
				break;
			case EDGE_GEOM_ATTRIBUTE_ID_TANGENT:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kSpuAttr_I16N; //kSpuAttr_UnitVector;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 4;
				format->m_vertexStride += 8;//3;
				break;
			default:
				throw std::runtime_error("Unrecognized vertex attribute ID while building secondary input stream format");
			}
			format->m_numAttributes++;
		}
	}

	// Generate RSX-only vertex format, if necessary.  It contains any remaining attributes that Edge
	// doesn't care about, such as texture coordinates.
	// EXCEPTION: For static geometry, all non-position attributes go into the RSX-only stream.
	if (numUVs > 0 || (isStaticScene && (hasNormal || hasTangent)) )
	{
		EdgeGeomRsxVertexFormat *format = (EdgeGeomRsxVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomRsxVertexFormat));
		edgeFormat.m_rsxOnlyVertexFormat = format;
		memset(format, 0, sizeof(EdgeGeomRsxVertexFormat));
		format->m_numAttributes = 0;
		for(uint8_t iAttr=0; iAttr<edgeScene.m_numVertexAttributes; ++iAttr)
		{
			if (isStaticScene)
			{
				// In static scenes, all non-position attributes are in this stream.
				if (edgeScene.m_vertexAttributeIds[iAttr] == EDGE_GEOM_ATTRIBUTE_ID_POSITION)
					continue;
			}
			else
			{
				// In non-static scenes, all non-skinnable attributes are in this stream.
				// In practice, this is only UVs.
				if (edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_UV0 &&
					edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_UV1 &&
					edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_UV2 &&
					edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_UV3)
					continue;
			}

			format->m_attributeDefinition[format->m_numAttributes].m_attributeId = edgeScene.m_vertexAttributeIds[iAttr];
			format->m_attributeDefinition[format->m_numAttributes].m_byteOffset = format->m_vertexStride;

			// Choose an appropriate type for this attribute, based on its ID.
			// This is difficult to generalize, so we err on the side of safety.
			switch(format->m_attributeDefinition[format->m_numAttributes].m_attributeId)
			{
			case EDGE_GEOM_ATTRIBUTE_ID_NORMAL:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kRsxAttr_X11Y11Z10N;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 1;
				format->m_vertexStride += 4;
				break;
			case EDGE_GEOM_ATTRIBUTE_ID_TANGENT:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kRsxAttr_I16N;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 4;
				format->m_vertexStride += 8;
				break;
			case EDGE_GEOM_ATTRIBUTE_ID_UV0:
			case EDGE_GEOM_ATTRIBUTE_ID_UV1:
			case EDGE_GEOM_ATTRIBUTE_ID_UV2:
			case EDGE_GEOM_ATTRIBUTE_ID_UV3:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kRsxAttr_F16;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 2; // assume two-component UV pairs
				format->m_vertexStride += 2 * sizeof(uint16_t);
				break;
			default:
				throw std::runtime_error("Unrecognized vertex attribute ID while building RSX-only stream format");
			}

			format->m_numAttributes++;
		}		
	}

	// Generate output vertex format
	if (numUsefulAttributes == 1 || isStaticScene)
		edgeFormat.m_spuOutputVertexFormat = edgeGeomGetRsxVertexFormat(EDGE_GEOM_RSX_VERTEX_FORMAT_F32c3);
	else if (numUsefulAttributes == 3 && hasNormal && hasTangent)
		edgeFormat.m_spuOutputVertexFormat = edgeGeomGetRsxVertexFormat(EDGE_GEOM_RSX_VERTEX_FORMAT_F32c3_X11Y11Z10N_I16Nc4);
	else
	{
		EdgeGeomRsxVertexFormat *format = (EdgeGeomRsxVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomRsxVertexFormat));
		edgeFormat.m_spuOutputVertexFormat = format;
		memset(format, 0, sizeof(EdgeGeomRsxVertexFormat));
		format->m_numAttributes = 0;
		for(uint8_t iAttr=0; iAttr<edgeScene.m_numVertexAttributes; ++iAttr)
		{
			// Only position, normal, tangent and binormal are stored here.
			if (edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_POSITION &&
				edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_NORMAL &&
				edgeScene.m_vertexAttributeIds[iAttr] != EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
				continue;

			format->m_attributeDefinition[format->m_numAttributes].m_attributeId = edgeScene.m_vertexAttributeIds[iAttr];
			format->m_attributeDefinition[format->m_numAttributes].m_byteOffset = format->m_vertexStride;

			// Choose an appropriate type for this attribute, based on its ID.
			// This is difficult to generalize, so we err on the side of safety.
			switch(format->m_attributeDefinition[format->m_numAttributes].m_attributeId)
			{
			case EDGE_GEOM_ATTRIBUTE_ID_POSITION:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kRsxAttr_F32;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 3;
				format->m_vertexStride += 3 * sizeof(float);
				break;
			case EDGE_GEOM_ATTRIBUTE_ID_NORMAL:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kRsxAttr_X11Y11Z10N;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 1;
				format->m_vertexStride += 4;
				break;
			case EDGE_GEOM_ATTRIBUTE_ID_TANGENT:
				format->m_attributeDefinition[format->m_numAttributes].m_type = kRsxAttr_I16N;
				format->m_attributeDefinition[format->m_numAttributes].m_count = 4;
				format->m_vertexStride += 8;
				break;
			default:
				throw std::runtime_error("Unrecognized vertex attribute ID while building custom output flavor");
			}

			format->m_numAttributes++;
		}
	}

	// Now do the same thing for the vertex delta format, if there are any blend shapes
	if (edgeScene.m_numBlendShapes > 0)
	{
		// We can't blend attributes that aren't also present in the SPU input streams.
		// So, first we find the intersection of the scene's blended attributes and 
		// the attributes in the SPU input vertex formats.
		EdgeGeomAttributeId legalBlendedAttributeIds[16];
		uint8_t numLegalIds = 0;
		for(uint32_t iAttr=0; iAttr<edgeScene.m_numBlendedAttributes; ++iAttr)
		{
			EdgeGeomAttributeId attrId = edgeScene.m_blendedAttributeIds[iAttr];
			bool found = false;
			// Check both SPU input streams
			for(uint32_t iInputStream=0; iInputStream<2; ++iInputStream)
			{
				if (found)
					break; // if we already found the attribute, we're done.
				EdgeGeomSpuVertexFormat *inputFormat = edgeFormat.m_spuInputVertexFormats[iInputStream];
				if (inputFormat == NULL)
					continue; // don't check streams that don't exist!
				for(uint32_t iInputAttr=0; iInputAttr<inputFormat->m_numAttributes; ++iInputAttr)
				{
					if (inputFormat->m_attributeDefinition[iInputAttr].m_attributeId == attrId)
					{
						legalBlendedAttributeIds[numLegalIds++] = attrId;
						found = true;
						break;
					}
				}
			}
		}

		FindAttributesInScene(legalBlendedAttributeIds, numLegalIds,
			&hasPosition, &hasNormal, &hasTangent, &hasBinormal, &numUVs);

		// Vertex delta stream formats always use a custom vertex format.
		{
			EdgeGeomSpuVertexFormat *format = (EdgeGeomSpuVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomSpuVertexFormat));
			edgeFormat.m_spuInputVertexDeltaFormat = format;
			memset(format, 0, sizeof(EdgeGeomSpuVertexFormat));
			format->m_numAttributes = numLegalIds;
			for(uint8_t iAttr=0; iAttr<numLegalIds; ++iAttr)
			{
				format->m_attributeDefinition[iAttr].m_attributeId = legalBlendedAttributeIds[iAttr];
				format->m_attributeDefinition[iAttr].m_byteOffset = format->m_vertexStride;

				// Choose an appropriate type for this attribute, based on its ID.
				// This is difficult to generalize, so we err on the side of safety.
				switch(format->m_attributeDefinition[iAttr].m_attributeId)
				{
				case EDGE_GEOM_ATTRIBUTE_ID_POSITION:
					// Fixed point position deltas are set to 4.12 fixed point format
					format->m_attributeDefinition[iAttr].m_type = kSpuAttr_FixedPoint;
					format->m_attributeDefinition[iAttr].m_count = 3;
					for(uint32_t iComponent=0; iComponent<3; ++iComponent)
					{
						format->m_attributeDefinition[iAttr].m_fixedPointBitDepthInteger[iComponent] = 4;
						format->m_attributeDefinition[iAttr].m_fixedPointBitDepthFractional[iComponent] = 12;
					}
					format->m_vertexStride += 6;
					break;
				case EDGE_GEOM_ATTRIBUTE_ID_NORMAL:
				case EDGE_GEOM_ATTRIBUTE_ID_TANGENT:
					format->m_attributeDefinition[iAttr].m_type = kSpuAttr_F16; // do NOT use a unit vector for vertex deltas; it will always have a magnitude of 1.0!
					format->m_attributeDefinition[iAttr].m_count = 3;
					format->m_vertexStride += format->m_attributeDefinition[iAttr].m_count * sizeof(uint16_t);
					break;
				default:
					throw std::runtime_error("Unrecognized vertex attribute ID while building custom blend shape flavor");
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
/**
	Populate the edge structures for one polygons object.
**/
//--------------------------------------------------------------------------------------------------

static void	CreateEdgeScene( const PolygonsInstance& instance, int32_t materialId, const std::vector<const FCDSceneNode*>& joints,
							EdgeGeomScene *edgeScene,	uint32_t processingFlags )
{
	(void)processingFlags;

	const FCDGeometryPolygons* polys = instance.m_polygons;
	// check there are no holes
	if ( polys->GetHoleCount() != 0 )
		throw std::runtime_error( "no support for polygons with holes" );

	// Iterate over the polygon set's inputs (basically, its vertex attributes), to determine
	// which attributes are present and how many components each has.
	const uint32_t numInputs = (uint32_t)polys->GetInputCount();
	edgeScene->m_vertexAttributeIds = (EdgeGeomAttributeId*)edgeGeomAlloc(numInputs * sizeof(EdgeGeomAttributeId)); // possibly more than we need, but hey
	edgeScene->m_vertexAttributeIndexes = (uint16_t*)edgeGeomAlloc(numInputs * sizeof(uint16_t));; // ditto
	memset(edgeScene->m_vertexAttributeIds, 0, numInputs*sizeof(EdgeGeomAttributeId));
	memset(edgeScene->m_vertexAttributeIndexes, 0, numInputs*sizeof(uint16_t));
	edgeScene->m_numVertexAttributes = 0;
	edgeScene->m_numFloatsPerVertex = 0;
	int32_t positionInputIndex = -1; // Indexes into each UniqueVert of various important attributes.  Remember these, as we'll need them later
	int32_t normalInputIndex = -1;
	int32_t tangentInputIndex = -1;
	int32_t binormalInputIndex = -1;
	uint32_t numUvSources = 0;
	std::vector<const float*> inputDataSources; // pointers to each input's data in the Collada scene
	std::vector<uint32_t> inputComponentCounts; // component count for each Collada input
	std::vector<uint32_t> attributeToInput; // maps Edge attributes in the scene to Collada inputs
	for(uint32_t iInput=0; iInput<numInputs; ++iInput)
	{
		const FCDGeometryPolygonsInput *pInput = polys->GetInput(iInput);
		const FCDGeometrySource *pSource = pInput->GetSource();
		inputComponentCounts.push_back(pSource->GetStride());
		inputDataSources.push_back(pSource->GetData());

		const FUDaeGeometryInput::Semantic semantic = pSource->GetType();
		EdgeGeomAttributeId edgeAttrId = ColladaSemanticToEdgeAttributeId(semantic);
		if (edgeAttrId == EDGE_GEOM_ATTRIBUTE_ID_UNKNOWN)
			continue;
		switch(semantic)
		{
		case FUDaeGeometryInput::POSITION:
			assert(positionInputIndex == -1); // edgegeomcompiler only supports one position stream per scene
			positionInputIndex = iInput;
			break;
		case FUDaeGeometryInput::NORMAL:
			assert(normalInputIndex == -1); // edgegeomcompiler only supports one normal stream per scene
			normalInputIndex = iInput;
			break;
		case FUDaeGeometryInput::TEXTANGENT:
			assert(tangentInputIndex == -1); // edgegeomcompiler only supports one tangent stream per scene
			tangentInputIndex = iInput;
			break;
		case FUDaeGeometryInput::TEXBINORMAL:
			assert(binormalInputIndex == -1); // edgegeomcompiler only supports one binormal stream per scene
			binormalInputIndex = iInput;
			break;
		case FUDaeGeometryInput::TEXCOORD:
			// Increment the attribute ID appropriately depending on howm any UV sources we've already encountered
			edgeAttrId = (EdgeGeomAttributeId)(EDGE_GEOM_ATTRIBUTE_ID_UV0 + numUvSources);
			numUvSources++;
			break;
		}
		attributeToInput.push_back(iInput);
		edgeScene->m_vertexAttributeIds[edgeScene->m_numVertexAttributes] = edgeAttrId;
		edgeScene->m_vertexAttributeIndexes[edgeScene->m_numVertexAttributes] = (uint16_t)edgeScene->m_numFloatsPerVertex;
		edgeScene->m_numVertexAttributes++;
		edgeScene->m_numFloatsPerVertex += pSource->GetStride();
		// Tangents need an extra component in the scene's vertex array to store the flip factor
		if (semantic == FUDaeGeometryInput::TEXTANGENT)
			edgeScene->m_numFloatsPerVertex += 1;
	}
	assert(positionInputIndex >= 0); // We must have positions!

	std::vector<int32_t> polyList; // we'll populate this list with vertex indexes for the triangulator
	std::map<UniqueVert, uint32_t, UniqueVertCompare> uniqueVerts;
	GetVertexesAndPolygons(polys, &uniqueVerts, &polyList);

	// generate a triangulated index list from the poly list
	uint32_t numFaces = (uint32_t)polys->GetFaceCount();
	std::vector<int32_t> materialIdPerPoly( numFaces, materialId );
    if (numFaces)
    {
	    edgeGeomTriangulatePolygons( &polyList[0], &materialIdPerPoly[0],
			&edgeScene->m_triangles, &edgeScene->m_materialIdPerTriangle, &edgeScene->m_numTriangles);
    }

	// allocate storage for the vertexes
	edgeScene->m_numVertexes = (uint32_t)uniqueVerts.size();
	edgeScene->m_vertexes = (float*)edgeGeomAlloc(edgeScene->m_numVertexes * edgeScene->m_numFloatsPerVertex * sizeof(float));
	memset(edgeScene->m_vertexes, 0, edgeScene->m_numVertexes * edgeScene->m_numFloatsPerVertex * sizeof(float));
	// loop over the vertexes and copy the data
	for ( std::map<UniqueVert, uint32_t, UniqueVertCompare>::const_iterator it = uniqueVerts.begin(); it != uniqueVerts.end(); ++it )
	{
		const UniqueVert& vert = it->first;
		const uint32_t index = it->second;
		// skip to this vertex's floats
		float* destVertex = edgeScene->m_vertexes + index*edgeScene->m_numFloatsPerVertex;
		// loop over inputs
		for ( uint32_t iAttr=0; iAttr<edgeScene->m_numVertexAttributes; ++iAttr )
		{
			uint32_t iInput = attributeToInput[iAttr];
			float *destAttr = destVertex + edgeScene->m_vertexAttributeIndexes[iAttr];
			uint32_t srcOffset = inputComponentCounts[iInput] * vert[iInput];
			const float *srcAttr = inputDataSources[iInput] + srcOffset;
			for(uint32_t iComponent=0; iComponent<inputComponentCounts[iInput]; ++iComponent)
			{
				destAttr[iComponent] = srcAttr[iComponent];
			}
		}
	}

	// Special case: tangents need a flip factor in their fourth component. We calculate the flip
	// factor by crossing the normal with the tangent, and dotting the result with the binormal.
	// The normals and binormals are read from the input Collada data, not what's stored in the scene.
	const FCDGeometryPolygonsInput *inputTangents = polys->FindInput(FUDaeGeometryInput::TEXTANGENT);
	if (inputTangents != NULL)
	{
		const FCDGeometryPolygonsInput *inputNormals = polys->FindInput(FUDaeGeometryInput::NORMAL);
		const FCDGeometryPolygonsInput *inputBinormals = polys->FindInput(FUDaeGeometryInput::TEXBINORMAL);
		if (inputNormals == NULL || inputBinormals == NULL)
		{
			// TODO: is this ignorable?
			throw std::runtime_error("Can't compute a flip factor for tangents without normals and binormals");
		}
		const float *sourceNormalData   = inputNormals->GetSource()->GetData();
		const float *sourceTangentData  = inputTangents->GetSource()->GetData();
		const float *sourceBinormalData = inputBinormals->GetSource()->GetData();
		const uint32_t sourceNormalStride   = inputNormals->GetSource()->GetStride();
		const uint32_t sourceTangentStride  = inputTangents->GetSource()->GetStride();
		const uint32_t sourceBinormalStride = inputBinormals->GetSource()->GetStride();
		// Find the attribute index of the tangent within the destination vertexes
		int32_t destTangentIndex = -1;
		for(uint32_t iAttr=0; iAttr<edgeScene->m_numVertexAttributes; ++iAttr)
		{
			if (edgeScene->m_vertexAttributeIds[iAttr] == EDGE_GEOM_ATTRIBUTE_ID_TANGENT)
			{
				destTangentIndex = (int32_t)(edgeScene->m_vertexAttributeIndexes[iAttr]);
				break;
			}
		}
		assert(destTangentIndex >= 0);

		for ( std::map<UniqueVert, uint32_t, UniqueVertCompare>::const_iterator it = uniqueVerts.begin(); it != uniqueVerts.end(); ++it )
		{
			const UniqueVert& vert = it->first;
			const uint32_t index = it->second;
			float* destTangent = edgeScene->m_vertexes + index*edgeScene->m_numFloatsPerVertex + destTangentIndex;

			FMVector3 normal(  sourceNormalData   + vert[normalInputIndex]  *sourceNormalStride);
			FMVector3 tangent( sourceTangentData  + vert[tangentInputIndex] *sourceTangentStride);
			FMVector3 binormal(sourceBinormalData + vert[binormalInputIndex]*sourceBinormalStride);
			float flip = ( normal ^ tangent ) * binormal;
			flip = ( flip >= 0.0f ) ? 1.0f : -1.0f;
			destTangent[3] = flip;	
		}
	}

	// look for deformation data
	bool hasDeformationData = instance.m_geometryInstance->HasType( FCDControllerInstance::GetClassType() );
	if ( hasDeformationData )
	{
		const FCDController* controller = (const FCDController*)instance.m_geometryInstance->GetEntity();
		// controllers can be chained - iterate over them
		bool haveSkin = false, haveBlendShapes = false;
		for (;;)
		{
			// skinning?
			if ( controller->IsSkin() && !haveSkin )
			{
				CopySkinData(controller->GetSkinController(), joints, uniqueVerts, positionInputIndex, edgeScene);
				haveSkin = true;
			}
			// blend shapes?
			if ( controller->IsMorph() && !haveBlendShapes )
			{
				CopyBlendShapeData( controller->GetMorphController(), edgeScene );
				haveBlendShapes = true;
			}
			// iterate
			const FCDEntity* next = controller->GetBaseTarget();
			if ( next->GetType() == FCDEntity::CONTROLLER )
				controller = (const FCDController*)next;
			else
				break;
		}
	}
}

//--------------------------------------------------------------------------------------------------
/**
	Save out inverse bind matrices to binary file
**/
//--------------------------------------------------------------------------------------------------

void SaveInvBindMatrices(std::string filename, std::vector< PolygonsInstance >& instances,	std::vector<const FCDSceneNode*>& joints)
{
	FMMatrix44List inverseBindMats;
	std::vector<bool> visitedJoints;
	for(unsigned int i=0; i<joints.size(); i++)
	{
		inverseBindMats.push_back(FMMatrix44::Identity);
		visitedJoints.push_back(false);
	}

	bool warn = false;

	for(unsigned int instLoop=0; instLoop < instances.size(); instLoop++)
	{
		PolygonsInstance instance = instances[instLoop];

		if(!instance.m_geometryInstance->HasType( FCDControllerInstance::GetClassType() ))
			continue;

		const FCDController* controller = (const FCDController*)instance.m_geometryInstance->GetEntity();

		// controllers can be chained - iterate over them
		bool haveSkin = false;
		for (;;)
		{
			// skinning?
			if(controller->IsSkin() && !haveSkin)
			{
                const FCDSkinController* skinController = controller->GetSkinController();
				FMMatrix44 bindShapeMatrix = skinController->GetBindShapeTransform();

                uint32_t numJoints = (uint32_t)skinController->GetJointCount();
				for(uint32_t jointId=0; jointId<numJoints; jointId++)
				{
                    fm::string name = skinController->GetJoint(jointId)->GetId();                    
					FMMatrix44 mat = skinController->GetJoint(jointId)->GetBindPoseInverse() * bindShapeMatrix;

					// get id of this joint in the skeleton
					for(unsigned int i=0; i<joints.size(); i++)
					{
						if(joints[i]->GetSubId() == name)
						{
							// does this joint influence a previously visited mesh?
							if((!warn) && visitedJoints[i])
							{
								// if this matrix differs to what's there already - flag a warning
								float diff = 0;
								for(int row=0; row<4; row++)
									for(int col=0; col<4; col++)
										diff += fabsf(mat[row][col] - inverseBindMats[i][row][col]);
								warn = (diff > 0.0001f);
							}
							inverseBindMats[i] = mat;
							visitedJoints[i] = true;
							break;
						}
					}
				}

				haveSkin = true;
			}

			// iterate
			const FCDEntity* next = controller->GetBaseTarget();
			if ( next->GetType() == FCDEntity::CONTROLLER )
				controller = (const FCDController*)next;
			else
				break;
		}
	}

	if(warn)
		printf("WARNING: SaveInvBindMatrices - meshes with different shape transforms are influenced by the same joint(s).\n");

	FILE* fp = fopen(filename.c_str(), "wb");
	if ( !fp )
	{
		printf("Error: unable to open file %s for writing\n", filename.c_str());
		throw std::runtime_error(std::string("Error: Cannot open file for writing."));
	}

	for(unsigned int i=0; i<joints.size(); i++)
	{
		for(unsigned int j=0; j<3; j++)
		{
			for(unsigned int k=0; k<4; k++)
			{
				union
				{
					unsigned int u32;
					float        f32;
				} safe;
				safe.f32 = inverseBindMats[i].m[k][j];
				unsigned int v = safe.u32;
				unsigned int r = 
				((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8) | ((v & 0x0000FF00) << 8) | ((v & 0x000000FF) << 24);
				fwrite(&r, 1, 4, fp);
			}
		}
	}
	fclose(fp);
}

//--------------------------------------------------------------------------------------------------
/**
	Entire process for one file.
**/
//--------------------------------------------------------------------------------------------------

void	Edge::Tools::GeomProcessFile( const std::string& inputFile, const std::string& outputFile, const std::string& invBindMatsFile, 
									  const unsigned processingFlags /*= 0*/ )
{
	// load the collada file
	printf("Loading %s...\n", inputFile.c_str());
	fflush(stdout);
	FCDocument *document = FCollada::NewTopDocument();
	if ( !FCollada::LoadDocumentFromFile(document, FUStringConversion::ToFString( inputFile.c_str() ) ) )
	{
		throw std::runtime_error( "Failed to load input file" );
	}

	// find blend shape targets, so we can avoid treating them as real meshes
	std::set<const FCDGeometry*> blendShapeTargets;
	FindBlendShapeTargets( document, blendShapeTargets );

	// find the joints in the scene
	std::vector<const FCDSceneNode*> joints;
	Edge::Tools::FindSkeletonJoints( document->GetVisualSceneInstance(), joints );

	// get all the polygons and their shaders
	std::vector< PolygonsInstance > instances;
	GetPolygons( document, instances, blendShapeTargets );

	// make material indexes
	std::map< const FCDMaterial*, int > materialIndexes;
	MakeMaterialIndexes( instances, materialIndexes );

	// Save out inverse bind matrices if neccessary
	if (processingFlags & EDGE_GEOM_FLAG_OUTPUT_INV_BIND_MATRICES)
		SaveInvBindMatrices(invBindMatsFile, instances, joints);

	// convert each collada polygons object into an EdgeGeomScene object.
	// Each scene object is partitioned into segments, and the segments for all
	// scenes are collected into a flat array and written out to disk.
	std::vector<EdgeGeomSegment> allSegments;
	for(uint32_t iInstance=0; iInstance<instances.size(); ++iInstance)
	{
		// Populate an Edge Scene object from this instance
		printf("Converting instance %d/%d to Edge scene...\n", iInstance+1, instances.size());
		fflush(stdout);
		int32_t materialIndex = materialIndexes.find( instances[iInstance].m_material )->second;
		EdgeGeomScene edgeScene;
		memset(&edgeScene, 0, sizeof(EdgeGeomScene));
		CreateEdgeScene(instances[iInstance], materialIndex, joints, &edgeScene, processingFlags);

		// Choose vertex flavors, culling type, etc.
		EdgeGeomSegmentFormat edgeFormat;
		memset(&edgeFormat, 0, sizeof(EdgeGeomSegmentFormat));
		//edgeFormat.m_cullType = kCullBackFacesAndFrustum; // DEPRECATED as of Edge 1.1.0
		//edgeFormat.m_canBeOccluded = true; // DEPRECATED as of Edge 1.1.0
		edgeFormat.m_indexesType = kIndexesCompressedTriangleListCCW;
		edgeFormat.m_skinType = (processingFlags & EDGE_GEOM_FLAG_ENABLE_SCALED_SKINNING) ?
			kSkinNonUniformScaling : kSkinNoScaling;
		edgeFormat.m_skinMatrixFormat = kMatrix3x4RowMajor;
		ChooseVertexFormatsForScene(edgeScene, edgeFormat);

		// Merge any identical vertices, to save memory and make it possible to determine topology.
		printf("Merging identical vertexes...");
		fflush(stdout);
		uint32_t numDuplicates = edgeGeomMergeIdenticalVertexes(edgeScene);
		printf("%d (out of %d) vertexes removed\n", numDuplicates, numDuplicates + edgeScene.m_numVertexes);

		// Partition the geometry.  Triangle lists which were determined earlier to be unskinned
		// could be partitioned separately for more efficient partitions.
		printf("Partitioning scene into segments...\n");
		fflush(stdout);
		EdgeGeomSegment *segments = NULL;
		uint32_t numSegments = 0;
		edgeGeomPartitionSceneIntoSegments(edgeScene, edgeFormat, &segments, &numSegments, NULL, NULL);

		// At this point, the scene is no longer needed
		edgeGeomFree(edgeScene.m_vertexes);
		edgeGeomFree(edgeScene.m_vertexDeltas);
		edgeGeomFree(edgeScene.m_triangles);
		edgeGeomFree(edgeScene.m_materialIdPerTriangle);
		edgeGeomFree(edgeScene.m_vertexAttributeIds);
		edgeGeomFree(edgeScene.m_vertexAttributeIndexes);
		edgeGeomFree(edgeScene.m_blendedAttributeIds);
		edgeGeomFree(edgeScene.m_blendedAttributeIndexes);
		edgeGeomFree(edgeScene.m_matrixIndexesPerVertex);
		edgeGeomFree(edgeScene.m_skinningWeightsPerVertex);

		// The scene format structure is no longer needed
		if (edgeFormat.m_spuInputVertexFormats[0] != NULL)
			edgeGeomFree(edgeFormat.m_spuInputVertexFormats[0]);
		if (edgeFormat.m_spuInputVertexFormats[1] != NULL)
			edgeGeomFree(edgeFormat.m_spuInputVertexFormats[1]);
		if (edgeFormat.m_spuInputVertexDeltaFormat != NULL)
			edgeGeomFree(edgeFormat.m_spuInputVertexDeltaFormat);
		if (edgeFormat.m_spuOutputVertexFormat != NULL)
			edgeGeomFree(edgeFormat.m_spuOutputVertexFormat);
		if (edgeFormat.m_rsxOnlyVertexFormat != NULL)
			edgeGeomFree(edgeFormat.m_rsxOnlyVertexFormat);
		

		// Append the segments onto the allSegments vector.  We can then delete this scene's segments
		// array (but not each segment's contents; those are still being referenced by allSegments)
		allSegments.reserve(allSegments.size() + numSegments);
		for(uint32_t iSegment=0; iSegment<numSegments; ++iSegment)
		{
			allSegments.push_back(segments[iSegment]);
		}
		edgeGeomFree(segments);
	}

	// We're finally done with the Collada input scene
	document->Release();

	// save output file
	printf("Dumping data to %s...\n", (outputFile+".h").c_str());
	DumpToFile( (outputFile + ".h").c_str(), &(allSegments[0]), (uint32_t)allSegments.size());
	printf("Writing output file %s...\n", outputFile.c_str());
	WriteGeomFile( outputFile.c_str(), &(allSegments[0]), (uint32_t)allSegments.size() );

	// Delete the segments' data buffers
	for(uint32_t iSegment=0; iSegment<allSegments.size(); ++iSegment)
	{
		edgeGeomFreeSegmentData(allSegments[iSegment]);
	}
}
