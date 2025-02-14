/* SCE CONFIDENTIAL
 $PSLibId$
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 * Author: John McCutchan john_mccutchan@playstation.sony.com
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cell/spurs.h>
#include <cell/camera.h>
#include <sysutil/sysutil_sysparam.h>
#include <cell/sysmodule.h>

#if USE_PS3_MOVE

#include "MoveKit.h"

#define CAMERA_DEVICE_NUM 0

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CODELOC __FILE__ ":" TOSTRING(__LINE__)

#define MoveKitAssertCELL_OK(exp) \
	do { \
	int ___ret = (exp); \
	if(___ret != CELL_OK) { \
	printf(CODELOC " - " #exp " ### ERROR = %x\n",___ret); \
	assert(0); \
	} \
	} while(0)

#define MoveKitErrorMsg(format, args...) \
	do { \
	printf(CODELOC " - " format, ## args); \
	} while (0)

static CellGemState _vrGemState[CELL_GEM_MAX_NUM];
static uint16_t _vrLastPadButtons[CELL_GEM_MAX_NUM];


static unsigned char* _arCameraBayerBuffer;
static unsigned char* _arCameraBayerBufferPreviousFrame;
static unsigned char* _arTextureBuffer;
static CellGemState _arGemState[CELL_GEM_MAX_NUM];
static CellGemImageState _arGemImageState[CELL_GEM_MAX_NUM];

static uint32_t _gemState[CELL_GEM_MAX_NUM];
static uint64_t _gemStatusFlags[CELL_GEM_MAX_NUM];

static MoveKitCameraState_t _cameraState;
static int _cameraErrorCode;

static int _updateThreadEnd;
static sys_ppu_thread_t _updateThreadId;
static sys_lwmutex_t _updateMutex;
static int _bayerRestore;
static CellGemAttribute _gemAttribute;

static void* _gem_memory;
static sys_memory_container_t _camera_memory_container;

static int _max_exposure;
static float _image_quality;

static void MoveKitInternalCameraPlugged ();
static void MoveKitInternalCameraUnplugged ();
static system_time_t MoveKitInternalCameraFrame ();
static void MoveKitInternalUpdateState (system_time_t frameTime);
static void MoveKitInternalThreadMain (uint64_t arg);
static int MoveKitInternalThreadShouldExit ();

/* internal implementation */
static void MoveKitInternalCameraPlugged ()
{
	CellCameraType type;
	cellCameraGetType(CAMERA_DEVICE_NUM, &type);
	if (type == CELL_CAMERA_EYETOY2)
	{
		CellCameraInfoEx camera_info;
		camera_info.format=CELL_CAMERA_RAW8;
		camera_info.framerate=60;
		camera_info.resolution=CELL_CAMERA_VGA;
		camera_info.container=_camera_memory_container;
		camera_info.info_ver=CELL_CAMERA_INFO_VER_101;
		int r = cellCameraOpenEx(CAMERA_DEVICE_NUM, &camera_info);
		if (r == CELL_OK)
		{
			_cameraState = MOVE_KIT_CAMERA_PLUGGED;
		} else {
			_cameraState = MOVE_KIT_CAMERA_ERROR;
			_cameraErrorCode = r;
		}
	} else {
		_cameraState = MOVE_KIT_CAMERA_NOT_EYETOY2;
	}
	if (_cameraState == MOVE_KIT_CAMERA_PLUGGED)
	{
		cellCameraReset(CAMERA_DEVICE_NUM);
		cellGemPrepareCamera(_max_exposure,_image_quality);
		cellCameraStart(CAMERA_DEVICE_NUM);
	}
}

static void MoveKitInternalCameraUnplugged ()
{
	cellCameraStop(CAMERA_DEVICE_NUM);
	cellCameraClose(CAMERA_DEVICE_NUM);
	_cameraState = MOVE_KIT_CAMERA_UNPLUGGED;
}

static system_time_t MoveKitInternalCameraFrame ()
{
	CellCameraReadEx camReadEx;
	camReadEx.version=CELL_CAMERA_READ_VER;
	camReadEx.timestamp = 0;
	int r = cellCameraReadEx(0,&camReadEx);
	if (r != CELL_OK)
	{
		_cameraState = MOVE_KIT_CAMERA_ERROR;
		_cameraErrorCode = r;
	}
	return camReadEx.timestamp;
}

