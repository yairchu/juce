#include "DocumentView.h"

#include "RegionSequenceView.h"
#include "TrackHeaderView.h"
#include "PlaybackRegionView.h"
#include "RulersView.h"

#include "ARASecondsPixelMapper.h"

constexpr double kMinSecondDuration = 1.0;
constexpr double kMinBorderSeconds = 1.0;

//==============================================================================
DocumentViewController::DocumentViewController (const AudioProcessorEditorARAExtension& extension)
: araExtension (extension)
{
    if (! araExtension.isARAEditorView())
    {
        // you shouldn't create a DocumentViewController/DocumentView if your instance can't support ARA.
        // notify user on your AudioProcessorEditorView or provide your own capture
        // alternative to ARA workflow.
        jassertfalse;
        return;
    }
    getARAEditorView()->addListener (this);
    getDocument()->addListener (this);
}

DocumentViewController::~DocumentViewController()
{
    getDocument()->removeListener (this);
    getARAEditorView()->removeListener (this);
}

Component* DocumentViewController::createCanvasComponent (DocumentView& owner)
{
    return new Component ("DocumentView Canvas");
}

PlaybackRegionView* DocumentViewController::createViewForPlaybackRegion (RegionSequenceView* ownerTrack ,ARAPlaybackRegion* playbackRegion)
{
    return new PlaybackRegionViewImpl (ownerTrack, playbackRegion);
}

TrackHeaderView* DocumentViewController::createHeaderViewForRegionSequence (RegionSequenceView& ownerTrack)
{
    return new TrackHeaderView (getARAEditorView(), ownerTrack);
}

Component* DocumentViewController::createTrackHeaderResizer (DocumentView &owner)
{
    return new TrackHeadersResizer (owner);
}

RegionSequenceView* DocumentViewController::createViewForRegionSequence (DocumentView& owner, ARARegionSequence* regionSequence)
{
    return new RegionSequenceView (owner, regionSequence);
}

RulersView* DocumentViewController::createRulersView (DocumentView &owner)
{
    auto rulers = new RulersView (owner.getViewport(), &owner.getPlayHeadPositionInfo());
    rulers->setColour (RulersView::ColourIds::rulersBackground, owner.getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    owner.setRulersHeight (3 * 20);

    rulers->addDefaultRulers();
    return rulers;
}

PlayHeadView* DocumentViewController::createPlayheadView (DocumentView &owner)
{
    return new PlayHeadView (owner.getViewport());
}

Component* DocumentViewController::createTimeRangeSelectionView (DocumentView &owner)
{
    return new TimeRangeSelectionView (owner);
}

//==============================================================================
void DocumentViewController::invalidateRegionSequenceViews (NotificationType notificationType)
{
    // TODO - add virtual to check if need to rebuildViews...
    if (! getDocumentController()->isHostEditingDocument())
    {
        //  dispatch views listening...
        switch (notificationType) {
            case NotificationType::dontSendNotification:
                return;
            case NotificationType::sendNotificationSync:
                sendSynchronousChangeMessage();
                return;
            case NotificationType::sendNotificationAsync:
                sendChangeMessage();
                return;
            case NotificationType::sendNotification:
            default:
                sendSynchronousChangeMessage();
                return;
        }
    }
}

Range<double> DocumentViewController::getDocumentTimeRange()
{
    // calculate viewport to be valid after construction!
    // default implementation provides range from based on earliest visible region
    // to last visible region.
    // (session/project/timeline can be bigger than that)
    juce::Range<double> timeRange = { 0.0, 0.0 };
    bool isFirst = true;
    for (auto regionSequence : getDocument()->getRegionSequences<ARARegionSequence>())
    {
        if (! ARA::contains (getARAEditorView()->getHiddenRegionSequences(), regionSequence))
        {
            const auto sequenceTimeRange = regionSequence->getTimeRange();
            if (isFirst)
            {
                timeRange = sequenceTimeRange;
                isFirst = false;
                continue;
            }
            timeRange = timeRange.getUnionWith (sequenceTimeRange);
        }
    }
    return timeRange;
}

Range<double> DocumentViewController::padTimeRange (Range<double> timeRange)
{
    if (timeRange.getLength() < kMinSecondDuration)
    {
        double startAdjustment = (kMinSecondDuration - timeRange.getLength()) / 2.0;
        timeRange.setStart (timeRange.getStart() - startAdjustment);
        timeRange.setEnd (timeRange.getStart() + kMinSecondDuration);
    }

    // apply kMinBorderSeconds offset to start and end
    timeRange.setStart (timeRange.getStart() - kMinBorderSeconds);
    timeRange.setEnd (timeRange.getEnd() + kMinBorderSeconds);

    return timeRange;
}

int DocumentViewController::getTopForCurrentTrackHeight (DocumentView& owner)
{
    // default would simply keep things the same
    return roundToInt (owner.getScrollBar (true).getCurrentRangeStart());
}

std::vector<ARARegionSequence *> DocumentViewController::getVisibleRegionSequences()
{
    return getARAEditorView()->getViewSelection().getEffectiveRegionSequences<ARARegionSequence>();
}

//==============================================================================
void DocumentViewController::onNewSelection (const ARA::PlugIn::ViewSelection& /*viewSelection*/)
{
    invalidateRegionSequenceViews();
}

void DocumentViewController::onHideRegionSequences (std::vector<ARARegionSequence*> const& /*regionSequences*/)
{
    invalidateRegionSequenceViews();
}

void DocumentViewController::didEndEditing (ARADocument* document)
{
    jassert (document == getDocument());
    invalidateRegionSequenceViews();
}

void DocumentViewController::didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* /*regionSequence*/)
{
    jassert (document == getDocument());
    invalidateRegionSequenceViews();
}

