#include "PlaybackRegionView.h"
#include "DocumentView.h"

PlaybackRegionView::PlaybackRegionView (RegionSequenceView* track, ARAPlaybackRegion* region)
    : playbackRegion (region), ownerTrack (track)
{}

PlaybackRegionView::~PlaybackRegionView()
{
    playbackRegion = nullptr;
}

Range<double> PlaybackRegionView::getVisibleTimeRange() const
{
    if (getLocalBounds().getWidth() == 0 || ! isVisible() || ownerTrack == nullptr)
        return {0.0, 0.0};

    auto range = getTimeRange();
    return { jmax (range.getStart(), ownerTrack->getParentDocumentView().getTimeMapper().getPositionForPixel (getBoundsInParent().getX())),
             jmin (range.getEnd(), ownerTrack->getParentDocumentView().getTimeMapper().getPositionForPixel (getBoundsInParent().getRight()))
           };
}


//==============================================================================
PlaybackRegionViewImpl::PlaybackRegionViewImpl (RegionSequenceView* track, ARAPlaybackRegion* region)
    : PlaybackRegionView (track, region),
      ownerTrack (track),
      playbackRegion (region),
      audioThumbCache (1),
      audioThumb (128, formatManager, audioThumbCache)
{
    audioThumb.addChangeListener (this);

    jassert (ownerTrack != nullptr);

    ownerTrack->getParentDocumentView().getController().getARAEditorView()->addListener (this);
    onNewSelection (ownerTrack->getParentDocumentView().getController().getARAEditorView()->getViewSelection());

    playbackRegion->getRegionSequence()->getDocument<ARADocument>()->addListener (this);
    playbackRegion->getAudioModification<ARAAudioModification>()->addListener (this);
    playbackRegion->getAudioModification()->getAudioSource<ARAAudioSource>()->addListener (this);
    playbackRegion->addListener (this);

    recreatePlaybackRegionReader();
    addAndMakeVisible (regionName);
    updateRegionName();
}

PlaybackRegionViewImpl::~PlaybackRegionViewImpl()
{
    jassert (ownerTrack != nullptr);
    ownerTrack->getParentDocumentView().getController().getARAEditorView()->removeListener (this);

    playbackRegion->removeListener (this);
    playbackRegion->getAudioModification<ARAAudioModification>()->removeListener (this);
    playbackRegion->getAudioModification()->getAudioSource<ARAAudioSource>()->removeListener (this);
    playbackRegion->getRegionSequence()->getDocument<ARADocument>()->removeListener (this);

    audioThumb.removeChangeListener (this);
    audioThumb.clear();
}

//==============================================================================
void PlaybackRegionViewImpl::paint (Graphics& g)
{
    Colour regionColour = convertOptionalARAColour (playbackRegion->getEffectiveColor());

    auto rect = getLocalBounds();
    g.setColour (isSelected ? Colours::yellow : Colours::black);
    g.drawRect (rect);
    rect.reduce (1, 1);

    g.setColour (regionColour);
    g.fillRect (rect);

    if (playbackRegion->getAudioModification()->getAudioSource()->isSampleAccessEnabled())
    {
        auto clipBounds = g.getClipBounds();
        if (clipBounds.getWidth() > 0)
        {
            const auto& mapper = ownerTrack->getParentDocumentView().getTimeMapper();
            const auto regionTimeRange = getTimeRange();
            const auto visibleRange = mapper.getRangeForPixels (getX(), getRight());

            auto drawBounds = getBounds() - getPosition();
            drawBounds.setHorizontalRange (clipBounds.getHorizontalRange());
            g.setColour (regionColour.contrasting (0.7f));
            audioThumb.drawChannels (g, drawBounds, visibleRange.getStart() - regionTimeRange.getStart(), visibleRange.getEnd() - regionTimeRange.getStart(), 1.0f);
        }
    }
    else
    {
        g.setColour (regionColour.contrasting (1.0f));
        g.setFont (Font (12.0f));
        g.drawText ("Access Disabled", getBounds(), Justification::centred);
    }
}

void PlaybackRegionViewImpl::resized()
{
    regionName.setBounds (0, 0, 1, regionName.getFont().getHeight());
    const int minTextWidth = 40.0;
    ownerTrack->getParentDocumentView().getViewport().anchorChildForTimeRange (getTimeRange(), getVisibleTimeRange(), regionName,  regionName.getFont().getStringWidthFloat (regionName.getText()) + minTextWidth);
}

