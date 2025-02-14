/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef PROFILE_BARS
#define PROFILE_BARS

class CellSpurs;
class CellSpursTracePacket;

class ProfileBars
{
public:
	static void						Init(CellSpurs* spurs, uint32_t numSpus, uint32_t maxTracePacketsPerSpu = 1024);
	static void						StartFrame();
	static void						AddPpuMarker(uint32_t color);
	static void						AddRsxMarker(uint32_t color);
	static void						AddRsxMarker(CellGcmContextData* context, uint32_t color);
	static void						Render();
	static void						Shutdown();
	static void						SetTargetFps( float targetFps );

private:
	struct Vert
	{
		float		x, y, z;
		uint32_t	color;
	};

	struct JobInfo
	{
		uint64_t	guid;
		uint32_t	color;
		uint32_t	pad;
	};

	struct PpuMarker
	{
		uint32_t	time;
		uint32_t	color;
	};

	struct RsxMarker
	{
		uint32_t	color;
	};

	static void						PrepareProfilerCommandBuffer(  CellGcmContextData *ctx );
	static Vert*					AddQuad(Vert* v, float x0, float x1, float y0, float y1, uint32_t color);

	static const uint32_t			kMaxPpuMarkers = 128;
	static const uint32_t			kMaxRsxMarkers = 128;
	static const uint32_t			kCmdbufSize = 0xffff;

	static void*					mSpursTraceBuffer;
	static uint32_t					mMaxTracePacketsPerSpu;
	static void*					mVertexBuffer;
	static void*					mVertexBufferCur;
	static uint32_t					mVertexBufferSize;
	static uint32_t					mNumSpus;
	static CGprogram                mCGVertexProgram;
	static CGprogram                mCGFragmentProgram;
	static void*                    mVertexProgramUCode;
	static void*                    mFragmentProgramUCode;
	static uint32_t                 mFragmentProgramOffset;
	static PpuMarker				mPpuMarkers[kMaxPpuMarkers];
	static RsxMarker				mRsxMarkers[kMaxRsxMarkers];
	static uint32_t					mNumPpuMarkers;
	static uint32_t					mNumRsxMarkers;
	static uint64_t					mFrameTimeStart;
	static float					mFpsTarget;
	static uint32_t					mLastValidPackedIndex[6];
	static uint32_t*				mCmdAddr;
};

inline ProfileBars::Vert* ProfileBars::AddQuad(Vert* v, float x0, float x1, float y0, float y1, uint32_t color)
{
	assert((uint32_t)v + 0x40 - (uint32_t)mVertexBufferCur <= mVertexBufferSize);

	v[0].x = x0;	v[0].y = y0;	v[0].z = 0;		v[0].color = color;
	v[1].x = x1;	v[1].y = y0;	v[1].z = 0;		v[1].color = color;
	v[2].x = x1;	v[2].y = y1;	v[2].z = 0;		v[2].color = color;
	v[3].x = x0;	v[3].y = y1;	v[3].z = 0;		v[3].color = color;

	return v+4;
}

#endif
