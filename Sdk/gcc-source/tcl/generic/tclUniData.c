/*
 * tclUtfData.c --
 *
 *	Declarations of Unicode character information tables.  This file is
 *	automatically generated by the tools/uniParse.tcl script.  Do not
 *	modify this file by hand.
 *
 * Copyright (c) 1998 by Scriptics Corporation.
 * All rights reserved.
 *
 * RCS: @(#) $Id: tclUniData.c,v 1.3.8.1 2000/04/06 22:38:29 spolk Exp $
 */

/*
 * A 16-bit Unicode character is split into two parts in order to index
 * into the following tables.  The lower OFFSET_BITS comprise an offset
 * into a page of characters.  The upper bits comprise the page number.
 */

#define OFFSET_BITS 5

/*
 * The pageMap is indexed by page number and returns an alternate page number
 * that identifies a unique page of characters.  Many Unicode characters map
 * to the same alternate page number.
 */

static unsigned char pageMap[] = {
    0, 1, 2, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 
    19, 20, 21, 22, 23, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 7, 33, 
    7, 34, 35, 16, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 
    49, 50, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 
    55, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 77, 78, 
    81, 82, 77, 16, 16, 16, 16, 83, 84, 85, 16, 86, 87, 88, 16, 89, 90, 
    91, 92, 93, 94, 16, 16, 16, 16, 16, 16, 16, 95, 96, 97, 47, 47, 98, 
    47, 47, 99, 47, 100, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 7, 
    7, 7, 7, 101, 7, 7, 102, 103, 104, 105, 106, 104, 107, 108, 109, 110, 
    111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 
    125, 126, 126, 126, 126, 126, 126, 126, 127, 128, 129, 123, 130, 16, 
    16, 16, 16, 123, 131, 125, 132, 133, 134, 135, 136, 123, 123, 123, 
    123, 137, 123, 123, 138, 139, 123, 123, 138, 16, 16, 16, 16, 140, 141, 
    142, 143, 144, 145, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 146, 147, 83, 47, 148, 83, 47, 149, 150, 151, 47, 47, 152, 
    16, 16, 16, 153, 154, 155, 156, 154, 157, 158, 159, 123, 123, 123, 
    160, 123, 123, 161, 159, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    162, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 163, 16, 16, 164, 164, 164, 164, 164, 164, 
    164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 
    164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 
    164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 
    164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 
    164, 164, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 47, 47, 47, 47, 47, 47, 47, 47, 47, 166, 
    16, 16, 16, 16, 16, 16, 167, 168, 169, 47, 47, 170, 171, 47, 47, 47, 
    47, 47, 47, 47, 47, 47, 47, 172, 173, 47, 174, 47, 175, 176, 16, 177, 
    178, 179, 47, 47, 47, 180, 181, 2, 182, 183, 184, 185, 186, 187
};

/*
 * The groupMap is indexed by combining the alternate page number with
 * the page offset and returns a group number that identifies a unique
 * set of character attributes.
 */