//==============================================================================
void PlaybackRegionViewImpl::changeListenerCallback (ChangeBroadcaster* /*broadcaster*/)
{
    // our thumb nail has changed
    repaint();
}

void PlaybackRegionViewImpl::onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection)
{
    bool selected = ARA::contains (viewSelection.getPlaybackRegions(), playbackRegion);
    if (selected != isSelected)
    {
        isSelected = selected;
        repaint();
    }
}

void PlaybackRegionViewImpl::didEndEditing (ARADocument* document)
{
    jassert (document == playbackRegion->getRegionSequence()->getDocument());

    // our reader will pick up any changes in audio samples or region time range
    if ((playbackRegionReader ==  nullptr) || ! playbackRegionReader->isValid())
    {
        recreatePlaybackRegionReader();
        ownerTrack->getParentDocumentView().setRegionBounds (this, ownerTrack->getParentDocumentView().getViewport().getVisibleRange());
    }
}

void PlaybackRegionViewImpl::didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool /*enable*/)
{
    jassert (audioSource == playbackRegion->getAudioModification()->getAudioSource());

    repaint();
}

void PlaybackRegionViewImpl::willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties)
{
    jassert (audioSource == playbackRegion->getAudioModification()->getAudioSource());
    if (playbackRegion->getName() == nullptr && playbackRegion->getAudioModification()->getName() == nullptr && newProperties->name != audioSource->getName())
    {
        updateRegionName();
    }
}

void PlaybackRegionViewImpl::willUpdateAudioModificationProperties (ARAAudioModification* audioModification, ARAAudioModification::PropertiesPtr newProperties)
{
    jassert (audioModification == playbackRegion->getAudioModification());
    if (playbackRegion->getName() == nullptr && newProperties->name != audioModification->getName())
    {
        updateRegionName();
    }
}

void PlaybackRegionViewImpl::willUpdatePlaybackRegionProperties (ARAPlaybackRegion* region, ARAPlaybackRegion::PropertiesPtr newProperties)
{
    jassert (playbackRegion == region);

    if ((playbackRegion->getName() != newProperties->name) ||
        (playbackRegion->getColor() != newProperties->color))
    {
        updateRegionName();
        ownerTrack->getParentDocumentView().setRegionBounds (this, ownerTrack->getParentDocumentView().getViewport().getVisibleRange());
    }
}

void PlaybackRegionViewImpl::didUpdatePlaybackRegionContent (ARAPlaybackRegion* region, ARAContentUpdateScopes scopeFlags)
{
    jassert (playbackRegion == region);

    // Our reader catches this too, but we only check for its validity after host edits.
    // If the update is triggered inside the plug-in, we need to update the view from this call
    // (unless we're within a host edit already).
    if (scopeFlags.affectSamples() &&
        ! playbackRegion->getAudioModification()->getAudioSource()->getDocument()->getDocumentController()->isHostEditingDocument())
    {
        ownerTrack->getParentDocumentView().setRegionBounds (this, ownerTrack->getParentDocumentView().getViewport().getVisibleRange());
    }
}

//==============================================================================
void PlaybackRegionViewImpl::recreatePlaybackRegionReader()
{
    audioThumbCache.clear();

    // create a non-realtime playback region reader for our audio thumb
    playbackRegionReader = new ARAPlaybackRegionReader ({playbackRegion}, true);
    audioThumb.setReader (playbackRegionReader, reinterpret_cast<intptr_t> (playbackRegion));   // TODO JUCE_ARA better hash?
    // TODO JUCE_ARA see juce_AudioThumbnail.cpp, line 122: AudioThumbnail handles zero-length sources
    // by deleting the reader, therefore we must clear our "weak" pointer to the reader in this case.
    if (playbackRegionReader->lengthInSamples <= 0)
        playbackRegionReader = nullptr;
}

void PlaybackRegionViewImpl::updateRegionName()
{
    Colour regionColour = convertOptionalARAColour (playbackRegion->getEffectiveColor());
    regionName.setFont (Font (12.0f));
    regionName.setMinimumHorizontalScale (1.0);
    regionName.setJustificationType (Justification::topLeft);
    regionName.setText (convertOptionalARAString (playbackRegion->getEffectiveName()), sendNotification);
    regionName.setColour (Label::ColourIds::textColourId, regionColour.contrasting (1.0f));
}
