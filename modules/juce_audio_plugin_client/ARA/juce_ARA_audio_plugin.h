#pragma once

// Include juce preamble
#include "AppConfig.h"
#include <juce_core/juce_core.h>

#if JucePlugin_Enable_ARA

namespace juce
{

// Configure ARA debug support prior to including ARA headers
#if (JUCE_DEBUG && ! JUCE_DISABLE_ASSERTIONS) || JUCE_LOG_ASSERTIONS

    #define ARA_ENABLE_INTERNAL_ASSERTS 1

    extern JUCE_API void JUCE_CALLTYPE handleARAAssertion (const char* file, const int line, const char* diagnosis) noexcept;
    #define ARA_HANDLE_ASSERT(file, line, diagnosis)    juce::handleARAAssertion (file, line, diagnosis)

   #if JUCE_LOG_ASSERTIONS
    #define ARA_ENABLE_DEBUG_OUTPUT 1
   #endif

#else

    #define ARA_ENABLE_INTERNAL_ASSERTS 0

#endif // (JUCE_DEBUG && ! JUCE_DISABLE_ASSERTIONS) || JUCE_LOG_ASSERTIONS

}

// Include ARA headers
#include <ARA_Library/PlugIn/ARAPlug.h>

namespace juce
{
    using ARAContentUpdateScopes = ARA::ContentUpdateScopes;

    inline String convertARAString (ARA::ARAUtf8String str) { return String (CharPointer_UTF8 (str)); }

    /* Tries to convert and ARA Colour, if fails this will return default Colour constructor.
     */
    inline Colour convertARAColour (const ARA::ARAColor* colour)
    {
        return colour != nullptr ? Colour::fromFloatRGBA (colour->r, colour->g, colour->b, 1.0f) :
        Colour();
    }
}

#endif
