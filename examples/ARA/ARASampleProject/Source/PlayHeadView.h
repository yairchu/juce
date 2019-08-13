#pragma once

#include "JuceHeader.h"

class TimelineViewport;

class PlayHeadView  : public Component
{
public:
    PlayHeadView (const TimelineViewport&);

    void setPlayHeadTimeInSec (double);
    virtual void updatePosition();

    void paint (Graphics&) override;
    void resized() override;

protected:
    bool getPixelPosition (int*) const;

    const TimelineViewport& timelineViewport;

private:
    double playHeadTimeInSec;
};
