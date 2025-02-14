/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <spu_intrinsics.h>

#include "edge/geom/edgegeom.h"


void ScaleAndAdd(uint32_t num, float deltaT, void *x, void *dx)
{
	qword s_ABCd = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x1c, 0x1d, 0x1e, 0x1f};

    uint32_t iterCount = (num + 3) >> 2;

	qword dt = (qword)spu_splats(deltaT);

	qword pX0 = si_from_ptr(x);
	qword pX1 = si_ai(pX0, 0x10);
	qword pX2 = si_ai(pX0, 0x20);
	qword pX3 = si_ai(pX0, 0x30);

	qword pDx0 = si_from_ptr(dx);
	qword pDx1 = si_ai(pDx0, 0x10);
	qword pDx2 = si_ai(pDx0, 0x20);
	qword pDx3 = si_ai(pDx0, 0x30);

	qword pOut0 = pX0;
	qword pOut1 = pX1;
	qword pOut2 = pX2;
	qword pOut3 = pX3;

	qword offset = (qword)spu_splats(0);
	qword offsetInc = (qword)spu_splats(0x40);

	do{
		qword x0 = si_lqx(pX0, offset);
		qword dx0 = si_lqx(pDx0, offset);

		qword x1 = si_lqx(pX1, offset);
		qword dx1 = si_lqx(pDx1, offset);

		qword x2 = si_lqx(pX2, offset);
		qword dx2 = si_lqx(pDx2, offset);

		qword out0 = si_shufb(si_fma(dx0, dt, x0), x0, s_ABCd);
		qword out1 = si_shufb(si_fma(dx1, dt, x1), x1, s_ABCd);

		qword x3 = si_lqx(pX3, offset);
		qword dx3 = si_lqx(pDx3, offset);

		qword out2 = si_shufb(si_fma(dx2, dt, x2), x2, s_ABCd);
		qword out3 = si_shufb(si_fma(dx3, dt, x3), x3, s_ABCd);

		si_stqx(out0, pOut0, offset);
		si_stqx(out1, pOut1, offset);
		si_stqx(out2, pOut2, offset);
		si_stqx(out3, pOut3, offset);

		offset = si_a(offset, offsetInc);
		--iterCount;
	} while (iterCount != 0);
}