static void MoveKitInternalUpdateState (system_time_t frameTime)
{
	// analyze the image / update gem (do this regardless of the pseye status)
	CellCameraInfoEx camera_info;
	camera_info.buffer=NULL; // set to NULL just in case the camera isn't ready
	cellCameraGetBufferInfoEx(0,&camera_info);
	cellGemUpdateStart(camera_info.buffer, frameTime);
	cellGemUpdateFinish();

	sys_lwmutex_lock (&_updateMutex, 0);
	if (camera_info.buffer)
	{
		for (int i=0; i<CELL_GEM_MAX_NUM; i++)
		{
			cellGemGetState(i, CELL_GEM_STATE_FLAG_LATEST_IMAGE_TIME, 0, &_arGemState[i]);
			cellGemGetImageState(i, &_arGemImageState[i]);
		}
		if (_arCameraBayerBuffer)
		{
			if (_bayerRestore)
			{
				CellGemVideoConvertAttribute gemVideoAttr;
				int conversion_flags = CELL_GEM_AUTO_WHITE_BALANCE|CELL_GEM_GAMMA_BOOST|CELL_GEM_FILTER_OUTLIER_PIXELS;
				cellGemVideoConvertAttributeInit(&gemVideoAttr,CELL_GEM_BAYER_RESTORED,conversion_flags,1.0f,1.0f,1.0f,1.0f,NULL,_arCameraBayerBuffer,255);
				MoveKitAssertCELL_OK(cellGemPrepareVideoConvert(&gemVideoAttr));
				cellGemConvertVideoStart(camera_info.buffer);
				cellGemConvertVideoFinish();
			} else {
				// raw copy of bayer video stream
				memcpy(_arCameraBayerBuffer, camera_info.buffer, 640*480);
			}
			
		}
	}
	sys_lwmutex_unlock (&_updateMutex);
}

static void MoveKitInternalThreadMain (uint64_t arg)
{
	sys_ipc_key_t ipcKey = 0x4a6f686e;
	sys_event_queue_t eventQueue;
	sys_event_queue_attribute_t eventAttr;
	sys_event_queue_attribute_initialize(eventAttr);
	MoveKitAssertCELL_OK(sys_event_queue_create(&eventQueue, &eventAttr, ipcKey, 10));
	MoveKitAssertCELL_OK(cellCameraSetNotifyEventQueue2(ipcKey, SYS_EVENT_PORT_NO_NAME, CELL_CAMERA_EFLAG_FRAME_UPDATE));

	while (MoveKitInternalThreadShouldExit() == 0)
	{
		sys_event_t event;
		int receive_r = sys_event_queue_receive(eventQueue, &event, 0);
		if (receive_r != CELL_OK && receive_r != ETIMEDOUT)
		{
			MoveKitAssertCELL_OK(receive_r);
		}
		if (receive_r == CELL_OK)
		{
			system_time_t frameTime = 0;
			switch(event.data1)
			{
			case CELL_CAMERA_ATTACH:
				MoveKitInternalCameraPlugged ();
				break;
			case CELL_CAMERA_DETACH:
				MoveKitInternalCameraUnplugged ();
				break;
			case CELL_CAMERA_FRAME_UPDATE:
				frameTime = MoveKitInternalCameraFrame ();
				break;
			}
			MoveKitInternalUpdateState(frameTime);
		} else {
			MoveKitErrorMsg("MoveKit::sys_event_queue_receive = 0x%08x\n", receive_r);
		}
	}

	cellCameraRemoveNotifyEventQueue2(ipcKey);
	sys_event_queue_destroy(eventQueue, 0);
	sys_ppu_thread_exit (0);
}

/* public implementation */
int MoveKitInit (CellSpurs* spurs, const uint8_t spu_priorities[8])
{
	for (int i = 0; i < CELL_GEM_MAX_NUM; i++)
	{
		memset(&_vrGemState[i], 0, sizeof(CellGemState));
		memset(&_arGemState[i], 0, sizeof(CellGemState));
		_vrLastPadButtons[i] = 0;
		_gemState[i] = 0;
		_gemStatusFlags[i] = 0;
	}
	_bayerRestore = 0;
	_arCameraBayerBuffer = NULL;
	_arTextureBuffer = NULL;
	_arCameraBayerBufferPreviousFrame = NULL;
	_updateThreadEnd = 0;
	_gem_memory = NULL;
	_cameraState = MOVE_KIT_CAMERA_UNPLUGGED;
	_camera_memory_container = 0xffffffff;
	MoveKitAssertCELL_OK(cellSysmoduleLoadModule(CELL_SYSMODULE_CAMERA));
	MoveKitAssertCELL_OK(cellCameraInit());
	MoveKitAssertCELL_OK(cellSysmoduleLoadModule(CELL_SYSMODULE_GEM));
	_gem_memory = malloc(cellGemGetMemorySize(CELL_GEM_MAX_NUM));
	MoveKitAssertCELL_OK(sys_memory_container_create(&_camera_memory_container, 0x100000));
	cellGemAttributeInit(&_gemAttribute, CELL_GEM_MAX_NUM, _gem_memory, spurs, spu_priorities);
	MoveKitAssertCELL_OK(cellGemInit (&_gemAttribute));
	MoveKitPrepareCameraAuto(MOVE_KIT_VIRTUAL_REALITY);
	sys_lwmutex_attribute_t lwMutexAttr;
	sys_lwmutex_attribute_initialize(lwMutexAttr);
	sys_lwmutex_attribute_name_set(lwMutexAttr.name, "jmGemMtx");
	MoveKitAssertCELL_OK(sys_lwmutex_create(&_updateMutex, &lwMutexAttr));
	MoveKitErrorMsg("Memory used = %d\n", cellGemGetMemorySize(CELL_GEM_MAX_NUM));
	return CELL_OK;
}

