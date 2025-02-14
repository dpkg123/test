/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include <sys/prx.h>

SYS_MODULE_INFO(SPUJobs, 0, 1, 1 );
SYS_MODULE_START( _start );
SYS_MODULE_STOP( _stop );

extern char _binary_PixelshaderPatching_bin_start[];
extern char _binary_PixelshaderPatching_bin_size[];
extern char _binary_PixelshaderPatching_bin_end[];

extern char _binary_WriteFence_bin_start[];    
extern char _binary_WriteFence_bin_size[];    
extern char _binary_WriteFence_bin_end[];

extern "C" int _start(unsigned int args, void *argp);
extern "C" int _stop(void);

int _start(unsigned int args, void *argp)
{
    (void)args;
    char **jobs = (char**)argp;
    jobs[0] = _binary_PixelshaderPatching_bin_start;
    jobs[1] = _binary_PixelshaderPatching_bin_size;
    jobs[2] = _binary_WriteFence_bin_start;
    jobs[3] = _binary_WriteFence_bin_size;
    return SYS_PRX_RESIDENT;
}

int _stop(void)
{
    return SYS_PRX_STOP_OK;
}