void DocumentViewController::didReorderRegionSequencesInDocument (ARADocument* document)
{
    jassert (document == getDocument());

    invalidateRegionSequenceViews();
}

//==============================================================================
DocumentView::DocumentView (DocumentViewController* ctrl, const AudioPlayHead::CurrentPositionInfo& posInfo)
    : viewController (ctrl),
    viewport (new ARASecondsPixelMapper (viewController->getARAEditorExtension())),
    timeMapper (static_cast<const ARASecondsPixelMapper&>(viewport.getPixelMapper())),
    positionInfo (posInfo)
{
    lastReportedPosition = positionInfo;
    viewport.updateComponentsForRange = [&](Range<double> newVisibleRange)
    {
        for (auto regionSequenceView : regionSequenceViews)
        {
            regionSequenceView->updateRegionsBounds (newVisibleRange);
        }

        if (playHeadView)
            playHeadView->updatePosition();
        if (rulersView)
            rulersView->invalidateLocators();

        viewport.repaint();
    };

    viewport.setViewedComponent (viewController->createCanvasComponent (*this));

    trackHeadersResizer.reset (viewController->createTrackHeaderResizer (*this));
    viewport.getViewedComponent()->addAndMakeVisible (trackHeadersResizer.get());
    trackHeadersResizer->setAlwaysOnTop (true);

    rulersView.reset (viewController->createRulersView (*this));
    viewport.addAndMakeVisible (*rulersView);

    playHeadView.reset (viewController->createPlayheadView (*this));
    viewport.addAndMakeVisible (playHeadView.get());
    playHeadView->setAlwaysOnTop (true);
    if (playHeadView)
        playHeadView->setPlayHeadTimeInSec (lastReportedPosition.timeInSeconds);

    timeRangeSelectionView.reset (viewController->createTimeRangeSelectionView(*this));

    timeRangeSelectionView->setAlwaysOnTop (true);
    viewport.getViewedComponent()->addAndMakeVisible (*timeRangeSelectionView);

    addAndMakeVisible (viewport);

    // force initial timerange after construction to be valid.
    viewport.setTimelineRange (viewController->padTimeRange (viewController->getDocumentTimeRange()));

    // register for invalidation of view
    viewController->addChangeListener (this);

    startTimerHz (60);
}

