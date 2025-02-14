/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <sys/sys_time.h>
#include <sys/time_util.h>
#include <cell/gcm.h>
#include <cell/spurs.h>
#include "cellutil.h"
#include "gcmutil.h"
#include "profile_bars.h"

using namespace cell::Gcm;

void*					ProfileBars::mSpursTraceBuffer;
uint32_t				ProfileBars::mMaxTracePacketsPerSpu;
void*					ProfileBars::mVertexBuffer;
void*					ProfileBars::mVertexBufferCur;
uint32_t				ProfileBars::mVertexBufferSize;
uint32_t				ProfileBars::mNumSpus;
CGprogram               ProfileBars::mCGVertexProgram;
CGprogram               ProfileBars::mCGFragmentProgram;
void*                   ProfileBars::mVertexProgramUCode;
void*                   ProfileBars::mFragmentProgramUCode;
uint32_t                ProfileBars::mFragmentProgramOffset;
ProfileBars::PpuMarker	ProfileBars::mPpuMarkers[];
ProfileBars::RsxMarker	ProfileBars::mRsxMarkers[];
uint32_t				ProfileBars::mNumPpuMarkers;
uint32_t				ProfileBars::mNumRsxMarkers;
uint64_t				ProfileBars::mFrameTimeStart;
float					ProfileBars::mFpsTarget = 60.f;
uint32_t				ProfileBars::mLastValidPackedIndex[];
uint32_t*				ProfileBars::mCmdAddr;


uint8_t vertexProgram[] = {
#include "vpprofileshader.h"
};

uint8_t fragmentProgram[] = {
#include "fpprofileshader.h"
};

#define CONVERT_GUID_TO_COLOR(guid)	(uint32_t)(((guid)&0xffffffULL) ^ (((guid)&0xffffff000000ULL)>>24) ^ (((guid)&0xffff000000000000ULL)>>48)) 

void ProfileBars::Init(CellSpurs* spurs, uint32_t numSpus, uint32_t maxTracePacketsPerSpu)
{
	mMaxTracePacketsPerSpu = maxTracePacketsPerSpu;
	mNumSpus = numSpus;

	// allocate vertex buffers
	// max quads = 8 background quads + max ppu markers + max rsx markers + max spu markers
	unsigned maxQuads = 8 + kMaxPpuMarkers + kMaxPpuMarkers + numSpus * maxTracePacketsPerSpu;
	mVertexBufferSize = maxQuads * 16 * 4;

	uint32_t meg = 1024*1024;
	uint32_t allocSize = ( ( kCmdbufSize + 2*mVertexBufferSize) + meg - 1) & ~(meg - 1);
	mCmdAddr = (uint32_t*)memalign(meg, allocSize);
	mVertexBuffer = mCmdAddr + ( kCmdbufSize / 4);
	uint32_t dummy;
	cellGcmMapMainMemory(mCmdAddr, allocSize, &dummy);
	mVertexBufferCur = mVertexBuffer;

	// init SPURS trace
	uint32_t spursTraceBufferSize = sizeof(CellSpursTraceInfo) + maxTracePacketsPerSpu * mNumSpus * sizeof(CellSpursTracePacket);
	mSpursTraceBuffer = memalign(128, spursTraceBufferSize);
	memset(mSpursTraceBuffer, 0, spursTraceBufferSize);
	cellSpursTraceInitialize(spurs, mSpursTraceBuffer, spursTraceBufferSize, CELL_SPURS_TRACE_MODE_FLAG_WRAP_BUFFER);
	cellSpursTraceStart(spurs);

	// init shaders
	mCGVertexProgram   = (CGprogram)(void*)vertexProgram;
	mCGFragmentProgram = (CGprogram)(void*)fragmentProgram;

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

	mNumPpuMarkers = 0;
	mNumRsxMarkers = 0;

	for ( unsigned int i = 0; i < mNumSpus; ++i )
		mLastValidPackedIndex[i] = mMaxTracePacketsPerSpu - 1;
}

