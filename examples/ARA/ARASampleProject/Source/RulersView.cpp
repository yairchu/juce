#include "RulersView.h"

#include "ARA_Library/Utilities/ARAPitchInterpretation.h"

//==============================================================================
RulersView::RulersView (TimelineViewport& timeline, AudioPlayHead::CurrentPositionInfo* hostPosition)
    : timeline (timeline),
      timeMapper (static_cast<const ARASecondsPixelMapper&>(timeline.getPixelMapper())),
      optionalHostPosition (hostPosition)
{
    lastPaintedPosition.resetToDefault();
    startTimerHz (10);
}

void RulersView::timerCallback()
{
    if (optionalHostPosition != nullptr && (lastPaintedPosition.ppqLoopStart != optionalHostPosition->ppqLoopStart ||
        lastPaintedPosition.ppqLoopEnd != optionalHostPosition->ppqLoopEnd ||
        lastPaintedPosition.isLooping  != optionalHostPosition->isLooping))
    {
        repaint();
    }
}

//==============================================================================
void RulersView::paint (juce::Graphics& g)
{
    const auto bounds = g.getClipBounds();

    g.setColour (Colours::lightslategrey);

    if (timeMapper.getCurrentMusicalContext() == nullptr)
    {
        g.setFont (Font (12.0f));
        g.drawText ("No musical context found in ARA document!", bounds, Justification::centred);
        return;
    }

    const auto visibleRange = timeline.getVisibleRange();

    // we'll draw three rulers: seconds, beats, and chords
    constexpr int lightLineWidth = 1;
    constexpr int heavyLineWidth = 3;
    const int chordRulerY = 0;
    const int chordRulerHeight = getBounds().getHeight() / 3;
    const int beatsRulerY = chordRulerY + chordRulerHeight;
    const int beatsRulerHeight = (getBounds().getHeight() - chordRulerHeight) / 2;
    const int secondsRulerY = beatsRulerY + beatsRulerHeight;
    const int secondsRulerHeight = getBounds().getHeight() - chordRulerHeight - beatsRulerHeight;

    // seconds ruler: one tick for each second
    if (true)
    {
        RectangleList<int> rects;
        const int endTime = roundToInt (floor (visibleRange.getEnd()));
        for (int time = roundToInt (ceil (visibleRange.getStart())); time <= endTime; ++time)
        {
            const int lineWidth = (time % 60 == 0) ? heavyLineWidth : lightLineWidth;
            const int lineHeight = (time % 10 == 0) ? secondsRulerHeight : secondsRulerHeight / 2;
            const int x = timeMapper.getPixelForPosition (time);
            rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, secondsRulerY + secondsRulerHeight - lineHeight, lineWidth, lineHeight));
        }
        g.fillRectList (rects);
    }
    g.setColour (juce::Colours::white);
    g.drawText ("seconds", bounds.withTrimmedRight (2), Justification::bottomRight);

    // beat ruler: evaluates tempo and bar signatures to draw a line for each beat
    if (timeMapper.canTempoMap())
    {
        RectangleList<int> rects;
        const double beatStart = timeMapper.getBeatForQuarter (timeMapper.getQuarterForTime (visibleRange.getStart()));
        const double beatEnd = timeMapper.getBeatForQuarter (timeMapper.getQuarterForTime (visibleRange.getEnd()));
        const int endBeat = roundToInt (floor (beatEnd));
        for (int beat = roundToInt (ceil (beatStart)); beat <= endBeat; ++beat)
        {
            const auto quarterPos = timeMapper.getQuarterForBeat (beat);
            const int x = timeMapper.getPixelForQuarter(quarterPos);
            const auto barSignature = timeMapper.getBarSignatureForQuarter (quarterPos);
            const int lineWidth = (quarterPos == barSignature.position) ? heavyLineWidth : lightLineWidth;
            const int beatsSinceBarStart = roundToInt( timeMapper.getBeatDistanceFromBarStartForQuarter (quarterPos));
            const int lineHeight = (beatsSinceBarStart == 0) ? beatsRulerHeight : beatsRulerHeight / 2;

            rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, beatsRulerY + beatsRulerHeight - lineHeight, lineWidth, lineHeight));
        }
        g.fillRectList (rects);
    }
    g.drawText ("beats", bounds.withTrimmedRight (2).withTrimmedBottom (secondsRulerHeight), Justification::bottomRight);

    // chord ruler: one rect per chord, skipping empty "no chords"
    if (timeMapper.canTempoMap())
    {
        RectangleList<int> rects;
        const ARA::ChordInterpreter interpreter;
        const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords> chordsReader (timeMapper.getCurrentMusicalContext());
        for (auto itChord = chordsReader.begin(); itChord != chordsReader.end(); ++itChord)
        {
            if (interpreter.isNoChord (*itChord))
                continue;

            Rectangle<int> chordRect = bounds;
            chordRect.setVerticalRange (Range<int> (chordRulerY, chordRulerY + chordRulerHeight));

            // find the starting position of the chord in pixels
            const auto chordStartTime = (itChord == chordsReader.begin()) ?
                                            timeline.getTimelineRange().getStart() : timeMapper.getTimeForQuarter (itChord->position);
            if (chordStartTime >= visibleRange.getEnd())
                break;
            chordRect.setLeft (timeMapper.getPixelForPosition (chordStartTime));

            // if we have a chord after this one, use its starting position to end our rect
            if (std::next(itChord) != chordsReader.end())
            {
                const auto nextChordStartTime = timeMapper.getTimeForQuarter (std::next (itChord)->position);
                if (nextChordStartTime < visibleRange.getStart())
                    continue;
                chordRect.setRight (timeMapper.getPixelForPosition (nextChordStartTime));
            }

            // draw chord rect and name
            g.drawRect (chordRect);
            g.drawText (convertARAString (interpreter.getNameForChord (*itChord).c_str()), chordRect.withTrimmedLeft (2), Justification::centredLeft);
        }
    }
    g.drawText ("chords", bounds.withTrimmedRight (2).withTrimmedBottom (beatsRulerHeight + secondsRulerHeight), Justification::bottomRight);

    // locators
    if (optionalHostPosition != nullptr)
    {
        lastPaintedPosition = *optionalHostPosition;
        const auto startInSeconds = timeMapper.getTimeForQuarter (lastPaintedPosition.ppqLoopStart);
        const auto endInSeconds = timeMapper.getTimeForQuarter (lastPaintedPosition.ppqLoopEnd);
        const int startX = timeMapper.getPixelForPosition (startInSeconds);
        const int endX = timeMapper.getPixelForPosition (endInSeconds);
        g.setColour (lastPaintedPosition.isLooping ? Colours::skyblue.withAlpha (0.3f) : Colours::white.withAlpha (0.3f));
        g.fillRect (startX, bounds.getY(), endX - startX, bounds.getHeight());
    }

    // borders
    {
        g.setColour (Colours::darkgrey);
        g.drawLine ((float) bounds.getX(), (float) beatsRulerY, (float) bounds.getRight(), (float) beatsRulerY);
        g.drawLine ((float) bounds.getX(), (float) secondsRulerY, (float) bounds.getRight(), (float) secondsRulerY);
        g.drawRect (bounds);
    }
}

//==============================================================================

void RulersView::mouseDown (const MouseEvent& event)
{
    // use mouse click to set the playhead position in the host (if they provide a playback controller interface)
    if (auto* musicalCtx = timeMapper.getCurrentMusicalContext())
    {
        auto playbackController = musicalCtx->getDocument()->getDocumentController()->getHostInstance()->getPlaybackController();
        if (playbackController != nullptr)
            playbackController->requestSetPlaybackPosition (timeMapper.getPositionForPixel( roundToInt (event.position.x)));
    }
}

void RulersView::mouseDoubleClick (const MouseEvent& /*event*/)
{
    // use mouse double click to start host playback (if they provide a playback controller interface)
    if (auto* musicalCtx = timeMapper.getCurrentMusicalContext())
    {
        auto playbackController = musicalCtx->getDocument()->getDocumentController()->getHostInstance()->getPlaybackController();
        if (playbackController != nullptr)
            playbackController->requestStartPlayback();
    }
}