static unsigned char groupMap[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 4, 3, 3, 3, 5, 6, 3, 7, 3, 8, 
    3, 3, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 3, 7, 7, 7, 3, 3, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 5, 3, 6, 11, 12, 11, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 5, 7, 6, 7, 1, 2, 3, 4, 4, 4, 4, 14, 14, 11, 14, 15, 16, 
    7, 8, 14, 11, 14, 7, 17, 17, 11, 15, 14, 3, 11, 17, 15, 18, 17, 17, 
    17, 3, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 7, 10, 10, 10, 10, 10, 10, 10, 15, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 7, 13, 13, 13, 13, 13, 13, 13, 19, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 22, 23, 20, 21, 20, 
    21, 20, 21, 15, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 15, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 24, 
    20, 21, 20, 21, 20, 21, 25, 15, 26, 20, 21, 20, 21, 27, 20, 21, 28, 
    28, 20, 21, 15, 29, 30, 31, 20, 21, 28, 32, 15, 33, 34, 20, 21, 15, 
    15, 33, 35, 15, 36, 20, 21, 20, 21, 20, 21, 37, 20, 21, 37, 38, 15, 
    20, 21, 37, 20, 21, 39, 39, 20, 21, 20, 21, 40, 20, 21, 15, 38, 20, 
    21, 38, 38, 38, 38, 38, 38, 41, 42, 43, 41, 42, 43, 41, 42, 43, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 44, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    15, 41, 42, 43, 20, 21, 0, 0, 0, 0, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 15, 15, 45, 
    46, 15, 47, 47, 15, 48, 15, 49, 15, 15, 15, 15, 47, 15, 15, 50, 15, 
    15, 15, 15, 51, 52, 15, 15, 15, 15, 15, 52, 15, 15, 53, 15, 15, 54, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 55, 15, 15, 55, 15, 15, 15, 
    15, 55, 15, 56, 56, 15, 15, 15, 15, 15, 15, 57, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    0, 0, 0, 0, 0, 0, 0, 58, 58, 58, 58, 58, 58, 58, 58, 58, 11, 11, 58, 
    58, 58, 58, 58, 58, 58, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 
    11, 11, 11, 58, 58, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 
    11, 0, 58, 58, 58, 58, 58, 11, 11, 11, 11, 11, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 59, 59, 59, 59, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 60, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    59, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 11, 
    0, 0, 0, 0, 58, 0, 0, 0, 3, 0, 0, 0, 0, 0, 11, 11, 61, 3, 62, 62, 62, 
    0, 63, 0, 64, 64, 15, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 65, 66, 
    66, 66, 15, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 67, 13, 13, 13, 13, 13, 13, 13, 13, 13, 68, 69, 69, 0, 
    70, 71, 72, 72, 72, 73, 74, 0, 0, 0, 72, 0, 72, 0, 72, 0, 72, 0, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 75, 76, 44, 38, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 77, 77, 77, 77, 77, 77, 77, 
    77, 77, 77, 77, 77, 0, 77, 77, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    10, 10, 10, 10, 10, 10, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 0, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 
    0, 76, 76, 20, 21, 14, 59, 59, 59, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 38, 20, 
    21, 20, 21, 0, 0, 20, 21, 0, 0, 20, 21, 0, 0, 0, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 0, 0, 20, 21, 20, 21, 20, 21, 20, 21, 0, 0, 
    20, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 
    78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 
    78, 78, 78, 78, 78, 78, 0, 0, 58, 3, 3, 3, 3, 3, 3, 0, 79, 79, 79, 
    79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 
    79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 
    79, 15, 0, 3, 0, 0, 0, 0, 0, 0, 0, 59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 59, 0, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 0, 
    59, 59, 59, 3, 59, 3, 59, 59, 3, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 0, 38, 38, 
    38, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 
    3, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 0, 58, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 59, 59, 59, 59, 59, 59, 59, 59, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 
    3, 3, 3, 0, 0, 59, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 38, 38, 38, 38, 38, 0, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 
    38, 3, 38, 59, 59, 59, 59, 59, 59, 59, 80, 80, 59, 59, 59, 59, 59, 
    59, 58, 58, 59, 59, 14, 59, 59, 59, 59, 0, 0, 9, 9, 9, 9, 9, 9, 9, 
    9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 59, 59, 81, 0, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 59, 38, 81, 
    81, 81, 59, 59, 59, 59, 59, 59, 59, 59, 81, 81, 81, 81, 59, 0, 0, 38, 
    59, 59, 59, 59, 0, 0, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 59, 
    59, 3, 3, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 59, 81, 81, 0, 38, 38, 38, 38, 38, 38, 38, 
    38, 0, 0, 38, 38, 0, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 38, 38, 
    38, 38, 0, 38, 0, 0, 0, 38, 38, 38, 38, 0, 0, 59, 0, 81, 81, 81, 59, 
    59, 59, 59, 0, 0, 81, 81, 0, 0, 81, 81, 59, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 81, 0, 0, 0, 0, 38, 38, 0, 38, 38, 38, 59, 59, 0, 0, 9, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 38, 38, 4, 4, 17, 17, 17, 17, 17, 17, 14, 0, 0, 0, 
    0, 0, 0, 0, 59, 0, 0, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 38, 38, 0, 
    0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 0, 
    38, 38, 0, 38, 38, 0, 0, 59, 0, 81, 81, 81, 59, 59, 0, 0, 0, 0, 59, 
    59, 0, 0, 59, 59, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 38, 38, 
    38, 0, 38, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 59, 59, 
    38, 38, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 59, 59, 81, 0, 38, 
    38, 38, 38, 38, 38, 38, 0, 38, 0, 38, 38, 38, 0, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 0, 38, 38, 38, 38, 38, 0, 
    0, 59, 38, 81, 81, 81, 59, 59, 59, 59, 59, 0, 59, 59, 81, 0, 81, 81, 
    59, 0, 0, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 0, 0, 
    0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 
    38, 38, 38, 38, 0, 38, 38, 0, 0, 38, 38, 38, 38, 0, 0, 59, 38, 81, 
    59, 81, 59, 59, 59, 0, 0, 0, 81, 81, 0, 0, 81, 81, 59, 0, 0, 0, 0, 
    0, 0, 0, 0, 59, 81, 0, 0, 0, 0, 38, 38, 0, 38, 38, 38, 0, 0, 0, 0, 
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 59, 81, 0, 38, 38, 38, 38, 38, 38, 0, 0, 0, 38, 38, 
    38, 0, 38, 38, 38, 38, 0, 0, 0, 38, 38, 0, 38, 0, 38, 38, 0, 0, 0, 
    38, 38, 0, 0, 0, 38, 38, 38, 0, 0, 0, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 38, 38, 38, 0, 0, 0, 0, 81, 81, 59, 81, 81, 0, 0, 0, 81, 81, 81, 
    0, 81, 81, 81, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 17, 17, 17, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 81, 81, 0, 38, 38, 38, 38, 
    38, 38, 38, 38, 0, 38, 38, 38, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 38, 38, 0, 0, 0, 0, 
    59, 59, 59, 81, 81, 81, 81, 0, 59, 59, 59, 0, 59, 59, 59, 59, 0, 0, 
    0, 0, 0, 0, 0, 59, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 38, 0, 0, 0, 
    0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 81, 81, 0, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 
    38, 38, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 0, 38, 38, 38, 38, 38, 0, 0, 0, 0, 81, 59, 81, 81, 81, 
    81, 81, 0, 59, 81, 81, 0, 81, 81, 59, 59, 0, 0, 0, 0, 0, 0, 0, 81, 
    81, 0, 0, 0, 0, 0, 0, 0, 38, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 0, 0, 0, 81, 81, 81, 59, 59, 59, 0, 0, 81, 81, 81, 0, 81, 81, 81, 
    59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 59, 38, 38, 59, 59, 
    59, 59, 59, 59, 59, 0, 0, 0, 0, 4, 38, 38, 38, 38, 38, 38, 58, 59, 
    59, 59, 59, 59, 59, 59, 59, 14, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 3, 
    0, 0, 0, 0, 0, 38, 38, 0, 38, 0, 0, 38, 38, 0, 38, 0, 0, 38, 0, 0, 
    0, 0, 0, 0, 38, 38, 38, 38, 0, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 
    38, 0, 38, 0, 38, 0, 0, 38, 38, 0, 38, 38, 38, 38, 59, 38, 38, 59, 
    59, 59, 59, 59, 59, 0, 59, 59, 38, 0, 0, 38, 38, 38, 38, 38, 0, 58, 
    0, 59, 59, 59, 59, 59, 59, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 
    38, 38, 0, 0, 38, 14, 14, 14, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 14, 14, 14, 14, 14, 59, 59, 14, 14, 14, 14, 14, 14, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 9, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 14, 59, 
    14, 59, 14, 59, 5, 6, 5, 6, 81, 81, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 0, 0, 0, 0, 0, 0, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 81, 59, 59, 59, 59, 59, 3, 59, 59, 38, 38, 38, 38, 0, 0, 0, 
    0, 59, 59, 59, 59, 59, 59, 0, 59, 0, 59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 0, 0, 0, 59, 59, 
    59, 59, 59, 59, 59, 0, 59, 0, 0, 0, 0, 0, 0, 78, 78, 78, 78, 78, 78, 
    78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 
    78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 0, 0, 0, 0, 3, 0, 0, 0, 0, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 0, 38, 38, 38, 38, 
    0, 0, 0, 0, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 0, 0, 0, 0, 0, 0, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 
    20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 15, 15, 15, 15, 15, 
    82, 0, 0, 0, 0, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 
    21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 0, 0, 0, 0, 0, 
    0, 83, 83, 83, 83, 83, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 84, 
    83, 83, 83, 83, 83, 83, 0, 0, 84, 84, 84, 84, 84, 84, 0, 0, 83, 83, 
    83, 83, 83, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 
    83, 83, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 83, 
    83, 83, 0, 0, 84, 84, 84, 84, 84, 84, 0, 0, 15, 83, 15, 83, 15, 83, 
    15, 83, 0, 84, 0, 84, 0, 84, 0, 84, 83, 83, 83, 83, 83, 83, 83, 83, 
    84, 84, 84, 84, 84, 84, 84, 84, 85, 85, 86, 86, 86, 86, 87, 87, 88, 
    88, 89, 89, 90, 90, 0, 0, 83, 83, 83, 83, 83, 83, 83, 83, 84, 84, 84, 
    84, 84, 84, 84, 84, 83, 83, 15, 91, 15, 0, 15, 15, 84, 84, 92, 92, 
    93, 11, 94, 11, 11, 11, 15, 91, 15, 0, 15, 15, 95, 95, 95, 95, 93, 
    11, 11, 11, 83, 83, 15, 15, 0, 0, 15, 15, 84, 84, 96, 96, 0, 11, 11, 
    11, 83, 83, 15, 15, 15, 97, 15, 15, 84, 84, 98, 98, 99, 11, 11, 11, 
    0, 0, 15, 91, 15, 0, 15, 15, 100, 100, 101, 101, 93, 11, 11, 0, 2, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 102, 102, 102, 102, 8, 8, 8, 8, 8, 
    8, 3, 3, 16, 18, 5, 16, 16, 18, 5, 16, 3, 3, 3, 3, 3, 3, 3, 3, 103, 
    104, 102, 102, 102, 102, 102, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 16, 18, 
    3, 3, 3, 3, 12, 12, 3, 3, 3, 7, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 102, 102, 102, 102, 102, 102, 17, 0, 0, 0, 17, 17, 17, 17, 17, 
    17, 7, 7, 7, 5, 6, 15, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 7, 7, 
    7, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 80, 80, 80, 80, 59, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 14, 14, 72, 14, 14, 14, 14, 72, 14, 14, 15, 72, 
    72, 72, 15, 15, 72, 72, 72, 15, 14, 72, 14, 14, 15, 72, 72, 72, 72, 
    72, 14, 14, 14, 14, 14, 14, 72, 14, 72, 14, 72, 14, 72, 72, 72, 72, 
    15, 15, 72, 72, 14, 72, 15, 38, 38, 38, 38, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 105, 105, 105, 105, 105, 105, 105, 
    105, 105, 105, 105, 105, 105, 105, 105, 105, 106, 106, 106, 106, 106, 
    106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 107, 107, 107, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 7, 14, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0, 14, 14, 14, 14, 14, 14, 7, 
    7, 7, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 7, 7, 14, 14, 14, 14, 14, 14, 14, 5, 6, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 14, 
    14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 108, 108, 108, 108, 108, 108, 108, 
    108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 
    108, 108, 108, 108, 108, 109, 109, 109, 109, 109, 109, 109, 109, 109, 
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 
    109, 109, 109, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 
    0, 14, 14, 14, 14, 14, 14, 0, 14, 14, 14, 14, 0, 14, 14, 14, 14, 0, 
    0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 14, 0, 14, 
    14, 14, 14, 0, 0, 0, 14, 0, 14, 14, 14, 14, 14, 14, 14, 0, 0, 14, 14, 
    14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 14, 0, 0, 0, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 0, 2, 3, 3, 3, 14, 58, 38, 107, 5, 6, 5, 6, 5, 6, 5, 6, 5, 
    6, 14, 14, 5, 6, 5, 6, 5, 6, 5, 6, 8, 5, 6, 6, 14, 107, 107, 107, 107, 
    107, 107, 107, 107, 107, 59, 59, 59, 59, 59, 59, 8, 58, 58, 58, 58, 
    58, 14, 14, 0, 0, 0, 0, 0, 0, 0, 14, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 59, 
    59, 11, 11, 58, 58, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 12, 
    58, 58, 58, 0, 0, 0, 0, 0, 0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 14, 14, 17, 17, 
    17, 17, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 0, 0, 38, 38, 38, 38, 38, 
    38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 38, 38, 38, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 110, 110, 110, 110, 
    110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 
    110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 
    111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 
    111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 
    111, 111, 111, 111, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 15, 
    15, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 15, 15, 
    15, 15, 0, 0, 0, 0, 0, 0, 59, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    7, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 38, 38, 38, 
    38, 38, 0, 38, 0, 38, 38, 0, 38, 38, 0, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 0, 59, 59, 59, 59, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 8, 8, 12, 12, 5, 6, 5, 6, 5, 
    6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 0, 0, 0, 0, 3, 3, 3, 3, 12, 12, 12, 
    3, 3, 3, 0, 3, 3, 3, 3, 8, 5, 6, 5, 6, 5, 6, 3, 3, 3, 7, 8, 7, 7, 7, 
    0, 3, 4, 3, 3, 0, 0, 0, 0, 38, 38, 38, 0, 38, 0, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    0, 0, 102, 0, 3, 3, 3, 4, 3, 3, 3, 5, 6, 3, 7, 3, 8, 3, 3, 9, 9, 9, 
    9, 9, 9, 9, 9, 9, 9, 3, 3, 7, 7, 7, 3, 11, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
    13, 13, 13, 5, 7, 6, 7, 0, 0, 3, 5, 6, 3, 12, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 58, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 58, 
    58, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 0, 0, 0, 
    38, 38, 38, 38, 38, 38, 0, 0, 38, 38, 38, 38, 38, 38, 0, 0, 38, 38, 
    38, 38, 38, 38, 0, 0, 38, 38, 38, 0, 0, 0, 4, 4, 7, 11, 14, 4, 4, 0, 
    14, 7, 7, 7, 7, 14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 
    14, 0, 0
};

