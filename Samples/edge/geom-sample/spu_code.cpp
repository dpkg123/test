/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

// This is a ridiculously simple example of a PRX file, since all we
// need to do is return some variables and stay resident. For
// information about doing anything more substantial with PRX files,
// please refer to the Lv2_PRX-Programming_GUIDE_e.pdf in the Cell SDK
// docs.

#include <sys/prx.h>

SYS_MODULE_INFO(edgejob, 0, 1, 1 );
SYS_MODULE_START( _start );
SYS_MODULE_STOP( _stop );

extern char _binary_job_job_geom_spu_bin_start[];
extern char _binary_job_job_geom_spu_bin_size[];
extern char _binary_job_job_geom_spu_bin_end[];

extern "C" int _start(unsigned int args, void *argp);
extern "C" int _stop(void);

int _start(unsigned int args, void *argp)
{
	(void)args;
	char **jobs = (char**)argp;
	jobs[0] = _binary_job_job_geom_spu_bin_start;
	jobs[1] = _binary_job_job_geom_spu_bin_size;
    return SYS_PRX_RESIDENT;
}

int _stop(void)
{
    return SYS_PRX_STOP_OK;
}