DocumentView::~DocumentView()
{
    viewController->removeAllChangeListeners();
}

//==============================================================================

int  DocumentView::getTrackHeaderWidth() const
{
    return layout.trackHeader.width;
}
int  DocumentView::getTrackHeaderMaximumWidth ()
{
    return layout.trackHeader.maxWidth;
}
int  DocumentView::getTrackHeaderMinimumWidth ()
{
    return layout.trackHeader.minWidth;
}

void DocumentView::setIsTrackHeadersVisible (bool shouldBeVisible)
{
    layout.trackHeader.visibleWidth = shouldBeVisible ? layout.trackHeader.width : 0;
    layout.invalidateLayout (*this);
    if (getParentComponent() != nullptr)
    {
        resized();
        repaint();
    }
}

bool DocumentView::isTrackHeadersVisible() const
{
    return layout.trackHeader.visibleWidth > 0;
}

void DocumentView::setTrackHeaderWidth (int newWidth)
{
    layout.trackHeader.width = newWidth;
    if (isTrackHeadersVisible())
        layout.trackHeader.visibleWidth = newWidth;

    layout.invalidateLayout (*this);
    if (getParentComponent() != nullptr)
        resized();

    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               l.trackHeaderChanged (layout.trackHeader.width, layout.trackHeader.visibleWidth > 0);
                                           });
}

void DocumentView::setTrackHeaderMaximumWidth (int newWidth)
{
    jassert (newWidth > 0);
    layout.trackHeader.maxWidth = newWidth;
    if (layout.trackHeader.maxWidth < layout.trackHeader.width)
        setTrackHeaderWidth (newWidth);
}

void DocumentView::setTrackHeaderMinimumWidth (int newWidth)
{
    jassert (newWidth > 0);
    layout.trackHeader.minWidth = newWidth;
    if (layout.trackHeader.width < layout.trackHeader.minWidth)
        setTrackHeaderWidth (newWidth);
}

void DocumentView::zoomBy (double zoomMultiply, bool relativeToPlay)
{
    const auto currentZoomFactor = viewport.getZoomFactor();
    const auto newZoomFactor = currentZoomFactor * zoomMultiply;
    if (newZoomFactor == currentZoomFactor)
        return;

    // note - this is for seconds only, currently it won't support ppq
    const auto playheadPosition = getPlayHeadPositionInfo().timeInSeconds;
    const auto curRange = getVisibleTimeRange();

    if (relativeToPlay && curRange.contains (playheadPosition) && curRange.getStart() != playheadPosition)
        viewport.setZoomFactorAroundPosition (newZoomFactor, playheadPosition);
    else
        viewport.setZoomFactor (newZoomFactor);

    if (getParentComponent() != nullptr)
        resized();

    if (playHeadView)
        playHeadView->updatePosition();
    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               l.visibleTimeRangeChanged (getVisibleTimeRange(), newZoomFactor);
                                           });
}

void DocumentView::setRegionBounds (PlaybackRegionView* regionView, Range<double> newVisibleRange, BorderSize<int> borders)
{
    const auto regionTimeRange = regionView->getTimeRange();
    const auto& mapper = getTimeMapper();
    const bool isIntersect = newVisibleRange.intersects (regionTimeRange);
    regionView->setVisible (isIntersect);
    if (isIntersect && regionView->getParentComponent() != nullptr)
    {
        auto visibleRegionArea = newVisibleRange.getIntersectionWith (regionTimeRange);
        const auto start = mapper.getPixelForPosition (visibleRegionArea.getStart() + borders.getLeft());
        const auto end   = mapper.getPixelForPosition (visibleRegionArea.getEnd() - borders.getLeftAndRight());
        regionView->setBounds (start, borders.getTop(), jmax (minRegionSizeInPixels, end - start), regionView->getParentHeight() - borders.getTopAndBottom());
        regionView->resized();
    }
}

