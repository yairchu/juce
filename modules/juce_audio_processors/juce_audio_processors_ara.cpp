/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#include <juce_core/system/juce_CompilerWarnings.h>
#include <juce_core/system/juce_TargetPlatform.h>

#include "utilities/ARA/juce_ARADebug.h"

/* Having WIN32_LEAN_AND_MEAN defined at the point of including ARADebug.c will produce warnings.

   To prevent such problems it's easiest to have it in its own translation unit.
*/

#if (JucePlugin_Enable_ARA || (JUCE_PLUGINHOST_ARA && (JUCE_PLUGINHOST_VST3 || JUCE_PLUGINHOST_AU))) && (JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX || JUCE_IOS)

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wgnu-zero-variadic-macro-arguments", "-Wmissing-prototypes")
 #include <ARA_Library/Debug/ARADebug.c>
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

#endif
