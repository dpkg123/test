/* SCE CONFIDENTIAL
 $PSLibId$
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 * Author: John McCutchan john_mccutchan@playstation.sony.com
 */

#ifndef __MOVE_KIT_H__
#define __MOVE_KIT_H__

#include <cell/gem.h>
struct CellSPurs;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MoveKitCameraState
{
	MOVE_KIT_CAMERA_UNPLUGGED,
	MOVE_KIT_CAMERA_PLUGGED,
	MOVE_KIT_CAMERA_NOT_EYETOY2,
	MOVE_KIT_CAMERA_ERROR
} MoveKitCameraState_t;

typedef enum MoveKitRealityType
{
	MOVE_KIT_AUGMENETED_REALITY,
	MOVE_KIT_VIRTUAL_REALITY
} MoveKitRealityType_t;


int							MoveKitInit (CellSpurs* spurs, const uint8_t spu_priorities[8]);
int							MoveKitEnd ();
int							MoveKitInitUpdateThread (int priority);
int							MoveKitEndUpdateThread ();

void						MoveKitPrepareCameraAuto (MoveKitRealityType_t reality);
void						MoveKitPrepareCameraManual (int max_exposure, float image_quality);

MoveKitCameraState_t		MoveKitGetCameraState ();
int							MoveKitGetCameraErrorCode ();

void						MoveKitUpdateVrStates (); // TODO: should take flag and time offset
CellGemState				MoveKitGetVrState (int gem_num);
uint16_t					MoveKitGetVrDownPadButtons (int gem_num);
uint16_t					MoveKitGetVrUpPadButtons (int gem_num);

uint32_t					MoveKitGetArCameraBufferSize ();
uint32_t					MoveKitGetArCameraBufferAlign ();
uint32_t					MoveKitGetArTextureBufferSize ();
uint32_t					MoveKitGetArTextureBufferAlign ();

int							MoveKitEnableAr (unsigned char* arCameraBayerBufferExtraBuffer, unsigned char* arCameraBayerBufferPreviousFrame, unsigned char* arTextureBuffer);
void						MoveKitEnableBayerRestore (int enable);
int							MoveKitBayerRestoreEnabled ();
void						MoveKitDisableAr ();
void						MoveKitUpdateAr (CellGemState* states, CellGemImageState* imageStates, MoveKitCameraState_t* cameraState, int* cameraErrorCode);

uint32_t					MoveKitGetGemStateCode (int gem_num);
uint64_t					MoveKitGetGemStatusFlags (int gem_num);

int							MoveKitTrackSpheres (const unsigned int* req_hues, unsigned int* res_hues);
int							MoveKitTrackAllSpheres ();

const char*					MoveKitCameraStateToString (MoveKitCameraState_t state);
const char*					MoveKitGemStateCodeToString (int32_t state);
void						MoveKitGemStatusFlags_snprintf (uint64_t flags, char* str, int n);

void						MoveKitCameraStateChangePrinter (MoveKitCameraState_t state);
void						MoveKitGemStateChangePrinter (int gem_num, uint32_t state, uint64_t flags);

#ifdef __cplusplus
}
#endif

#endif
