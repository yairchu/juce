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

void ARARegionSequence::willUpdateRegionSequenceProperties (ARARegionSequence::PropertiesPtr newProperties) noexcept
{
    listeners.call ([this, &newProperties] (Listener& l) { l.willUpdateRegionSequenceProperties (this, newProperties); });
}

void ARARegionSequence::didUpdateRegionSequenceProperties () noexcept
{
    listeners.call ([this] (Listener& l) { l.didUpdateRegionSequenceProperties (this); });
}

void ARARegionSequence::willDestroyRegionSequence () noexcept
{
    // TODO JUCE_ARA 
    // same concerns involving removal as with other listeners
    auto listenersCopy (listeners.getListeners ());
    for (auto listener : listenersCopy)
    {
        if (listeners.contains (listener))
            listener->willDestroyRegionSequence (this);
    }
}

void ARARegionSequence::didAddPlaybackRegionToRegionSequence (ARAPlaybackRegion* playbackRegion) noexcept
{
    listeners.call ([this, playbackRegion] (Listener& l) { l.didAddPlaybackRegionToRegionSequence (this, playbackRegion); });
}

void ARARegionSequence::willRemovePlaybackRegionFromRegionSequence (ARAPlaybackRegion* playbackRegion) noexcept
{
    listeners.call ([this, playbackRegion] (Listener& l) { l.willRemovePlaybackRegionFromRegionSequence (this, playbackRegion); });
}

void ARARegionSequence::addListener (Listener * l)
{
    listeners.add (l);
}

void ARARegionSequence::removeListener (Listener * l)
{
    listeners.remove (l);
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
        ARA::PlugIn::AudioSource* source = modification->getAudioSource();

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
            sourceReaders[source] = new ARAAudioSourceReader (source);
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
