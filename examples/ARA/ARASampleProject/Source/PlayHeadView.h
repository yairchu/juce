#pragma once

#include "JuceHeader.h"

class ARASecondsPixelMapper;

class PlayHeadView  : public Component
{
public:
    PlayHeadView (const ARASecondsPixelMapper&);

    void setPlayHeadTimeInSec (double);

    void paint (Graphics&) override;

private:
    const ARASecondsPixelMapper& pixelMapper;
    double playHeadTimeInSec { 0 };
};