/*
 * Each group represents a unique set of character attributes.  The attributes
 * are encoded into a 32-bit value as follows:
 *
 * Bits 0-4	Character category: see the constants listed below.
 *
 * Bits 5-7	Case delta type: 000 = identity
 *				 010 = add delta for lower
 *				 011 = add delta for lower, add 1 for title
 *				 100 = sutract delta for title/upper
 *				 101 = sub delta for upper, sub 1 for title
 *				 110 = sub delta for upper, add delta for lower
 *
 * Bits 8-21	Reserved for future use.
 *
 * Bits 22-31	Case delta: delta for case conversions.  This should be the
 *			    highest field so we can easily sign extend.
 */

static int groups[] = {
    0, 15, 12, 25, 27, 21, 22, 26, 20, 9, 134217793, 28, 19, 134217858, 
    29, 2, 23, 11, 24, -507510654, 4194369, 4194434, -834666431, 973078658, 
    -507510719, 1258291330, 880803905, 864026689, 859832385, 331350081, 
    847249473, 851443777, 868220993, 884998209, 876609601, 893386817, 
    897581121, 914358337, 5, 910164033, 918552641, 8388705, 4194499, 
    8388770, 331350146, 880803970, 864026754, 859832450, 847249538, 
    851443842, 868221058, 876609666, 884998274, 893386882, 897581186, 
    914358402, 910164098, 918552706, 4, 6, -352321338, 159383617, 
    155189313, 268435521, 264241217, 159383682, 155189378, 130023554, 
    268435586, 264241282, 260046978, 239075458, 1, 197132418, 226492546, 
    360710274, 335544450, 335544385, 201326657, 201326722, 7, 8, 247464066, 
    -33554302, -33554367, -310378366, -360710014, -419430270, -536870782, 
    -469761918, -528482174, -37748606, -310378431, -37748671, 155189442, 
    -360710079, -419430335, -29359998, -469761983, -29360063, -536870847, 
    -528482239, 16, 13, 14, 67108938, 67109002, 10, 109051997, 109052061, 
    18, 17
};

