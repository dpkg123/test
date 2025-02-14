/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <cell/gcm.h>
#include <sys/spu_initialize.h>
#include <sys/spu_thread.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <sys/timer.h>

#define PI								3.141592653
#define SPURS_SPU_NUM					1
#define SPU_THREAD_GROUP_PRIORITY		250
#define EVENT_FLAG_BIT					(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS					(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event

#include <cell/spurs.h>
#include <spu_printf.h>

#include "main.h"
#include "gcm/FWCellGCMWindow.h"
#include "FWDebugFont.h"
#include "FWTime.h"
#include "cellutil.h"
#include "gcmutil.h"
#include "../common/profile_bars.h"

#include "edge/geom/edgegeom_structs.h"

using namespace cell;
using namespace cell::Gcm;

struct SphereVert
{
	float m_pos[3];
	float m_norm[3];
	float m_tan[4];
	float m_tex[2];
};


static inline void SetVec(float *vec, float x, float y) { vec[0] = x; vec[1] = y; }
static inline void SetVec(float *vec, float x, float y, float z) { vec[0] = x; vec[1] = y; vec[2] = z; }
static inline void SetVec(float *vec, float x, float y, float z, float w) { vec[0] = x; vec[1] = y; vec[2] = z; vec[3] = w; }
static inline void SetVec2(float *vec, float *from) { vec[0] = from[0]; vec[1] = from[1]; }
static inline void SetVec3(float *vec, float *from) { vec[0] = from[0]; vec[1] = from[1]; vec[2] = from[2]; }
static inline void AddVec3(float *vec, float *from) { vec[0] += from[0]; vec[1] += from[1]; vec[2] += from[2]; }

// instantiate the class
SampleApp app;

// shader
extern unsigned long _binary_vpshader_vpo_start;
extern unsigned long _binary_vpshader_vpo_end;
extern unsigned long _binary_fpshader_fpo_start;
extern unsigned long _binary_fpshader_fpo_end;

// embedded SPU ELF symbols
extern char _binary_job_job_geom_spu_bin_start[];
extern char _binary_job_job_geom_spu_bin_size[];
extern char _binary_job_job_send_event_spu_bin_start[];	
extern char _binary_job_job_send_event_spu_bin_size[];

#define MAX_SEGMENTS 0x100
#define AVAILABLE_SPU_SPACE 64*1024 //(235*1024 - 96*1024 - 384 - 112*3 - 64*3)
static CellSpursJob256 jobs[MAX_SEGMENTS];
static CellSpursEventFlag spursEventFlag;
static uint32_t numSegments = 0;
static void *decompressedVertexData = NULL;
static void *vertexData = NULL;
static void *indexData = NULL;
static uint32_t outputAddr = 0;
static void *textures[2];
static uint8_t streamDescriptionBuffer[128] __attribute__((aligned(128)));
static EdgeGeomVertexStreamDescription *streamDescription=(EdgeGeomVertexStreamDescription*)streamDescriptionBuffer;

static const unsigned int textureDimension = 512;

static void buildMipmaps(const void *tex, void *result, uint32_t topDim, uint32_t dim)
{
	if (dim <= 1)
		return;

	const uint8_t *in0 = (const uint8_t*)tex;
	const uint8_t *in1 = (const uint8_t*)tex + 4;
	const uint8_t *in2 = (const uint8_t*)tex + 4 * topDim;
	const uint8_t *in3 = (const uint8_t*)tex + 4 * topDim + 4;
	uint8_t *out = (uint8_t *)result;
	for (uint32_t i = 0; i < dim * topDim / 2; i++) 
	{
		for (int k = 0; k < 4; k++)
		{
			*out = (*in0 + *in1 + *in2 + *in3)/4;
			in0++;
			in1++;
			in2++;
			in3++;
			out++;
		}
		in0 += 4;
		in1 += 4;
		in2 += 4;
		in3 += 4;
	}

	buildMipmaps(result, out, topDim, dim >> 1);
}

