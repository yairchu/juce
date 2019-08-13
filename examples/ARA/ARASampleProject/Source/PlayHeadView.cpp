#include "PlayHeadView.h"

#include "ARASecondsPixelMapper.h"

PlayHeadView::PlayHeadView (const ARASecondsPixelMapper& mapper)
    : pixelMapper (mapper)
{
    setInterceptsMouseClicks (false, true);
    setWantsKeyboardFocus (false);
}

void PlayHeadView::paint (Graphics &g)
{
    g.setColour (findColour (ScrollBar::ColourIds::thumbColourId));
    g.drawVerticalLine (pixelMapper.getPixelForPosition (playHeadTimeInSec), 0, getHeight());
}

void PlayHeadView::setPlayHeadTimeInSec (double x)
{
    playHeadTimeInSec = x;
    repaint();
}
