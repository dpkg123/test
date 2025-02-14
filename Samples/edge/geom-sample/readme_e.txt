[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


Rendering a variety of simple scenes which showcase features of the Edge
Geometry Library


<Point of Sample Program>
  This sample demonstrates how to load and render a scene that has been
  processed by the edgegeomcompiler tool.  A number of simple test
  scenes are provided which make use of Edge Geometry features such as
  skinning, blend shapes, triangle culling, custom vertex compression and
  static scene optimizations.

<Description of Sample Program>
  When edgegeomcompiler processes a scene, it can write its output in two
  different formats: a binary .edge file, and a human-readable C header file.
  The header file is useful for learning how the various Edge Geometry
  structures fit together, as well as verifying the data's integrity.
  This sample can load either type of scene.  By default, the sample loads
  a binary scene file (provided as a command-line argument).  To load a
  header scene file instead, uncomment the definition of USE_SCENE_IN_HEADER
  in main.cpp and specify the path to the header file directly below.  

  The following scenes are included:
  - bs_cube.edge: A simple cube with blend shapes
  - bs_cube2.edge: Another simple cube with blend shapes
  - bs_skin_cyl.edge: A skinned cylinder with blend shapes
  - skin_cyl.edge: A skinned cylinder

  For simplicity, animation matrices and blend shape factors are computed
  procedurally by the PPU.  One consequence of this is that the sample can not
  support arbitrary animated scenes besides the ones included; they will render,
  but probably won't animate correctly.

  Regardless of the features used, the code to create Edge Geometry
  SPURS jobs is identical.  Rendering with Edge Geometry is a very data-
  driven process; the contents of the EdgeGeomSpuConfigInfo structure 
  largely control which features are enabled.  This makes the runtime code
  especially versatile.

<Files>
../common/assets/bs_cube.dae      : Blendshaped cube scene (Collada source)
../common/assets/bs_cube2.dae     : Blendshaped cube2 scene (Collada source)
../common/assets/bs_skin_cyl.dae  : Blendshaped skinned cylinder scene (Collada source)
../common/assets/skin_cyl.dae     : Skinned cylinder scene (Collada source)
../common/assets/logo-color.bin   : Binary file containing color texture (raw pixel data)
../common/assets/logo-normal.bin  : Binary file containing normal texture (raw pixel data)
assets.mk	   : Makefile for building scene data
fpshader.cg        : Fragment shader program
geom-sample.sln    : Visual Studio Solution
geom-sample.vcproj : Visual Studio Project
main.cpp           : Main sample source
main.h             : Main sample header
Makefile           : Makefile
readme_e.txt       : readme text file (English)
readme_j.txt       : readme text file (Japanese)
vpshader.cg        : Vertex shader program
spu/Makefile	   : Makefile for all SPU programs
spu/job_geom.cpp    : Edge Geometry SPURS job source
spu/job_geom.mk     : Edge Geometry SPURS job makefile
spu/job_geom.vcproj : Edge Geometry SPURS job Visual Studio project

<Running the Program>
    - Run the sample with one command line argument: the path to the binary .edge scene file
      to load.
