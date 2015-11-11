/*
##**************************************************
## ViSUS Visualization Project                    **
## Copyright (c) 2010 University of Utah          **
## Scientific Computing and Imaging Institute     **
## 72 S Central Campus Drive, Room 3750           **
## Salt Lake City, UT 84112                       **
##                                                **
## For information about this project see:        **
## http://www.pascucci.org/visus/                 **
##                                                **
##      or contact: pascucci@sci.utah.edu         **
##                                                **
##**************************************************/

#ifndef _VISUS_JUCE_APPCONFIG_H__
#define _VISUS_JUCE_APPCONFIG_H__

#define JUCE_MODULE_AVAILABLE_juce_core                  1
#define JUCE_MODULE_AVAILABLE_juce_gui_basics            1
#define JUCE_MODULE_AVAILABLE_juce_gui_extra             1

#if !VISUS_OPTION_DIRECTX
#define JUCE_MODULE_AVAILABLE_juce_opengl                1
#define JUCE_OPENGL                                      1
#endif

#define  JUCE_DONT_AUTOLINK_TO_WIN32_LIBRARIES 1
#define  JUCE_DIRECTSOUND                      0
#define  JUCE_USE_FLAC                         0
#define  JUCE_USE_OGGVORBIS                    0
#define  JUCE_USE_CDBURNER                     0
#define  JUCE_USE_CDREADER                     0
#define  JUCE_WEB_BROWSER                      0
#define  JUCE_USE_DIRECTWRITE                  0 //problems with juce CodeEditorComponent if using 1
#define  JUCE_INCLUDE_PNGLIB_CODE              1
#define  JUCE_INCLUDE_JPEGLIB_CODE             1
#define  JUCE_CHECK_MEMORY_LEAKS               0
#define  JUCE_DIRECTSOUND                      0 
#define  JUCE_CATCH_UNHANDLED_EXCEPTIONS       0

#define  JUCE_CHECK_OPENGL_ERROR ;

#endif //_VISUS_JUCE_APPCONFIG_H__

