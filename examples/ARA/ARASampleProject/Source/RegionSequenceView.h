#pragma once

#include "JuceHeader.h"

class DocumentView;
class TrackHeaderView;
class PlaybackRegionView;

//==============================================================================
/**
    RegionSequenceView
    JUCE component used to display all ARA playback regions in a region sequences
*/
class RegionSequenceView  : public juce::Component,
                            private ARARegionSequence::Listener
{
public:
    RegionSequenceView (DocumentView& documentView, ARARegionSequence* sequence);
    ~RegionSequenceView();

    ARARegionSequence* getRegionSequence() const { return regionSequence; }     // careful: may return nullptr!
    Range<double> getTimeRange() const { return (regionSequence != nullptr) ? regionSequence->getTimeRange() : Range<double>(); }
    bool isEmpty() const { return (regionSequence == nullptr) || regionSequence->getPlaybackRegions().empty(); }

    /* Updates current RegionSequence regions to new visible range */
    void updateRegionsBounds (Range<double> newVisibleRange);

    // ARARegionSequence::Listener overrides
    void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* sequence, ARAPlaybackRegion* playbackRegion) override;
    void didAddPlaybackRegionToRegionSequence (ARARegionSequence* sequence, ARAPlaybackRegion* playbackRegion) override;
    void willDestroyRegionSequence (ARARegionSequence* sequence) override;
    void willUpdateRegionSequenceProperties (ARARegionSequence* regionSequence, ARARegionSequence::PropertiesPtr newProperties) override;

    // juce::Component
    void resized() override;

private:
    void addRegionSequenceViewAndMakeVisible (ARAPlaybackRegion* playbackRegion);
    void detachFromRegionSequence();

private:
    DocumentView& documentView;
    ARARegionSequence* regionSequence;

    std::unique_ptr<TrackHeaderView> trackHeaderView;
    OwnedArray<PlaybackRegionView> playbackRegionViews;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionSequenceView)
};
