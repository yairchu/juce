/*
  ==============================================================================

    TimelineViewport.cpp
    Created: 18 Feb 2019 12:24:05pm
    Author:  Tal Aviram
    Copyright (c) 2019, Sound Radix LTD, All Rights Reserved.

  ==============================================================================
*/

#include "TimelineViewport.h"

TimelineViewport::TimelineViewport (TimelinePixelMapperBase* pixelMapperToOwn, juce::ScrollBar* vScrollBarToOwn, juce::ScrollBar* hScrollBarToOwn)
    : pixelMapper (pixelMapperToOwn)
    , viewportBorders (0,0,0,0)
    , hScrollBar (hScrollBarToOwn)
    , vScrollBar (vScrollBarToOwn)
{
    viewportClip.setWantsKeyboardFocus (false);
    viewportClip.setInterceptsMouseClicks (false, false);
    setShouldClipBorders (true);
    hScrollBar->setRangeLimits (pixelMapper->getTimelineRange(), dontSendNotification);
    hScrollBar->addListener (this);
    vScrollBar->addListener (this);
    setZoomFactor (1.0);
}

TimelineViewport::~TimelineViewport()
{
    hScrollBar->removeListener (this);
    vScrollBar->removeListener (this);
    if (contentComp.get())
        contentComp->addComponentListener (this);
}

void TimelineViewport::componentMovedOrResized (Component &component, bool wasMoved, bool wasResized)
{
    // update vertical scrollBar
    const int startPos = vScrollBar->getCurrentRangeStart();
    const int viewportHeight = getHeightExcludingBorders();
    const int contentHeight = jmax (contentComp.get() ? contentComp->getHeight() : 0, viewportHeight);
    vScrollBar->setRangeLimits (0, contentHeight, dontSendNotification);
    vScrollBar->setCurrentRange (startPos, viewportHeight, dontSendNotification);

    invalidateViewport();
}

Range<double> TimelineViewport::getTimelineRange() const
{
    return pixelMapper->getTimelineRange();
}

void TimelineViewport::scrollBarMoved (juce::ScrollBar *scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == hScrollBar.get())
    {
        pixelMapper->setStartPixelPosition (newRangeStart);
    }
    invalidateViewport();
}

juce::ScrollBar& TimelineViewport::getScrollBar (bool isVertical)
{
    return isVertical ? *vScrollBar : *hScrollBar;
}

void TimelineViewport::setZoomFactor (double newFactor)
{
    pixelMapper->setZoomFactor (newFactor);
    invalidateViewport();
}

double TimelineViewport::getZoomFactor()
{
    return pixelMapper->getZoomFactor();
}

// TODO: for developing this is kept during testing/code-reviewing.
#ifdef JUCE_DEBUG
void TimelineViewport::paint (juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    // end of timeline...
    const int endPixel = pixelMapper->getEndPixelForBoundsWithinTimeline (g.getClipBounds().withTrimmedLeft (viewportBorders.getLeft()));
    double rightMostPixel;
    if (endPixel == pixelMapper->getTimelineEndPixel())
    {
        g.setColour (Colours::blue);
        g.drawLine (endPixel -1 + viewportBorders.getLeft(), viewportBorders.getTop(), endPixel -1 + viewportBorders.getLeft(), getHeight() - viewportBorders.getBottom());
        rightMostPixel = getTimelineRange().getEnd();
    }
    else
    {
        const auto endPixel = getLocalBounds().getWidth();
        rightMostPixel = pixelMapper->isPixelPositionWithinBounds (endPixel) ? pixelMapper->getPositionForPixel (endPixel): -1;
    }

    g.setColour (Colours::white);
    auto range = pixelMapper->getTimelineRange();
    String positionText = "Timeline Length:\n" + String(range.getStart()) + " - " + String(range.getEnd()) + "\nVisible Length: " + String(componentsRange.getStart()) + " - " + String(componentsRange.getEnd())  + "\nZoom: 1px:" + String(pixelMapper->getZoomFactor()) + pixelMapper->getBaseUnitDescription() +
    "\nLeftPos(t): " + String(pixelMapper->getStartPixelPosition()) +
    " RightPos(t): " + String(rightMostPixel) + " Width(px) exld border: " + String(getLocalBounds().getWidth() - viewportBorders.getLeft()) +
    " Height: " + String(getLocalBounds().getHeight()) +
    "\nExpected End Pixel (if valid): " + String (endPixel);
    g.drawFittedText (positionText, getLocalBounds(), Justification::centred, 5);
}
#endif

