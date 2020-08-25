#include "DocumentView.h"

#include "RegionSequenceViewController.h"
#include "PlaybackRegionView.h"
#include "RulersView.h"

constexpr int kTrackHeight { 80 };

static double lastPixelsPerSecond { 1.0 };

//==============================================================================
DocumentView::DocumentView (ARAEditorView* ev, const AudioPlayHead::CurrentPositionInfo& posInfo)
    : editorView (ev),
      playbackRegionsViewport (*this),
      playHeadView (*this),
      timeRangeSelectionView (*this),
      rulersView (*this),
      pixelsPerSecond (lastPixelsPerSecond),
      positionInfo (posInfo)
{
    calculateTimeRange();

    playHeadView.setAlwaysOnTop (true);
    playHeadView.setInterceptsMouseClicks (false, false);
    playbackRegionsView.addAndMakeVisible (playHeadView);
    timeRangeSelectionView.setAlwaysOnTop (true);
    timeRangeSelectionView.setInterceptsMouseClicks (false, false);
    playbackRegionsView.addAndMakeVisible (timeRangeSelectionView);

    playbackRegionsViewport.setScrollBarsShown (true, true, false, false);
    playbackRegionsViewport.setViewedComponent (&playbackRegionsView, false);
    addAndMakeVisible (playbackRegionsViewport);

    trackHeadersViewport.setSize (120, getHeight());
    trackHeadersViewport.setScrollBarsShown (false, false, false, false);
    trackHeadersViewport.setViewedComponent (&trackHeadersView, false);
    addAndMakeVisible (trackHeadersViewport);

    rulersViewport.setScrollBarsShown (false, false, false, false);
    rulersViewport.setViewedComponent (&rulersView, false);
    addAndMakeVisible (rulersViewport);

    getARAEditorView()->addListener (this);
    getDocument()->addListener (this);

    lastReportedPosition.resetToDefault();

    startTimerHz (60);
}

DocumentView::~DocumentView()
{
    getDocument()->removeListener (this);
    getARAEditorView()->removeListener (this);
}

//==============================================================================
void DocumentView::onNewSelection (const ARAViewSelection& /*viewSelection*/)
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

void DocumentView::didEndEditing (ARADocument* /*document*/)
{
    if (regionSequenceViewsAreInvalid)
        rebuildRegionSequenceViews();

    if (timeRangeIsInvalid)
        calculateTimeRange();
}

void DocumentView::didAddRegionSequenceToDocument (ARADocument* /*document*/, ARARegionSequence* /*regionSequence*/)
{
    invalidateRegionSequenceViews();
}

void DocumentView::didReorderRegionSequencesInDocument (ARADocument* /*document*/)
{
    invalidateRegionSequenceViews();
}

//==============================================================================
Range<double> DocumentView::getVisibleTimeRange() const
{
    const double start = getPlaybackRegionsViewsTimeForX (playbackRegionsViewport.getViewArea().getX());
    const double end = getPlaybackRegionsViewsTimeForX (playbackRegionsViewport.getViewArea().getRight());
    return { start, end };
}

int DocumentView::getPlaybackRegionsViewsXForTime (double time) const
{
    return roundToInt ((time - timeRange.getStart()) / timeRange.getLength() * playbackRegionsView.getWidth());
}

double DocumentView::getPlaybackRegionsViewsTimeForX (int x) const
{
    return timeRange.getStart() + ((double) x / (double) playbackRegionsView.getWidth()) * timeRange.getLength();
}

//==============================================================================
void DocumentView::invalidateRegionSequenceViews()
{
    if (getDocumentController()->isHostEditingDocument() || getParentComponent() == nullptr)
        regionSequenceViewsAreInvalid = true;
    else
        rebuildRegionSequenceViews();
}

void DocumentView::invalidateTimeRange()
{
    timeRangeIsInvalid = true;
}

//==============================================================================
void DocumentView::setShowOnlySelectedRegionSequences (bool newVal)
{
    showOnlySelectedRegionSequences = newVal;
    invalidateRegionSequenceViews();
}

void DocumentView::zoomBy (double factor)
{
    pixelsPerSecond *= factor;
    if (getParentComponent() != nullptr)
        resized();  // this will both constrain pixelsPerSecond range properly and update all views
}

