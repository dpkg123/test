[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2010 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

Morphological Anti-Aliasing on SPUs

<Point of Sample Program>
  This sample contains the code necessary to perform morphological anti-aliasing
  on SPUs and provides a GTF viewer using the method.
  
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
  -Method is purely image-space and thus requires no geometry information and can be
   used at any point during the rendering process. (We recommend after transparent and
   before other post-processing)
  -Method is in-place, requiring only one full-size buffer in XDR.
  -Edge detection threshold can be set during runtime, using the 'threshold0' and 
   'threshold1' parameters of the function edgePostMlaaPrepareProcessing.
  -Detected edges can be visualized by pressing SQUARE.
  -Deactivate filtering by pressing TRIANGLE.
  -Switch between absolute and relative thresholding by pressing CIRCLE.
  -Quickly toggle between two predefined scales by pressing CROSS.
  -Average cost for a single 720p buffer is about 10ms of SPU time, depending on
   the number of edges found. Image complexity and threshold influence performance.
