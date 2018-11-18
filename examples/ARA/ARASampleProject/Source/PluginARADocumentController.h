#pragma once

#include "JuceHeader.h"

//==============================================================================
/** 
    ARA DocumentController class for ARA sample project
    This is our plug-in's document controller implementation, which will 
    be the central point of communication between the ARA host and our plug-in
*/
class ARASampleProjectDocumentController : public juce::ARADocumentController
{
public:
    ARASampleProjectDocumentController() noexcept;

    // ARA class creation overrides
    ARA::PlugIn::EditorView* doCreateEditorView() noexcept override;
    ARA::PlugIn::PlaybackRenderer* doCreatePlaybackRenderer() noexcept override;
    // called when the ARA host stops editing the document
    virtual void doEndEditing () noexcept override;

private:
    // Thread used by buffering audio sources to read samples from the host
    std::unique_ptr<TimeSliceThread> araAudioSourceReadingThread;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARASampleProjectDocumentController)
};