//==============================================================================
void DocumentView::parentHierarchyChanged()
{
    // trigger lazy initial update after construction if needed
    if (regionSequenceViewsAreInvalid && ! getDocumentController()->isHostEditingDocument())
        rebuildRegionSequenceViews();
}

void DocumentView::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void DocumentView::resized()
{
    // store visible playhead postion (in main view coordinates)
    int previousPlayHeadX = getPlaybackRegionsViewsXForTime (lastReportedPosition.timeInSeconds) - playbackRegionsViewport.getViewPosition().getX();

    const int trackHeaderWidth = trackHeadersViewport.getWidth();
    const int rulersViewHeight = rulersViewport.isVisible() ? 3*20 : 0;

    // update zoom
    double playbackRegionsViewWidthDbl = timeRange.getLength() * pixelsPerSecond;

    // limit max zoom by roughly 2 pixel per sample (we're just assuming some arbitrary high sample rate here),
    // but we also must make sure playbackRegionsViewWidth does not exceed integer range (with additional safety margin for rounding)
    playbackRegionsViewWidthDbl = jmin (playbackRegionsViewWidthDbl, timeRange.getLength() * 2.0 * 192000.0);
    playbackRegionsViewWidthDbl = jmin (playbackRegionsViewWidthDbl, static_cast<double> (std::numeric_limits<int>::max() - 1));
    int playbackRegionsViewWidth = roundToInt (floor (playbackRegionsViewWidthDbl));

    // min zoom is limited by covering entire view range
    // TODO JUCE_ARA getScrollBarThickness() should only be substracted if vertical scroll bar is actually visible
    const int minPlaybackRegionsViewWidth = getWidth() - trackHeaderWidth - playbackRegionsViewport.getScrollBarThickness();
    playbackRegionsViewWidth = jmax (minPlaybackRegionsViewWidth, playbackRegionsViewWidth);
    pixelsPerSecond = playbackRegionsViewWidth / timeRange.getLength();
    lastPixelsPerSecond = pixelsPerSecond;

    // update sizes and positions of all views
    playbackRegionsViewport.setBounds (trackHeaderWidth, rulersViewHeight, getWidth() - trackHeaderWidth, getHeight() - rulersViewHeight);
    playbackRegionsView.setBounds (0, 0, playbackRegionsViewWidth, jmax (kTrackHeight * regionSequenceViewControllers.size(), playbackRegionsViewport.getHeight() - playbackRegionsViewport.getScrollBarThickness()));

    rulersViewport.setBounds (trackHeaderWidth, 0, playbackRegionsViewport.getMaximumVisibleWidth(), rulersViewHeight);
    rulersView.setBounds (0, 0, playbackRegionsViewWidth, rulersViewHeight);

    trackHeadersViewport.setBounds (0, rulersViewHeight, trackHeadersViewport.getWidth(), playbackRegionsViewport.getMaximumVisibleHeight());
    trackHeadersView.setBounds (0, 0, trackHeadersViewport.getWidth(), playbackRegionsView.getHeight());

    int y = 0;
    for (auto v : regionSequenceViewControllers)
    {
        v->setRegionsViewBoundsByYRange (y, kTrackHeight);
        y += kTrackHeight;
    }

    playHeadView.setBounds (playbackRegionsView.getBounds());
    timeRangeSelectionView.setBounds (playbackRegionsView.getBounds());

    // keep viewport position relative to playhead
    // TODO JUCE_ARA if playhead is not visible in new position, we should rather keep the
    //               left or right border stable, depending on which side the playhead is.
    auto relativeViewportPosition = playbackRegionsViewport.getViewPosition();
    relativeViewportPosition.setX (getPlaybackRegionsViewsXForTime (lastReportedPosition.timeInSeconds) - previousPlayHeadX);
    playbackRegionsViewport.setViewPosition (relativeViewportPosition);
    rulersViewport.setViewPosition (relativeViewportPosition.getX(), 0);
}