void ProfileBars::Shutdown()
{
	free(mSpursTraceBuffer);
	free(mCmdAddr);
}

// Note: RSX timestamp 0 must be written at the time of this call (e.g. at the start of the frame)
//       It is the responsibility of the caller to do this
void ProfileBars::StartFrame()
{
	// prepare command context to render previous frame profiler samples
	CellGcmContextData ctx;
	cellGcmSetupContextData( &ctx, mCmdAddr, kCmdbufSize, 0 );
	PrepareProfilerCommandBuffer( &ctx );
	cellGcmSetReturnCommand( &ctx );

	// reset for next frame
	mNumPpuMarkers = 0;
	mNumRsxMarkers = 0;
	SYS_TIMEBASE_GET(mFrameTimeStart);
}

void ProfileBars::AddPpuMarker(uint32_t color)
{
	assert(mNumPpuMarkers < kMaxPpuMarkers);

	uint64_t time;
	SYS_TIMEBASE_GET(time);

	time -= mFrameTimeStart;

	mPpuMarkers[mNumPpuMarkers].time = (uint32_t)time;
	mPpuMarkers[mNumPpuMarkers].color = color;

	mNumPpuMarkers++;
}

void ProfileBars::AddRsxMarker(uint32_t color)
{
	assert(mNumRsxMarkers < kMaxRsxMarkers);

	cellGcmSetTimeStamp(mNumRsxMarkers+1);

	mRsxMarkers[mNumRsxMarkers].color = color;

	mNumRsxMarkers++;
}

void ProfileBars::AddRsxMarker(CellGcmContextData* context, uint32_t color)
{
	assert(mNumRsxMarkers < kMaxRsxMarkers);

	cellGcmSetTimeStamp(context, mNumRsxMarkers+1);

	mRsxMarkers[mNumRsxMarkers].color = color;

	mNumRsxMarkers++;
}

void ProfileBars::SetTargetFps( float targetFps )
{
	mFpsTarget = targetFps;
}

static inline CellSpursTracePacket AtomicRead( const CellSpursTracePacket* pPacket )
{
	//In case an SPU is updating the SPURS Trace buffer at the same time as someone is
	//reading it, we need to make sure that the read of the CellSpursTracePacket is atomic.
	//A load with VMX is a 16-byte atomic read, so does exactly what we need.

	union
	{
		CellSpursTracePacket	packet;
		vector unsigned char	packetVec;
	} myUnion;

	myUnion.packetVec = vec_lvlx( 0, (const unsigned char*)pPacket );
	return myUnion.packet;
}

inline float clamp( float a, float min, float max )
{
	a = a > min ? a : min;
	return a < max ? a : max;
}

void ProfileBars::Render()
{
	// call profiler command buffer
	uint32_t cmdoffset;
	cellGcmAddressToOffset( mCmdAddr, &cmdoffset );
	cellGcmSetCallCommand( cmdoffset );
}

static inline bool IsInRange( uint32_t testValue, uint32_t startValue, uint32_t endValue )
{
	if ( ( testValue - startValue ) >= 0x80000000 )
	{
		return false;
	}

	if ( ( endValue - testValue ) >= 0x80000000 )
	{
		return false;
	}

	return true;
}