int MoveKitEnd ()
{
	MoveKitAssertCELL_OK(cellGemEnd());
	MoveKitAssertCELL_OK(cellSysmoduleUnloadModule(CELL_SYSMODULE_GEM));
	MoveKitAssertCELL_OK(cellCameraEnd());
	MoveKitAssertCELL_OK(cellSysmoduleUnloadModule(CELL_SYSMODULE_CAMERA));
	free (_gem_memory);
	MoveKitAssertCELL_OK(sys_memory_container_destroy(_camera_memory_container));
	_camera_memory_container = 0xffffffff;
	return CELL_OK;
}
int MoveKitInitUpdateThread (int priority)
{
	_updateThreadEnd = false;
	int r = 0;
	const int stacksize = 16*1024; // 16k
	r = sys_ppu_thread_create (&_updateThreadId, MoveKitInternalThreadMain, 0, priority, stacksize, SYS_PPU_THREAD_CREATE_JOINABLE, "MoveKitUpdateThread");
	if (r != CELL_OK)
		return r;
	return r;
}

int MoveKitEndUpdateThread ()
{
	_updateThreadEnd = true;
	uint64_t tr;
	int r;
	r = sys_ppu_thread_join (_updateThreadId, &tr);
	return r;
}

static int MoveKitInternalThreadShouldExit ()
{
	return _updateThreadEnd;
}

void MoveKitPrepareCameraAuto (MoveKitRealityType_t reality)
{

}
void MoveKitPrepareCameraManual (int max_exposure, float image_quality)
{

}

MoveKitCameraState_t MoveKitGetCameraState ()
{
	return _cameraState;
}

int MoveKitGetCameraErrorCode ()
{
	return _cameraErrorCode;
}

void MoveKitUpdateVrStates ()
{
	for (int i = 0; i < CELL_GEM_MAX_NUM; i++)
	{
		_vrLastPadButtons[i] = _vrGemState[i].pad.digitalbuttons;
		_gemState[i] = cellGemGetState(i, CELL_GEM_STATE_FLAG_CURRENT_TIME, CELL_GEM_LATENCY_OFFSET, &_vrGemState[i]);
		cellGemGetStatusFlags(i, &_gemStatusFlags[i]);
	}
}

CellGemState MoveKitGetVrState (int gem_num)
{
	return _vrGemState[gem_num];
}

uint16_t MoveKitGetVrDownPadButtons (int gem_num)
{
	uint16_t changedButtons = _vrGemState[gem_num].pad.digitalbuttons ^ _vrLastPadButtons[gem_num];
	return changedButtons & _vrGemState[gem_num].pad.digitalbuttons;
}

uint16_t MoveKitGetVrUpPadButtons (int gem_num)
{
	uint16_t changedButtons = _vrGemState[gem_num].pad.digitalbuttons ^ _vrLastPadButtons[gem_num];
	return changedButtons & ~_vrGemState[gem_num].pad.digitalbuttons;
}

uint32_t MoveKitGetArCameraBufferSize ()
{
	return 640*480*sizeof(unsigned char);
}

uint32_t MoveKitGetArCameraBufferAlign ()
{
	return 128;
}

uint32_t MoveKitGetArTextureBufferSize ()
{
	return 640*480*4;
}

uint32_t MoveKitGetArTextureBufferAlign ()
{
	return 128;
}