//==============================================================================
void DocumentView::timerCallback()
{
    if (lastReportedPosition.timeInSeconds != positionInfo.timeInSeconds)
    {
        lastReportedPosition = positionInfo;

        if (scrollFollowsPlayHead)
        {
            const auto visibleRange = getVisibleTimeRange();
            if (lastReportedPosition.timeInSeconds < visibleRange.getStart() || lastReportedPosition.timeInSeconds > visibleRange.getEnd())
                playbackRegionsViewport.setViewPosition (playbackRegionsViewport.getViewPosition().withX (getPlaybackRegionsViewsXForTime (lastReportedPosition.timeInSeconds)));
        };

        playHeadView.repaint();
    }
}

//==============================================================================
void DocumentView::rebuildRegionSequenceViews()
{
    // always deleting all region sequence views and in turn their playback regions including their
    // audio thumbs isn't particularly effective - in an actual plug-in this would need to optimized.
    regionSequenceViewControllers.clear();

    if (showOnlySelectedRegionSequences)
    {
        for (auto selectedSequence : getARAEditorView()->getViewSelection().getEffectiveRegionSequences<ARARegionSequence>())
            regionSequenceViewControllers.add (new RegionSequenceViewController (*this, selectedSequence));
    }
    else    // show all RegionSequences of Document...
    {
        for (auto regionSequence : getDocument()->getRegionSequences<ARARegionSequence>())
        {
            if (! ARA::contains (getARAEditorView()->getHiddenRegionSequences(), regionSequence))
                regionSequenceViewControllers.add (new RegionSequenceViewController (*this, regionSequence));
        }
    }

    regionSequenceViewsAreInvalid = false;
    repaint();

    calculateTimeRange();
}

void DocumentView::calculateTimeRange()
{
    Range<double> newTimeRange;
    if (! regionSequenceViewControllers.isEmpty())
    {
        bool isFirst = true;
        for (auto v : regionSequenceViewControllers)
        {
            if (v->isEmpty())
                continue;

            const auto sequenceTimeRange = v->getTimeRange();
            if (isFirst)
            {
                newTimeRange = sequenceTimeRange;
                isFirst = false;
                continue;
            }

            newTimeRange = newTimeRange.getUnionWith (sequenceTimeRange);
        }
    }

    newTimeRange = newTimeRange.expanded (1.0);   // add a 1 second border left and right of first/last region

    timeRangeIsInvalid = false;
    if (timeRange != newTimeRange)
    {
        timeRange = newTimeRange;
        if (getParentComponent() != nullptr)
            resized();
    }
}

//==============================================================================
DocumentView::PlayHeadView::PlayHeadView (DocumentView& docView)
    : documentView (docView)
{}

void DocumentView::PlayHeadView::paint (juce::Graphics &g)
{
    const int playheadX = documentView.getPlaybackRegionsViewsXForTime (documentView.getPlayHeadPositionInfo().timeInSeconds);
    g.setColour (findColour (ScrollBar::ColourIds::thumbColourId));
    g.fillRect (playheadX, 0, 1, getHeight());
}

//==============================================================================
DocumentView::TimeRangeSelectionView::TimeRangeSelectionView (DocumentView& docView)
    : documentView (docView)
{}

void DocumentView::TimeRangeSelectionView::paint (juce::Graphics& g)
{
    const auto selection = documentView.getARAEditorView()->getViewSelection();
    if (selection.getTimeRange() != nullptr && selection.getTimeRange()->duration > 0.0)
    {
        const int startPixel = documentView.getPlaybackRegionsViewsXForTime (selection.getTimeRange()->start);
        const int endPixel = documentView.getPlaybackRegionsViewsXForTime (selection.getTimeRange()->start + selection.getTimeRange()->duration);
        g.setColour (juce::Colours::yellow.withAlpha (0.2f));
        g.fillRect (startPixel, 0, endPixel - startPixel, getHeight());
    }
}

//==============================================================================
// see https://forum.juce.com/t/viewport-scrollbarmoved-mousewheelmoved/20226
void DocumentView::ScrollMasterViewport::visibleAreaChanged (const Rectangle<int>& newVisibleArea)
{
    Viewport::visibleAreaChanged (newVisibleArea);
    
    documentView.getRulersViewport().setViewPosition (newVisibleArea.getX(), 0);
    documentView.getTrackHeadersViewport().setViewPosition (0, newVisibleArea.getY());
}