// Loads a texture and creates mipmaps. 
// Assumes standard texture dimension (defined by the constant "textureDimension")
// totalSize parameter is set to size of texture including mipmaps.
static void* loadTexture(const char* fileName, uint32_t& totalSize)
{
	totalSize = 0;

	// Open file
	FILE* fp = fopen(fileName, "rb");
	if (!fp) {
		printf("cannot open file %s\n", fileName);
		return NULL;
	}

	// Get the file size
	fseek(fp, 0, SEEK_END);
	uint32_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	assert(fileSize == textureDimension*textureDimension*4);

	// Allocate memory for texture and mipmaps
	// (double texture buffer size to leave room for mipmaps)
	void* data = memalign(1024*1024, 2*fileSize);  
	if (!data) {
		printf("can't allocate memory\n");
		return NULL;
	}
	totalSize = 2*fileSize;

	// Load texture
	fread(data, 1, fileSize, fp);
	fclose(fp);

	// Create mipmaps for the loaded texture
	buildMipmaps(data, (uint8_t*)data + fileSize, textureDimension, textureDimension);

	return data;
}

static uint32_t CreateJobs(void *vertexes, uint32_t vertexCount, 
	EdgeGeomVertexStreamDescription *streamDesc, uint32_t numAttributes, 
	uint32_t inputStride, uint32_t outputStride, CellGcmContextData *ctx)
{
	// Find number of vertexes per segment
	vertexCount = (vertexCount + 7) & ~0x7;
	uint32_t maxStride = (inputStride > outputStride) ? inputStride : outputStride;
	uint32_t numVertsPerSegment = AVAILABLE_SPU_SPACE / (maxStride + 16*numAttributes);
	numVertsPerSegment = (numVertsPerSegment > (0xC000 / maxStride)) ? (0xC000 / maxStride) : numVertsPerSegment;
	numVertsPerSegment &= ~0xF;
	uint32_t startingAddr = outputAddr;
	uint32_t vertexesEaA = (uint32_t)vertexes;
	while(vertexCount > 0)
	{
		uint32_t numVerts = (vertexCount > numVertsPerSegment) ? numVertsPerSegment : vertexCount;
		uint32_t vertexesSize = (numVerts*inputStride + 15) & ~0xF;
		uint32_t vertexesSizeA = (vertexesSize >= 0x4000) ? 0x4000 : vertexesSize;
        uint32_t vertexesSizeB = ((vertexesSize-vertexesSizeA) >= 0x4000) ? 0x4000 : (vertexesSize-vertexesSizeA);
		uint32_t vertexesSizeC = (vertexesSize >= 0x8000) ? (vertexesSize-0x8000) : 0;
		uint32_t vertexesEaB = vertexesEaA + vertexesSizeA;
		uint32_t vertexesEaC = vertexesEaB + vertexesSizeB;

		// Jump to Self Sync
		if(ctx->current >= ctx->end)
		{
			if((*ctx->callback)(ctx, 1) != CELL_OK)
				return 0;
		}
		uint64_t inputStreamCount = 1;
		uint32_t holeEa = (uint32_t)ctx->current;
		uint32_t jumpOffset;
		cellGcmAddressToOffset(ctx->current, &jumpOffset);
		cellGcmSetJumpCommandUnsafeInline(ctx, jumpOffset);

		CellSpursJob256 *job = &jobs[numSegments];
		job->workArea.dmaList[0] = (uint64_t)streamDesc | ((uint64_t)128 << 32); // the stream description is smaller than 128 bytes, but the extra alignment helps performance
		job->workArea.dmaList[1] = vertexesEaA | ((uint64_t)vertexesSizeA << 32);
        job->workArea.dmaList[2] = vertexesEaB | ((uint64_t)vertexesSizeB << 32);
		job->workArea.dmaList[3] = vertexesEaC | ((uint64_t)vertexesSizeC << 32);
		job->workArea.userData[4] = outputAddr | EDGE_GEOM_DIRECT_OUTPUT_TO_LOCAL_MEMORY; // lets Edge know that this is direct output
        job->workArea.userData[5] = 0xFF; // input vertex format ID: custom, via compiled-in callback functions
        job->workArea.userData[6] = 0xFF; // output vertex format ID: custom, via the stream description
        job->workArea.userData[7] = numVerts;
        job->workArea.userData[8] = numAttributes - 1;
		job->workArea.userData[9] = (uint64_t)holeEa | (inputStreamCount << 32); // store input stream count in high word of holeEa
		job->header.eaBinary = (uintptr_t)_binary_job_job_geom_spu_bin_start;
		job->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_geom_spu_bin_size);
		job->header.sizeDmaList = 4 * 8;
		job->header.eaHighInput = 0;
		job->header.useInOutBuffer = 1;
		job->header.sizeInOrInOut = (numVerts * maxStride + 16/*JTS*/ + 16/*VRAM READBACK*/ + 127) & ~0x7F;
		job->header.sizeStack = 0;
		job->header.sizeScratch = (numVerts * numAttributes + 7) & ~0x7;
		job->header.sizeCacheDmaList = 0;
		vertexCount -= numVerts;
		vertexesEaA += numVerts * inputStride;
		outputAddr += numVerts * outputStride;
		++numSegments;

        // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
        // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
        // will cause this flag to be ignored.
        job->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;

		int ret = cellSpursCheckJob(job, sizeof(CellSpursJob256),
			sizeof(CellSpursJob256));
		if ( ret != CELL_OK){
			printf("cellSpursCheckJob failed : 0x%x\n", ret);
		}

	}
	outputAddr = (outputAddr + 0x7F) & ~0x7f;
	
	return startingAddr;
}