void ProfileBars::PrepareProfilerCommandBuffer( CellGcmContextData *ctx )
{
	// top left of screen = (0,0)
	// bottom right of screen = (1,1)
	float x0 = 0.045f;
	float y0 = 0.88f;
	float barWidth = 1 - 2*x0;
	float barHeight = 0.00555f;

	float x1 = x0 + barWidth;
	float ticksToSecs, incXPerTick;

	// swap vertex buffer
	mVertexBufferCur = mVertexBufferCur == mVertexBuffer ? (void*)((uint32_t)mVertexBuffer + mVertexBufferSize) : mVertexBuffer;

	Vert* v = (Vert*)mVertexBufferCur;

	// black background 
	for(uint32_t i=0; i<8; i++)
	{
		float y = y0 + barHeight*2*i;
		v = AddQuad(v, x0, x1, y, y+barHeight, 0x00000000);
	}

	// ppu bar
	float y = y0;
	ticksToSecs = 1.0f / sys_time_get_timebase_frequency();
	incXPerTick = barWidth * ticksToSecs * mFpsTarget;
	float lastX = x0;
	for(uint32_t i=0; i<mNumPpuMarkers; i++)
	{
		uint32_t time = mPpuMarkers[i].time;
		uint32_t color = mPpuMarkers[i].color;
		float x = x0 + time * incXPerTick;
		v = AddQuad(v, lastX, x, y, y+barHeight, color);
		lastX = x;
	}
	y += barHeight*2;

	// rsx bar
	ticksToSecs = 1.0f / 1000000000.0f;
	incXPerTick = barWidth * ticksToSecs * mFpsTarget;
	lastX = x0;
	uint64_t t0 = cellGcmGetTimeStamp(0);
	for(uint32_t i=0; i<mNumRsxMarkers; i++)
	{
		uint32_t color = mRsxMarkers[i].color;
		uint32_t time = (uint32_t)(cellGcmGetTimeStamp(i+1) - t0);
		float x = x0 + time * incXPerTick;
		v = AddQuad(v, lastX, x, y, y+barHeight, color);
		lastX = x;
	}
	y += barHeight*2;

	// spu bars
	y = y0 + barHeight*4;
	ticksToSecs = 1.0f / sys_time_get_timebase_frequency();
	incXPerTick = barWidth * ticksToSecs * mFpsTarget;
	uint32_t timebase = (uint32_t)mFrameTimeStart;
	
	// get end frame time
	uint64_t endFrameTime;
	SYS_TIMEBASE_GET(endFrameTime);

	// parse SPURS trace
	for(unsigned int i=0; i<mNumSpus; i++)
	{
		CellSpursTracePacket lastJobPacket	= (CellSpursTracePacket){{0,0,0,0,0},{{0,0}}};;
		bool lastJobPacketIsValid			= false;
		CellSpursTracePacket lastTaskPacket	= (CellSpursTracePacket){{0,0,0,0,0},{{0,0}}};;
		bool lastTaskPacketIsValid			= false;
		uint64_t lastGuid					= 0LL;
		uint32_t lastGuidColor				= 0;


		unsigned int startMarker = ( mLastValidPackedIndex[i] + 1) % mMaxTracePacketsPerSpu;
		unsigned int endMarker = mLastValidPackedIndex[i];

		for( unsigned int j = startMarker; j != endMarker; j = ( j + 1) % mMaxTracePacketsPerSpu )
		{
			const CellSpursTracePacket* tracePacketBasePointer = (const CellSpursTracePacket*)((uint32_t)mSpursTraceBuffer + sizeof(CellSpursTraceInfo));
			CellSpursTracePacket thisPacket = AtomicRead( &tracePacketBasePointer[ i * mMaxTracePacketsPerSpu + j ] );
			
			if ( IsInRange( thisPacket.header.time, timebase, (uint32_t)endFrameTime))
			{
				switch(thisPacket.header.tag)
				{
				case CELL_SPURS_TRACE_TAG_JOB:
					if ( thisPacket.data.job.jobDescriptor )
					{
						lastJobPacket			= thisPacket;
						lastJobPacketIsValid	= true;
					}
					else
					{
						//pipeline stall
						lastJobPacketIsValid	= false;
					}
					break;

				case CELL_SPURS_TRACE_TAG_GUID:

					// select a new color based on GUID
					if(thisPacket.data.rawData != lastGuid)
					{
						lastGuid = thisPacket.data.rawData;
						lastGuidColor = CONVERT_GUID_TO_COLOR(lastGuid);
					}

					if(lastJobPacketIsValid)
					{
						if ( lastJobPacket.header.time < thisPacket.header.time )
						{
							// job start/end
							float m0 = (lastJobPacket.header.time - timebase) * incXPerTick;
							float m1 = (thisPacket.header.time - timebase) * incXPerTick;

							v = AddQuad(v, x0+m0, x0+m1, y, y+barHeight, lastGuidColor);
						}

						lastJobPacketIsValid = false;

						// mark as processed
						mLastValidPackedIndex[i] = j;

					} else if( lastTaskPacketIsValid)
					{
						
						// Handle mapping/unmapping of new code inside an SPU task
						if ( lastTaskPacket.header.time < thisPacket.header.time )
						{
							float m0 = incXPerTick * clamp( lastTaskPacket.header.time - timebase, .0f, endFrameTime - timebase);
							float m1 = incXPerTick * clamp( thisPacket.header.time - timebase, .0f, endFrameTime - timebase);

							v = AddQuad(v, x0+m0, x0+m1, y, y+barHeight, lastGuidColor);
							lastTaskPacket = thisPacket;
						}
					} else {
						
						// a rough packet from last frame ?
						float m1 = incXPerTick * clamp( thisPacket.header.time - timebase, .0f, endFrameTime - timebase);

						v = AddQuad(v, x0, x0+m1, y, y+barHeight, lastGuidColor);
					}

					break;

				case CELL_SPURS_TRACE_TAG_TASK:
					switch(thisPacket.data.task.incident)
					{
					case CELL_SPURS_TRACE_TASK_DISPATCH:
						lastTaskPacket			= thisPacket;
						lastTaskPacketIsValid	= true;
						break;

					case CELL_SPURS_TRACE_TASK_WAIT:
					case CELL_SPURS_TRACE_TASK_YIELD:
					case CELL_SPURS_TRACE_TASK_EXIT:
						if(lastTaskPacketIsValid)
						{
							float m0 = incXPerTick * clamp( lastTaskPacket.header.time - timebase, .0f, endFrameTime - timebase);
							float m1 = incXPerTick * clamp( thisPacket.header.time - timebase, .0f, endFrameTime - timebase);

							v = AddQuad(v, x0+m0, x0+m1, y, y+barHeight, lastGuidColor);

							lastTaskPacketIsValid = false;

							// mark as processed
							mLastValidPackedIndex[i] = j;
						} else {

							// a rough packet from last frame ?
							float m1 = incXPerTick * clamp( thisPacket.header.time - timebase, .0f, endFrameTime - timebase);

							v = AddQuad(v, x0, x0+m1, y, y+barHeight, lastGuidColor);
						}
						break;
					}
					break;
				}
			}
		}
		
		// close the last sample
		if ( lastTaskPacketIsValid)
		{
			float m0 = incXPerTick * clamp( lastTaskPacket.header.time - timebase, .0f, endFrameTime - timebase);

			v = AddQuad(v, x0+m0, x1, y, y+barHeight, lastGuidColor);

			lastTaskPacketIsValid = false;
		}

		y += barHeight*2;
	}

	// draw the whole lot
	int numVerts = v - (Vert*)mVertexBufferCur;
	uint32_t vertsOfs;
	cellGcmSetBlendEnable( ctx, CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable( ctx, CELL_GCM_FALSE);
	cellGcmSetVertexProgram( ctx, mCGVertexProgram, mVertexProgramUCode);
	cellGcmSetFragmentProgram( ctx, mCGFragmentProgram, mFragmentProgramOffset);
	cellGcmAddressToOffset( mVertexBufferCur, &vertsOfs);
	cellGcmSetVertexDataArray( ctx, 0, 0, 16, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, vertsOfs);
	cellGcmSetVertexDataArray( ctx, 1, 0, 16, 4, CELL_GCM_VERTEX_UB, CELL_GCM_LOCATION_MAIN, vertsOfs + 12);
	cellGcmSetDrawArrays( ctx, CELL_GCM_PRIMITIVE_QUADS, 0, numVerts);
}
