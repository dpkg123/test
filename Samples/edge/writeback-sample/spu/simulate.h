/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __SIMULATE_H__
#define __SIMULATE_H__

//extern "C" 
//{
//}

void ScaleAndAdd(uint32_t num, float deltaT, void *x, void *dx);
void ParticleDerivatives(uint32_t count, void *positions, void *velocities, void *tmp);

#endif
