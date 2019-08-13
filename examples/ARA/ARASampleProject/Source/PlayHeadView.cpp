#include "PlayHeadView.h"

#include "TimelineViewport/TimelineViewport.h"

PlayHeadView::PlayHeadView (const TimelineViewport& viewport)
    : timelineViewport (viewport)
{
    setInterceptsMouseClicks (false, true);
    setWantsKeyboardFocus (false);
    setPlayHeadTimeInSec (0);
}

void PlayHeadView::paint (Graphics &g)
{
    g.fillAll (findColour (ScrollBar::ColourIds::thumbColourId));
}

void PlayHeadView::resized()
{
    updatePosition();
}

void PlayHeadView::setPlayHeadTimeInSec (double x)
{
    playHeadTimeInSec = x;
    updatePosition();
}

bool PlayHeadView::getPixelPosition (int* resPos) const
{
    const auto& mapper = timelineViewport.getPixelMapper();
    const int pos = mapper.getPixelForPosition (playHeadTimeInSec);
    if (! mapper.getRangeForPixels (pos-1, pos+1).contains (playHeadTimeInSec) ||
        0 > pos || pos >= timelineViewport.getWidthExcludingBorders())
        return false;
    *resPos = pos;
    return true;
}

void PlayHeadView::updatePosition()
{
    int pos;
    setVisible (getPixelPosition (&pos));
    if (isVisible())
        setBounds (
            timelineViewport.getViewedComponentBorders().getLeft() + pos,
            getY(), 1, getHeight());
}
