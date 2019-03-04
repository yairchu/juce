#include "DocumentView.h"

#include "RegionSequenceView.h"
#include "TrackHeaderView.h"
#include "PlaybackRegionView.h"
#include "RulersView.h"

#include "ARASecondsPixelMapper.h"

constexpr double kMinSecondDuration = 1.0;
constexpr double kMinBorderSeconds = 1.0;
constexpr int    kMinRegionSizeInPixels = 2;

//==============================================================================
DocumentView::DocumentView (const AudioProcessorEditorARAExtension& extension, const AudioPlayHead::CurrentPositionInfo& posInfo)
    : araExtension (extension),
      trackHeadersView (*this),
      viewport (new ARASecondsPixelMapper (extension)),
      timeMapper (static_cast<const ARASecondsPixelMapper&>(viewport.getPixelMapper())),
      rulersView (viewport, &lastReportedPosition),
      playHeadView (*this),
      timeRangeSelectionView (*this),
      positionInfo (posInfo)
{
    if (! araExtension.isARAEditorView())
    {
        // you shouldn't create a DocumentView if your instance can't support ARA.
        // notify user on your AudioProcessorEditorView or provide your own capture
        // alternative to ARA workflow.
        jassertfalse;
        return;
    }

    viewport.setViewedComponent (new Component());
    viewport.addAndMakeVisible (rulersView);
    viewport.addAndMakeVisible (playHeadView);
    playHeadView.setAlwaysOnTop (true);

    viewport.getViewedComponent()->addAndMakeVisible (trackHeadersView);
    timeRangeSelectionView.setAlwaysOnTop (true);
    viewport.getViewedComponent()->addAndMakeVisible (timeRangeSelectionView);


    viewport.updateComponentsForRange = [=](Range<double> newVisibleRange)
    {
        for (auto regionSequenceView : regionSequenceViews)
        {
            regionSequenceView->updateRegionsBounds (newVisibleRange);
        }
        viewport.repaint();
    };

    addAndMakeVisible (viewport);

    getARAEditorView()->addListener (this);
    getARADocumentController()->getDocument<ARADocument>()->addListener (this);

    lastReportedPosition.resetToDefault();

    startTimerHz (60);
}

DocumentView::~DocumentView()
{
    if (! araExtension.isARAEditorView())
        return;

    getARADocumentController()->getDocument<ARADocument>()->removeListener (this);
    getARAEditorView()->removeListener (this);
}

//==============================================================================
PlaybackRegionView* DocumentView::createViewForPlaybackRegion (ARAPlaybackRegion* playbackRegion)
{
    return new PlaybackRegionView (*this, playbackRegion);
}

TrackHeaderView* DocumentView::createHeaderViewForRegionSequence (ARARegionSequence* regionSequence)
{
    return new TrackHeaderView (getARAEditorView(), regionSequence);
}

RegionSequenceView* DocumentView::createViewForRegionSequence (ARARegionSequence* regionSequence)
{
    return new RegionSequenceView (*this, regionSequence);
}

void DocumentView::createRulers()
{
    setRulersHeight (3 * 20);
    rulersView.addDefaultRulers();
}

//==============================================================================
void DocumentView::invalidateRegionSequenceViews()
{
    if (getARADocumentController()->isHostEditingDocument() || getParentComponent() == nullptr)
        regionSequenceViewsAreInvalid = true;
    else
        rebuildRegionSequenceViews();
}

//==============================================================================
void DocumentView::setShowOnlySelectedRegionSequences (bool newVal)
{
    showOnlySelectedRegionSequences = newVal;
    invalidateRegionSequenceViews();
}

void DocumentView::setIsTrackHeadersVisible (bool shouldBeVisible)
{
    trackHeadersView.setVisible (shouldBeVisible);
    if (getParentComponent() != nullptr)
        resized();
}

void DocumentView::setTrackHeaderWidth (int newWidth)
{
    trackHeadersView.setBoundsForComponent (&trackHeadersView, trackHeadersView.getBounds().withWidth (newWidth), false, false, false, true);
}