/*
 * The following constants are used to determine the category of a
 * Unicode character.
 */

#define UNICODE_CATEGORY_MASK 0X1F

enum {
    UNASSIGNED,
    UPPERCASE_LETTER,
    LOWERCASE_LETTER,
    TITLECASE_LETTER,
    MODIFIER_LETTER,
    OTHER_LETTER,
    NON_SPACING_MARK,
    ENCLOSING_MARK,
    COMBINING_SPACING_MARK,
    DECIMAL_DIGIT_NUMBER,
    LETTER_NUMBER,
    OTHER_NUMBER,
    SPACE_SEPARATOR,
    LINE_SEPARATOR,
    PARAGRAPH_SEPARATOR,
    CONTROL,
    FORMAT,
    PRIVATE_USE,
    SURROGATE,
    CONNECTOR_PUNCTUATION,
    DASH_PUNCTUATION,
    OPEN_PUNCTUATION,
    CLOSE_PUNCTUATION,
    INITIAL_QUOTE_PUNCTUATION,
    FINAL_QUOTE_PUNCTUATION,
    OTHER_PUNCTUATION,
    MATH_SYMBOL,
    CURRENCY_SYMBOL,
    MODIFIER_SYMBOL,
    OTHER_SYMBOL
};

/*
 * The following macros extract the fields of the character info.  The
 * GetDelta() macro is complicated because we can't rely on the C compiler
 * to do sign extension on right shifts.
 */

#define GetCaseType(info) (((info) & 0xE0) >> 5)
#define GetCategory(info) ((info) & 0x1F)
#define GetDelta(infO) (((info) > 0) ? ((info) >> 22) : (~(~((info)) >> 22)))

/*
 * This macro extracts the information about a character from the
 * Unicode character tables.
 */

#define GetUniCharInfo(ch) (groups[groupMap[(pageMap[(((int)(ch)) & 0xffff) >> OFFSET_BITS] << OFFSET_BITS) | ((ch) & ((1 << OFFSET_BITS)-1))]])

