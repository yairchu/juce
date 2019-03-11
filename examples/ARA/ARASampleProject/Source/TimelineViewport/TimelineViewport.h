/*
  ==============================================================================

    TimelineViewport.h -
    A convenient juce::Component to provide viewport specific for
    timeline/chronologic events.
 
    Rationale: map of chronological events could be very large.
    (for example showing a 192khz session when every pixel corresponds to sample
     could easily overflow juce::Rectangle<int>. also saving a complex map of
     juce::Component objects such as audio-regions might be very memory intensive).
 
    Created: 18 Feb 2019 12:24:05pm
    Author:  Tal Aviram
    Copyright (c) 2019, Sound Radix LTD, All Rights Reserved.

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#include "TimelinePixelMapper.h"

using namespace juce;

class TimelineViewport   : public Component
                         , public ComponentListener
                         , public ScrollBar::Listener
{
public:
    TimelineViewport (TimelinePixelMapperBase* pixelMapperToOwn, juce::ScrollBar* vScrollBarToOwn = new juce::ScrollBar (true), juce::ScrollBar* hScrollBarToOwn = new juce::ScrollBar (false));
    ~TimelineViewport();

    /* In current implementation ScrollBars visibility and position
     * aren't managed by TimelineViewport. (you should add them to desired view
     * and set their bounds.
     * This method provides this for you.
     */
    juce::ScrollBar& getScrollBar (bool isVertical);

    //==============================================================================
    /** Sets the component that this viewport will contain and scroll around.

     This will add the given component to this Viewport and position it at (0, 0).

     (Don't add or remove any child components directly using the normal
     Component::addChildComponent() methods).

     @param newViewedComponent   the component to add to this viewport, or null to remove
     the current component.
     @see getViewedComponent
     */
    void setViewedComponent (Component* newViewedComponentToOwn);

    /* Some elements on our viewport might be static...
     * (for example track inspector, ruler in audio apps).
     * This sets additional padding to keep some UI elements static.
     */
    void setViewedComponentBorders (BorderSize<int> borders);

    /* Returns borders for viewed component.
     * This should be taken into juce::Components added directly to view!
     */
    BorderSize<int> getViewedComponentBorders() const { return viewportBorders; };

    /** Returns the component that's currently being used inside the Viewport.

     @see setViewedComponent
     */
    Component* getViewedComponent() const noexcept                  { return contentComp.get(); }

    // juce::ComponentListener
    void componentMovedOrResized (Component &component, bool wasMoved, bool wasResized) override;

    // juce::ScrollBar::Listener
    void scrollBarMoved (ScrollBar *scrollBarThatHasMoved, double newRangeStart) override;

    // based on juce::Viewport
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;
    bool useMouseWheelMoveIfNeeded (const MouseEvent&, const MouseWheelDetails&);

    void setIsScrollWheelAllowed (bool isHorizontalAllowed, bool isVerticalAllowed);

    void setZoomFactor (double newFactor);
    double getZoomFactor();

    /** This lambda for every change in visible timeline range.
            - viewport scrolled
            - zoom factor updates
            - component resized
     */
    std::function<void (Range<double> timeRange)> updateComponentsForRange {nullptr};

    /* Updates timeline range.
       This would try keep viewport visible area if still valid in new range.
     */
    void setTimelineRange (Range<double> newRange);

    /* Sets new visible range to show.
     */
    void setVisibleRange (Range<double> newVisibleRange);

    /* Returns the viewport's timeline range in the relevant BaseUnit.
     */
    Range<double> getTimelineRange() const;

    /* Returns the visible range in the relevant BaseUnit.
     */
    Range<double> getVisibleRange() const { return componentsRange; }

    const TimelinePixelMapperBase& getPixelMapper() const { return *pixelMapper; }

    void setShouldClipBorders (bool shouldClip);

    bool anchorChildForTimeRange (const Range<double> entireRangeOfParent, const Range<double> visibleRangeOfParent, Component& componentToBound, const float absoluteWidth, bool anchorToEnd = true);

    void resized() override;
#ifdef JUCE_DEBUG
    void paint (Graphics& g) override;
#endif

    int getWidthExcludingBorders();
    int getHeightExcludingBorders();
private:
    void invalidateViewport();
    class ViewportClip : public Component
    {
    public:
        juce::Rectangle<int> transparentBounds;
        void paint (Graphics&) override;
    } viewportClip;
private:
    std::unique_ptr<TimelinePixelMapperBase> pixelMapper;
    BorderSize<int> viewportBorders;
    std::unique_ptr<ScrollBar> hScrollBar, vScrollBar;
    std::shared_ptr<Component> contentComp;
    // range of components currently visible
    Range<double> componentsRange;
    bool shouldClipBorders;
    int singleStepX = 16, singleStepY = 16;
    bool allowScrollH, allowScrollV;
};