void DocumentView::setFitTrackHeight (bool shouldFit)
{
    fitTrackHeight = shouldFit;
    if (!fitTrackHeight && layout.track.height == 0)
    {
        layout.track.height = layout.track.minHeight;
    }
    layout.invalidateLayout (*this);
    if (getParentComponent() != nullptr)
        resized();
}

void DocumentView::setFitTrackWidth (bool shouldFit)
{
    fitTrackWidth = shouldFit;
    scrollFollowsPlayHead = !shouldFit;
    resized();
}

void DocumentView::setTrackHeight (int newHeight)
{
    if (newHeight == layout.track.height)
        return;

    layout.track.height = newHeight;
    layout.track.visibleHeight = jmax (layout.track.height, layout.track.minHeight);
    layout.invalidateLayout (*this);

    if (getParentComponent() != nullptr)
        resized();

    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               // should we notify the visible height or expected height?
                                               l.trackHeightChanged (layout.track.height);
                                           });
}

void DocumentView::setRulersHeight (const int newHeight)
{
    layout.rulers.height = newHeight;
    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               // should we notify the visible height or expected height?
                                               l.rulersHeightChanged (layout.rulers.height);
                                           });
}

bool DocumentView::canVerticalZoomOutFurther() const
{
    return layout.track.height > layout.track.minHeight;
}

//==============================================================================
void DocumentView::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void DocumentView::resized()
{
    viewport.setBounds (getLocalBounds());
    rulersView->setBounds (0, 0, viewport.getWidth(), layout.rulers.height);
    if (fitTrackHeight)
        setTrackHeight (calcSingleTrackFitHeight());

    // width before the actual track regions
    const auto preContentWidth = layout.trackHeader.visibleWidth + layout.resizer.visibleWidth;

    viewport.setViewedComponentBorders (BorderSize<int> (layout.rulers.height,  preContentWidth, 0, 0));
    viewport.getViewedComponent()->setBounds (0, 0, getWidth(), jmax (layout.track.visibleHeight * getNumOfTracks(), viewport.getHeightExcludingBorders()));

    // should be calculated after viewport borders have been updated.
    if (viewport.getWidthExcludingBorders() > 0 && fitTrackWidth)
        setVisibleTimeRange (viewController->padTimeRange (viewController->getDocumentTimeRange()));

    layout.tracksLayout.performLayout (viewport.getViewedComponent()->getLocalBounds());

    if (playHeadView != nullptr)
    {
        playHeadView->setBounds (preContentWidth, 0, viewport.getWidthExcludingBorders(), viewport.getHeight() - viewport.getViewedComponentBorders().getBottom());
        playHeadView->updatePosition();
    }
    // apply needed borders
    auto timeRangeBounds = viewport.getViewedComponent()->getBounds();
    timeRangeBounds.setTop (viewController->getTopForCurrentTrackHeight (*this));
    timeRangeBounds.setLeft (preContentWidth);
    timeRangeSelectionView->setBounds (timeRangeBounds);
}

void DocumentView::timerCallback()
{
    if (lastReportedPosition.timeInSeconds != positionInfo.timeInSeconds)
    {
        lastReportedPosition = positionInfo;
        
        if (scrollFollowsPlayHead && positionInfo.isPlaying)
            followPlayheadIfNeeded();

        if (playHeadView != nullptr)
        {
            playHeadView->setPlayHeadTimeInSec (positionInfo.timeInSeconds);
        }
    }
}

void DocumentView::followPlayheadIfNeeded()
{
    // TODO - this is for seconds only, currently it won't support ppq
    const auto visibleRange = getVisibleTimeRange();
    if (lastReportedPosition.timeInSeconds < visibleRange.getStart() || lastReportedPosition.timeInSeconds > visibleRange.getEnd())
    {
        // out of known range, but we still support showing it
        if (lastReportedPosition.timeInSeconds < timeMapper.getStartPixelPosition() ||
            lastReportedPosition.timeInSeconds > timeMapper.getPositionForPixel (viewport.getWidthExcludingBorders())
            )
        {
            viewport.setVisibleRange (lastReportedPosition.timeInSeconds, viewport.getZoomFactor());
        }
    }
}

