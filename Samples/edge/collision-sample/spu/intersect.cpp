/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <float.h>
#include "intersect.h"
#include <spu_intrinsics.h>

#define MAX_FLOAT (10000.f)
#define EPSILON (FLT_EPSILON * 2.f)
const qword s_AAAA = {0x00, 0x01, 0x02, 0x03,  0x00, 0x01, 0x02, 0x03,  0x00, 0x01, 0x02, 0x03,  0x00, 0x01, 0x02, 0x03};//si_ila(0x10203);
const qword s_ABCD = {0x00, 0x01, 0x02, 0x03,  0x04, 0x05, 0x06, 0x07,  0x08, 0x09, 0x0a, 0x0b,  0x0c, 0x0d, 0x0e, 0x0f};
const qword s_AaBb = {0x00, 0x01, 0x02, 0x03,  0x10, 0x11, 0x12, 0x13,  0x04, 0x05, 0x06, 0x07,  0x14, 0x15, 0x16, 0x17};
const qword s_BbAa = {0x04, 0x05, 0x06, 0x07,  0x14, 0x15, 0x16, 0x17,  0x00, 0x01, 0x02, 0x03,  0x10, 0x11, 0x12, 0x13};
const qword s_CcCc = {0x08, 0x09, 0x0a, 0x0b,  0x18, 0x19, 0x1a, 0x1b,  0x08, 0x09, 0x0a, 0x0b,  0x18, 0x19, 0x1a, 0x1b};
const qword s_cdAB = {0x18, 0x19, 0x1a, 0x1b,  0x1c, 0x1d, 0x1e, 0x1f,  0x00, 0x01, 0x02, 0x03,  0x04, 0x05, 0x06, 0x07};
const qword s_DDDDDDDDDDDDDDDD = {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};//(qword)spu_splats(0x03030303);
const qword signMask = {0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00};//(qword)spu_splats(0x80000000);
const qword sel_00FF = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};// si_fsm(si_from_uint(0x3));

const qword zero = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const qword fZero = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const qword fOne = {0x3f, 0x80, 0x00, 0x00, 0x3f, 0x80, 0x00, 0x00, 0x3f, 0x80, 0x00, 0x00, 0x3f, 0x80, 0x00, 0x00};//(qword)spu_splats(1.f);
const qword epsilonQw = {0x34, 0xc0, 0x00, 0x00, 0x34, 0xc0, 0x00, 0x00, 0x34, 0xc0, 0x00, 0x00, 0x34, 0xc0, 0x00, 0x00};//(qword)spu_splats(2.f * FLT_EPSILON/*EPSILON*/);
const qword maxFloatQw = {0x46, 0x1C, 0x40, 0x00, 0x46, 0x1C, 0x40, 0x00, 0x46, 0x1C, 0x40, 0x00, 0x46, 0x1C, 0x40, 0x00};
const qword s_offsetBase = {    0x08, 0x08, 0x08, 0x08,
                                0x08, 0x08, 0x08, 0x08,
                                0x18, 0x18, 0x18, 0x18,
                                0x18, 0x18, 0x18, 0x18};

