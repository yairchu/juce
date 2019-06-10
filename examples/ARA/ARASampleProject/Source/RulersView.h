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
    RulersView (TimelineViewport& timeline, const AudioPlayHead::CurrentPositionInfo* optionalHostPosition = nullptr);

    enum ColourIds
    {
        rulersBackground = 0x10A8A01
    };

    void addRulerComponent (Component* rulerToOwn);

    // Remove rulers, to add custom rulers instead.
    void clear();

    void addDefaultRulers();
    int getNumOfRulers() { return rulers.size(); }

    void paint (Graphics&) override;
    void resized() override;

    void setIsLocatorsVisible (bool isVisible) { shouldShowLocators = isVisible; }
    bool isLocatorsVisible() { return shouldShowLocators; }

    // MouseListener overrides
    void mouseDown (const MouseEvent& event) override;
    void mouseDoubleClick (const MouseEvent& event) override;

    // juce::Timer overrides
    void timerCallback() override;

    // ARA Default Rulers
    class ARASelectionRuler : public Component
    {
    public:
        ARASelectionRuler (const RulersView& rulersView);
        void paint (Graphics&);
    private:
        const RulersView& rulersView;
    };

    class ARASecondsRuler : public Component
    {
    public:
        ARASecondsRuler (const RulersView& rulersView);
        void paint (Graphics&) override;
    private:
        const RulersView& rulersView;
    };

    class ARABeatsRuler : public Component
    {
    public:
        ARABeatsRuler (const RulersView& rulersView);
        void paint (Graphics&);
    private:
        const RulersView& rulersView;
    };

    class ARAChordsRuler : public Component
    {
    public:
        ARAChordsRuler (const RulersView& rulersView);
        void paint (Graphics&);
    private:
        const RulersView& rulersView;
    };

    /* Returns the width of the header area.
     */
    int getRulerHeaderWidth() const { return timeline.getViewedComponentBorders().getLeft(); }

private:
    TimelineViewport& timeline;
    const ARASecondsPixelMapper& timeMapper;
    ARADocument* document;
    AudioPlayHead::CurrentPositionInfo lastPaintedPosition;
    const AudioPlayHead::CurrentPositionInfo* optionalHostPosition;
    bool shouldShowLocators;
    OwnedArray<Component> rulers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RulersView)
};