void DocumentView::setTrackHeaderMaximumWidth (int newWidth)
{
    trackHeadersView.setIsResizable (getTrackHeaderMinimumWidth() < newWidth);
    trackHeadersView.setMaximumWidth (newWidth);
    trackHeadersView.checkComponentBounds (&trackHeadersView);
}

void DocumentView::setTrackHeaderMinimumWidth (int newWidth)
{
    trackHeadersView.setIsResizable (newWidth < getTrackHeaderMaximumWidth());
    trackHeadersView.setMinimumWidth (newWidth);
    trackHeadersView.checkComponentBounds (&trackHeadersView);
}

void DocumentView::zoomBy (double zoomMultiply)
{
    const auto currentZoomFactor = viewport.getZoomFactor();
    const auto newZoomFactor = currentZoomFactor * zoomMultiply;
    if (newZoomFactor == currentZoomFactor)
        return;

    viewport.setZoomFactor (newZoomFactor);

    if (getParentComponent() != nullptr)
        resized();

    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               l.visibleTimeRangeChanged (getVisibleTimeRange(), newZoomFactor);
                                           });
}

void DocumentView::setTrackHeight (int newHeight)
{
    if (newHeight == trackHeight)
        return;

    trackHeight = newHeight;
    if (getParentComponent() != nullptr)
        resized();

    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               l.trackHeightChanged (trackHeight);
                                           });
}

void DocumentView::setRulersHeight (int rulersHeight)
{
    DocumentView::rulersHeight = rulersHeight;
}

//==============================================================================
void DocumentView::parentHierarchyChanged()
{
    if (rulersView.getNumOfRulers() == 0)
    {
        // virtual function should be called after constructor
        // and parentHierarchyChanged can happen more than once!
        createRulers();
    }
    // trigger lazy initial update after construction if needed
    if (regionSequenceViewsAreInvalid && ! getARADocumentController()->isHostEditingDocument())
        rebuildRegionSequenceViews();
}

void DocumentView::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void DocumentView::resized()
{
    viewport.setBounds (getLocalBounds());
    const int trackHeaderWidth = trackHeadersView.isVisible() ? trackHeadersView.getWidth() : 0;
    rulersView.setBounds (0, 0, viewport.getWidth(), rulersHeight);
    const int minTrackHeight = (viewport.getHeightExcludingBorders() / (regionSequenceViews.isEmpty() ? 1 : regionSequenceViews.size()));
    if (showOnlySelectedRegionSequences)
        setTrackHeight (minTrackHeight);
    else
        setTrackHeight (jmax (trackHeight, minTrackHeight));

    int y = 0; // viewport below handles border offsets.
    for (auto v : regionSequenceViews)
    {
        // this also triggers RegionSequence's trackHeader resizing
        v->setBounds (trackHeaderWidth, y, getWidth(), trackHeight);
        y += trackHeight;
    }
    viewport.setViewedComponentBorders (BorderSize<int>(rulersHeight, trackHeaderWidth, 0, 0));
    viewport.getViewedComponent()->setBounds (0, 0, getWidth(), y);
    trackHeadersView.setBounds (0, 0, getTrackHeaderWidth(), viewport.getViewedComponent()->getHeight());
    playHeadView.setBounds (trackHeaderWidth, rulersHeight, viewport.getWidthExcludingBorders(), viewport.getHeightExcludingBorders());
    // apply needed borders
    auto timeRangeBounds = viewport.getViewedComponent()->getBounds();
    timeRangeBounds.setTop (0);
    timeRangeBounds.setLeft (trackHeaderWidth);
    timeRangeSelectionView.setBounds (timeRangeBounds);
}

