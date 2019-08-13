#pragma once

#include "RegionSequenceView.h"

//==============================================================================
/**
    PlaybackRegionView
    abstract component to visualize and handle interaction with ARAPlaybackRegion.
 */
class PlaybackRegionView    : public Component
{
public:
    PlaybackRegionView (RegionSequenceView* ownerTrack, ARAPlaybackRegion* region);
    virtual ~PlaybackRegionView();

    ARAPlaybackRegion* getPlaybackRegion() const { return playbackRegion; }
    /* Returns this region time range on timeline */
    Range<double> getTimeRange() const { return playbackRegion->getTimeRange(); }
    /* Returns the visible region area on timeline.
       If regions bounds are invalid or it's invisible it'll return {0,0}
     */
    Range<double> getVisibleTimeRange() const;

private:
    ARAPlaybackRegion* playbackRegion;
    RegionSequenceView* ownerTrack;
};

/**
    PlaybackRegionViewImpl
    JUCE component used to display ARA playback regions
    along with their output waveform, name, color, and selection state
*/
class PlaybackRegionViewImpl : public PlaybackRegionView,
                               public ChangeListener,
                               public SettableTooltipClient,
                               private ARAEditorView::Listener,
                               private ARADocument::Listener,
                               private ARAAudioSource::Listener,
                               private ARAAudioModification::Listener,
                               private ARAPlaybackRegion::Listener
{
public:
    PlaybackRegionViewImpl (RegionSequenceView* ownerTrack, ARAPlaybackRegion* region);
    ~PlaybackRegionViewImpl() override;

    void paint (Graphics&) override;
    void resized() override;

    // ChangeListener overrides
    void changeListenerCallback (ChangeBroadcaster*) override;

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection) override;

    // ARADocument::Listener overrides: used to check if our reader has been invalidated
    void didEndEditing (ARADocument* document) override;

    // ARAAudioSource::Listener overrides
    void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) override;
    void willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties) override;
    // ARAAudioModification::Listener overrides
    void willUpdateAudioModificationProperties (ARAAudioModification* audioModification, ARAAudioModification::PropertiesPtr newProperties) override;

    // ARAPlaybackRegion::Listener overrides
    void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) override;
    void didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags) override;
private:
    void updateRegionName();
    void recreatePlaybackRegionReader();
    String playbackRegionToString() const;

private:
    RegionSequenceView* ownerTrack;
    ARAPlaybackRegion* playbackRegion;
    ARAPlaybackRegionReader* playbackRegionReader { nullptr };  // careful: "weak" pointer, actual pointer is owned by our audioThumb
    bool isSelected { false };
    Label regionName;

    AudioThumbnailCache audioThumbCache;
    AudioThumbnail audioThumb;
    AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionViewImpl)
};