void DocumentView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    // TODO JUCE_ARA -
    // the example project sample read isn't thread-safe
    // so for now we make sure this is from the main thread
    // but this needs reviewing and I guess better design.
    if (source == viewController.get())
    {
        triggerAsyncUpdate();
    }
}

void DocumentView::handleAsyncUpdate()
{
    // TODO JUCE_ARA always deleting the region sequence views and in turn their playback regions
    //               with their audio thumbs isn't particularly effective. we should optimized this
    //               and preserve all views that can still be used. We could also try to build some
    //               sort of LRU cache for the audio thumbs if that is easier...

    regionSequenceViews.clear();

    for (auto selectedSequence : viewController->getVisibleRegionSequences())
    {
        auto sequence = viewController->createViewForRegionSequence (*this, selectedSequence);
        regionSequenceViews.add (sequence);
        getViewport().getViewedComponent()->addAndMakeVisible (sequence);
        getViewport().getViewedComponent()->addAndMakeVisible (sequence->getTrackHeaderView());
    }

    layout.invalidateLayout (*this);

    // calculate maximum visible time range
    juce::Range<double> timeRange = { 0.0, 0.0 };
    if (! regionSequenceViews.isEmpty())
    {
        timeRange = getController().getDocumentTimeRange();
        timeRange = getController().padTimeRange (timeRange);
    }

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

    if (getParentComponent() != nullptr)
    {
        resized();
        repaint();
    }
}

int DocumentView::calcSingleTrackFitHeight() const
{
    return jmax (
        layout.track.minHeight,
        viewport.getHeightExcludingBorders() / (jmax (1, regionSequenceViews.size()))
    );
}

void DocumentView::setMinTrackHeight (int newVal)
{
    if (layout.track.minHeight == newVal)
        return;
    layout.track.minHeight = newVal;
    setTrackHeight (layout.track.height); // Apply changes if necessary
}

//==============================================================================
DocumentViewController::TimeRangeSelectionView::TimeRangeSelectionView (DocumentView& view)
    : documentView (view)
{
    setInterceptsMouseClicks (false, true);
    setWantsKeyboardFocus (false);
}

void DocumentViewController::TimeRangeSelectionView::paint (juce::Graphics& g)
{
    const auto selection = documentView.getController().getARAEditorView()->getViewSelection();
    if (selection.getTimeRange() != nullptr && selection.getTimeRange()->duration > 0.0)
    {
        const auto& mapper = documentView.getTimeMapper();
        const int startPixel = mapper.getPixelForPosition (selection.getTimeRange()->start);
        const int endPixel = mapper.getPixelForPosition (selection.getTimeRange()->start + selection.getTimeRange()->duration);
        const int pixelDuration = endPixel - startPixel;
        const int height = documentView.getTrackHeight();
        int y = 0;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        // JUCE ARA TODO - regionsequenceview isn't (and wasn't!) thread safe.
        //                 we should provide a lock for it.
        for (int idx = 0; idx < documentView.getNumOfTracks(); ++idx)
        {
            const auto regionSequence = documentView.getRegionSequenceView (idx).getRegionSequence();
            if (regionSequence != nullptr && ARA::contains (selection.getRegionSequences(), regionSequence))
                g.fillRect (startPixel, y, pixelDuration, height);
            y += height;
        }
    }
}

//==============================================================================
DocumentViewController::TrackHeadersResizer::TrackHeadersResizer (DocumentView& docView)
    : colour (Colours::grey.withAlpha (0.2f)),
      documentView (docView) {}
void DocumentViewController::TrackHeadersResizer::paint (Graphics& g)
{
    g.setColour (colour);
    g.fillRect (static_cast<int> (floor (documentView.layout.resizer.width * 0.5)), 0, documentView.layout.resizer.visibleWidth, getHeight());
}

