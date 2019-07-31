/*
  ==============================================================================

    ARAPixelMapper.cpp
    Created: 25 Feb 2019 4:37:05pm
    Author:  Tal Aviram, Sound Radix LTD

  ==============================================================================
*/

#include "ARASecondsPixelMapper.h"

ARASecondsPixelMapper::ARASecondsPixelMapper (const juce::AudioProcessorEditorARAExtension& extension)
: TimelinePixelMapperBase (),
  araExtension (extension),
  musicalContext (nullptr)
{
    document = extension.getARAEditorView()->getDocumentController()->getDocument<ARADocument>();
    document->addListener (this);
    findMusicalContext();
}

ARASecondsPixelMapper::~ARASecondsPixelMapper()
{
    detachFromMusicalContext();
    detachFromDocument();
}

String ARASecondsPixelMapper::getBaseUnitDescription() const
{
    // TODO JUCE_ARA:
    // currently we're showing timeline only in seconds.
    // eventually this might support also using PPQ as base unit.
    return "Seconds";
}

int ARASecondsPixelMapper::getPixelForPosition (double time) const
{
    return roundToInt ( (time - getStartPixelPosition()) * pixelsPerSecond);
}

double ARASecondsPixelMapper::getPositionForPixel (int pixelPosition) const
{
    return getStartPixelPosition() + (pixelPosition / pixelsPerSecond);
}

void ARASecondsPixelMapper::detachFromDocument()
{
    if (document == nullptr)
        return;
    
    document->removeListener (this);

    document = nullptr;
}

void ARASecondsPixelMapper::detachFromMusicalContext()
{
    if (musicalContext == nullptr)
        return;

    musicalContext->removeListener (this);

    musicalContext = nullptr;
}

void ARASecondsPixelMapper::findMusicalContext()
{
    // evaluate selection
    ARAMusicalContext* newMusicalContext = nullptr;
    auto viewSelection = araExtension.getARAEditorView()->getViewSelection();
    if (! viewSelection.getRegionSequences().empty())
        newMusicalContext = viewSelection.getRegionSequences().front()->getMusicalContext<ARAMusicalContext>();
    else if (! viewSelection.getPlaybackRegions().empty())
        newMusicalContext = viewSelection.getPlaybackRegions().front()->getRegionSequence()->getMusicalContext<ARAMusicalContext>();

    // if no context used yet and selection does not yield a new one, use the first musical context in the docment
    if (musicalContext == nullptr && newMusicalContext == nullptr && ! document->getMusicalContexts().empty())
        newMusicalContext = document->getMusicalContexts<ARAMusicalContext>().front();
    
    if (newMusicalContext != nullptr && newMusicalContext != musicalContext)
    {
        detachFromMusicalContext();

        musicalContext = newMusicalContext;
        musicalContext->addListener (this);
    }
    // TODO INVALIDATE
}


//==============================================================================

void ARASecondsPixelMapper::onNewSelection (const ARA::PlugIn::ViewSelection& /*viewSelection*/)
{
    findMusicalContext();
}

void ARASecondsPixelMapper::didEndEditing (ARADocument* /*doc*/)
{
    if (musicalContext == nullptr)
        findMusicalContext();
}

void ARASecondsPixelMapper::willRemoveMusicalContextFromDocument (ARADocument* doc, ARAMusicalContext* context)
{
    jassert (document == doc);
    
    if (musicalContext == context)
        detachFromMusicalContext();     // will restore in didEndEditing()
}

void ARASecondsPixelMapper::didReorderMusicalContextsInDocument (ARADocument* doc)
{
    jassert (document == doc);
    
    if (musicalContext != document->getMusicalContexts().front())
        detachFromMusicalContext();     // will restore in didEndEditing()
}
void ARASecondsPixelMapper::willDestroyDocument (ARADocument* doc)
{
    jassert (document == doc);
    
    detachFromDocument();
}

void ARASecondsPixelMapper::doUpdateMusicalContextContent (ARAMusicalContext* context, ARAContentUpdateScopes /*scopeFlags*/)
{
    jassert (musicalContext == context);
    // TODO INVALIDATE
}

double ARASecondsPixelMapper::getQuarterForTime (double timeInSeconds) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeTempoEntries tempoReader (musicalContext);
    return ARATempoConverter (tempoReader).getQuarterForTime (timeInSeconds);
}

double ARASecondsPixelMapper::getTimeForQuarter (double quarterPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeTempoEntries tempoReader (musicalContext);
    return ARATempoConverter (tempoReader).getTimeForQuarter (quarterPosition);
}

int ARASecondsPixelMapper::getPixelForQuarter (double quarterPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    return getPixelForPosition (getTimeForQuarter (quarterPosition));
}

double ARASecondsPixelMapper::getBeatForQuarter (double beatPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeBarSignatures barSignaturesReader (musicalContext);
    return ARABarSignaturesConverter (barSignaturesReader).getBeatForQuarter (beatPosition);
}

double ARASecondsPixelMapper::getQuarterForBeat (double quarterPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeBarSignatures barSignaturesReader (musicalContext);
    return ARABarSignaturesConverter (barSignaturesReader).getQuarterForBeat (quarterPosition);
}

ARA::ARAContentBarSignature ARASecondsPixelMapper::getBarSignatureForQuarter (double quarterPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeBarSignatures barSignaturesReader (musicalContext);
    return ARABarSignaturesConverter (barSignaturesReader).getBarSignatureForQuarter (quarterPosition);
}

double ARASecondsPixelMapper::getBeatDistanceFromBarStartForQuarter (double quarterPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeBarSignatures barSignaturesReader (musicalContext);
    return ARABarSignaturesConverter (barSignaturesReader).getBeatDistanceFromBarStartForQuarter (quarterPosition);
}

void ARASecondsPixelMapper::onZoomChanged()
{
    pixelsPerSecond = getZoomFactor();
}

int ARASecondsPixelMapper::getBarIndexForQuarter (ARA::ARAQuarterPosition quarterPosition) const
{
    // you shouldn't call this method before making sure there's musicalContext!
    jassert (canTempoMap());
    const ARAContentTypeBarSignatures barSignaturesReader (musicalContext);
    return ARABarSignaturesConverter (barSignaturesReader).getBeatForQuarter (quarterPosition);
}

bool ARASecondsPixelMapper::canTempoMap() const
{
    return musicalContext != nullptr;
}