void ParticleDerivatives(uint32_t count, void *positions, void *velocities, void *tmp)
{
	const qword s_DDDD = (qword)spu_splats(0x0c0d0e0f);
	qword fGravity = si_and((qword)spu_splats(-9.8f), si_fsm(si_from_uint(0x4)));

	uint32_t iterCount = (count + 3) >> 2;
	uint32_t iterCountCopy = iterCount;

	count = (count + 3) & ~3;

	// clear forces
	qword zero = (qword)spu_splats(0);
	qword pCurr0 = si_from_ptr(tmp);

	do
	{
		si_stqd(zero, pCurr0, 0x00);
		si_stqd(zero, pCurr0, 0x10);
		si_stqd(zero, pCurr0, 0x20);
		si_stqd(zero, pCurr0, 0x30);
		pCurr0 = si_ai(pCurr0, 0x40);
		--iterCount;
	} while(iterCount != 0);

	iterCount = iterCountCopy;
	pCurr0 = si_from_ptr(tmp);
	qword pCurr1 = si_ai(pCurr0, 0x10);
	qword pCurr2 = si_ai(pCurr0, 0x20);
	qword pCurr3 = si_ai(pCurr0, 0x30);

	qword pV0 = si_from_ptr(velocities);
	qword pV1 = si_ai(pV0, 0x10);
	qword pV2 = si_ai(pV0, 0x20);
	qword pV3 = si_ai(pV0, 0x30);

	qword pX0 = si_from_ptr(positions);
	qword pX1 = si_ai(pX0, 0x10);
	qword pX2 = si_ai(pX0, 0x20);
	qword pX3 = si_ai(pX0, 0x30);

	qword offset = (qword)spu_splats(0);

	vec_float4 firstImpulse = {0.f, -1.5f, 0.f, 0.f};
	vec_float4 secondImpulse = {0.f, -1.25f, 0.f, 0.f};
	vec_float4 thirdImpulse = {0.f, -1.f, -1.f, 0.f};

	vec_float4 firstBumpBounds = {-3.f, -10.f, 14.f, 0.f};
	vec_float4 secondBumpBounds = {3.f, -30.f, 16.f, 0.f};
	vec_float4 thirdBumpBounds = {50.f, -40.f, 50.f, 0.f};

	// compute forces
	do{
		qword f0 = si_lqx(pCurr0, offset);
		qword f1 = si_lqx(pCurr1, offset);
		qword f2 = si_lqx(pCurr2, offset);
		qword f3 = si_lqx(pCurr3, offset);

		qword v0 = si_lqx(pV0, offset);
		qword v1 = si_lqx(pV1, offset);
		qword v2 = si_lqx(pV2, offset);
		qword v3 = si_lqx(pV3, offset);

		qword rm0 = si_shufb(v0, zero, s_DDDD);
		qword rm1 = si_shufb(v1, zero, s_DDDD);
		qword rm2 = si_shufb(v2, zero, s_DDDD);
		qword rm3 = si_shufb(v3, zero, s_DDDD);

		qword m0 = si_fi(rm0, si_frest(rm0));
		qword m1 = si_fi(rm1, si_frest(rm1));
		qword m2 = si_fi(rm2, si_frest(rm2));
		qword m3 = si_fi(rm3, si_frest(rm3));

		f0 = si_fma(m0, fGravity, f0);
		f1 = si_fma(m1, fGravity, f1);
		f2 = si_fma(m2, fGravity, f2);
		f3 = si_fma(m3, fGravity, f3);

		qword p0 = si_lqx(pX0, offset);
		qword p1 = si_lqx(pX1, offset);
		qword p2 = si_lqx(pX2, offset);
		qword p3 = si_lqx(pX3, offset);

		qword firstBump0 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)firstBumpBounds, p0)), 0xe), 0xe));
		qword firstBump1 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)firstBumpBounds, p1)), 0xe), 0xe));
		qword firstBump2 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)firstBumpBounds, p2)), 0xe), 0xe));
		qword firstBump3 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)firstBumpBounds, p3)), 0xe), 0xe));

		qword firstImpulse0 = si_and(firstBump0, (qword)firstImpulse);
		qword firstImpulse1 = si_and(firstBump1, (qword)firstImpulse);
		qword firstImpulse2 = si_and(firstBump2, (qword)firstImpulse);
		qword firstImpulse3 = si_and(firstBump3, (qword)firstImpulse);

		firstImpulse0 = si_selb(firstImpulse0, zero, si_and(si_fcgt(v0, zero), si_fsm(si_from_int(0x4))));
		firstImpulse1 = si_selb(firstImpulse1, zero, si_and(si_fcgt(v1, zero), si_fsm(si_from_int(0x4))));
		firstImpulse2 = si_selb(firstImpulse2, zero, si_and(si_fcgt(v2, zero), si_fsm(si_from_int(0x4))));
		firstImpulse3 = si_selb(firstImpulse3, zero, si_and(si_fcgt(v3, zero), si_fsm(si_from_int(0x4))));

		v0 = si_fma(firstImpulse0, v0, v0);
		v1 = si_fma(firstImpulse1, v1, v1);
		v2 = si_fma(firstImpulse2, v2, v2);
		v3 = si_fma(firstImpulse3, v3, v3);

		qword secondBump0 = si_fsm(si_ceqi(si_andi(si_xori(si_gb(si_fcgt((qword)secondBumpBounds, p0)), 0x8), 0xe), 0xe));
		qword secondBump1 = si_fsm(si_ceqi(si_andi(si_xori(si_gb(si_fcgt((qword)secondBumpBounds, p1)), 0x8), 0xe), 0xe));
		qword secondBump2 = si_fsm(si_ceqi(si_andi(si_xori(si_gb(si_fcgt((qword)secondBumpBounds, p2)), 0x8), 0xe), 0xe));
		qword secondBump3 = si_fsm(si_ceqi(si_andi(si_xori(si_gb(si_fcgt((qword)secondBumpBounds, p3)), 0x8), 0xe), 0xe));

		qword secondImpulse0 = si_and(secondBump0, (qword)secondImpulse);
		qword secondImpulse1 = si_and(secondBump1, (qword)secondImpulse);
		qword secondImpulse2 = si_and(secondBump2, (qword)secondImpulse);
		qword secondImpulse3 = si_and(secondBump3, (qword)secondImpulse);

		secondImpulse0 = si_selb(secondImpulse0, zero, si_and(si_fcgt(v0, zero), si_fsm(si_from_int(0x4))));
		secondImpulse1 = si_selb(secondImpulse1, zero, si_and(si_fcgt(v1, zero), si_fsm(si_from_int(0x4))));
		secondImpulse2 = si_selb(secondImpulse2, zero, si_and(si_fcgt(v2, zero), si_fsm(si_from_int(0x4))));
		secondImpulse3 = si_selb(secondImpulse3, zero, si_and(si_fcgt(v3, zero), si_fsm(si_from_int(0x4))));

		v0 = si_fma(secondImpulse0, v0, v0);
		v1 = si_fma(secondImpulse1, v1, v1);
		v2 = si_fma(secondImpulse2, v2, v2);
		v3 = si_fma(secondImpulse3, v3, v3);

		qword thirdBump0 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)thirdBumpBounds, p0)), 0xe), 0xe));
		qword thirdBump1 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)thirdBumpBounds, p1)), 0xe), 0xe));
		qword thirdBump2 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)thirdBumpBounds, p2)), 0xe), 0xe));
		qword thirdBump3 = si_fsm(si_ceqi(si_andi(si_gb(si_fcgt((qword)thirdBumpBounds, p3)), 0xe), 0xe));

		qword thirdImpulse0 = si_and(thirdBump0, (qword)thirdImpulse);
		qword thirdImpulse1 = si_and(thirdBump1, (qword)thirdImpulse);
		qword thirdImpulse2 = si_and(thirdBump2, (qword)thirdImpulse);
		qword thirdImpulse3 = si_and(thirdBump3, (qword)thirdImpulse);

		thirdImpulse0 = si_selb(thirdImpulse0, zero, si_and(si_fcgt(v0, zero), si_fsm(si_from_int(0x6))));
		thirdImpulse1 = si_selb(thirdImpulse1, zero, si_and(si_fcgt(v1, zero), si_fsm(si_from_int(0x6))));
		thirdImpulse2 = si_selb(thirdImpulse2, zero, si_and(si_fcgt(v2, zero), si_fsm(si_from_int(0x6))));
		thirdImpulse3 = si_selb(thirdImpulse3, zero, si_and(si_fcgt(v3, zero), si_fsm(si_from_int(0x6))));

		v0 = si_fma(thirdImpulse0, v0, v0);
		v1 = si_fma(thirdImpulse1, v1, v1);
		v2 = si_fma(thirdImpulse2, v2, v2);
		v3 = si_fma(thirdImpulse3, v3, v3);


		si_stqx(f0, pCurr0, offset);
		si_stqx(f1, pCurr1, offset);
		si_stqx(f2, pCurr2, offset);
		si_stqx(f3, pCurr3, offset);

		si_stqx(v0, pV0, offset);
		si_stqx(v1, pV1, offset);
		si_stqx(v2, pV2, offset);
		si_stqx(v3, pV3, offset);

		offset = si_ai(offset, 0x40);

		--iterCount;
	} while(iterCount != 0);

	// update derivs
	iterCount = iterCountCopy;
	qword pDx0 = pCurr0;
	qword pDx1 = pCurr1;
	qword pDx2 = pCurr2;
	qword pDx3 = pCurr3;

	qword pDv0 = si_from_uint((uint32_t)tmp + 16 * count);
	qword pDv1 = si_ai(pDv0, 0x10);
	qword pDv2 = si_ai(pDv0, 0x20);
	qword pDv3 = si_ai(pDv0, 0x30);

	offset = (qword)spu_splats(0);

	do{
		//xdot = v
		//vdot = f/m

		qword v0 = si_lqx(pV0, offset);
		qword v1 = si_lqx(pV1, offset);
		qword v2 = si_lqx(pV2, offset);
		qword v3 = si_lqx(pV3, offset);

		qword m0 = si_shufb(v0, v0, s_DDDD);
		qword m1 = si_shufb(v1, v1, s_DDDD);
		qword m2 = si_shufb(v2, v2, s_DDDD);
		qword m3 = si_shufb(v3, v3, s_DDDD);

		qword f0 = si_lqx(pCurr0, offset);
		qword f1 = si_lqx(pCurr1, offset);
		qword f2 = si_lqx(pCurr2, offset);
		qword f3 = si_lqx(pCurr3, offset);

		si_stqx(v0, pDx0, offset);
		si_stqx(v1, pDx1, offset);
		si_stqx(v2, pDx2, offset);
		si_stqx(v3, pDx3, offset);

		qword vdot0 = si_fm(f0, m0);
		qword vdot1 = si_fm(f1, m1);
		qword vdot2 = si_fm(f2, m2);
		qword vdot3 = si_fm(f3, m3);

		si_stqx(vdot0, pDv0, offset);
		si_stqx(vdot1, pDv1, offset);
		si_stqx(vdot2, pDv2, offset);
		si_stqx(vdot3, pDv3, offset);

		offset = si_ai(offset, 0x40);

		--iterCount;
	} while(iterCount != 0);
}