static void BuildSphere(float radius, long xdiv, long ydiv, SphereVert *outV, unsigned short *outI,
	EdgeGeomVertexStreamDescription *streamDesc)
{
	long vertexCount = (xdiv + 1) * (ydiv + 1);
	long triangleCount = xdiv * (ydiv - 1) * 2;
	
	float gx = 6.283185307179586f / (float) xdiv;
	float gy = 3.141592653589793f / (float) ydiv;
	
	for (long i = 0; i < xdiv; ++i)
	{
		float theta = (float)i * gx;
		float ct = cosf(theta);
		float st = sinf(theta);
		
		long k = i * (ydiv + 1);
		for (long j = 1; j < ydiv; ++j)
		{
			float phi = (float) j * gy;
			float sp = sinf(phi);
			float x = ct * sp;
			float y = st * sp;
			float z = cosf(phi);
			SetVec(outV[k+j].m_pos, x*radius,y*radius,z*radius);
			SetVec(outV[k+j].m_norm, x,y,z);
			SetVec(outV[k+j].m_tex, 
				theta * 0.1591549430918953f,
				1.0F - phi * 0.31830988618379f);
		}
	}
	
	long kk = xdiv * (ydiv + 1);
	for (long j = 1; j < ydiv; ++j)
	{
		float phi = (float)j * gy;
		float x = sinf(phi);
		float z = cosf(phi);
		SetVec(outV[kk+j].m_pos, x*radius,0,z*radius);
		SetVec(outV[kk+j].m_norm, x,0,z);
		SetVec(outV[kk+j].m_tex, 
			1, 
			1.f - phi * 0.31830988618379f);
	}
	
	for (long i = 0; i < xdiv; i++)
	{
		long k1 = i * (ydiv + 1) + 1;
		long k2 = (i + 1) * (ydiv + 1) + 1;
		float s = (outV[k1].m_tex[0] + outV[k2].m_tex[0]) * 0.5f;
		SetVec(outV[k1-1].m_pos, 0,0,radius);
		SetVec(outV[k1-1].m_norm, 0,0,1);
		SetVec(outV[k1-1].m_tex, s,1);
		SetVec(outV[k1+ydiv-1].m_pos, 0,0,-radius);
		SetVec(outV[k1+ydiv-1].m_norm, 0,0,-1);
		SetVec(outV[k1+ydiv-1].m_tex, s,0);
	}
	
	SetVec3(outV[xdiv*(ydiv+1)].m_pos, outV[0].m_pos);
	SetVec3(outV[xdiv*(ydiv+1)].m_norm, outV[0].m_norm);
	SetVec2(outV[xdiv*(ydiv+1)].m_tex, outV[0].m_tex);
	SetVec3(outV[xdiv*(ydiv+1)+ydiv].m_pos, outV[ydiv].m_pos);
	SetVec3(outV[xdiv*(ydiv+1)+ydiv].m_norm, outV[ydiv].m_norm);
	SetVec2(outV[xdiv*(ydiv+1)+ydiv].m_tex, outV[ydiv].m_tex);
	
	long ii = 0;
	for(long i = 0; i < xdiv; ++i)
	{
		long k = i * (ydiv + 1);
		outI[ii+0] = (unsigned short) k;
		outI[ii+1] = (unsigned short) (k + 1);
		outI[ii+2] = (unsigned short) (k + ydiv + 2);
		ii += 3;
		
		for(long j = 1; j < ydiv - 1; ++j)
		{
			outI[ii+0] = (unsigned short) (k + j);
			outI[ii+1] = (unsigned short) (k + j + 1);
			outI[ii+2] = (unsigned short) (k + j + ydiv + 2);
			outI[ii+3] = (unsigned short) (k + j);
			outI[ii+4] = (unsigned short) (k + j + ydiv + 2);
			outI[ii+5] = (unsigned short) (k + j + ydiv + 1);
			ii += 6;
		}
		
		outI[ii+0] = (unsigned short) (k + ydiv - 1);
		outI[ii+1] = (unsigned short) (k + ydiv);
		outI[ii+2] = (unsigned short) (k + ydiv * 2);
		ii += 3;
	}
	
	// Double texcoords
	for(long i = 0; i < vertexCount; ++i)
	{
		outV[i].m_tex[0] *= 4.f;
		outV[i].m_tex[1] *= 2.f;
	}
	
	// Calculate tangents
	float *tan1 = (float*)calloc(vertexCount*3, 4);
	float *tan2 = (float*)calloc(vertexCount*3, 4);
	for(long i = 0; i < triangleCount; ++i)
	{
		long i1 = outI[i*3+0];
		long i2 = outI[i*3+1];
		long i3 = outI[i*3+2];
		const float *v1 = outV[i1].m_pos;
		const float *v2 = outV[i2].m_pos;
		const float *v3 = outV[i3].m_pos;
		const float *w1 = outV[i1].m_tex;
		const float *w2 = outV[i2].m_tex;
		const float *w3 = outV[i3].m_tex;
		float x1 = v2[0] - v1[0];
		float x2 = v3[0] - v1[0];
		float y1 = v2[1] - v1[1];
		float y2 = v3[1] - v1[1];
		float z1 = v2[2] - v1[2];
		float z2 = v3[2] - v1[2];
		float s1 = w2[0] - w1[0];
		float s2 = w3[0] - w1[0];
		float t1 = w2[1] - w1[1];
		float t2 = w3[1] - w1[1];
		float r = 1.f / (s1*t2-s2*t1);
		float sdir[3] = { (t2*x1-t1*x2)*r, (t2*y1-t1*y2)*r, (t2*z1-t1*z2)*r };
		float tdir[3] = { (s1*x2-s2*x1)*r, (s1*y2-s2*y1)*r, (s1*z2-s2*z1)*r };
		AddVec3(&tan1[i1*3], sdir);
		AddVec3(&tan1[i2*3], sdir);
		AddVec3(&tan1[i3*3], sdir);
		AddVec3(&tan2[i1*3], tdir);
		AddVec3(&tan2[i2*3], tdir);
		AddVec3(&tan2[i3*3], tdir);
	}
	
	long count = vertexCount;
	for(long i = 0; i < count; ++i)
	{
		const float *n = outV[i].m_norm;
		const float *t = &tan1[i*3];
		float nDotT = n[0]*t[0] + n[1]*t[1] + n[2]*t[2];
		float tan_a[3] = { t[0]-n[0]*nDotT, t[1]-n[1]*nDotT, t[2]-n[2]*nDotT };
		float ooLen = 1.f/sqrtf(tan_a[0]*tan_a[0] + tan_a[1]*tan_a[1] + tan_a[2]*tan_a[2]);
		SetVec(outV[i].m_tan, tan_a[0]*ooLen, tan_a[1]*ooLen, tan_a[2]*ooLen, 1);
	}
	
	free(tan1);
	free(tan2);

	// Build the output stream description
	streamDesc->numAttributes = 4;
	streamDesc->stride = 28;
	streamDesc->numBlocks = 4;
	// position
	streamDesc->blocks[0].attributeBlock.offset = 0;
	streamDesc->blocks[0].attributeBlock.format = EDGE_GEOM_ATTRIBUTE_FORMAT_F32;
	streamDesc->blocks[0].attributeBlock.componentCount = 3;
	streamDesc->blocks[0].attributeBlock.edgeAttributeId = EDGE_GEOM_ATTRIBUTE_ID_POSITION;
	streamDesc->blocks[0].attributeBlock.size = 12;
	streamDesc->blocks[0].attributeBlock.vertexProgramSlotIndex = 0;
	streamDesc->blocks[0].attributeBlock.fixedBlockOffset = 0;
	// normal
	streamDesc->blocks[1].attributeBlock.offset = 12;
	streamDesc->blocks[1].attributeBlock.format = EDGE_GEOM_ATTRIBUTE_FORMAT_X11Y11Z10N;
	streamDesc->blocks[1].attributeBlock.componentCount = 1;
	streamDesc->blocks[1].attributeBlock.edgeAttributeId = EDGE_GEOM_ATTRIBUTE_ID_NORMAL;
	streamDesc->blocks[1].attributeBlock.size = 4;
	streamDesc->blocks[1].attributeBlock.vertexProgramSlotIndex = 2;
	streamDesc->blocks[1].attributeBlock.fixedBlockOffset = 0;
	// tangent
	streamDesc->blocks[2].attributeBlock.offset = 16;
	streamDesc->blocks[2].attributeBlock.format = EDGE_GEOM_ATTRIBUTE_FORMAT_I16N;
	streamDesc->blocks[2].attributeBlock.componentCount = 4;
	streamDesc->blocks[2].attributeBlock.edgeAttributeId = EDGE_GEOM_ATTRIBUTE_ID_TANGENT;
	streamDesc->blocks[2].attributeBlock.size = 8;
	streamDesc->blocks[2].attributeBlock.vertexProgramSlotIndex = 14;
	streamDesc->blocks[2].attributeBlock.fixedBlockOffset = 0;
	// UV
	streamDesc->blocks[3].attributeBlock.offset = 24;
	streamDesc->blocks[3].attributeBlock.format = EDGE_GEOM_ATTRIBUTE_FORMAT_F16;
	streamDesc->blocks[3].attributeBlock.componentCount = 2;
	streamDesc->blocks[3].attributeBlock.edgeAttributeId = EDGE_GEOM_ATTRIBUTE_ID_UV0;
	streamDesc->blocks[3].attributeBlock.size = 4;
	streamDesc->blocks[3].attributeBlock.vertexProgramSlotIndex = 8;
	streamDesc->blocks[3].attributeBlock.fixedBlockOffset = 0;
}

