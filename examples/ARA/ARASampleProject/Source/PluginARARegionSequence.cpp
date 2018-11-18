#pragma once

#include "PluginARARegionSequence.h"

namespace juce
{

#if JUCE_DEBUG
 bool ARASampleProjectRegionSequence::stateUpdatePlaybackRegionProperties = false;
#endif

class ARASampleProjectRegionSequence::Reader : public AudioFormatReader
{
    friend class ARASampleProjectRegionSequence;

public:
    Reader (ARASampleProjectRegionSequence*, double sampleRate);
    virtual ~Reader();

    bool readSamples (
        int** destSamples,
        int numDestChannels,
        int startOffsetInDestBuffer,
        int64 startSampleInFile,
        int numSamples) override;

private:
    Ref::Ptr ref;
    std::map<ARA::PlugIn::AudioSource*, AudioFormatReader*> sourceReaders;
    AudioSampleBuffer sampleBuffer;
};

ARASampleProjectRegionSequence::ARASampleProjectRegionSequence (ARA::PlugIn::Document* document, ARA::ARARegionSequenceHostRef hostRef)
    : ARA::PlugIn::RegionSequence (document, hostRef)
{
    ref = new Ref (this);
    prevSequenceForNewPlaybackRegion = nullptr;
}

ARASampleProjectRegionSequence::~ARASampleProjectRegionSequence()
{
    ref->reset();
}

AudioFormatReader* ARASampleProjectRegionSequence::newReader (double sampleRate)
{
    return new Reader (this, sampleRate);
}

/*static*/ void ARASampleProjectRegionSequence::willUpdatePlaybackRegionProperties (
    ARA::PlugIn::PlaybackRegion* region,
    ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties> properties)
{
   #if JUCE_DEBUG
    jassert (! stateUpdatePlaybackRegionProperties);
    stateUpdatePlaybackRegionProperties = true;
   #endif

    ARASampleProjectRegionSequence* oldSequence = static_cast<ARASampleProjectRegionSequence*> (region->getRegionSequence());
    ARASampleProjectRegionSequence* newSequence = static_cast<ARASampleProjectRegionSequence*> (ARA::PlugIn::fromRef (properties->regionSequenceRef));
    jassert (newSequence->prevSequenceForNewPlaybackRegion == nullptr);

    newSequence->ref->reset();
    newSequence->prevSequenceForNewPlaybackRegion = oldSequence;

    if (oldSequence != nullptr && oldSequence != newSequence)
    {
        oldSequence->ref->reset();
        auto it = oldSequence->sourceRefCount.find (region->getAudioModification()->getAudioSource());
        --it->second;
        if (it->second == 0)
            oldSequence->sourceRefCount.erase (it);
    }
}

/*static*/ void ARASampleProjectRegionSequence::didUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion* region)
{
   #if JUCE_DEBUG
    jassert (stateUpdatePlaybackRegionProperties);
    stateUpdatePlaybackRegionProperties = false;
   #endif

    ARASampleProjectRegionSequence* newSequence = static_cast<ARASampleProjectRegionSequence*> (region->getRegionSequence());
    ARASampleProjectRegionSequence* oldSequence = newSequence->prevSequenceForNewPlaybackRegion;
    newSequence->prevSequenceForNewPlaybackRegion = nullptr;

    auto* source = region->getAudioModification()->getAudioSource();
    jassert (source != nullptr);

    if (newSequence != oldSequence)
    {
        if (oldSequence != nullptr)
            oldSequence->ref = new Ref (oldSequence);
        ++newSequence->sourceRefCount[source];
    }

    newSequence->ref = new Ref (newSequence);
}

bool ARASampleProjectRegionSequence::isSampleAccessEnabled() const
{
    Ref::ScopedAccess access (ref);
    for (auto& x : sourceRefCount)
        if (! x.first->isSampleAccessEnabled())
            return false;
    return true;
}

ARASampleProjectRegionSequence::Reader::Reader (ARASampleProjectRegionSequence* sequence, double sampleRate_)
    : AudioFormatReader (nullptr, "ARASampleProjectRegionSequenceReader")
    , ref (sequence->ref)
{
    bitsPerSample = 32;
    usesFloatingPointData = true;
    numChannels = 0;
    lengthInSamples = 0;
    sampleRate = sampleRate_;

    Ref::ScopedAccess access (ref);
    jassert (access);
    for (ARA::PlugIn::PlaybackRegion* region : sequence->getPlaybackRegions())
    {
        ARA::PlugIn::AudioModification* modification = region->getAudioModification();
        ARAAudioSource* source = static_cast<ARAAudioSource*> (modification->getAudioSource());

        if (sampleRate == 0.0)
            sampleRate = source->getSampleRate();

        if (sampleRate != source->getSampleRate())
        {
            // Skip regions with mis-matching sample-rates!
            continue;
        }

        if (sourceReaders.find (source) == sourceReaders.end())
        {
            numChannels = std::max (numChannels, (unsigned int) source->getChannelCount());
            sourceReaders[source] = source->newReader();
        }

        lengthInSamples = std::max (lengthInSamples, region->getEndInPlaybackSamples (sampleRate));
    }
}

ARASampleProjectRegionSequence::Reader::~Reader()
{
    for (auto& x : sourceReaders)
        delete x.second;
}

bool ARASampleProjectRegionSequence::Reader::readSamples (
    int** destSamples,
    int numDestChannels,
    int startOffsetInDestBuffer,
    int64 startSampleInFile,
    int numSamples)
{
    Ref::ScopedAccess sequence (ref, true);
    if (! sequence)
        return false;

    return renderARAPlaybackRegionsSamples (
        [=](ARA::PlugIn::PlaybackRegion* region, int64 startSampleInRegion, int numRegionSamples)
        {
            ARA::PlugIn::AudioSource* source = region->getAudioModification()->getAudioSource();
            if (source->getSampleRate() != sampleRate)
            {
                // Skip regions with wrong sample rate.
                sampleBuffer.clear (0, numRegionSamples);
                return true;
            }
            AudioFormatReader* sourceReader = sourceReaders[source];
            jassert (sourceReader != nullptr);
            return sourceReader->read (
                (int**) sampleBuffer.getArrayOfWritePointers(),
                numDestChannels,
                region->getStartInAudioModificationSamples() + startSampleInRegion,
                numRegionSamples,
                false);
        },
        sequence->getPlaybackRegions(), sampleRate, &sampleBuffer,
        (float**) destSamples, numDestChannels, startOffsetInDestBuffer, startSampleInFile, numSamples);
}

} // namespace juce
