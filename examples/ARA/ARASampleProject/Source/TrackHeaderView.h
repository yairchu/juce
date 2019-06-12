#pragma once

#include "JuceHeader.h"
#include "RegionSequenceView.h"

//==============================================================================
/**
    TrackHeaderView
    JUCE component used to display ARA region sequence name, color, and selection state
*/
class TrackHeaderView   : public Component,
                          private ARAEditorView::Listener,
                          private ARARegionSequence::Listener
{
public:
    TrackHeaderView (ARAEditorView* editorView, RegionSequenceView& ownerTrack);
    ~TrackHeaderView();

    void paint (Graphics&) override;

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection) override;

    // ARARegionSequence::Listener overrides
    void didUpdateRegionSequenceProperties (ARARegionSequence* sequence) override;
    void willDestroyRegionSequence (ARARegionSequence* sequence) override;

    ARARegionSequence* getRegionSequence() const { return owner.getRegionSequence(); }

private:
    void detachFromRegionSequence();

private:
    ARAEditorView* editorView;
    RegionSequenceView& owner;
    bool isSelected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeaderView)
};
