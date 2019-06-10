#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "ARASampleProjectAudioProcessor.h"
#include "DocumentView.h"

//==============================================================================
/**
    Editor class for ARA sample project
*/
class ARASampleProjectAudioProcessorEditor  : public AudioProcessorEditor,
                                              public AudioProcessorEditorARAExtension,
                                              private DocumentView::Listener,
                                              private juce::Timer
{
public:
    ARASampleProjectAudioProcessorEditor (ARASampleProjectAudioProcessor&);
    ~ARASampleProjectAudioProcessorEditor();

    void paint (Graphics&) override;
    void resized() override;

    // DocumentView::Listener overrides
    void visibleTimeRangeChanged (Range<double> newVisibleTimeRange, double zoomFactor) override;
    void trackHeightChanged (int newTrackHeight) override;

    // juce::Timer
    void timerCallback() override;

private:
    class ARASampleProjectDocumentViewController : public DocumentViewController
    {
    public:
        ARASampleProjectDocumentViewController (const AudioProcessorEditorARAExtension& editorARAExtension);

        std::vector<ARARegionSequence*> getVisibleRegionSequences() override;
        void setShouldShowSelectedTracksOnly (bool selectedOnly);
    private:
        bool shouldShowSelectedTracksOnly;
    };

    void setSelectedTrackOnly (bool selectedOnly);

    std::unique_ptr<DocumentView> documentView;
    ARASampleProjectDocumentViewController* documentViewController; // owned by documentView

    TextButton hideTrackHeaderButton;
    TextButton followPlayHeadButton;
    TextButton onlySelectedTracksButton;
    Label horizontalZoomLabel, verticalZoomLabel;
    Label playheadLinearPositionLabel, playheadMusicalPositionLabel;
    TextButton horizontalZoomInButton, horizontalZoomOutButton;
    TextButton verticalZoomInButton, verticalZoomOutButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARASampleProjectAudioProcessorEditor)
};