void DocumentView::rebuildRegionSequenceViews()
{
    // TODO JUCE_ARA always deleting the region sequence views and in turn their playback regions
    //               with their audio thumbs isn't particularly effective. we should optimized this
    //               and preserve all views that can still be used. We could also try to build some
    //               sort of LRU cache for the audio thumbs if that is easier...

    for (auto seq : regionSequenceViews)
    {
        viewport.getViewedComponent()->removeChildComponent(seq);
    }
    regionSequenceViews.clear();

    if (showOnlySelectedRegionSequences)
    {
        for (auto selectedSequence : getARAEditorView()->getViewSelection().getEffectiveRegionSequences<ARARegionSequence>())
        {
            auto sequence = createViewForRegionSequence (selectedSequence);
            regionSequenceViews.add (sequence);
            viewport.getViewedComponent()->addAndMakeVisible (sequence);
        }
    }
    else    // show all RegionSequences of Document...
    {
        for (auto regionSequence : getARADocumentController()->getDocument()->getRegionSequences<ARARegionSequence>())
        {
            if (! ARA::contains (getARAEditorView()->getHiddenRegionSequences(), regionSequence))
            {
                auto sequence = createViewForRegionSequence (regionSequence);
                regionSequenceViews.add (sequence);
                viewport.getViewedComponent()->addAndMakeVisible (sequence);
            }
        }
    }

    // calculate maximum visible time range
    juce::Range<double> timeRange = { 0.0, 0.0 };
    if (! regionSequenceViews.isEmpty())
    {
        bool isFirst = true;
        for (auto v : regionSequenceViews)
        {
            if (v->isEmpty())
                continue;

            const auto sequenceTimeRange = v->getTimeRange();
            if (isFirst)
            {
                timeRange = sequenceTimeRange;
                isFirst = false;
                continue;
            }
            timeRange = timeRange.getUnionWith (sequenceTimeRange);
        }
    }

    // ensure visible range covers kMinSecondDuration
    if (timeRange.getLength() < kMinSecondDuration)
    {
        double startAdjustment = (kMinSecondDuration - timeRange.getLength()) / 2.0;
        timeRange.setStart (timeRange.getStart() - startAdjustment);
        timeRange.setEnd (timeRange.getStart() + kMinSecondDuration);
    }

    // apply kMinBorderSeconds offset to start and end
    timeRange.setStart (timeRange.getStart() - kMinBorderSeconds);
    timeRange.setEnd (timeRange.getEnd() + kMinBorderSeconds);

    // TODO JUCE_ARA - currently the entire DocumentView is rebuilt each time
    //                 showOnlySelectedRegionSequences is changed.
    //                 TimelineViewport only invalidates when range is really changed.
    //                 Once adding some caching or better mechanism it won't be necessary
    //                 to updateRegionBounds even if timeline isn't changing.
    //                 it would be better to keep current visible RegionSequeneces and
    //                 just remove the others.
    if (viewport.getTimelineRange() != timeRange)
    {
        viewport.setTimelineRange (timeRange);
    }

    // always recalculate everything since we already re-create the view and this is called
    // for now everything. as the comment above suggests this entire call should be improved
    // to use caching mechanism.
    for (auto regionSequenceView : regionSequenceViews)
    {
        regionSequenceView->updateRegionsBounds (timeRange);
    }
    resized();

    regionSequenceViewsAreInvalid = false;
}

void DocumentView::setRegionBounds (PlaybackRegionView* regionView, Range<double> newVisibleRange)
{
    const auto regionTimeRange = regionView->getTimeRange();
    const auto& mapper = getTimeMapper();
    const bool isIntersect = newVisibleRange.intersects (regionTimeRange);
    regionView->setVisible (isIntersect);
    if (isIntersect && regionView->getParentComponent() != nullptr)
    {
        auto visibleRegionArea = newVisibleRange.getIntersectionWith (regionTimeRange);
        const auto start = mapper.getPixelForPosition (visibleRegionArea.getStart());
        const auto end   = mapper.getPixelForPosition (visibleRegionArea.getEnd());
        regionView->setBounds (start, 0, jmax (kMinRegionSizeInPixels, end - start), regionView->getParentHeight());
    }
}

//==============================================================================
void DocumentView::onNewSelection (const ARA::PlugIn::ViewSelection& /*viewSelection*/)
{
    if (showOnlySelectedRegionSequences)
        invalidateRegionSequenceViews();
    else
        timeRangeSelectionView.repaint();
}

