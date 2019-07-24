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
    setInterceptsMouseClicks (false, true);

    addAndMakeVisible (viewportClip);
    viewportClip.setWantsKeyboardFocus (false);
    viewportClip.setInterceptsMouseClicks (false, true);

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
    if (viewportHeight > 0)
    {
        const auto range = contentHeight == viewportHeight ? Range<double> (0, contentHeight) : Range<double> (static_cast<double>(startPos), static_cast<double>(jmin (startPos + viewportHeight, contentHeight)));
        vScrollBar->setCurrentRange (range, dontSendNotification);
    }
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
        if (hScrollBar->getCurrentRange() == getVisibleRange())
            return;

        // this wouldn't change for ScrollBar length.
        // use setVisibleRange().
        // rationale: if you'd like to show smaller than visible timeline area
        //            this would've break persistancy for smaller zoom ratios.
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

void TimelineViewport::mouseMagnify (const MouseEvent& e, float scaleFactor)
{
    setZoomFactorAroundPosition (
        scaleFactor * getZoomFactor(),
        pixelMapper->getPositionForPixel (e.x - viewportBorders.getLeft()));
}

typedef AnimatedPosition<AnimatedPositionBehaviours::ContinuousWithMomentum> ViewportDragPosition;

struct TimelineViewport::DragToScrollListener   : private MouseListener,
                                                  private ViewportDragPosition::Listener
{
    DragToScrollListener (TimelineViewport& v)  : viewport (v)
    {
        viewport.viewportClip.addMouseListener (this, true);
        offsetX.addListener (this);
        offsetY.addListener (this);
        offsetX.behaviour.setMinimumVelocity (60);
        offsetY.behaviour.setMinimumVelocity (60);
    }

    ~DragToScrollListener() override
    {
        viewport.viewportClip.removeMouseListener (this);
        Desktop::getInstance().removeGlobalMouseListener (this);
    }

    void positionChanged (ViewportDragPosition&, double) override
    {
        double offsetPos = offsetX.getPosition() * viewport.getPixelMapper().getZoomFactor();
        viewport.getScrollBar (false).setCurrentRangeStart (originalStartPos - offsetPos);
        viewport.getScrollBar (true).setCurrentRangeStart (originalY - offsetY.getPosition());
        viewport.invalidateViewport();
    }

    void mouseDown (const MouseEvent&) override
    {
        if (! isGlobalMouseListener)
        {
            offsetX.setPosition (offsetX.getPosition());
            offsetY.setPosition (offsetY.getPosition());

            // switch to a global mouse listener so we still receive mouseUp events
            // if the original event component is deleted
            viewport.viewportClip.removeMouseListener (this);
            Desktop::getInstance().addGlobalMouseListener (this);

            isGlobalMouseListener = true;
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (Desktop::getInstance().getNumDraggingMouseSources() == 1 && ! doesMouseEventComponentBlockViewportDrag (e.eventComponent))
        {
            auto totalOffset = e.getOffsetFromDragStart().toFloat();

            if (! isDragging && totalOffset.getDistanceFromOrigin() > 8.0f)
            {
                isDragging = true;
                originalStartPos = viewport.getScrollBar (false).getCurrentRangeStart();
                originalY = viewport.getScrollBar (true).getCurrentRangeStart();
                offsetX.setPosition (0.0);
                offsetX.beginDrag();
                offsetY.setPosition (0.0);
                offsetY.beginDrag();
            }

            if (isDragging)
            {
                offsetX.drag (totalOffset.x);
                offsetY.drag (totalOffset.y);
            }
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (isGlobalMouseListener && Desktop::getInstance().getNumDraggingMouseSources() == 0)
            endDragAndClearGlobalMouseListener();
    }

    void endDragAndClearGlobalMouseListener()
    {
        offsetX.endDrag();
        offsetY.endDrag();
        isDragging = false;

        viewport.viewportClip.addMouseListener (this, true);
        Desktop::getInstance().removeGlobalMouseListener (this);

        isGlobalMouseListener = false;
    }

    bool doesMouseEventComponentBlockViewportDrag (const Component* eventComp)
    {
        for (auto c = eventComp; c != nullptr && c != &viewport; c = c->getParentComponent())
            if (c->getViewportIgnoreDragFlag())
                return true;

        return false;
    }

    TimelineViewport& viewport;
    ViewportDragPosition offsetX, offsetY;
    double originalStartPos = 0.0;
    double originalY = 0;
    bool isDragging = false;
    bool isGlobalMouseListener = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragToScrollListener)
};

void TimelineViewport::setScrollOnDragEnabled (bool shouldScrollOnDrag)
{
    if (isScrollOnDragEnabled() != shouldScrollOnDrag)
    {
        if (shouldScrollOnDrag)
            dragToScrollListener.reset (new DragToScrollListener (*this));
        else
            dragToScrollListener.reset();
    }
}

bool TimelineViewport::isScrollOnDragEnabled() const noexcept
{
    return dragToScrollListener != nullptr;
}

bool TimelineViewport::isCurrentlyScrollingOnDrag() const noexcept
{
    return dragToScrollListener != nullptr && dragToScrollListener->isDragging;
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

void TimelineViewport::setZoomFactorAroundPosition (double newFactor, double position)
{
    setVisibleRange (
        getTimelineRange().clipValue (
            position - pixelMapper->getPixelForPosition (position) / newFactor),
        newFactor);
}

double TimelineViewport::getZoomFactor() const
{
    return pixelMapper->getZoomFactor();
}

// TODO: for developing this is kept during testing/code-reviewing.
#ifdef JUCE_DEBUG
void TimelineViewport::paint (juce::Graphics &g)
{
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
    const auto bounds = getLocalBounds();
    viewportClip.setBounds (bounds.withY (viewportBorders.getTop()).withHeight (getHeightExcludingBorders()));
}

void TimelineViewport::setViewedComponent (juce::Component *newViewedComponentToOwn)
{
    if (contentComp.get())
    {
        contentComp->removeComponentListener (this);
        viewportClip.removeChildComponent (contentComp.get());
    }
    contentComp.reset(newViewedComponentToOwn);
    if (newViewedComponentToOwn == nullptr)
        return;

    contentComp->addComponentListener (this);
    viewportClip.addAndMakeVisible (*contentComp);
    if (shouldClipBorders)
    {
        viewportClip.toFront (false);
    }
    // init on attach
    vScrollBar->setRangeLimits (0, jmax (contentComp->getHeight(), getHeightExcludingBorders()), dontSendNotification);
    vScrollBar->setCurrentRange (0, getHeightExcludingBorders(), dontSendNotification);
}

void TimelineViewport::invalidateViewport (Range<double> newTimelineRange)
{
    // invalidate vertical axis
    if (contentComp.get())
    {
        // update components time range.
        Range<double> curRange = newTimelineRange.isEmpty() ? Range<double>(pixelMapper->getStartPixelPosition(), (pixelMapper->getPositionForPixel (getWidthExcludingBorders()))) : newTimelineRange;

        if (componentsRange != curRange)
        {
            componentsRange = curRange;
            const auto newVisibleRange = componentsRange.getIntersectionWith (getTimelineRange());
            jassert (newVisibleRange.getLength() <= getTimelineRange().getLength());
            hScrollBar->setCurrentRange (newVisibleRange, dontSendNotification);
            if (updateComponentsForRange != nullptr)
            {
                updateComponentsForRange (componentsRange);
            }
        }
        Point<int> newPos (0, roundToInt (-vScrollBar->getCurrentRangeStart()));
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
    invalidateViewport();
    resized();
}

int TimelineViewport::getWidthExcludingBorders() const
{
    return getWidth() - viewportBorders.getLeftAndRight();
}

int TimelineViewport::getHeightExcludingBorders() const
{
    return getHeight() - viewportBorders.getTopAndBottom();
}

void TimelineViewport::setTimelineRange (Range<double> newRange)
{
    const auto prevRange = pixelMapper->getTimelineRange();
    if (newRange.isEmpty() || prevRange == newRange)
        return;

    pixelMapper->setTimelineRange (newRange);
    hScrollBar->setRangeLimits (pixelMapper->getTimelineRange(), dontSendNotification);
    // if this is the first timeline update it might start at a negative position.
    if (! newRange.contains (pixelMapper->getStartPixelPosition()) || prevRange.getLength() == 0.0)
    {
        pixelMapper->setStartPixelPosition (newRange.getStart());
    }
    invalidateViewport();
}

void TimelineViewport::setVisibleRange (Range<double> newVisibleRange, int constrainWidthInPixels)
{
    // visible range should be within timeline range!
    jassert (getTimelineRange().contains(newVisibleRange));
    int constrainWidth = constrainWidthInPixels;
    
    if (constrainWidth != -1)
    {
        jassert (constrainWidth > 0 && constrainWidth <= getWidthExcludingBorders());
    }
    else
    {
        constrainWidth = getWidthExcludingBorders();
    }

    // are you trying to call this method before view has proper bounds?
    jassert (constrainWidth > 0);
    const Range<double> newLength (
                        getTimelineRange().clipValue (newVisibleRange.getStart()),
                        getTimelineRange().clipValue (newVisibleRange.getEnd())
                                   );
    pixelMapper->setStartPixelPosition (newLength.getStart());
    pixelMapper->setZoomFactor (constrainWidth / newLength.getLength());
    invalidateViewport (newLength);

}

void TimelineViewport::setVisibleRange (double startPos, double pixelRatio)
{
    pixelMapper->setStartPixelPosition (startPos);
    pixelMapper->setZoomFactor (pixelRatio);
    invalidateViewport();
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

    const auto bounds = componentToBound.getBounds();
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

bool TimelineViewport::autoScroll (const int mouseX, const int mouseY, const int activeBorderThickness, const int maximumSpeed)
{
    if (contentComp != nullptr)
    {
        int dx = 0, dy = 0;

        if (getScrollBar (false).isVisible() || allowScrollH)
        {
            if (mouseX < activeBorderThickness)
                dx = activeBorderThickness - mouseX;
            else if (mouseX >= viewportClip.getWidth() - activeBorderThickness)
                dx = (viewportClip.getWidth() - activeBorderThickness) - mouseX;
            double pixelRatio = getZoomFactor();
            if (dx < 0)
                dx = jmax (dx * pixelRatio, -maximumSpeed * pixelRatio, (getPixelMapper().getStartPixelPosition() + viewportClip.getWidth() * pixelRatio)  - getPixelMapper().getTimelineRange().getEnd());
            else
                dx = jmin (dx * pixelRatio, maximumSpeed * pixelRatio, -getPixelMapper().getTimelineRange().getStart());
        }

        if (getScrollBar (true).isVisible() || allowScrollV)
        {
            if (mouseY < activeBorderThickness)
                dy = activeBorderThickness - mouseY;
            else if (mouseY >= viewportClip.getHeight() - activeBorderThickness)
                dy = (viewportClip.getHeight() - activeBorderThickness) - mouseY;

            if (dy < 0)
                dy = jmax (dy, -maximumSpeed, viewportClip.getHeight() - contentComp->getBottom());
            else
                dy = jmin (dy, maximumSpeed, -contentComp->getY());
        }

        // dx is relative to time base
        if (dx != 0 || dy != 0)
        {
            auto& hScroll = getScrollBar (false);
            auto& vScroll = getScrollBar (true);
            hScroll.setCurrentRangeStart (hScroll.getCurrentRangeStart() + dx);
            vScroll.setCurrentRangeStart (contentComp->getY() + dy);
            invalidateViewport();
            return true;
        }
    }
    return false;
}