int MoveKitEnableAr (unsigned char* arCameraBayerBuffer, unsigned char* arCameraBayerBufferPreviousFrame, unsigned char* arTextureBuffer)
{
	_arCameraBayerBuffer = arCameraBayerBuffer;
	_arCameraBayerBufferPreviousFrame = arCameraBayerBufferPreviousFrame;
	_arTextureBuffer = arTextureBuffer;
	
	return CELL_OK;
}

void MoveKitEnableBayerRestore (int enable)
{
	_bayerRestore = enable;
}

int MoveKitBayerRestoreEnabled ()
{
	return _bayerRestore;
}

void MoveKitDisableAr ()
{
	_arCameraBayerBuffer = NULL;
	_arTextureBuffer = NULL;
	_arCameraBayerBufferPreviousFrame = NULL;
}

void MoveKitUpdateAr (CellGemState* states, CellGemImageState* imageStates, MoveKitCameraState_t* cameraState, int* cameraErrorCode)
{
	if (_arCameraBayerBuffer == NULL)
		return;

	sys_lwmutex_lock (&_updateMutex, 0);

	{
		CellGemVideoConvertAttribute gemVideoAttr;
		int conversion_flags = CELL_GEM_AUTO_WHITE_BALANCE|CELL_GEM_GAMMA_BOOST;
		if (_bayerRestore)
		{
			conversion_flags = 0; // gamma and white balance boost already applied during bayer restoration
		}
		cellGemVideoConvertAttributeInit(&gemVideoAttr,CELL_GEM_RGBA_640x480,conversion_flags,1.0f,1.0f,1.0f,1.0f,_arCameraBayerBufferPreviousFrame,_arTextureBuffer,255);
		MoveKitAssertCELL_OK(cellGemPrepareVideoConvert(&gemVideoAttr));
	}

		cellGemConvertVideoStart(_arCameraBayerBuffer);

		// copy state
		for (int i = 0; i < CELL_GEM_MAX_NUM; i++)
		{
			states[i] = _arGemState[i];
			imageStates[i] = _arGemImageState[i];
		}
		*cameraState = _cameraState;
		*cameraErrorCode = _cameraErrorCode;
		cellGemConvertVideoFinish();
	sys_lwmutex_unlock (&_updateMutex);
}

uint32_t MoveKitGetGemStateCode (int gem_num)
{
	return _gemState[gem_num];
}

uint64_t MoveKitGetGemStatusFlags (int gem_num)
{
	return _gemStatusFlags[gem_num];
}

int MoveKitTrackSpheres (const unsigned int* req_hues, unsigned int* res_hues)
{
	return cellGemTrackHues(req_hues, res_hues);
}

int MoveKitTrackAllSpheres ()
{
	unsigned int req_hues[4] = { CELL_GEM_DONT_CARE_HUE, CELL_GEM_DONT_CARE_HUE, CELL_GEM_DONT_CARE_HUE, CELL_GEM_DONT_CARE_HUE};
	return MoveKitTrackSpheres(&req_hues[0], NULL);
}

const char* cameraEventToString(sys_event_t event)
{
	switch (event.data1)
	{
	case CELL_CAMERA_ATTACH:
		return "ATTACH";
		break;
	case CELL_CAMERA_DETACH:
		return "DETACH";
		break;
	case CELL_CAMERA_FRAME_UPDATE:
		return "FRAME";
		break;
	case CELL_CAMERA_OPEN:
		return "OPEN";
		break;
	case CELL_CAMERA_CLOSE:
		return "CLOSE";
		break;
	case CELL_CAMERA_START:
		return "START";
		break;
	case CELL_CAMERA_STOP:
		return "STOP";
		break;
	case CELL_CAMERA_RESET:
		return "RESET";
		break;
	}
	//printf("%08x\n", (uint32_t)event.data1);
	return "UNKNOWN";
}

const char* MoveKitCameraStateToString (MoveKitCameraState_t state)
{
	switch(state)
	{
	case MOVE_KIT_CAMERA_UNPLUGGED:
		return "UNPLUGGED";
	case MOVE_KIT_CAMERA_PLUGGED:
		return "PLUGGED";
	case MOVE_KIT_CAMERA_NOT_EYETOY2:
		return "REQUIRE EYETOY2";
	case MOVE_KIT_CAMERA_ERROR:
		return "ERROR";
	}
	return "UNKNOWN";
}