SampleApp::SampleApp() 
:	mLastUpdate(FWTime::sZero)
,	mTurnRate(500.f)
,	mMoveRate(50.f)
{
	mFar = 10000.f;
}

SampleApp::~SampleApp()
{
}

void SampleApp::initShader()
{
	mCGVertexProgram   = (CGprogram)(void*)&_binary_vpshader_vpo_start;
	mCGFragmentProgram = (CGprogram)(void*)&_binary_fpshader_fpo_start;

	// init
	cellGcmCgInitProgram(mCGVertexProgram);
	cellGcmCgInitProgram(mCGFragmentProgram);

	// allocate video memory for fragment program
	unsigned int ucodeSize;
	void *ucode;
	cellGcmCgGetUCode(mCGFragmentProgram, &ucode, &ucodeSize);

	mFragmentProgramUCode = (void*)cellGcmUtilAllocateLocalMemory(ucodeSize, 1024);
	cellGcmAddressToOffset(mFragmentProgramUCode, &mFragmentProgramOffset);

	memcpy(mFragmentProgramUCode, ucode, ucodeSize); 

	// get and copy 
	cellGcmCgGetUCode(mCGVertexProgram, &ucode, &ucodeSize);
	mVertexProgramUCode = ucode;
}

bool SampleApp::onInit(int argc, char **ppArgv)
{
	FWGCMCamApplication::onInit(argc, ppArgv);	// note: calls sys_spu_initialize() internally

	FWInputDevice *pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad != NULL) 
	{
		for(unsigned i = 0; i < IN_COUNT; ++i)
		{
			mInputs[i] = pPad->bindFilter();
			mInputs[i]->setChannel(sInputChannels[i]);
		}
		mInputs[IN_X0]->setGain(0.5f);
		mInputs[IN_X0]->setDeadzone(0.08f);
		mInputs[IN_Y0]->setGain(-0.5f);
		mInputs[IN_Y0]->setDeadzone(0.08f);
		mInputs[IN_X1]->setGain(0.002f);
		mInputs[IN_X1]->setDeadzone(0.0008f);
		mInputs[IN_Y1]->setGain(0.002f);
		mInputs[IN_Y1]->setDeadzone(0.0008f);
	}

	// Get PPU thread priority
	int ppu_thr_prio;
	sys_ppu_thread_t my_ppu_thread_id;
	int ret = sys_ppu_thread_get_id(&my_ppu_thread_id);
	if (ret != CELL_OK){
		printf("sys_ppu_thread_get_id failed : 0x%x\n", ret);
	}
	ret = sys_ppu_thread_get_priority(my_ppu_thread_id, &ppu_thr_prio);	
	if (ret != CELL_OK){
		printf("sys_ppu_thread_get_priority failed : 0x%x\n", ret);
		return false;
	}

	// Initialize spu_printf handling
	ret = spu_printf_initialize(ppu_thr_prio - 1, NULL);
    if (ret != CELL_OK){
        printf("spu_printf_initialize failed : 0x%x\n", ret);
		return false;
    }

	// Initialize a SPURS attribute structure.  This contains all the data that
	// was originally passed directly to cellSpursInitialize(), plus some additional
	// fields.
	CellSpursAttribute spursAttributes;
	ret = cellSpursAttributeInitialize(&spursAttributes, SPURS_SPU_NUM,
	SPU_THREAD_GROUP_PRIORITY, ppu_thr_prio - 1, false);
	if (ret != CELL_OK){
		printf("cellSpursAttributeInitialize failed : 0x%x\n", ret);
		return false;
	}
	// Enable support for spu_printf in the SPURS attributes.
	ret = cellSpursAttributeEnableSpuPrintfIfAvailable(&spursAttributes);
	if (ret != CELL_OK){
		printf("cellSpursAttributeEnableSpuPrintfItAvailable failed : 0x%x\n", ret);
		return false;
	}
	// Initialize SPURS using our attributes structure.
	ret = cellSpursInitializeWithAttribute(&mSpurs, &spursAttributes);
	if (ret != CELL_OK){
		printf("cellSpursInitializeWithAttribute failed : 0x%x\n", ret);
		return false;
	}

	// Initialize Spurs event flag (used for job chain completion)
	ret = cellSpursEventFlagInitializeIWL(&mSpurs, &spursEventFlag,
		CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
		CELL_SPURS_EVENT_FLAG_SPU2PPU);
	if (ret != CELL_OK){
		printf("cellSpursEventFlagInitializeIWL failed : 0x%x\n", ret);
		return false;
	}
	ret = cellSpursEventFlagAttachLv2EventQueue(&spursEventFlag);
	if (ret != CELL_OK){
		printf("cellSpursEventFlagAttachLv2EventQueue failed : 0x%x\n", ret);
		return false;
	}

	vertexData = memalign(1024*1024, 1024*1024);
	indexData = memalign(1024*1024, 1024*1024);
	
	BuildSphere(2, 64, 32, (SphereVert*)vertexData, (unsigned short *)indexData, streamDescription);
	// Textures
	uint32_t colorTexSize = 0;
	uint32_t normalTexSize = 0;
	// loadTexture allocates enough space after the texture for mipmaps and creates the mipmaps
	// The second parameter is set to the total texture size including mipmaps
	textures[0] = loadTexture(SYS_APP_HOME"/assets/logo-color.bin", colorTexSize); 
	textures[1] = loadTexture(SYS_APP_HOME"/assets/logo-normal.bin", normalTexSize);

	uint32_t dummy;
	cellGcmMapMainMemory(indexData, 1024*1024, &dummy);
	cellGcmMapMainMemory(textures[0], colorTexSize, &dummy);
	cellGcmMapMainMemory(textures[1], normalTexSize, &dummy);

	decompressedVertexData = (void*)cellGcmUtilAllocateLocalMemory(1024*1024, 1024 * 1024);

	initShader();

	// init profile bars
	ProfileBars::Init(&mSpurs, SPURS_SPU_NUM);

	return true;
}