void DocumentView::onHideRegionSequences (std::vector<ARARegionSequence*> const& /*regionSequences*/)
{
    invalidateRegionSequenceViews();
}

void DocumentView::didEndEditing (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());
    
    if (regionSequenceViewsAreInvalid)
        rebuildRegionSequenceViews();
}

void DocumentView::didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* regionSequence)
{
    jassert (document == getARADocumentController()->getDocument());

    invalidateRegionSequenceViews();
}

void DocumentView::didReorderRegionSequencesInDocument (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());

    invalidateRegionSequenceViews();
}

void DocumentView::timerCallback()
{
    if (lastReportedPosition.timeInSeconds != positionInfo.timeInSeconds)
    {
        lastReportedPosition = positionInfo;

        if (scrollFollowsPlayHead)
        {
            const auto visibleRange = getVisibleTimeRange();
            if (lastReportedPosition.timeInSeconds < visibleRange.getStart() || lastReportedPosition.timeInSeconds > visibleRange.getEnd())
                viewport.getScrollBar (false).setCurrentRangeStart (lastReportedPosition.timeInSeconds);
        };

        playHeadView.repaint();
    }
}

//==============================================================================
void DocumentView::addListener (Listener* const listener)
{
    listeners.add (listener);
}

void DocumentView::removeListener (Listener* const listener)
{
    listeners.remove (listener);
}

//==============================================================================
DocumentView::PlayHeadView::PlayHeadView (DocumentView& documentView)
    : documentView (documentView)
{}

void DocumentView::PlayHeadView::paint (juce::Graphics &g)
{
    const auto& mapper = documentView.getTimeMapper();
    const auto endPos = mapper.getPositionForPixel (g.getClipBounds().getRight());
    const auto playheadPos = documentView.getPlayHeadPositionInfo().timeInSeconds;
    if (playheadPos <= endPos)
    {
        g.setColour (findColour (ScrollBar::ColourIds::thumbColourId));
        g.fillRect (mapper.getPixelForPosition (playheadPos), 0, 1, getHeight());
    }
}

//==============================================================================
DocumentView::TimeRangeSelectionView::TimeRangeSelectionView (DocumentView& documentView)
    : documentView (documentView)
{}

void DocumentView::TimeRangeSelectionView::paint (juce::Graphics& g)
{
    const auto selection = documentView.getARAEditorView()->getViewSelection();
    if (selection.getTimeRange() != nullptr && selection.getTimeRange()->duration > 0.0)
    {
        const auto& mapper = documentView.getTimeMapper();
        const int startPixel = mapper.getPixelForPosition (selection.getTimeRange()->start);
        const int endPixel = mapper.getPixelForPosition (selection.getTimeRange()->start + selection.getTimeRange()->duration);
        const int pixelDuration = endPixel - startPixel;
        const int trackHeight = documentView.getTrackHeight();
        int y = 0;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        for (const auto regionSequenceView : documentView.regionSequenceViews)
        {
            const auto regionSequence = regionSequenceView->getRegionSequence();
            if (regionSequence != nullptr && ARA::contains (selection.getRegionSequences(), regionSequence))
                g.fillRect (startPixel, y, pixelDuration, trackHeight);
            y += trackHeight;
        }
    }
}

//==============================================================================
DocumentView::TrackHeadersView::TrackHeadersView (DocumentView &documentView)
    : documentView (documentView),
      resizeBorder (this, this, ResizableEdgeComponent::Edge::rightEdge)
{
    setSize (120, getHeight());
    setMinimumWidth (60);
    setMaximumWidth (240);
    resizeBorder.setAlwaysOnTop (true);
    addAndMakeVisible (resizeBorder);
}

void DocumentView::TrackHeadersView::setIsResizable (bool isResizable)
{
    resizeBorder.setVisible (isResizable);
}

void DocumentView::TrackHeadersView::resized()
{
    resizeBorder.setBounds (getWidth() - 1, 0, 1, getHeight());
    for (auto header : getChildren())
    {
        header->setBounds (header->getBounds().withWidth (getWidth()));
    }
    if (isShowing())
        documentView.resized();
}
