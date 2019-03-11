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
    , allowScrollH (true)
    , allowScrollV (true)
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

static int rescaleMouseWheelDistance (float distance, int singleStepSize) noexcept
{
    if (distance == 0.0f)
        return 0;
    
    distance *= 14.0f * singleStepSize;
    
    return roundToInt (distance < 0 ? jmin (distance, -1.0f)
                       : jmax (distance,  1.0f));
}

void TimelineViewport::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! useMouseWheelMoveIfNeeded (e, wheel))
        Component::mouseWheelMove (e, wheel);
}

bool TimelineViewport::useMouseWheelMoveIfNeeded (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    bool didUpdate = false;
    if (contentComp != nullptr && ! (e.mods.isAltDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()))
    {
        if (allowScrollH || allowScrollV)
        {
            const auto deltaX = rescaleMouseWheelDistance (wheel.deltaX, singleStepX);
            const auto deltaY = rescaleMouseWheelDistance (wheel.deltaY, singleStepY);
            const auto factor = pixelMapper->getZoomFactor();

            auto newTimePos = pixelMapper->getStartPixelPosition();
            const auto posY = getScrollBar (true).getCurrentRangeStart();
            auto newPosY = posY;

            if (deltaX != 0 && deltaY != 0 && allowScrollH && allowScrollV)
            {
                newTimePos -= deltaX / factor;
                newPosY -= deltaY;
            }
            else if (allowScrollH && (deltaX != 0 || e.mods.isShiftDown() || ! allowScrollV))
            {
                newTimePos -= deltaX != 0 ? deltaX / factor : deltaY / factor;
                newTimePos = getTimelineRange().clipValue (newTimePos);
            }
            else if (allowScrollV && deltaY != 0)
            {
                newPosY -= deltaY;
            }

            if (posY != newPosY)
            {
                getScrollBar (true).setCurrentRangeStart (newPosY, dontSendNotification);
                invalidateViewport();
                didUpdate = true;
            }

            if (! getVisibleRange().contains (getTimelineRange()) && newTimePos != pixelMapper->getStartPixelPosition())
            {
                getScrollBar (false).setCurrentRangeStart (newTimePos);
                didUpdate = true;
            }
        }
    }
    return didUpdate;
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
        repaint();
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


void TimelineViewport::ViewportClip::paint (juce::Graphics& g)
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

bool TimelineViewport::anchorChildForTimeRange (const Range<double> entireRangeOfParent, const Range<double> visibleRangeOfParent, Component& componentToBound, const float absoluteWidth, bool anchorToEnd)
{
    jassert (entireRangeOfParent.contains (visibleRangeOfParent));

    // this method anchor child to a parent!
    jassert (componentToBound.getParentComponent() != nullptr);
    const auto parentBounds = componentToBound.getParentComponent()->getLocalBounds();

    const auto relativeWidth = absoluteWidth / pixelMapper->getZoomFactor();
    const auto componentRelativeRange = anchorToEnd ? entireRangeOfParent.withStart (entireRangeOfParent.getEnd() - relativeWidth) : entireRangeOfParent.withLength (relativeWidth);
    const auto visibleChildRange = visibleRangeOfParent.getIntersectionWith (componentRelativeRange);

    if (visibleChildRange.isEmpty())
    {
        componentToBound.setVisible (false);
        return false;
    }

    const auto bounds = componentToBound.getLocalBounds();
    const int start = pixelMapper->getPixelForPosition (visibleChildRange.getStart());
    const int end = pixelMapper->getPixelForPosition (visibleChildRange.getEnd());
    if (anchorToEnd) // to right
    {
        const int visibleRange = end - start;
        // handles round-off errors
        if (visibleRange == 0)
        {
            componentToBound.setVisible (false);
            return false;
        }
        jassert (visibleRange > 0);
        componentToBound.setBounds (parentBounds.getWidth() - visibleRange, bounds.getY(), visibleRange, bounds.getHeight());
    }
    else // positionToStart - to left
    {
        const int x = static_cast<int> (jmax (0.0f, absoluteWidth - end));
        componentToBound.setBounds (-x, bounds.getY(), roundToInt (absoluteWidth), bounds.getHeight());
    }

    componentToBound.setVisible (true);
    return true;
}

void TimelineViewport::setIsScrollWheelAllowed (const bool isHorizontalAllowed, const bool isVerticalAllowed)
{
    allowScrollV = isVerticalAllowed;
    allowScrollH = isHorizontalAllowed;
}
