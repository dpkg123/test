[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


A sample to demonstrate dynamic DXT compression of textures.

<Description of Sample Program>
  This sample demonstrates how to render an image using the RSX, compress
  the results on SPU to a DXT format, then using the results as a texture
  for use later in the frame.

<Files>
main.cpp         : Main sample source
main.h           : Main sample header
test-image.bin   : Sample image (raw pixel data)
vpshader.cg      : Example vertex shader to render a dynamic texture
fpshader.cg      : Example fragment shader to render a dynamic texture
Makefile         : Makefile for the sample
spu/job_dxt.cpp  : Makefile for all SPU code
spu/job_dxt.mk   : Edge DXT SPURS job source
spu/Makefile     : Edge DXT SPURS job Makefile

<Running the Program>
  Run the program directly, no arguments required.