void intersectRayTriangles(uint32_t numTriangles, void *pVertexes, void *pIndexes, void *ray, RayResult *pOut, float *transform)
{
    si_stqd(si_from_float(MAX_FLOAT), si_from_ptr(pOut), 0x20);

    qword pos = si_lqd(si_from_ptr(ray), 0x00);
    qword dir = si_lqd(si_from_ptr(ray), 0x10);

    qword pIndexes0 = si_from_ptr(pIndexes);

    const qword pIndexesInc = si_from_int(0x18);
    const qword pIndexesOffsetBack0 = si_from_int(-0x48); // 3*pIndexesOffsetInc
    const qword pIndexesOffsetBack1 = si_from_int(-0x38); // 3*pIndexesOffsetInc + 0x10

    qword s_offset = (qword)s_ABCD;

	const unsigned int iterCountBase = ((numTriangles + 3) >> 2);
    unsigned int iterCount = iterCountBase + 2;
	qword iterCountBC = (qword)spu_splats(iterCount);
    qword iterCountBaseBC = (qword)spu_splats(iterCountBase);
        
    qword remainder = si_andi((qword)spu_splats(numTriangles), 0x3);
    qword switch0 = si_ceqi(remainder, 0);
    qword switch1 = si_ceqi(remainder, 1);
    qword switch2 = si_ceqi(remainder, 2);
    qword switch3 = si_ceqi(remainder, 3);

    qword lastLoopMask = zero;
    lastLoopMask = si_selb(lastLoopMask, (qword)spu_splats(0), switch0);
    lastLoopMask = si_selb(lastLoopMask, si_fsm(si_from_uint(0x7)), switch1);
    lastLoopMask = si_selb(lastLoopMask, si_fsm(si_from_uint(0x3)), switch2);
    lastLoopMask = si_selb(lastLoopMask, si_fsm(si_from_uint(0x1)), switch3);

    const qword pTransVerts = si_from_ptr(pVertexes);

    qword minT = maxFloatQw;
    qword minNormal_x = fZero;
    qword minNormal_y = fZero;
    qword minNormal_z = fZero;

    qword xxxxA0 = (qword)spu_splats(0);
    qword xxxxB0 = (qword)spu_splats(0);
    qword yyyyA0 = (qword)spu_splats(0);
    qword yyyyB0 = (qword)spu_splats(0);
    qword xxxxC0 = (qword)spu_splats(0);
    qword yyyyC0 = (qword)spu_splats(0);

    qword xyz0A = (qword)spu_splats(0);
    qword xyz0B = (qword)spu_splats(0);
    qword xyz0C = (qword)spu_splats(0);
                  
    qword xyz0D = (qword)spu_splats(0);
    qword xyz0E = (qword)spu_splats(0);
    qword xyz0F = (qword)spu_splats(0);
                  
    qword xyz0G = (qword)spu_splats(0);
    qword xyz0H = (qword)spu_splats(0);
    qword xyz1A = (qword)spu_splats(0);
                  
    qword xyz1B = (qword)spu_splats(0);
    qword xyz1C = (qword)spu_splats(0);
    qword xyz1D = (qword)spu_splats(0);

    while (true) {
        bool exitLoop = (iterCount == 0);

        if (__builtin_expect(exitLoop, false)) {
            break;
        }

        iterCount--;
        iterCountBC = si_ai(iterCountBC, -1);

        qword inputQuad0 = si_lqd(pIndexes0, 0x00);
        qword inputQuad1 = si_lqd(pIndexes0, 0x10);

        pIndexes0 = si_a(pIndexes0, pIndexesInc);

                //these are the subtriangle areas using the point (0,0)
                qword a0 = si_andc(si_fms(xxxxA0, yyyyB0, si_fm(yyyyA0, xxxxB0)),signMask);
                qword a1 = si_andc(si_fms(xxxxB0, yyyyC0, si_fm(yyyyB0, xxxxC0)),signMask);
                qword a2 = si_andc(si_fms(xxxxC0, yyyyA0, si_fm(yyyyC0, xxxxA0)),signMask);
        
                //now we need the big triangle areas
                qword e0_x = si_fs(xxxxB0, xxxxA0);
                qword e0_y = si_fs(yyyyB0, yyyyA0);
        
                qword e1_x = si_fs(xxxxC0, xxxxA0);
                qword e1_y = si_fs(yyyyC0, yyyyA0);
        
                qword cross_z = si_fms(e0_x, e1_y, si_fm(e0_y, e1_x));
        
                qword areaSum = si_fa(a0, si_fa(a1, a2));
                qword error = si_fm(cross_z, epsilonQw);
        
                qword adjA = si_fa(cross_z, error);
        
                qword outOfTriangle = si_fcmgt(areaSum, adjA);
        
                qword gathered = si_gb(outOfTriangle);

            /* 0A, 0D, 0G, 1B */
            qword temp10 = si_shufb(xyz0A, xyz0D, s_AaBb);		// temp1 = x1 x2 y1 y2
            qword temp20 = si_shufb(xyz0G, xyz1B, s_BbAa); 		// temp2 = y3 y4 x3 x4
            xxxxA0 = si_selb(temp10, temp20, sel_00FF);   // out1  = x1 x2 x3 x4
            yyyyA0 = si_shufb(temp20, temp10, s_cdAB);	// out2  = y1 y2 y3 y4

            /* 0B, 0E, 0H, 1C */
            qword temp11 = si_shufb(xyz0B, xyz0E, s_AaBb);		// temp1 = x1 x2 y1 y2
            qword temp21 = si_shufb(xyz0H, xyz1C, s_BbAa); 		// temp2 = y3 y4 x3 x4
            xxxxB0 = si_selb(temp11, temp21, sel_00FF);   // out1  = x1 x2 x3 x4
            yyyyB0 = si_shufb(temp21, temp11, s_cdAB);	// out2  = y1 y2 y3 y4

            /* 0C, 0F, 1A, 1D */
            qword temp12 = si_shufb(xyz0C, xyz0F, s_AaBb);		// temp1 = x1 x2 y1 y2
            qword temp22 = si_shufb(xyz1A, xyz1D, s_BbAa); 		// temp2 = y3 y4 x3 x4
            xxxxC0 = si_selb(temp12, temp22, sel_00FF);   // out1  = x1 x2 x3 x4
            yyyyC0 = si_shufb(temp22, temp12, s_cdAB);	// out2  = y1 y2 y3 y4

        qword indexQuad0 = si_shufb(inputQuad0, inputQuad1, s_offset);
        qword indexQuad1 = si_shufb(inputQuad1, inputQuad1, s_offset);

        s_offset = si_xor(s_offset, s_offsetBase);

        qword indexOffsets0B = si_shli(indexQuad0, 4);
        qword indexOffsets1B = si_shli(indexQuad1, 4);

        qword indexOffsets0A = si_rotmi(indexOffsets0B, -16);
        qword indexOffsets1A = si_rotmi(indexOffsets1B, -16);

        qword indexOffsets0D = si_shlqbyi(indexOffsets0B, 4);
        qword indexOffsets1D = si_shlqbyi(indexOffsets1B, 4);

        qword indexOffsets0C = si_rotmi(indexOffsets0D, -16);
        qword indexOffsets1C = si_rotmi(indexOffsets1D, -16);

        qword indexOffsets0F = si_shlqbyi(indexOffsets0B, 8);
        qword indexOffsets0E = si_rotmi(indexOffsets0F, -16);

        qword indexOffsets0H = si_shlqbyi(indexOffsets0B, 12);
        qword indexOffsets0G = si_rotmi(indexOffsets0H, -16);
        
        xyz0A = si_lqx(pTransVerts, indexOffsets0A);
        xyz0B = si_lqx(pTransVerts, indexOffsets0B);
        xyz0C = si_lqx(pTransVerts, indexOffsets0C);

        xyz0D = si_lqx(pTransVerts, indexOffsets0D);
        xyz0E = si_lqx(pTransVerts, indexOffsets0E);
        xyz0F = si_lqx(pTransVerts, indexOffsets0F);

        xyz0G = si_lqx(pTransVerts, indexOffsets0G);
        xyz0H = si_lqx(pTransVerts, indexOffsets0H);
        xyz1A = si_lqx(pTransVerts, indexOffsets1A);

        xyz1B = si_lqx(pTransVerts, indexOffsets1B);
        xyz1C = si_lqx(pTransVerts, indexOffsets1C);
        xyz1D = si_lqx(pTransVerts, indexOffsets1D);

        if (__builtin_expect(si_to_uint(gathered) == 0xf, true)) {
             //all of the intersection points are outside the triangle
          continue;
        }

        //this part from here down isn't rolled, but it shouldn't be used that often b/c most triangles
        //shouldn't be hit

        //alright, we have to redo some work because we're out of sync with the rest of the loop
        qword inputQuad0Redo = si_lqx(pIndexes0, pIndexesOffsetBack0);
        qword inputQuad1Redo = si_lqx(pIndexes0, pIndexesOffsetBack1);

        qword s_offsetRedo = si_xor(s_offset, s_offsetBase);

        qword indexQuad0Redo = si_shufb(inputQuad0Redo, inputQuad1Redo, s_offsetRedo);
        qword indexQuad1Redo = si_shufb(inputQuad1Redo, inputQuad1Redo, s_offsetRedo);

        qword indexOffsets0BRedo = si_shli(indexQuad0Redo, 4);
        qword indexOffsets1BRedo = si_shli(indexQuad1Redo, 4);

        qword indexOffsets0ARedo = si_rotmi(indexOffsets0BRedo, -16);
        qword indexOffsets1ARedo = si_rotmi(indexOffsets1BRedo, -16);

        qword indexOffsets0DRedo = si_shlqbyi(indexOffsets0BRedo, 4);
        qword indexOffsets1DRedo = si_shlqbyi(indexOffsets1BRedo, 4);

        qword indexOffsets0CRedo = si_rotmi(indexOffsets0DRedo, -16);
        qword indexOffsets1CRedo = si_rotmi(indexOffsets1DRedo, -16);

        qword indexOffsets0FRedo = si_shlqbyi(indexOffsets0BRedo, 8);
        qword indexOffsets0ERedo = si_rotmi(indexOffsets0FRedo, -16);

        qword indexOffsets0HRedo = si_shlqbyi(indexOffsets0BRedo, 12);
        qword indexOffsets0GRedo = si_rotmi(indexOffsets0HRedo, -16);

        qword xyz0ARedo = si_lqx(pTransVerts, indexOffsets0ARedo);
        qword xyz0BRedo = si_lqx(pTransVerts, indexOffsets0BRedo);
        qword xyz0CRedo = si_lqx(pTransVerts, indexOffsets0CRedo);

        qword xyz0DRedo = si_lqx(pTransVerts, indexOffsets0DRedo);
        qword xyz0ERedo = si_lqx(pTransVerts, indexOffsets0ERedo);
        qword xyz0FRedo = si_lqx(pTransVerts, indexOffsets0FRedo);

        qword xyz0GRedo = si_lqx(pTransVerts, indexOffsets0GRedo);
        qword xyz0HRedo = si_lqx(pTransVerts, indexOffsets0HRedo);
        qword xyz1ARedo = si_lqx(pTransVerts, indexOffsets1ARedo);

        qword xyz1BRedo = si_lqx(pTransVerts, indexOffsets1BRedo);
        qword xyz1CRedo = si_lqx(pTransVerts, indexOffsets1CRedo);
        qword xyz1DRedo = si_lqx(pTransVerts, indexOffsets1DRedo);

        qword temp30 = si_shufb(xyz0ARedo, xyz0DRedo, s_CcCc);		// temp3 = z1 z2 z1 z2
        qword temp40 = si_shufb(xyz0GRedo, xyz1BRedo, s_CcCc); 		// temp4 = z3 z4 z3 z4
        qword zzzzA0 = si_selb(temp30, temp40, sel_00FF);   // out3  = z1 z2 z3 z4

        qword temp31 = si_shufb(xyz0BRedo, xyz0ERedo, s_CcCc);		// temp3 = z1 z2 z1 z2
        qword temp41 = si_shufb(xyz0HRedo, xyz1CRedo, s_CcCc); 		// temp4 = z3 z4 z3 z4
        qword zzzzB0 = si_selb(temp31, temp41, sel_00FF);   // out3  = z1 z2 z3 z4

        qword temp32 = si_shufb(xyz0CRedo, xyz0FRedo, s_CcCc);		// temp3 = z1 z2 z1 z2
        qword temp42 = si_shufb(xyz1ARedo, xyz1DRedo, s_CcCc); 		// temp4 = z3 z4 z3 z4
        qword zzzzC0 = si_selb(temp32, temp42, sel_00FF);   // out3  = z1 z2 z3 z4

        qword e0_z = si_fs(zzzzB0, zzzzA0);
        qword e1_z = si_fs(zzzzC0, zzzzA0);

        qword cross_x = si_fms(e0_y, e1_z, si_fm(e0_z, e1_y));
        qword cross_y = si_fms(e0_z, e1_x, si_fm(e0_x, e1_z));

        qword sumSq = si_fma(cross_z, cross_z, si_fma(cross_y, cross_y, si_fm(cross_x, cross_x)));

        qword crossMagReciprocal = si_fi(sumSq, si_frsqest(sumSq));

        qword normal_x = si_fm(cross_x, crossMagReciprocal);
        qword normal_y = si_fm(cross_y, crossMagReciprocal);
        qword normal_z = si_fm(cross_z, crossMagReciprocal);

        //okay, now we need to compute z (t)
        qword reciprocalNzTmp0 = si_fi(normal_z, si_frest(normal_z));
        qword reciprocalNzTmp1 = si_fnms(reciprocalNzTmp0, normal_z, fOne);
        qword reciprocalNz = si_fma(reciprocalNzTmp1, reciprocalNzTmp0, reciprocalNzTmp0);

        qword dZdX = si_fm(normal_x, reciprocalNz);
        qword dZdY = si_fm(normal_y, reciprocalNz);

        qword temp10Redo = si_shufb(xyz0ARedo, xyz0DRedo, s_AaBb);		// temp1 = x1 x2 y1 y2
        qword temp20Redo = si_shufb(xyz0GRedo, xyz1BRedo, s_BbAa); 		// temp2 = y3 y4 x3 x4
        qword xxxxA0Redo = si_selb(temp10Redo, temp20Redo, sel_00FF);   // out1  = x1 x2 x3 x4
        qword yyyyA0Redo = si_shufb(temp20Redo, temp10Redo, s_cdAB);	// out2  = y1 y2 y3 y4

        qword intersectionZ = si_fma(xxxxA0Redo, dZdX, si_fma(yyyyA0Redo, dZdY, zzzzA0));
        qword t = si_xor(intersectionZ, signMask);

        qword negative = si_fcgt(fZero, t);
        //mask out negative t's

        qword isLastLoop = si_ceqi(iterCountBC, 0x0);
        qword currLoopMask = si_selb(zero, lastLoopMask, isLastLoop);
        qword noHit = si_or(outOfTriangle, negative);
        qword iterCountMask = si_cgt(iterCountBaseBC, iterCountBC);

        t = si_selb(t, maxFloatQw, noHit);        
        t = si_selb(t, maxFloatQw, currLoopMask);
        t = si_selb(maxFloatQw, t, iterCountMask);

        qword cmp = si_fcgt(minT, t); 
        minT = si_selb(minT, t, cmp);

        minNormal_x = si_selb(minNormal_x, normal_x, cmp);
        minNormal_y = si_selb(minNormal_y, normal_y, cmp);
        minNormal_z = si_selb(minNormal_z, normal_z, cmp);
    }

    //ok, we've got 4 t's and 4 normals
    //find the minimum one then untranspose
    qword minTrot1 = si_rotqbyi(minT, 4);
    qword minTrot2 = si_rotqbyi(minT, 8);
    qword minTrot3 = si_rotqbyi(minT, 12);

    qword minTtmp0 = si_xori(si_fcgt(minT, minTrot1),-1);
    qword minTtmp1 = si_xori(si_fcgt(minT, minTrot2),-1);
    qword minTtmp2 = si_xori(si_fcgt(minT, minTrot3),-1);

    qword res = si_and(minTtmp0, si_and(minTtmp1, minTtmp2));

    qword tBits = si_gb(res);
    qword offset = si_shli(si_ai(si_clz(tBits),-28), 2);
    qword offsetBC = si_shufb(offset, offset, s_DDDDDDDDDDDDDDDD);

    const qword s_tBase = s_AAAA;

    qword s_t = si_a(s_tBase, offsetBC);
    qword tttt = si_shufb(minT, minT, s_t);

    const qword s_nxyBase = {   0x00, 0x01, 0x02, 0x03, 
                                0x10, 0x11, 0x12, 0x13, 
                                0x80, 0x80, 0x80, 0x80, 
                                0x80, 0x80, 0x80, 0x80};

    qword s_nxy = si_a(s_nxyBase, offsetBC);
    qword nxy = si_shufb(minNormal_x, minNormal_y, s_nxy);

    const qword s_nzBase = {    0x80, 0x80, 0x80, 0x80, 
                                0x80, 0x80, 0x80, 0x80, 
                                0x00, 0x01, 0x02, 0x03, 
                                0x80, 0x80, 0x80, 0x80};

    qword s_nz = si_a(s_nzBase, offsetBC);
    qword nz = si_shufb(minNormal_z, minNormal_z, s_nz);

    qword n = si_selb(nxy, nz, sel_00FF);

    // so in theory, transform is a rotation matrix + a translation, so the inverse should just be the transpose
    float inverse[9] = {transform[0], transform[4], transform[8],
                        transform[1], transform[5], transform[9],
                        transform[2], transform[6], transform[10]};

    float *minNormal = (float*)&n;
    //transform minNormal by Inverse
    float transNormal[3] = {inverse[0] * minNormal[0] + inverse[1] * minNormal[1] + inverse[2] * minNormal[2],
                            inverse[3] * minNormal[0] + inverse[4] * minNormal[1] + inverse[5] * minNormal[2],
                            inverse[6] * minNormal[0] + inverse[7] * minNormal[1] + inverse[8] * minNormal[2]};

    qword i = si_fma(dir, tttt, pos);    

    si_stqd(i, si_from_ptr(pOut), 0x00);

    pOut->normal[0] = transNormal[0];
    pOut->normal[1] = transNormal[1];
    pOut->normal[2] = transNormal[2];
    pOut->normal[3] = 0.f;

    si_stqd(tttt, si_from_ptr(pOut), 0x20);
}

