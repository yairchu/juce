#pragma once

#include "JuceHeader.h"

#include "TimelineViewport/TimelineViewport.h"
#include "ARASecondsPixelMapper.h"

//==============================================================================
/**
    RulersView
    JUCE component used to display rulers for song time (in seconds and musical beats) and chords
*/
class RulersView  : public Component,
                    private juce::Timer
{
public:
    RulersView (TimelineViewport& timeline, AudioPlayHead::CurrentPositionInfo* optionalHostPosition = nullptr);

    void paint (Graphics&) override;

    // MouseListener overrides
    void mouseDown (const MouseEvent& event) override;
    void mouseDoubleClick (const MouseEvent& event) override;

    // juce::Timer overrides
    void timerCallback() override;

private:
    TimelineViewport& timeline;
    const ARASecondsPixelMapper& timeMapper;
    ARADocument* document;
    AudioPlayHead::CurrentPositionInfo lastPaintedPosition;
    AudioPlayHead::CurrentPositionInfo* optionalHostPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RulersView)
};
