[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


Demonstrates a (relatively) simple use of the tools-side libedgegeomtool library to
process a contrived scene format.


<Point of Sample Program>
  This sample reads in a file containing the geometry data for an elephant,
  stored in an imaginary custom scene format.  It then processes the scene
  using libedgegeomtool.  The results, suitable for passing to the Edge Geometry runtime,
  are written to a C header file for simplicity.  The sample favors readability
  and abundant comments over flexibility.

<Description of Sample Program>
  The input file, elephant.fake, is stored in an imaginary format called FakeSceneFormat
  (defined in fakesceneformat.h).  This format is not meant to be used; it is provided
  solely as an example of converting an arbitrary geometry representation into a format
  that libedgegeomtool can process.

<Files>
    elephant.edge.h    : Output elephant scene (C header Edge scene), suitable for copying
                         into one of the Edge Geometry runtime samples (such as elephant-sample)
    elephant.fake      : Input elephant scene (FakeSceneFormat scene)
    fakesceneformat.h  : Definition of the imaginary FakeSceneFormat, which we
                         convert and process using libedgegeomtool
    fakesceneformat.cpp: Contains the functions to convert FakeSceneFormat objects into
                         libedgegeomtool's intermediate format.
    geomtoolsample.cpp : Main sample source
    geomtoolsample.sln : Visual Studio 2003 solution file
    geomtoolsample.vs8.sln : Visual Studio 2005 solution file
    geomtoolsample.vcproj : Visual Studio 2003 project file
    geomtoolsample.vs8.vcproj : Visual Studio 2005 project file
    readme_e.txt       : readme text file (English)
    readme_j.txt       : readme text file (Japanese)
    utils.h            : Defines assorted utility functions which may require modifications by
                         the user, but can for the most part be integrated as-is into other
                         projects.
    utils.cpp          : Contains the functions defined in utils.h
