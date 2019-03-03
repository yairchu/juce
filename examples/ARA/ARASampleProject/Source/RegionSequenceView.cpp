#include "RegionSequenceView.h"
#include "DocumentView.h"
#include "TrackHeaderView.h"
#include "PlaybackRegionView.h"

//==============================================================================
RegionSequenceView::RegionSequenceView (DocumentView& documentView, ARARegionSequence* sequence)
    : documentView (documentView),
      regionSequence (sequence),
      trackHeaderView (documentView.createHeaderViewForRegionSequence (regionSequence))
{
    regionSequence->addListener (this);

    documentView.getTrackHeadersView().addAndMakeVisible (*trackHeaderView);
    for (auto playbackRegion : regionSequence->getPlaybackRegions<ARAPlaybackRegion>())
        addRegionSequenceViewAndMakeVisible (playbackRegion);
}

RegionSequenceView::~RegionSequenceView()
{
    detachFromRegionSequence();
}

void RegionSequenceView::addRegionSequenceViewAndMakeVisible (ARAPlaybackRegion* playbackRegion)
{
    auto view = documentView.createViewForPlaybackRegion (playbackRegion);
    playbackRegionViews.add (view);
    addChildComponent (view);
    documentView.setRegionBounds (view, documentView.getVisibleTimeRange());
}

void RegionSequenceView::detachFromRegionSequence()
{
    if (regionSequence == nullptr)
        return;

    regionSequence->removeListener (this);

    regionSequence = nullptr;
}

//==============================================================================
void RegionSequenceView::updateRegionsBounds (Range<double> newVisibleRange)
{
    for (auto regionView : playbackRegionViews)
    {
        documentView.setRegionBounds (regionView, newVisibleRange);
    }
}

void RegionSequenceView::resized()
{
    // updates TrackHeader height, width is handled by the TrackHeaderView
    trackHeaderView->setBounds (0, getBoundsInParent().getY(), trackHeaderView->getParentWidth(), getHeight());
    // updates all visible PlaybackRegions to new position.
    for (auto region : playbackRegionViews)
    {
        if (region->isVisible())
            region->setBounds (region->getBounds().withHeight(getHeight()));
    }
}

//==============================================================================
void RegionSequenceView::willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* sequence, ARAPlaybackRegion* playbackRegion)
{
    jassert (regionSequence == sequence);

    for (int i = 0; i < playbackRegionViews.size(); ++i)
    {
        if (playbackRegionViews[i]->getPlaybackRegion() == playbackRegion)
        {
            removeChildComponent (playbackRegionViews[i]);
            playbackRegionViews.remove (i);
            break;
        }
    }

    documentView.invalidateRegionSequenceViews();
}

void RegionSequenceView::didAddPlaybackRegionToRegionSequence (ARARegionSequence* sequence, ARAPlaybackRegion* playbackRegion)
{
    jassert (regionSequence == sequence);

    addRegionSequenceViewAndMakeVisible (playbackRegion);

    documentView.invalidateRegionSequenceViews();
}

void RegionSequenceView::willDestroyRegionSequence (ARARegionSequence* sequence)
{
    jassert (regionSequence == sequence);

    detachFromRegionSequence();

    documentView.invalidateRegionSequenceViews();
}

void RegionSequenceView::willUpdateRegionSequenceProperties (ARARegionSequence* sequence, ARARegionSequence::PropertiesPtr newProperties)
{
    jassert (regionSequence == sequence);
    if (newProperties->color != regionSequence->getColor())
    {
        //  repaints any PlaybackRegion that should follow RegionSequence color
        for (auto region : playbackRegionViews)
        {
            if  (region->getPlaybackRegion()->getColor() == nullptr)
            {
                region->repaint();
            }
        }
    }
}

