/*
  ==============================================================================

    TimelinePixelMapper.h
    A general purpose abstract class to allow conversion between timeline base-units to pixels on
    screen and vice-versa.

    Timeline - a display of a list of events in chronological order.
    BaseUnit - a linear unit for that particular timeline.
               some mappers might need to convert between multiple units.
               so
 
    Created: 15 Jan 2019 12:53:53pm
    Author:  Tal Aviram
    Copyright (c) 2019, Sound Radix LTD, All Rights Reserved.

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

class TimelinePixelMapperBase
{
public:
    /* Construct TimelinePixelMapperBase.
        Abstract class to describe relationship between timeline
        (only requirement from timeline is to be chronological)

        supportedTimelineRange - the range of time supported by this object.
                        notice! this can be greater than visible time of course.
     */
    TimelinePixelMapperBase (Range<double> supportedTimelineRange = {0.0, 0.0})
    : timelineRange (supportedTimelineRange) {}

    virtual ~TimelinePixelMapperBase() {}

    /* Returns human-readable description of the base unit.
     * (Eg. seconds, PPQ, meter, frame)
     */
    virtual String getBaseUnitDescription() const = 0;

    void setTimelineRange (Range<double> newRange) { timelineRange = newRange; }

    /* Returns the actual supported timeline range that can be mapped.
     */
    const Range<double> getTimelineRange() const { return timelineRange; }

    /* Sets the start position in BaseUnit for timelinePixelStart pixel.
     */
    void setStartPixelPosition (double newStartPosition) { pixelsStartPosition = newStartPosition; }
    double getStartPixelPosition() const { return pixelsStartPosition; }

    double getTimelineStartPosition() const { return timelineRange.getStart(); }
    double getTimelineEndPosition() const { return timelineRange.getEnd(); }

    /* Returns timeline's end in pixel.
     * this will return 0 if overflows.
     */
    int getTimelineEndPixel() const
    {
        // TODO: getPixelForPosition needs to become long sometime...
        long endPx = getPixelForPosition (timelineRange.getEnd());
        return jlimit (static_cast<long>(std::numeric_limits<int>::min()), static_cast<long>(std::numeric_limits<int>::max()), endPx) == endPx ? static_cast<int>(endPx) : 0;
    }

    /* Sets the zoom factor to be considered.
     */
    void setZoomFactor (double newZoomFactor)
    {
        jassert (newZoomFactor > 0.0 && std::isfinite (newZoomFactor));
        if (zoomFactor == newZoomFactor)
            return;

        zoomFactor = newZoomFactor;
        onZoomChanged();
    }

    double getZoomFactor() const { return zoomFactor; }

    /* Returns horizontal pixel (x) position closest
       to provided positionInBaseUnit.
       for negative values you should check if position is valid as position might
       be invalid for current pixels.
     */
    virtual int getPixelForPosition (double positionInBaseUnit) const = 0;

    /* @return position on timeline for X.
       timeline range *can* be negative.
     */
    virtual double getPositionForPixel (int pixelPosition) const = 0;

    /* Returns rightest pixel position within timeline for bounds.
     * - if timeline end < mapped position from bounds, returns last valid pixel position.
     * - if timeline end >= mapped position from bounds, returns currentBounds.getRight()
     */
    int getEndPixelForBoundsWithinTimeline (juce::Rectangle<int> currentBounds) const
    {
        int timelineEnd = getPixelForPosition (timelineRange.getEnd());
        return timelineEnd < currentBounds.getRight() ? timelineEnd : currentBounds.getRight();
    }

    /* Returns if pixel is within the timeline range.
     * isInclusiveEnd - if end is conisdered within bounds.
     */
    bool isPixelPositionWithinBounds (int pixelPosition, bool isInclusiveEnd = true) const
    {
        const auto pos = getPositionForPixel (pixelPosition);
        return timelineRange.contains (pos) || (isInclusiveEnd && round(pos) == timelineRange.getEnd());
    }

    /* Utility function to get range on timeline from pixels. */
    Range<double> getRangeForPixels (int startX, int endX) const
    {
        return { getPositionForPixel (startX), getPositionForPixel (endX) };
    }

protected:
    Range<double> timelineRange;
private:
    /* notifies when new zoomFactor is set */
    virtual void onZoomChanged() {}
    double pixelsStartPosition {0};
    double zoomFactor {1.0};
};