bool SampleApp::onUpdate()
{
	FWTimeVal time = FWTime::getCurrentTime();
	float deltaTime = (mLastUpdate == FWTime::sZero) ? 0.f : (float)(time - mLastUpdate);
	mLastUpdate = time;

	mCamera.setPan(mCamera.getPan() + (mInputs[IN_X1] ? mInputs[IN_X1]->getFloatValue() : 0.f) * deltaTime * mTurnRate);
	mCamera.setTilt(mCamera.getTilt() + (mInputs[IN_Y1] ? mInputs[IN_Y1]->getFloatValue() : 0.f) * deltaTime * mTurnRate);

	Matrix3 xRot = Matrix3::rotationX(-mCamera.getTilt());
	Matrix3 yRot = Matrix3::rotationY(-mCamera.getPan());
	Vector3 xVec = (yRot * xRot) * Vector3::xAxis();
	Vector3 yVec = (yRot * xRot) * Vector3::yAxis();
	Vector3 zVec = (yRot * xRot) * -Vector3::zAxis();

	mCamera.setPosition(mCamera.getPosition() + (
		xVec * (mInputs[IN_X0] ? mInputs[IN_X0]->getFloatValue() : 0.f) 
		+ yVec * ((mInputs[IN_UP] && mInputs[IN_UP]->getBoolValue()) ? 1.0f : 0.f)
		+ yVec * ((mInputs[IN_DOWN] && mInputs[IN_DOWN]->getBoolValue()) ? -1.0f : 0.f)
		+ zVec * (mInputs[IN_Y0] ? mInputs[IN_Y0]->getFloatValue() : 0.f)
		) * deltaTime * mMoveRate);

	return FWGCMCamApplication::onUpdate();
}

