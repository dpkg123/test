/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edgecompress.h"

/* ========================================================================= */

static int ZEXPORT segcompDeflateInit_
(
    z_streamp strm,
    int level,
    const char *version,
    int stream_size
)
{
    // Negate MAX_WBITS below as a flag for zlib to *not* output the 2 byte header
    //  and 4 byte footer
    return deflateInit2_(strm, level, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL,
                         Z_DEFAULT_STRATEGY, version, stream_size);
    /* To do: ignore strm->next_in if we use it as window */
}


/* ===========================================================================

	Segcomp version of compress2, except that the 2 byte header and 4 byte
	footer have been removed.

	Compresses the source buffer into the destination buffer. The level
	parameter has the same meaning as in deflateInit.  sourceLen is the byte
	length of the source buffer. Upon entry, destLen is the total size of the
	destination buffer, which must be at least 0.1% larger than sourceLen plus
	12 bytes. Upon exit, destLen is the actual size of the compressed buffer.

	compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
	memory, Z_BUF_ERROR if there was not enough room in the output buffer,
	Z_STREAM_ERROR if the level parameter is invalid.
*/
int ZEXPORT segcompCompress
(
    Bytef *dest,
    uLongf *destLen,
    const Bytef *source,
    uLong sourceLen,
    int level
)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
#ifdef MAXSEG_64K
    /* Check for source > 64K on 16-bit machine: */
	if ((uLong)stream.avail_in != sourceLen)
		return Z_BUF_ERROR;
#endif
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
	if ((uLong)stream.avail_out != *destLen)
		return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    //err = deflateInit(&stream, level);
    err = segcompDeflateInit_( &stream, level, ZLIB_VERSION, sizeof(z_stream) );
	if (err != Z_OK)
		return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}