#if 0
void intersectRayTrianglesUnrolled(uint32_t numTriangles, void *pVertexes, void *pIndexes, void *ray, RayResult *pOut, float *transform)
{
    si_stqd(si_from_float(MAX_FLOAT), si_from_ptr(pOut), 0x20);

    qword pos = si_lqd(si_from_ptr(ray), 0x00);
    qword dir = si_lqd(si_from_ptr(ray), 0x10);

    const qword pIndexes0 = si_from_ptr(pIndexes);
    const qword pIndexes1 = si_ai(pIndexes0, 0x10);
    qword pIndexesOffset = (qword)si_from_int(0);
	qword pIndexesOffsetInc = si_from_int(0x18);

    qword s_offset = (qword)s_ABCD;
	unsigned int iterCount = ((numTriangles + 3) >> 2);
	qword iterCountBC = (qword)spu_splats(iterCount);

    qword remainder = si_andi((qword)spu_splats(numTriangles), 0x3);
    qword switch0 = si_ceqi(remainder, 0);
    qword switch1 = si_ceqi(remainder, 1);
    qword switch2 = si_ceqi(remainder, 2);
    qword switch3 = si_ceqi(remainder, 3);

    qword lastLoopMask = zero;
    lastLoopMask = si_selb(lastLoopMask, (qword)spu_splats(0), switch0);
    lastLoopMask = si_selb(lastLoopMask, si_fsm(si_from_uint(0x7)), switch1);
    lastLoopMask = si_selb(lastLoopMask, si_fsm(si_from_uint(0x3)), switch2);
    lastLoopMask = si_selb(lastLoopMask, si_fsm(si_from_uint(0x1)), switch3);

    const qword pTransVerts = si_from_ptr(pVertexes);

    qword minT = maxFloatQw;
    qword minNormal_x = fZero;
    qword minNormal_y = fZero;
    qword minNormal_z = fZero;

    while (true) {
        if (si_to_uint(iterCountBC) == 0) {
            break;
        }

        iterCountBC = si_ai(iterCountBC, -1);

        qword inputQuad0 = si_lqx(pIndexes0, pIndexesOffset);
        qword inputQuad1 = si_lqx(pIndexes1, pIndexesOffset);

        pIndexesOffset = si_a(pIndexesOffset, pIndexesOffsetInc);

        qword indexQuad0 = si_shufb(inputQuad0, inputQuad1, s_offset);
        qword indexQuad1 = si_shufb(inputQuad1, inputQuad1, s_offset);

        s_offset = si_xor(s_offset, s_offsetBase);

        qword indexOffsets0B = si_shli(indexQuad0, 4);
        qword indexOffsets1B = si_shli(indexQuad1, 4);

        qword indexOffsets0A = si_rotmi(indexOffsets0B, -16);
        qword indexOffsets1A = si_rotmi(indexOffsets1B, -16);

        qword indexOffsets0D = si_shlqbyi(indexOffsets0B, 4);
        qword indexOffsets1D = si_shlqbyi(indexOffsets1B, 4);

        qword indexOffsets0C = si_rotmi(indexOffsets0D, -16);
        qword indexOffsets1C = si_rotmi(indexOffsets1D, -16);

        qword indexOffsets0F = si_shlqbyi(indexOffsets0B, 8);
        qword indexOffsets0E = si_rotmi(indexOffsets0F, -16);

        qword indexOffsets0H = si_shlqbyi(indexOffsets0B, 12);
        qword indexOffsets0G = si_rotmi(indexOffsets0H, -16);
        
        qword xyz0A = si_lqx(pTransVerts, indexOffsets0A);
        qword xyz0B = si_lqx(pTransVerts, indexOffsets0B);
        qword xyz0C = si_lqx(pTransVerts, indexOffsets0C);

        qword xyz0D = si_lqx(pTransVerts, indexOffsets0D);
        qword xyz0E = si_lqx(pTransVerts, indexOffsets0E);
        qword xyz0F = si_lqx(pTransVerts, indexOffsets0F);

        qword xyz0G = si_lqx(pTransVerts, indexOffsets0G);
        qword xyz0H = si_lqx(pTransVerts, indexOffsets0H);
        qword xyz1A = si_lqx(pTransVerts, indexOffsets1A);

        qword xyz1B = si_lqx(pTransVerts, indexOffsets1B);
        qword xyz1C = si_lqx(pTransVerts, indexOffsets1C);
        qword xyz1D = si_lqx(pTransVerts, indexOffsets1D);

        /* 0A, 0D, 0G, 1B */
    	qword temp10 = si_shufb(xyz0A, xyz0D, s_AaBb);		// temp1 = x1 x2 y1 y2
    	qword temp20 = si_shufb(xyz0G, xyz1B, s_BbAa); 		// temp2 = y3 y4 x3 x4
    	qword xxxxA0 = si_selb(temp10, temp20, sel_00FF);   // out1  = x1 x2 x3 x4
    	qword yyyyA0 = si_shufb(temp20, temp10, s_cdAB);	// out2  = y1 y2 y3 y4

        /* 0B, 0E, 0H, 1C */
        qword temp11 = si_shufb(xyz0B, xyz0E, s_AaBb);		// temp1 = x1 x2 y1 y2
        qword temp21 = si_shufb(xyz0H, xyz1C, s_BbAa); 		// temp2 = y3 y4 x3 x4
        qword xxxxB0 = si_selb(temp11, temp21, sel_00FF);   // out1  = x1 x2 x3 x4
        qword yyyyB0 = si_shufb(temp21, temp11, s_cdAB);	// out2  = y1 y2 y3 y4

        /* 0C, 0F, 1A, 1D */
        qword temp12 = si_shufb(xyz0C, xyz0F, s_AaBb);		// temp1 = x1 x2 y1 y2
        qword temp22 = si_shufb(xyz1A, xyz1D, s_BbAa); 		// temp2 = y3 y4 x3 x4
        qword xxxxC0 = si_selb(temp12, temp22, sel_00FF);   // out1  = x1 x2 x3 x4
        qword yyyyC0 = si_shufb(temp22, temp12, s_cdAB);	// out2  = y1 y2 y3 y4

        //OK, we've got 4 triangles.

        //these are the subtriangle areas using the point (0,0)
        qword a0 = si_andc(si_fms(xxxxA0, yyyyB0, si_fm(yyyyA0, xxxxB0)),signMask);
        qword a1 = si_andc(si_fms(xxxxB0, yyyyC0, si_fm(yyyyB0, xxxxC0)),signMask);
        qword a2 = si_andc(si_fms(xxxxC0, yyyyA0, si_fm(yyyyC0, xxxxA0)),signMask);

        //now we need the big triangle areas
        qword e0_x = si_fs(xxxxB0, xxxxA0);
        qword e0_y = si_fs(yyyyB0, yyyyA0);
        
        qword e1_x = si_fs(xxxxC0, xxxxA0);
        qword e1_y = si_fs(yyyyC0, yyyyA0);

        qword cross_z = si_fms(e0_x, e1_y, si_fm(e0_y, e1_x));

        qword areaSum = si_fa(a0, si_fa(a1, a2));
        qword error = si_fm(cross_z, epsilonQw);

        qword adjA = si_fa(cross_z, error);

        qword outOfTriangle = si_fcmgt(areaSum, adjA);

        qword gathered = si_gb(outOfTriangle);

        if (si_to_uint(gathered) == 0xf) {
             //all of the intersection points are outside the triangle
          continue;
        }

        qword temp30 = si_shufb(xyz0A, xyz0D, s_CcCc);		// temp3 = z1 z2 z1 z2
        qword temp40 = si_shufb(xyz0G, xyz1B, s_CcCc); 		// temp4 = z3 z4 z3 z4
        qword zzzzA0 = si_selb(temp30, temp40, sel_00FF);   // out3  = z1 z2 z3 z4

        qword temp31 = si_shufb(xyz0B, xyz0E, s_CcCc);		// temp3 = z1 z2 z1 z2
        qword temp41 = si_shufb(xyz0H, xyz1C, s_CcCc); 		// temp4 = z3 z4 z3 z4
        qword zzzzB0 = si_selb(temp31, temp41, sel_00FF);   // out3  = z1 z2 z3 z4

        qword temp32 = si_shufb(xyz0C, xyz0F, s_CcCc);		// temp3 = z1 z2 z1 z2
        qword temp42 = si_shufb(xyz1A, xyz1D, s_CcCc); 		// temp4 = z3 z4 z3 z4
        qword zzzzC0 = si_selb(temp32, temp42, sel_00FF);   // out3  = z1 z2 z3 z4

        qword e0_z = si_fs(zzzzB0, zzzzA0);
        qword e1_z = si_fs(zzzzC0, zzzzA0);

        qword cross_x = si_fms(e0_y, e1_z, si_fm(e0_z, e1_y));
        qword cross_y = si_fms(e0_z, e1_x, si_fm(e0_x, e1_z));

        qword sumSq = si_fma(cross_z, cross_z, si_fma(cross_y, cross_y, si_fm(cross_x, cross_x)));

        qword crossMagReciprocal = si_fi(sumSq, si_frsqest(sumSq));

        qword normal_x = si_fm(cross_x, crossMagReciprocal);
        qword normal_y = si_fm(cross_y, crossMagReciprocal);
        qword normal_z = si_fm(cross_z, crossMagReciprocal);

        //okay, now we need to compute z (t)
        qword reciprocalNzTmp0 = si_fi(normal_z, si_frest(normal_z));
        qword reciprocalNzTmp1 = si_fnms(reciprocalNzTmp0, normal_z, fOne);
        qword reciprocalNz = si_fma(reciprocalNzTmp1, reciprocalNzTmp0, reciprocalNzTmp0);

        qword dZdX = si_fm(normal_x, reciprocalNz);
        qword dZdY = si_fm(normal_y, reciprocalNz);

        qword intersectionZ = si_fma(xxxxA0, dZdX, si_fma(yyyyA0, dZdY, zzzzA0));
        qword t = si_xor(intersectionZ, signMask);

        qword negative = si_fcgt(fZero, t);
        //mask out negative t's

        qword isLastLoop = si_ceqi(iterCountBC, 0x0);
        qword currLoopMask = si_selb(zero, lastLoopMask, isLastLoop);
        qword noHit = si_or(outOfTriangle, negative);

        t = si_selb(t, maxFloatQw, noHit);        
        
        t = si_selb(t, maxFloatQw, currLoopMask);

        qword cmp = si_fcgt(minT, t); 
        minT = si_selb(minT, t, cmp);

        minNormal_x = si_selb(minNormal_x, normal_x, cmp);
        minNormal_y = si_selb(minNormal_y, normal_y, cmp);
        minNormal_z = si_selb(minNormal_z, normal_z, cmp);
    }

    //ok, we've got 4 t's and 4 normals
    //find the minimum one then untranspose
    qword minTrot1 = si_rotqbyi(minT, 4);
    qword minTrot2 = si_rotqbyi(minT, 8);
    qword minTrot3 = si_rotqbyi(minT, 12);

    qword minTtmp0 = si_xori(si_fcgt(minT, minTrot1),-1);
    qword minTtmp1 = si_xori(si_fcgt(minT, minTrot2),-1);
    qword minTtmp2 = si_xori(si_fcgt(minT, minTrot3),-1);

    qword res = si_and(minTtmp0, si_and(minTtmp1, minTtmp2));

    qword tBits = si_gb(res);
    qword offset = si_shli(si_ai(si_clz(tBits),-28), 2);
    qword offsetBC = si_shufb(offset, offset, s_DDDDDDDDDDDDDDDD);

    const qword s_tBase = s_AAAA;

    qword s_t = si_a(s_tBase, offsetBC);
    qword tttt = si_shufb(minT, minT, s_t);

    const qword s_nxyBase = {   0x00, 0x01, 0x02, 0x03, 
                                0x10, 0x11, 0x12, 0x13, 
                                0x80, 0x80, 0x80, 0x80, 
                                0x80, 0x80, 0x80, 0x80};

    qword s_nxy = si_a(s_nxyBase, offsetBC);
    qword nxy = si_shufb(minNormal_x, minNormal_y, s_nxy);

    const qword s_nzBase = {    0x80, 0x80, 0x80, 0x80, 
                                0x80, 0x80, 0x80, 0x80, 
                                0x00, 0x01, 0x02, 0x03, 
                                0x80, 0x80, 0x80, 0x80};

    qword s_nz = si_a(s_nzBase, offsetBC);
    qword nz = si_shufb(minNormal_z, minNormal_z, s_nz);

    qword n = si_selb(nxy, nz, sel_00FF);

    // so in theory, transform is a rotation matrix + a translation, so the inverse should just be the transpose
    float inverse[9] = {transform[0], transform[4], transform[8],
                        transform[1], transform[5], transform[9],
                        transform[2], transform[6], transform[10]};

    float *minNormal = (float*)&n;
    //transform minNormal by Inverse
    float transNormal[3] = {inverse[0] * minNormal[0] + inverse[1] * minNormal[1] + inverse[2] * minNormal[2],
                            inverse[3] * minNormal[0] + inverse[4] * minNormal[1] + inverse[5] * minNormal[2],
                            inverse[6] * minNormal[0] + inverse[7] * minNormal[1] + inverse[8] * minNormal[2]};

    qword i = si_fma(dir, tttt, pos);    

    si_stqd(i, si_from_ptr(pOut), 0x00);

    pOut->normal[0] = transNormal[0];
    pOut->normal[1] = transNormal[1];
    pOut->normal[2] = transNormal[2];
    pOut->normal[3] = 0.f;

    si_stqd(tttt, si_from_ptr(pOut), 0x20);
}

