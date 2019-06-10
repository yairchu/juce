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
TrackHeadersView::TrackHeadersView ()
: resizeBorder (this, this, ResizableEdgeComponent::Edge::rightEdge)
{
    setSize (120, getHeight());
    setMinimumWidth (60);
    setMaximumWidth (240);
    resizeBorder.setAlwaysOnTop (true);
    addAndMakeVisible (resizeBorder);
}

void TrackHeadersView::setIsResizable (bool isResizable)
{
    resizeBorder.setVisible (isResizable);
}

void TrackHeadersView::resized()
{
    resizeBorder.setBounds (getWidth() - 1, 0, 1, getHeight());
    for (auto header : getChildren())
    {
        header->setBounds (header->getBounds().withWidth (getWidth()));
    }
    if (isShowing() && getParentComponent())
    {
        getParentComponent()->resized();
    }
}

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
    getARADocumentController()->getDocument<ARADocument>()->addListener (this);
}

DocumentViewController::~DocumentViewController()
{
    getARADocumentController()->getDocument<ARADocument>()->removeListener (this);
    getARAEditorView()->removeListener (this);
}

Component* DocumentViewController::createCanvasComponent()
{
    return new Component ("DocumentView Canvas");
}

PlaybackRegionView* DocumentViewController::createViewForPlaybackRegion (RegionSequenceView* ownerTrack ,ARAPlaybackRegion* playbackRegion)
{
    return new PlaybackRegionViewImpl (ownerTrack, playbackRegion);
}

TrackHeaderView* DocumentViewController::createHeaderViewForRegionSequence (ARARegionSequence* regionSequence)
{
    return new TrackHeaderView (getARAEditorView(), regionSequence);
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

Component* DocumentViewController::createPlayheadView (DocumentView &owner)
{
    return new PlayHeadView (owner);
}

Component* DocumentViewController::createTimeRangeSelectionView (DocumentView &owner)
{
    return new TimeRangeSelectionView (owner);
}

//==============================================================================
void DocumentViewController::invalidateRegionSequenceViews (NotificationType notificationType)
{
    // TODO - add virtual to check if need to rebuildViews...
    if (! getARADocumentController()->isHostEditingDocument())
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
    for (auto regionSequence : getARADocumentController()->getDocument()->getRegionSequences<ARARegionSequence>())
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
    jassert (document == getARADocumentController()->getDocument());
    invalidateRegionSequenceViews();
}

void DocumentViewController::didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* /*regionSequence*/)
{
    jassert (document == getARADocumentController()->getDocument());
    invalidateRegionSequenceViews();
}

void DocumentViewController::didReorderRegionSequencesInDocument (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());

    invalidateRegionSequenceViews();
}

//==============================================================================
DocumentView::DocumentView (DocumentViewController* ctrl, const AudioPlayHead::CurrentPositionInfo& posInfo)
    : viewController (ctrl),
    viewport (new ARASecondsPixelMapper (viewController->getARAEditorExtension())),
    timeMapper (static_cast<const ARASecondsPixelMapper&>(viewport.getPixelMapper())),
    trackHeadersView (new TrackHeadersView ()),
    positionInfo (posInfo)
{
    lastReportedPosition.resetToDefault();
    viewport.updateComponentsForRange = [&](Range<double> newVisibleRange)
    {
        for (auto regionSequenceView : regionSequenceViews)
        {
            regionSequenceView->updateRegionsBounds (newVisibleRange);
        }
        viewport.repaint();
    };

    viewport.setViewedComponent (viewController->createCanvasComponent());

    rulersView.reset (viewController->createRulersView (*this));
    viewport.addAndMakeVisible (*rulersView);

    playHeadView.reset (viewController->createPlayheadView (*this));
    viewport.addAndMakeVisible (playHeadView.get());
    playHeadView->setAlwaysOnTop (true);

    timeRangeSelectionView.reset (viewController->createTimeRangeSelectionView(*this));

    viewport.getViewedComponent()->addAndMakeVisible (getTrackHeadersView());
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
    return trackHeadersView->getWidth();
}
int  DocumentView::getTrackHeaderMaximumWidth ()
{
    return trackHeadersView->getMaximumWidth();
}
int  DocumentView::getTrackHeaderMinimumWidth ()
{
    return trackHeadersView->getMinimumWidth();
}