void SampleApp::onRender()
{
	// time stamp 0 is the base rsx time used by the profile bars for this frame
	// wait for vsync and write the timestamp
	ProfileBars::StartFrame();
	cellGcmSetTimeStamp(0);
	cellGcmFlush();

	// base implementation clears screen and sets up camera
	FWGCMCamApplication::onRender();

	static CellSpursJob256 jobEnd;
	__builtin_memset(&jobEnd,0,sizeof(CellSpursJob256));
	jobEnd.header.eaBinary = (uintptr_t)_binary_job_job_send_event_spu_bin_start;
	jobEnd.header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_send_event_spu_bin_size);
	jobEnd.workArea.userData[0] = (uintptr_t)&spursEventFlag;
	jobEnd.workArea.userData[1] = (uint64_t)EVENT_FLAG_BITS;

	// Reset Parameters
	uint32_t vertexCount = 2145;
	uint32_t indexCount = 11904;
	numSegments = 0;
	outputAddr = (uint32_t)decompressedVertexData;	
	__builtin_memset(decompressedVertexData, 0xCC, 1024*1024);

	cellGcmSetInvalidateVertexCache();
	
	CellGcmTexture tex;
	uint32_t format;
	cellGcmUtilGetTextureAttribute(CELL_GCM_UTIL_RGBA8, &format, &tex.remap, 0, 1);

	tex.format = format;
	tex.mipmap = 10;
	tex.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	tex.cubemap = CELL_GCM_FALSE;
	tex.width = 512;
	tex.height = 512;
	tex.depth = 1;
	tex.pitch = 512*4;
	tex.location = CELL_GCM_LOCATION_MAIN;

	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0<<8, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:12
	cellGcmSetTextureAddress(0,	CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTextureControl(1, CELL_GCM_TRUE, 0<<8, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:12
	cellGcmSetTextureAddress(1,	CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureFilter(1, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetVertexProgram(mCGVertexProgram, mVertexProgramUCode);
	cellGcmSetFragmentProgram(mCGFragmentProgram, mFragmentProgramOffset);
	
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);
	cellGcmSetShadeMode(CELL_GCM_SMOOTH);

	Matrix4 mat = Matrix4::rotationZYX(Vector3(0, 0, 0));
	mat.setTranslation(Vector3(0.0f, 0.0f, -5.0f));
	Matrix4 mvp = transpose(getProjectionMatrix() * getViewMatrix() * mat);
	float lp[4] = {10,10,10,1};
	Point3 ep = mCamera.getPosition();
	cellGcmSetVertexProgramConstants(0, 16, (float*)&mvp);
	cellGcmSetVertexProgramConstants(4, 4, lp);
	cellGcmSetVertexProgramConstants(5, 4, (float*)&ep);

	// Create a job list to decompress the vertexes	
	uint32_t outputEa = CreateJobs(vertexData, vertexCount, 
		streamDescription, 4, sizeof(SphereVert), 28, gCellGcmCurrentContext);

	static CellSpursJobList jobList;
	jobList.eaJobList = (uint64_t)jobs;
	jobList.numJobs = numSegments;
	jobList.sizeOfJob = 256;

	static uint64_t command_list[5];
	command_list[0] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
	command_list[1] = CELL_SPURS_JOB_COMMAND_SYNC;
	command_list[2] = CELL_SPURS_JOB_COMMAND_FLUSH;
	command_list[3] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
	command_list[4] = CELL_SPURS_JOB_COMMAND_END;

	static CellSpursJobChain jobChain __attribute__((aligned(128)));
	static CellSpursJobChainAttribute jobChainAttributes;        
	static uint8_t prios[8] = {1, 1, 1, 1, 1, 1, 1, 1};

	int ret = cellSpursJobChainAttributeInitialize(&jobChainAttributes, command_list, 
		sizeof(CellSpursJob256), (SPURS_SPU_NUM == 1) ? 1 : 16, prios, 
		SPURS_SPU_NUM, true, 0, 1, false, sizeof(CellSpursJob256), 6);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
	}
	//When the job manager detects an error, halt on SPU to help with debugging
	ret = cellSpursJobChainAttributeSetHaltOnError(&jobChainAttributes);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeSetHaltOnError failed : 0x%x\n", ret);
	}
	ret = cellSpursCreateJobChainWithAttribute(&mSpurs, &jobChain, &jobChainAttributes);
	if (ret != CELL_OK){
		printf("cellSpursCreateJobChainWithAttribute failed : 0x%x\n", ret);
	}
	ret = cellSpursRunJobChain(&jobChain);
	if (ret != CELL_OK){
		printf("cellSpursRunJobChain failed : 0x%x\n", ret);
	}

	uint32_t vertexesOffset, indexesOffset, texOff[2];
	cellGcmAddressToOffset((void*)outputEa, &vertexesOffset);
	cellGcmAddressToOffset(indexData, &indexesOffset);
	cellGcmAddressToOffset(textures[0], &texOff[0]);
	cellGcmAddressToOffset(textures[1], &texOff[1]);

	ProfileBars::AddRsxMarker(0x20202000);		// grey rsx bar until that point (clear etc)

	tex.offset = texOff[0];
	cellGcmSetTexture(0, &tex);
	tex.offset = texOff[1];
	cellGcmSetTexture(1, &tex);
	cellGcmSetVertexDataArray(0, 0, 28, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, vertexesOffset + 0);
	cellGcmSetVertexDataArray(2, 0, 28, 1, CELL_GCM_VERTEX_CMP, CELL_GCM_LOCATION_LOCAL, vertexesOffset + 12);
	cellGcmSetVertexDataArray(6, 0, 28, 4, CELL_GCM_VERTEX_S1, CELL_GCM_LOCATION_LOCAL, vertexesOffset + 16);
	cellGcmSetVertexDataArray(8, 0, 28, 2, CELL_GCM_VERTEX_SF, CELL_GCM_LOCATION_LOCAL, vertexesOffset + 24);
	cellGcmSetDrawIndexArray(CELL_GCM_PRIMITIVE_TRIANGLES, indexCount, CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, CELL_GCM_LOCATION_MAIN, indexesOffset);

	ProfileBars::AddRsxMarker(0xff000000);	// red rsx bar

	// kick rsx
    cellGcmFlush();

    uint16_t ev_mask = EVENT_FLAG_BITS;
	ret = cellSpursEventFlagWait(&spursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
	if (ret != CELL_OK){
		printf("cellSpursEventFlagWait failed : 0x%x\n", ret);
	}
	ret = cellSpursShutdownJobChain(&jobChain);
	if (ret != CELL_OK){
		printf("cellSpursShutdownJobChain failed : 0x%x\n", ret);
	}
	ret = cellSpursJoinJobChain(&jobChain);
	if (ret != CELL_OK){
		printf("cellSpursJoinJobChain failed : 0x%x\n", ret);
	}

	// cellGcmFinish is only there because of profile bars displayed for the current frame.
	ProfileBars::AddPpuMarker(0x0000ff00);
	static uint32_t finishReference = 0;
	cellGcmFinish(finishReference++);		
	ProfileBars::Render();
	
	{
		// calc fps
		FWTimeVal	time = FWTime::getCurrentTime();
		float fFPS = 1.f / (float)(time - mLastTime);
		mLastTime = time;

		// print some messages
		FWDebugFont::setPosition(0, 0);
		FWDebugFont::setColor(1.f, 1.f, 1.f, 1.0f);

		FWDebugFont::print("Edge Vertexes Only Sample Application\n\n");
		FWDebugFont::printf("FPS: %.2f\n", fFPS);
	}
}

void SampleApp::onSize(const FWDisplayInfo& rDispInfo)
{
	FWGCMCamApplication::onSize(rDispInfo);
}

void SampleApp::onShutdown()
{
	int ret;
	ret = cellSpursEventFlagDetachLv2EventQueue(&spursEventFlag);
	if (ret != CELL_OK) {
		printf("cellSpursEventFlagDetachLv2EventQueue failed : 0x%x\n", ret);
	}
	ret = cellSpursFinalize(&mSpurs);
	if (ret != CELL_OK){
		printf("cellSpursFinalize failed : 0x%x\n", ret);
	}
	ret = spu_printf_finalize();
	if (ret != CELL_OK){
		printf("spu_printf_finalize failed : 0x%x\n", ret);
	}

	free( vertexData );
	free( indexData );
	free( textures[0] );
	free( textures[1] );

	FWGCMCamApplication::onShutdown();

	// unbind input filters
	FWInputDevice	*pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad != NULL) 
		for(unsigned i = 0; i < IN_COUNT; ++i)
			pPad->unbindFilter(mInputs[i]);
}