void intersectRayTrianglesC(uint32_t numTriangles, void *pVertexes, void *pIndexes, void *ray, RayResult *pOut, float *transform)
{
    pOut->t = MAX_FLOAT;
    float minT = MAX_FLOAT;
    float minNormal[] = {0.f, 0.f, 0.f};

    for (uint32_t i = 0; i < numTriangles; i++) {
        uint16_t *triangle = ((uint16_t *)pIndexes) + 3*i;

        float *v0 = ((float *)pVertexes) + 4*triangle[0];
        float *v1 = ((float *)pVertexes) + 4*triangle[1];
        float *v2 = ((float *)pVertexes) + 4*triangle[2];

        //v0 x v1
        float a0 = v0[0] * v1[1] - v0[1] * v1[0];
        a0 = (a0 < 0.f) ? -a0 : a0;
        //v1 x v2
        float a1 = v1[0] * v2[1] - v1[1] * v2[0];
        a1 = (a1 < 0.f) ? -a1 : a1;
        //v2 x v0
        float a2 = v2[0] * v0[1] - v2[1] * v0[0];
        a2 = (a2 < 0.f) ? -a2 : a2;

        float e0[] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
        float e1[] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};

        //we need the normal
        float cross[] = {   e0[1] * e1[2] - e0[2] * e1[1],
                            e0[2] * e1[0] - e0[0] * e1[2],
                            e0[0] * e1[1] - e0[1] * e1[0]};
        float tmp = (cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

        float crossMagEst = si_to_float(si_frsqest(si_from_float(tmp)));
        float crossMagReciprocal = si_to_float(si_fi(si_from_float(tmp), si_from_float(crossMagEst)));

        float normal[] = {cross[0] * crossMagReciprocal, cross[1] * crossMagReciprocal, cross[2] * crossMagReciprocal};

        float A = cross[2];
        A = (A < 0.f) ? -A : A;

        if (a0 + a1 + a2 > A + (EPSILON * A)) {
            continue;
        }

        //compute Z

        //float reciprocalNz = 1.f/normal[2];
        qword reciprocalNzTmp0 = si_fi(si_from_float(normal[2]), si_frest(si_from_float(normal[2])));
        qword reciprocalNzTmp1 = si_fnms(reciprocalNzTmp0, si_from_float(normal[2]), (qword)spu_splats(1.f));
        float reciprocalNz = si_to_float(si_fma(reciprocalNzTmp1, reciprocalNzTmp0, reciprocalNzTmp0));

        float dZdX = normal[0] * reciprocalNz;
        float dZdY = normal[1] * reciprocalNz;

        float intersectionZ = v0[0] * dZdX + v0[1] * dZdY + v0[2];
        float t = -intersectionZ;

        if (t < 0.f) {
            continue;
        }

        if (t < minT) { // this normal is, of course, in the transformed space
            minT = t;
            minNormal[0] = normal[0];
            minNormal[1] = normal[1];
            minNormal[2] = normal[2];
        }
    }

    if (minT == MAX_FLOAT) {
        return;
    }

    //fill out pOut
    float *rayPosition = (float*)ray;
    float *rayDirection = ((float*)ray) + 4;

    float intersection[] = {rayPosition[0] + rayDirection[0] * minT,
                            rayPosition[1] + rayDirection[1] * minT,
                            rayPosition[2] + rayDirection[2] * minT};


    // so in theory, transform is a rotation matrix + a translation, so the inverse should just be the transpose
    float inverse[9] = {transform[0], transform[4], transform[8],
                        transform[1], transform[5], transform[9],
                        transform[2], transform[6], transform[10]};

    //transform minNormal by Inverse
    float transNormal[3] = {inverse[0] * minNormal[0] + inverse[1] * minNormal[1] + inverse[2] * minNormal[2],
                            inverse[3] * minNormal[0] + inverse[4] * minNormal[1] + inverse[5] * minNormal[2],
                            inverse[6] * minNormal[0] + inverse[7] * minNormal[1] + inverse[8] * minNormal[2]};


    pOut->intersection[0] = intersection[0];
    pOut->intersection[1] = intersection[1];
    pOut->intersection[2] = intersection[2];
    pOut->intersection[3] = 1.f;

    pOut->normal[0] = transNormal[0];
    pOut->normal[1] = transNormal[1];
    pOut->normal[2] = transNormal[2];
    pOut->normal[3] = 0.f;

    pOut->t = minT;
}
#endif