void DocumentView::setIsTrackHeadersVisible (bool shouldBeVisible)
{
    trackHeadersView->setVisible (shouldBeVisible);
    if (getParentComponent() != nullptr)
        resized();
}

bool DocumentView::isTrackHeadersVisible() const
{
    return trackHeadersView->isVisible();
}

void DocumentView::setTrackHeaderWidth (int newWidth)
{
    trackHeadersView->setBoundsForComponent (trackHeadersView.get(), trackHeadersView->getBounds().withWidth (newWidth), false, false, false, true);
}

void DocumentView::setTrackHeaderMaximumWidth (int newWidth)
{
    trackHeadersView->setIsResizable (getTrackHeaderMinimumWidth() < newWidth);
    trackHeadersView->setMaximumWidth (newWidth);
    trackHeadersView->checkComponentBounds (trackHeadersView.get());
}

void DocumentView::setTrackHeaderMinimumWidth (int newWidth)
{
    trackHeadersView->setIsResizable (newWidth < getTrackHeaderMaximumWidth());
    trackHeadersView->setMinimumWidth (newWidth);
    trackHeadersView->checkComponentBounds (trackHeadersView.get());
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

    listeners.callExpectingUnregistration ([&] (Listener& l)
                                           {
                                               l.visibleTimeRangeChanged (getVisibleTimeRange(), newZoomFactor);
                                           });
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
        regionView->resized();
    }
}

void DocumentView::setFitTrackHeight (bool shouldFit)
{
    fitTrackHeight = shouldFit;
    resized();
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
void DocumentView::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void DocumentView::resized()
{
    viewport.setBounds (getLocalBounds());
    const int trackHeaderWidth = trackHeadersView->isVisible() ? trackHeadersView->getWidth() : 0;
    rulersView->setBounds (0, 0, viewport.getWidth(), rulersHeight);
    const int minTrackHeight = (viewport.getHeightExcludingBorders() / (jmax (1, regionSequenceViews.size())));
    if (fitTrackHeight)
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
    trackHeadersView->setBounds (0, 0, getTrackHeaderWidth(), viewport.getViewedComponent()->getHeight());
    if (playHeadView != nullptr)
    {
        playHeadView->setBounds (trackHeaderWidth, rulersHeight, viewport.getWidthExcludingBorders(), viewport.getHeightExcludingBorders());
    }
    // apply needed borders
    auto timeRangeBounds = viewport.getViewedComponent()->getBounds();
    timeRangeBounds.setTop (viewController->getTopForCurrentTrackHeight (*this));
    timeRangeBounds.setLeft (trackHeaderWidth);
    timeRangeSelectionView->setBounds (timeRangeBounds);
}

void DocumentView::timerCallback()
{
    if (lastReportedPosition.timeInSeconds != positionInfo.timeInSeconds)
    {
        lastReportedPosition = positionInfo;

        if (scrollFollowsPlayHead && positionInfo.isPlaying)
        {
            const auto visibleRange = getVisibleTimeRange();
            if (lastReportedPosition.timeInSeconds < visibleRange.getStart() || lastReportedPosition.timeInSeconds > visibleRange.getEnd())
                viewport.getScrollBar (false).setCurrentRangeStart (lastReportedPosition.timeInSeconds);
        };
        if (playHeadView != nullptr)
        {
            playHeadView->repaint();
        }
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

    timeRange = getController().padTimeRange (timeRange);

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
    repaint();
}

//==============================================================================
DocumentViewController::PlayHeadView::PlayHeadView (DocumentView& documentView)
    : documentView (documentView)
{
    setInterceptsMouseClicks (false, true);
    setWantsKeyboardFocus (false);
}

void DocumentViewController::PlayHeadView::paint (juce::Graphics &g)
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
