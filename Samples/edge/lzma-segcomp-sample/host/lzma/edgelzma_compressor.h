#ifndef	LZMA_COMPRESSOR_H
#define LZMA_COMPRESSOR_H


extern "C" int32_t
lzmaCompress
(	uint8_t*		pCompr,
	uint32_t*		pComprSize,
	const uint8_t*	pUncompr,
	uint32_t		uncomprSize,
	int32_t			level
);

#endif