int DocumentViewController::TrackHeadersResizer::getMouseXforResizableArea (const MouseEvent &event) const
{
    return event.getEventRelativeTo (documentView.getViewport().getViewedComponent()).getPosition().getX();
}

void DocumentViewController::TrackHeadersResizer::setCursor()
{
    if (documentView.getTrackHeaderWidth() == documentView.getTrackHeaderMaximumWidth())
        setMouseCursor (MouseCursor::LeftEdgeResizeCursor);
    else if (documentView.getTrackHeaderWidth() == documentView.getTrackHeaderMinimumWidth())
        setMouseCursor (MouseCursor::RightEdgeResizeCursor);
    else
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
}

void DocumentViewController::TrackHeadersResizer::mouseEnter (const MouseEvent&)
{
    setCursor();
}

void DocumentViewController::TrackHeadersResizer::mouseExit (const MouseEvent&)
{
    setMouseCursor (MouseCursor::ParentCursor);
}

void DocumentViewController::TrackHeadersResizer::mouseDrag (const MouseEvent &event)
{
    // ruler height and 1px for resizer excluded
    const auto& viewport = documentView.getViewport();
    auto newWidth = jlimit (
                            documentView.getTrackHeaderMinimumWidth(),
                            jmin (viewport.getWidthExcludingBorders(), documentView.getTrackHeaderMaximumWidth()),
                            getMouseXforResizableArea(event));
    documentView.setTrackHeaderWidth (newWidth);
    setCursor();
}
//==============================================================================

void DocumentLayout::invalidateLayout (DocumentView& view)
{
    using Track = Grid::TrackInfo;
    // invalidate col layout
    tracksLayout.templateColumns =
    {
        Track ("headerColStart", Grid::Px (trackHeader.visibleWidth), "headerColEnd"),
        Track ("resizerColStart", Grid::Px (resizer.visibleWidth), "resizerColEnd"),
        Track ("contentColStart", 1_fr, "contentColEnd")
    };

    tracksLayout.autoRows = Grid::TrackInfo(1_fr);
    tracksLayout.autoColumns = Grid::TrackInfo(1_fr);

    // invalidate row layout
    auto& tracks = tracksLayout.templateRows;
    tracks.clear();
    const auto totalTrackIndexes = view.getNumOfTracks() -1;
    // this is inclusive (see totalTrackIndexes), for 0 tracks it'll be -1 so it won't add.
    for (auto i = 0; i <= totalTrackIndexes; ++i)
    {
        if (i < totalTrackIndexes)
        {
            tracks.add (Grid::TrackInfo (Grid::Px (track.visibleHeight)));
        }
        else
        {
            tracks.add (Grid::TrackInfo (Grid::Px (track.visibleHeight), "lastTrack"));
        }
    }
    tracks.add (Track (1_fr, "endOfTracks"));

    // rebuilds layout
    auto& items = tracksLayout.items;
    items.clear();

    auto resizerItem = GridItem (view.getTrackHeadersResizer());
    resizerItem.setArea (1, "resizerColStart", "endOfTracks", "resizerColEnd");
    resizerItem.alignSelf = resizer.alignment;
    resizerItem.justifySelf = resizer.justification;
    resizerItem.width = resizer.width;
    items.add (resizerItem);

    // add track items
    for (auto i = 0; i < view.getNumOfTracks(); ++i)
    {
        auto& trackView = view.getRegionSequenceView (i);
        // TODO - host provides track orders. (see getOrderIndex())
        // but if tracks are invisible or not within selection,
        // we must sort them to our current grid rows. (to avoid 1 row of track ordered 5 for example).
        {
            auto headerItem = GridItem (trackView.getTrackHeaderView());
            headerItem.setArea (i+1, "headerColStart");
            items.add (headerItem);
        }
        auto trackToLayout = GridItem (trackView);
        trackToLayout.setArea (i+1, "contentColStart");
        items.add (trackToLayout);
    }

    if (onInvalidateLayout != nullptr)
    {
        onInvalidateLayout (tracksLayout);
    }
}
