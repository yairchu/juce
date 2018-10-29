#pragma once

#include "juce_ARARegionSequence.h"
#include "juce_ARAAudioSource.h"

namespace juce
{

#if JUCE_DEBUG
bool ARARegionSequence::stateUpdatePlaybackRegionProperties = false;
#endif

class ARARegionSequence::Reader : public AudioFormatReader
{
    friend class ARARegionSequence;

public:
    Reader (ARARegionSequence*, double sampleRate);
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

ARARegionSequence::ARARegionSequence (ARA::PlugIn::Document* document, ARA::ARARegionSequenceHostRef hostRef)
    : ARA::PlugIn::RegionSequence (document, hostRef)
{
    ref = new Ref (this);
    prevSequenceForNewPlaybackRegion = nullptr;
}

ARARegionSequence::~ARARegionSequence()
{
    ref->reset();
}

AudioFormatReader* ARARegionSequence::newReader (double sampleRate)
{
    return new Reader (this, sampleRate);
}

/*static*/ void ARARegionSequence::willUpdatePlaybackRegionProperties (
    ARA::PlugIn::PlaybackRegion* region,
    ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties> properties)
{
#if JUCE_DEBUG
    jassert (! stateUpdatePlaybackRegionProperties);
    stateUpdatePlaybackRegionProperties = true;
#endif

    ARARegionSequence* oldSequence = static_cast<ARARegionSequence*> (region->getRegionSequence());
    ARARegionSequence* newSequence = static_cast<ARARegionSequence*> (ARA::PlugIn::fromRef (properties->regionSequenceRef));
    jassert (newSequence != nullptr);
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

/*static*/ void ARARegionSequence::didUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion* region)
{
#if JUCE_DEBUG
    jassert (stateUpdatePlaybackRegionProperties);
    stateUpdatePlaybackRegionProperties = false;
#endif

    ARARegionSequence* newSequence = static_cast<ARARegionSequence*> (region->getRegionSequence());
    jassert (newSequence != nullptr);

    ARARegionSequence* oldSequence = newSequence->prevSequenceForNewPlaybackRegion;
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

bool ARARegionSequence::isSampleAccessEnabled() const
{
    Ref::ScopedAccess access (ref);
    for (auto& x : sourceRefCount)
        if (! x.first->isSampleAccessEnabled())
            return false;
    return true;
}

ARARegionSequence::Reader::Reader (ARARegionSequence* sequence, double sampleRate_)
    : AudioFormatReader (nullptr, "ARARegionSequenceReader")
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
        jassert (modification != nullptr);
        ARAAudioSource* source = static_cast<ARAAudioSource*> (modification->getAudioSource());
        jassert (source != nullptr);

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

ARARegionSequence::Reader::~Reader()
{
    for (auto& x : sourceReaders)
        delete x.second;
}

bool ARARegionSequence::Reader::readSamples (
    int** destSamples,
    int numDestChannels,
    int startOffsetInDestBuffer,
    int64 startSampleInFile,
    int numSamples)
{
    // Clear buffers
    for (int i = 0; i < numDestChannels; ++i)
        if (float* destBuf = (float*) destSamples[i])
            FloatVectorOperations::clear (destBuf + startOffsetInDestBuffer, numSamples);

    Ref::ScopedAccess sequence (ref, true);
    if (! sequence)
        return false;

    return renderPlaybackRegionsSamples (
        [=](ARA::PlugIn::PlaybackRegion* region, int64 startSampleInRegion, int numRegionSamples)
        {
            ARA::PlugIn::AudioSource* source = region->getAudioModification()->getAudioSource();
            if (source->getSampleRate() != sampleRate)
            {
                // Skip regions with wrong sample rate.
                buf_.clear (0, numRegionSamples);
                return true;
            }
            AudioFormatReader* sourceReader = sourceReaders_[source];
            jassert (sourceReader != nullptr);
            return sourceReader->read (
                (int**) buf_.getArrayOfWritePointers(),
                numDestChannels,
                region->getStartInAudioModificationSamples() + startSampleInRegion,
                numRegionSamples,
                false);
        },
        sequence->getPlaybackRegions(), sampleRate, &buf_,
        (float**) destSamples, numDestChannels, startOffsetInDestBuffer, startSampleInFile, numSamples);
}

} // namespace juce
