/*
  ==============================================================================

    TimelinePixelMapper.h
    A general purpose object to allow conversion between timeline to pixels on
    screen and vice-versa.
    Created: 15 Jan 2019 12:53:53pm
    Author:  Tal Aviram, Sound Radix LTD

  ==============================================================================
*/

#pragma once

class TimelinePixelMapper
{
public:
    // Timeline can be shown in two ways.
    // - TIME_LINEAR: this corresponds to clock time.
    //                every second has the same proportion in pixels.
    // - BEAT_LINEAR: this corresponds to musical time.
    //                every pulse (ppq) has the same proportion in pixels.
    //
    enum Timebase { TIME_LINEAR, BEAT_LINEAR };

    TimelinePixelMapper (Type, Range<double> timelineRange, double pixelsPerTimebase = 1.0);
    virtual ~TimelinePixelMapper();

    /* Gets this timeline's timebase.
     * It can be relative to time (seconds) or relative to music/beat (ppq).
     */
    Timebase getTimebase() const { return timebase; }

    /* To keep it optimized. TimelinePixelMapper should contain *only*
     * relevant range.
     * Range can be in ppq's or seconds.
     */
    Range<double> getTimelineRange();

    /* Sets new pixels per time value.
     * Time can be ppq or second.
     * You should check by @see getTimebase()
     */
    void setPixelsPerTimebase (double newPixelsPerTimebase);

    /* Returns pixels per timebase.
     * To check if timebase is ppq or second @see getTimebase()
     */
    double getPixelsPerTimebase() const { return pixelsPerTimebase; }

    /* Returns horizontal pixel (x) position closest
       to request timePosition.
       It should return -1 for out of range (timeline) values.
       timePosition should be in ppq's or seconds (@see getTimebase())
     */
    virtual int getXforTime (double timePosition) const = 0;

    /* @return position on timeline for X.
       timeline range *can* be negative.
     */
    virtual double getTimeForX (int x) const = 0;

    /* Returns true if timePosition is within timeline range
       and can be mapped to pixel range.
     */
    bool isValidTime (double timePosition) = 0;

    /* Returns true if pixel position
       can be converted to time properly or false
       if x is out of timelineRange bounds.
     */
    bool isValidTimeForX (int x) = 0;

    /* Utility function to get sample position from time */
    static double convertToSamples (double timeInSeconds, double sampleRate);
private:
    Timebase timebase;
    Range<double> timelineRage;
    double pixelsPerTimebase;
};
