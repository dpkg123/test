[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2010 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

Young-van-Vliet recursive gaussian blur on SPUs

  This sample provides an implementation of a gaussian blur that has constant
  cost for all filter widths and thus allows much stronger blurring than possible
  with kernel based implementations.

<Description of Sample Program>
  The sample program reads the contents of the 'assets' directory and reads all 
  720p RGBA8 textures. Additional textures with different parameters are ignored.
  The current texture can be selected using the left/right bottons on the d-pad.
  Each frame, the current texture is transfered from DDR to XDR using an RSX
  2D copy. The image is then processed by a number of SPUs (defined in 
  SpuPostProcessing.cpp as SPU_NUM).
  After the SPUs have performed the operation, the resulting image is copied into
  the back buffer by RSX, to be displayed.

<Key Features>
  - There are currently two implementations provided, one for ARGB8 and one for 
    XRGB8 buffers. (A 720p ARGB buffer takes 7ms, which goes down to 5.8ms for XRGB)
    These can be selected by using the YVV_MODE define in yvv-gaussian.cpp.
  - Boundary cases are handled properly, with the algorithm assuming that the signal 
    continues beyond the image edges.
  - Can smoothly progress from no blurring, which does not change pixel value, to
    very strong blurring (sigma values of about 100.f produce accurate results).
    This can be tried using the left analog stick, which controls the sigma.
  - The ratio of horizontal to vertical blur can be controlled with the right
    analog stick
  - TRIANGLE turns the blur off or on.

<Caveats>
  - The XRGB algorithm needs a relatively large amount of local-store memory, 
    which currently limits it to not more than 1280 pixels in either dimension.
    The ARGB version supports up to 2304 (=18*128) pixels in either dimension.
