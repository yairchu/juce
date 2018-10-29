#pragma once

#include "juce_ARA_audio_plugin.h"
#include "juce_SafeRef.h"

namespace juce
{

class ARARegionSequence : public ARA::PlugIn::RegionSequence
{
public:
    ARARegionSequence (ARA::PlugIn::Document*, ARA::ARARegionSequenceHostRef);
    ~ARARegionSequence();

    // If not given a `sampleRate` will figure it out from the first playback region within.
    // Playback regions with differing sample rates will be ignored.
    // Future alternative could be to perform resampling.
    AudioFormatReader* newReader (double sampleRate = 0.0);

    // These methods need to be called by the document controller in its corresponding methods:
    static void willUpdatePlaybackRegionProperties (
        ARA::PlugIn::PlaybackRegion*,
        ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties>);
    static void didUpdatePlaybackRegionProperties (ARA::PlugIn::PlaybackRegion*);

    // Is sample access enabled in all audio sources in sequence?
    bool isSampleAccessEnabled() const;

private:
    class Reader;
    typedef SafeRef<ARARegionSequence> Ref;

    Ref::Ptr ref;

    std::map<ARA::PlugIn::AudioSource*, int> sourceRefCount;

    // Used to unlock old sequence for region in `didUpdatePlaybackRegionProperties`.
    ARARegionSequence* prevSequenceForNewPlaybackRegion;

#if JUCE_DEBUG
    static bool stateUpdatePlaybackRegionProperties;
#endif
};

// Utility function for rendering samples for a vector of `PlaybackRegion`s.
// Vectors of `PlaybackRegion`s occur not just in `RegionSequence`,
// but also in `PlaybackRenderer`s.
//
// `renderRegion` should be a function of type
// `bool(ARA::PlugIn::PlaybackRegion* region, int64 startSampleInRegion, int numRegionSamples)`
// which should render the requested region samples into `tmpBuf`,
// and return `true` on success or `false` on failure.
//
// Note that `renderRegion` is in charge of behavior across sample-rates,
// it may either fail, fill the buffer in zeros (ignoring the region), or perform SRC.
//
// (This is also used for `ARARegionSequence`'s reader)
template<typename RenderRegionSamples>
bool renderARAPlaybackRegionsSamples (
    RenderRegionSamples renderRegion,
    std::vector<ARA::PlugIn::PlaybackRegion*> const& playbackRegions,
    double sampleRate,
    AudioSampleBuffer* tmpBuf,
    float** destSamples,
    int numDestChannels,
    int startOffsetInDestBuffer,
    int64 startSampleInFile,
    int numSamples)
{
    if (tmpBuf->getNumSamples() < numSamples || tmpBuf->getNumChannels() < numDestChannels)
        tmpBuf->setSize (numDestChannels, numSamples, false, false, true);

    // Clear buffers
    for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
    {
        if (destSamples[chan_i] == nullptr)
            continue;
        FloatVectorOperations::clear (destSamples[chan_i] + startOffsetInDestBuffer, numSamples);
    }

    const double start = (double) startSampleInFile / sampleRate;
    const double stop = (double) (startSampleInFile + (int64) numSamples) / sampleRate;

    // Fill in content from relevant regions
    for (ARA::PlugIn::PlaybackRegion* region : playbackRegions)
    {
        if (region->getEndInPlaybackTime() <= start || region->getStartInPlaybackTime() >= stop)
            continue;

        const int64 regionStartSample = region->getStartInPlaybackSamples (sampleRate);

        const int64 startSampleInRegion = std::max ((int64) 0, startSampleInFile - regionStartSample);
        const int destOffest = (int) std::max ((int64) 0, regionStartSample - startSampleInFile);
        const int numRegionSamples = std::min (
                (int) (region->getDurationInPlaybackSamples (sampleRate) - startSampleInRegion),
                numSamples - destOffest);

        if (! renderRegion (region, startSampleInRegion, numRegionSamples))
            return false;

        for (int chan_i = 0; chan_i < numDestChannels; ++chan_i)
        {
            if (destSamples[chan_i] == nullptr)
                continue;
            FloatVectorOperations::add (
                destSamples[chan_i] + startOffsetInDestBuffer + destOffest,
                tmpBuf->getReadPointer (chan_i), numRegionSamples);
        }
    }

    return true;
}

} // namespace juce
