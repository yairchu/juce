/*
  ==============================================================================

    ARAPixelMapper.h
    Created: 25 Feb 2019 4:37:05pm
    Author:  Tal Aviram, Sound Radix LTD

  ==============================================================================
*/

#pragma once

#include "TimelineViewport/TimelinePixelMapper.h"
#include "ARA_Library/Utilities/ARATimelineConversion.h"

class ARASecondsPixelMapper : public TimelinePixelMapperBase,
                              private ARAEditorView::Listener,
                              private ARADocument::Listener,
                              private ARAMusicalContext::Listener
{
public:
    ARASecondsPixelMapper (const AudioProcessorEditorARAExtension& araExtension);
    ~ARASecondsPixelMapper() override;

    // TimelinePixelMapperBase
    String getBaseUnitDescription() const override;
    int getPixelForPosition (double positionInBaseUnit) const override;
    double getPositionForPixel (int pixelPosition) const override;

    // ARA Utility Mapping
    bool canTempoMap() const;
    int getPixelForQuarter (double quarterPosition) const;
    double getQuarterForTime (double timeInSeconds) const;
    double getTimeForQuarter (double quarterPosition) const;
    double getBeatForQuarter (double beatPosition) const;
    double getQuarterForBeat (double quarterPosition) const;
    ARA::ARAContentBarSignature getBarSignatureForQuarter (double quarterPosition) const;
    double getBeatDistanceFromBarStartForQuarter (double quarterPosition) const;
    int getBarIndexForQuarter (ARA::ARAQuarterPosition quarterPosition) const;

    // may return nullptr
    ARAMusicalContext* getCurrentMusicalContext() const { return musicalContext; }

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection) override;

    // ARADocument::Listener overrides
    void didEndEditing (ARADocument* document) override;
    void willRemoveMusicalContextFromDocument (ARADocument* document, ARAMusicalContext* musicalContext) override;
    void didReorderMusicalContextsInDocument (ARADocument* document) override;
    void willDestroyDocument (ARADocument* document) override;

    // ARAMusicalContext::Listener overrides
    void didUpdateMusicalContextContent (ARAMusicalContext* context, ARAContentUpdateScopes scopeFlags) override;
private:
    void onZoomChanged() override;
    void detachFromDocument();
    void detachFromMusicalContext();
    void findMusicalContext();
private:
    const AudioProcessorEditorARAExtension& araExtension;
    ARAMusicalContext* musicalContext;
    ARADocument* document;

    // ARA Converters
    typedef ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> ARAContentTypeBarSignatures;
    typedef ARA::BarSignaturesConverter<const ARAContentTypeBarSignatures> ARABarSignaturesConverter;
    typedef ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> ARAContentTypeTempoEntries;
    typedef ARA::TempoConverter<const ARAContentTypeTempoEntries> ARATempoConverter;

    double pixelsPerSecond {1.0};
};