void TimelineViewport::resized()
{
    invalidateViewport();
    viewportClip.setBounds (getBounds());
}

void TimelineViewport::setViewedComponent (juce::Component *newViewedComponentToOwn)
{
    if (contentComp.get())
    {
        contentComp->removeComponentListener (this);
        removeChildComponent (contentComp.get());
    }
    contentComp.reset(newViewedComponentToOwn);
    if (newViewedComponentToOwn == nullptr)
        return;

    contentComp->addComponentListener (this);
    addAndMakeVisible (*contentComp);
    if (shouldClipBorders)
    {
        viewportClip.toFront (false);
    }
    // init on attach
    vScrollBar->setRangeLimits (0, jmax (contentComp->getHeight(), getHeightExcludingBorders()));
    vScrollBar->setCurrentRange (0, getHeightExcludingBorders());
}

void TimelineViewport::invalidateViewport()
{
    // invalidate vertical axis
    if (contentComp.get())
    {
        // update components time range.
        Range<double> curRange (pixelMapper->getStartPixelPosition(), (pixelMapper->getPositionForPixel (getWidthExcludingBorders())));
        if (componentsRange != curRange)
        {
            componentsRange = curRange;
            hScrollBar->setCurrentRange (componentsRange);
            if (updateComponentsForRange != nullptr)
            {
                updateComponentsForRange (componentsRange);
            }
        }
        Point<int> newPos (0, roundToInt (-vScrollBar->getCurrentRangeStart()) + viewportBorders.getTop());
        if (contentComp->getBounds().getPosition() != newPos)
        {
            contentComp->setTopLeftPosition (newPos);  // (this will re-entrantly call invalidateViewport again)
            return;
        }
    }
}

void TimelineViewport::setViewedComponentBorders (BorderSize<int> borders)
{
    viewportBorders = borders;
    viewportClip.transparentBounds = Rectangle<int>(viewportBorders.getLeft(), viewportBorders.getTop(), getWidthExcludingBorders(), getHeightExcludingBorders());
    invalidateViewport();
}

int TimelineViewport::getWidthExcludingBorders()
{
    return getWidth() - viewportBorders.getLeftAndRight();
}

int TimelineViewport::getHeightExcludingBorders()
{
    return getHeight() - viewportBorders.getTopAndBottom();
}

void TimelineViewport::setTimelineRange (Range<double> newRange)
{
    const auto prevRange = pixelMapper->getTimelineRange();
    pixelMapper->setTimelineRange (newRange);
    hScrollBar->setRangeLimits (pixelMapper->getTimelineRange(), dontSendNotification);
    // if this is the first timeline update it might start at a negative position.
    if (! newRange.contains (pixelMapper->getStartPixelPosition()) || prevRange.getLength() == 0.0)
    {
        pixelMapper->setStartPixelPosition (newRange.getStart());
    }
    invalidateViewport();
}

void TimelineViewport::setVisibleRange (Range<double> newVisibleRange)
{
    // visible range should be within timeline range!
    jassert (getTimelineRange().contains(newVisibleRange));

    const auto start = getTimelineRange().clipValue (newVisibleRange.getStart());
    const auto end = jlimit(start, getTimelineRange().getEnd(), newVisibleRange.getEnd());
    pixelMapper->setStartPixelPosition (start);
    pixelMapper->setZoomFactor (Range<double>(start, end).getLength() / getWidthExcludingBorders());
    invalidateViewport();
}


void TimelineViewport::ViewportClip::paint(juce::Graphics& g)
{
    // avoid transparency.
    g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillRect (0, 0, getWidth(), transparentBounds.getY());
    g.fillRect (0, transparentBounds.getBottom(), getWidth(), getHeight() - transparentBounds.getBottom());
    g.setColour (Colours::transparentBlack);
    g.fillRect (transparentBounds);
}

void TimelineViewport::setShouldClipBorders (bool shouldClip)
{
    shouldClipBorders = shouldClip;
    if (shouldClipBorders)
    {
        addAndMakeVisible (viewportClip);
    }
    else
    {
        viewportClip.setVisible (false);
    }
}