const char* MoveKitGemStateCodeToString (int32_t state)
{
	switch (state)
	{
	case CELL_OK:
		return "TRACKING";
	break;
	case CELL_GEM_NOT_CONNECTED:
		return "NOT CONNECTED";
	break;
	case CELL_GEM_SPHERE_NOT_CALIBRATED:
		return "NOT CALIBRATED";
	break;
	case CELL_GEM_SPHERE_CALIBRATING:
		return "CALIBRATING";
	break;
	case CELL_GEM_COMPUTING_AVAILABLE_COLORS:
		return "COMPUTING AVAILABLE COLORS";
	break;
	case CELL_GEM_HUE_NOT_SET:
		return "HUE NOT SET";
	break;
	case CELL_GEM_NO_VIDEO:
		return "NO VIDEO";
	break;
	}
	return "UNKNOWN STATE";
}

static const char* MoveKitGemStatusToString (uint64_t flags, uint64_t flag)
{
	if ((flags & flag) == 0)
		return "";
	switch (flag)
	{
	case CELL_GEM_FLAG_CALIBRATION_OCCURRED:
		return "CALIBRATION OCCURED ";
		break;
	case CELL_GEM_FLAG_CALIBRATION_SUCCEEDED:
		return "CALIBRATION SUCCEEDED ";
		break;
	case CELL_GEM_FLAG_CALIBRATION_FAILED_CANT_FIND_SPHERE:
		return "FAIL(CANT FIND SPHERE) ";
		break;
	case CELL_GEM_FLAG_CALIBRATION_FAILED_MOTION_DETECTED:
		return "FAIL(MOTION DETECTED) ";
		break;
	case CELL_GEM_FLAG_CALIBRATION_FAILED_BRIGHT_LIGHTING:
		return "FAIL(BRIGHT LIGHTING) ";
		break;
	case CELL_GEM_FLAG_CALIBRATION_WARNING_MOTION_DETECTED:
		return "WARN(MOTION DETECTED) ";
		break;
	case CELL_GEM_FLAG_CALIBRATION_WARNING_BRIGHT_LIGHTING:
		return "WARN(BRIGHT LIGHTING) ";
		break;
	case CELL_GEM_FLAG_VERY_COLORFUL_ENVIRONMENT:
		return "VERY COLORFUL ENVIRONMENT ";
		break;
	case CELL_GEM_FLAG_CURRENT_HUE_CONFLICTS_WITH_ENVIRONMENT:
		return "HUE CONFLICTS WITH ENVIRONMENT ";
		break;
	}
	return "UNKNOWN FLAG";
}

void MoveKitGemStatusFlags_snprintf (uint64_t flags, char* str, int n)
{
	snprintf(&str[0], n, "%s%s%s%s%s%s%s%s",
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CALIBRATION_OCCURRED),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CALIBRATION_SUCCEEDED),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CALIBRATION_FAILED_CANT_FIND_SPHERE),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CALIBRATION_FAILED_MOTION_DETECTED),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CALIBRATION_WARNING_MOTION_DETECTED),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CALIBRATION_WARNING_BRIGHT_LIGHTING),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_VERY_COLORFUL_ENVIRONMENT),
		MoveKitGemStatusToString(flags, CELL_GEM_FLAG_CURRENT_HUE_CONFLICTS_WITH_ENVIRONMENT)
		);
}


void MoveKitCameraStateChangePrinter (MoveKitCameraState_t state)
{
	static MoveKitCameraState_t _previous_state = MOVE_KIT_CAMERA_UNPLUGGED;
	if (_previous_state != state)
	{
		// state changed
		_previous_state = state;
		printf("CameraState = %s\n", MoveKitCameraStateToString(state));
		if (state == MOVE_KIT_CAMERA_ERROR)
		{
			printf("CameraState Error code = 0x%08x\n", MoveKitGetCameraErrorCode());
		}
	}
}

void MoveKitGemStateChangePrinter (int gem_num, uint32_t state, uint64_t flags)
{
	static uint32_t _previous_gemStates[CELL_GEM_MAX_NUM] = { 0, 0, 0, 0};
	static uint64_t _previous_gemStatusFlags[CELL_GEM_MAX_NUM] = { 0, 0, 0, 0 };
#define STATUS_FLAGS_BUFFER_SIZE 1024
	char print_buffer[STATUS_FLAGS_BUFFER_SIZE];
	print_buffer[0] = '\0';
	if (_previous_gemStates[gem_num] != state)
	{
		// state changed
		_previous_gemStates[gem_num] = state;
		_previous_gemStatusFlags[gem_num] = flags;
		printf("GemState[%d] = %s\n", gem_num, MoveKitGemStateCodeToString(state));
		MoveKitGemStatusFlags_snprintf(flags, &print_buffer[0], STATUS_FLAGS_BUFFER_SIZE);
		printf("GemStatus[%d] = %s\n", gem_num, print_buffer);
	}
}

#endif