#pragma once

#include "juce_ARA_audio_plugin.h"

namespace juce
{

class ARADocumentController;
class ARADocument;
class ARAMusicalContext;
class ARARegionSequence;
class ARAAudioSource;
class ARAAudioModification;
class ARAPlaybackRegion;

template<class ModelClassType>
class ARAListenableModelClass
{
public:
    class Listener
    {
    public:
        /** Default Listener constructor */
        Listener() = default;
        /** Does not remove listener, you must do this yourself. */
        virtual ~Listener() = default;
    };

    ARAListenableModelClass() = default;
    virtual ~ARAListenableModelClass() = default;

    /** Subscribe \p l to notified by changes to the object.
        @param l The listener instance. 
    */
    inline void addListener (Listener* l) { listeners.add (l); }

    /** Unsubscribe \p l from object notifications.
        @param l The listener instance.
    */
    inline void removeListener (Listener* l) { listeners.remove (l); }

    template<typename Callback>
    inline void notifyListeners (Callback&& callback)
    {
        reinterpret_cast<ListenerList<typename ModelClassType::Listener>*> (&listeners)->callExpectingUnregistration (callback);
    }

private:
    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAListenableModelClass)
};

//==============================================================================
/**
    Base class representing an ARA document. 

    @tags{ARA}
*/
class ARADocument: public ARA::PlugIn::Document,
                   public ARAListenableModelClass<ARADocument>
{
public:
    using PropertiesPtr = ARA::PlugIn::PropertiesPtr<ARA::ARADocumentProperties>;

    ARADocument (ARADocumentController* documentController);

    class Listener  : public ARAListenableModelClass<ARADocument>::Listener
    {
    public:
       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        /** Called before the document enters an editing state.
            @param document The document about to enter an editing state. 
        */
        virtual void willBeginEditing (ARADocument* document) {}

        /** Called after the document exits an editing state.
            @param document The document about exit an editing state. 
        */
        virtual void didEndEditing (ARADocument* document) {}

        /** Called before the document's properties are updated.
            @param document The document whose properties will be updated. 
            @param newProperties The document properties that will be assigned to \p document. 
        */
        virtual void willUpdateDocumentProperties (ARADocument* document, ARADocument::PropertiesPtr newProperties) {}

        /** Called after the document's properties are updated.
            @param document The document whose properties were updated. 
        */
        virtual void didUpdateDocumentProperties (ARADocument* document) {}

        /** Called after a musical context is added to the document.
            @param document The document that \p musicalContext was added to. 
            @param musicalContext The musical context that was added to \p document. 
        */
        virtual void didAddMusicalContextToDocument (ARADocument* document, ARAMusicalContext* musicalContext) {}

        /** Called before a musical context is removed from the document.
            @param document The document that \p musicalContext will be removed from.
            @param musicalContext The musical context that will be removed from \p document.
        */
        virtual void willRemoveMusicalContextFromDocument (ARADocument* document, ARAMusicalContext* musicalContext) {}

        /** Called after the musical contexts are reordered in an ARA document

            Musical contexts are sorted by their order index - 
            this callback signals a change in this ordering within the document. 
            
            @param document The document with reordered musical contexts. 
        */
        virtual void didReorderMusicalContextsInDocument (ARADocument* document) {}

        /** Called after a region sequence is added to the document.
            @param document The document that \p regionSequence was added to. 
            @param regionSequence The region sequence that was added to \p document. 
        */
        virtual void didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* regionSequence) {}

        /** Called before a region sequence is removed from the document.
            @param document The document that \p regionSequence will be removed from.
            @param regionSequence The region sequence that will be removed from \p document.
        */
        virtual void willRemoveRegionSequenceFromDocument (ARADocument* document, ARARegionSequence* regionSequence) {}

        /** Called after the region sequences are reordered in an ARA document

            Region sequences are sorted by their order index -
            this callback signals a change in this ordering within the document.

            @param document The document with reordered region sequences.
        */
        virtual void didReorderRegionSequencesInDocument (ARADocument* document) {}

        /** Called after an audio source is added to the document.
            @param document The document that \p audioSource was added to.
            @param audioSource The audio source that was added to \p document.
        */
        virtual void didAddAudioSourceToDocument (ARADocument* document, ARAAudioSource* audioSource) {}

        /** Called before an audio source is removed from the document.
            @param document The document that \p audioSource will be removed from .
            @param audioSource The audio source that will be removed from \p document.
        */
        virtual void willRemoveAudioSourceFromDocument (ARADocument* document, ARAAudioSource* audioSource) {}

        /** Called before the document is destroyed by the ARA host.
            @param document The document that will be destroyed. 
        */
        virtual void willDestroyDocument (ARADocument* document) {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };
};


//==============================================================================
/**
    Base class representing an ARA musical context.

    @tags{ARA}
*/
class ARAMusicalContext  : public ARA::PlugIn::MusicalContext,
                           public ARAListenableModelClass<ARAMusicalContext>
{
public:
    using PropertiesPtr = ARA::PlugIn::PropertiesPtr<ARA::ARAMusicalContextProperties>;

    ARAMusicalContext (ARADocument* document, ARA::ARAMusicalContextHostRef hostRef);

    class Listener  : public ARAListenableModelClass<ARAMusicalContext>::Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        /** Called before the musical context's properties are updated.
            @param musicalContext The musical context whose properties will be updated. 
            @param newProperties The musical context properties that will be assigned to \p musicalContext. 
        */
        virtual void willUpdateMusicalContextProperties (ARAMusicalContext* musicalContext, ARAMusicalContext::PropertiesPtr newProperties) {}

        /** Called after the musical context's properties are updated by the host.
            @param musicalContext The musical context whose properties were updated.
        */
        virtual void didUpdateMusicalContextProperties (ARAMusicalContext* musicalContext) {}

        /** Called when the musical context's content changes.

            Use this notification to respond to changes in musical context content
            (i.e tempo entries or chord changes). This notification is triggered by the ARA host.

            @param musicalContext The musical context with updated content
            @param scopeFlags The scope of the content update indicating what has changed.
        */
        virtual void doUpdateMusicalContextContent (ARAMusicalContext* musicalContext, ARAContentUpdateScopes scopeFlags) {}

        /** Called before the musical context is destroyed.
            @param musicalContext The musical context that will be destoyed. 
        */
        virtual void willDestroyMusicalContext (ARAMusicalContext* musicalContext) {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };
};


//==============================================================================
/**
    Base class representing an ARA region sequence. 

    @tags{ARA}
*/
class ARARegionSequence  : public ARA::PlugIn::RegionSequence,
                           public ARAListenableModelClass<ARARegionSequence>
{
public:
    using PropertiesPtr = ARA::PlugIn::PropertiesPtr<ARA::ARARegionSequenceProperties>;

    ARARegionSequence (ARADocument* document, ARA::ARARegionSequenceHostRef hostRef);

    class Listener  : public ARAListenableModelClass<ARARegionSequence>::Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        /** Called before the region sequence's properties are updated.
            @param regionSequence The region sequence whose properties will be updated. 
            @param newProperties The region sequence properties that will be assigned to \p regionSequence. 
        */
        virtual void willUpdateRegionSequenceProperties (ARARegionSequence* regionSequence, ARARegionSequence::PropertiesPtr newProperties) {}

        /** Called after the region sequence's properties are updated.
            @param regionSequence The region sequence whose properties were updated.
        */
        virtual void didUpdateRegionSequenceProperties (ARARegionSequence* regionSequence) {}

        /** Called before a playback region is removed from the region sequence.
            @param regionSequence The region sequence that \p playbackRegion will be removed from.
            @param playbackRegion The playback region that will be removed from \p regionSequence.
        */
        virtual void willRemovePlaybackRegionFromRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) {}

        /** Called after a playback region is added to the region sequence.
            @param regionSequence The region sequence that \p playbackRegion was added to.
            @param playbackRegion The playback region that was added to \p regionSequence.
        */
        virtual void didAddPlaybackRegionToRegionSequence (ARARegionSequence* regionSequence, ARAPlaybackRegion* playbackRegion) {}

        /** Called before the region sequence is destroyed.
            @param regionSequence The region sequence that will be destoyed.
        */
        virtual void willDestroyRegionSequence (ARARegionSequence* regionSequence) {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    /** Returns time range covered by the regions in this sequence. 
        @param includeHeadAndTail Whether or not the range includes the playback region's head and tail time. 
    */
    Range<double> getTimeRange (bool includeHeadAndTail = false) const;

    /** If all audio sources used by the playback regions in this region sequence have the
        same sample rate, this rate is returned here, otherwise 0.0 is returned.
        If the region sequence has no playback regions, this also returns 0.0.
     */
    double getCommonSampleRate() const;
};

//==============================================================================
/**
    Base class representing an ARA audio source.

    @tags{ARA}
*/
class ARAAudioSource  : public ARA::PlugIn::AudioSource,
                        public ARAListenableModelClass<ARAAudioSource>
{
public:
    using PropertiesPtr = ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties>;

    ARAAudioSource (ARADocument* document, ARA::ARAAudioSourceHostRef hostRef);

    class Listener  : public ARAListenableModelClass<ARAAudioSource>::Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        /** Called before the audio source's properties are updated.
            @param audioSource The audio source whose properties will be updated. 
            @param newProperties The audio source properties that will be assigned to \p audioSource.  
        */
        virtual void willUpdateAudioSourceProperties (ARAAudioSource* audioSource, ARAAudioSource::PropertiesPtr newProperties) {}

        /** Called after the audio source's properties are updated.
            @param audioSource The audio source whose properties were updated.
        */
        virtual void didUpdateAudioSourceProperties (ARAAudioSource* audioSource) {}

        /** Called when the audio source's analysis progress changes.

            Note that this may be triggered internally by the plug-in, in which case it may be called
            outside of a host edit cycle - see ARADocumentController::notifyAudioSourceAnalysisProgress().
        */
        virtual void doUpdateAudioSourceAnalysisProgress (ARAAudioSource* audioSource, ARA::ARAAnalysisProgressState state, float value) {}

        /** Called when the audio source's content changes.
            @param audioSource The audio source with updated content
            @param scopeFlags The scope of the content update.

            Note that this may be triggered internally by the plug-in, in which case it may be called
            outside of a host edit cycle - see ARADocumentController::notifyAudioSourceContentChanged().
        */
        virtual void doUpdateAudioSourceContent (ARAAudioSource* audioSource, ARAContentUpdateScopes scopeFlags) {}

        /** Called before access to an audio source's samples is enabled or disabled.
            @param audioSource The audio source whose sample access state will be changed.
            @param enable A bool indicating whether or not sample access will be enabled or disabled.
        */
        virtual void willEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) {}

        /** Called after access to an audio source's samples is enabled or disabled.
            @param audioSource The audio source whose sample access state was changed. 
            @param enable A bool indicating whether or not sample access was enabled or disabled.
        */
        virtual void didEnableAudioSourceSamplesAccess (ARAAudioSource* audioSource, bool enable) {}

        /** Called after an audio source is activated or deactivated when being removed / added from the host's undo history.
            @param audioSource The audio source that was activated or deactivated
            @param deactivate A bool indicating whether \p audioSource was deactivated or activated.
        */
        virtual void doDeactivateAudioSourceForUndoHistory (ARAAudioSource* audioSource, bool deactivate) {}

        /** Called after an audio modification is added to the audio source.
            @param audioSource The region sequence that \p audioModification was added to.
            @param audioModification The playback region that was added to \p audioSource.
        */
        virtual void didAddAudioModificationToAudioSource (ARAAudioSource* audioSource, ARAAudioModification* audioModification) {}

        /** Called before an audio modification is removed from the audio source.
            @param audioSource The audio source that \p audioModification will be removed from.
            @param audioModification The audio modification that will be removed from \p audioSource.
        */
        virtual void willRemoveAudioModificationFromAudioSource (ARAAudioSource* audioSource, ARAAudioModification* audioModification) {}

        /** Called before the audio source is destroyed.
            @param audioSource The audio source that will be destoyed.
        */
        virtual void willDestroyAudioSource (ARAAudioSource* audioSource) {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    /** Notify the ARA host and any listeners of a content update. 

        Audio source content changes should be triggered if, for example,
        the user adjusts some analysis parameter and causes the analysis to yield new results.

        @param scopeFlags The scope of the content update.
        @param notifyAllAudioModificationsAndPlaybackRegions A bool indicating whether all audio modifications and playback 
                                                             regions associated with this audio source should be notified. 
    */
    void notifyContentChanged (ARAContentUpdateScopes scopeFlags, bool notifyAllAudioModificationsAndPlaybackRegions = false);
};


//==============================================================================
/**
    Base class representing an ARA audio modification. 

    @tags{ARA}
*/
class ARAAudioModification  : public ARA::PlugIn::AudioModification,
                              public ARAListenableModelClass<ARAAudioModification>
{
public:
    using PropertiesPtr = ARA::PlugIn::PropertiesPtr<ARA::ARAAudioModificationProperties>;

    ARAAudioModification (ARAAudioSource* audioSource, ARA::ARAAudioModificationHostRef hostRef, ARAAudioModification* optionalModificationToClone);

    class Listener  : public ARAListenableModelClass<ARAAudioModification>::Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        /** Called before the audio modification's properties are updated.
            @param audioModification The audio modification whose properties will be updated. 
            @param newProperties The audio modification properties that will be assigned to \p audioModification.  
        */
        virtual void willUpdateAudioModificationProperties (ARAAudioModification* audioModification, ARAAudioModification::PropertiesPtr newProperties) {}

        /** Called after the audio modification's properties are updated.
            @param audioModification The audio modification whose properties were updated.
        */
        virtual void didUpdateAudioModificationProperties (ARAAudioModification* audioModification) {}

        /** Called when the audio modification's content changes.
            @param audioModification The audio modification with updated content
            @param scopeFlags The scope of the content update.

            Note that this may be triggered internally by the plug-in, in which case it may be called
            outside of a host edit cycle - see ARADocumentController::notifyAudioModificationContentChanged().
        */
        virtual void doUpdateAudioModificationContent (ARAAudioModification* audioModification, ARAContentUpdateScopes scopeFlags) {}

        /** Called after an audio modification is activated or deactivated when being removed / added from the host's undo history.
            @param audioModification The audio modification that was activated or deactivated
            @param deactivate A bool indicating whether \p audioModification was deactivated or activated.
        */
        virtual void doDeactivateAudioModificationForUndoHistory (ARAAudioModification* audioModification, bool deactivate) {}

        /** Called after a playback region is added to the audio modification.
            @param audioModification The audio modification that \p playbackRegion was added to.
            @param playbackRegion The playback region that was added to \p audioModification.
        */
        virtual void didAddPlaybackRegionToAudioModification (ARAAudioModification* audioModification, ARAPlaybackRegion* playbackRegion) {}

        /** Called before a playback region is removed from the audio modification.
            @param audioModification The audio modification that \p playbackRegion will be removed from.
            @param playbackRegion The playback region that will be removed from \p audioModification.
        */
        virtual void willRemovePlaybackRegionFromAudioModification (ARAAudioModification* audioModification, ARAPlaybackRegion* playbackRegion) {}

        /** Called before the audio modification is destroyed.
            @param audioModification The audio modification that will be destoyed.
        */
        virtual void willDestroyAudioModification (ARAAudioModification* audioModification) {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    /** Notify the ARA host and any listeners of a content update 

        Audio modification content changes should be triggered if, for example, 
        the user adjusts some analysis parameter and causes the analysis to yield new results. 

        @param scopeFlags The scope of the content update. 
        @param notifyAllPlaybackRegions A bool indicating whether the audio modification's 
                                        playback regions should be notified of the content change. 
    */
    void notifyContentChanged (ARAContentUpdateScopes scopeFlags, bool notifyAllPlaybackRegions = false);
};


//==============================================================================
/**
    Base class representing an ARA playback region. 

    @tags{ARA}
*/
class ARAPlaybackRegion  : public ARA::PlugIn::PlaybackRegion,
                           public ARAListenableModelClass<ARAPlaybackRegion>
{
public:
    using PropertiesPtr = ARA::PlugIn::PropertiesPtr<ARA::ARAPlaybackRegionProperties>;

    ARAPlaybackRegion (ARAAudioModification* audioModification, ARA::ARAPlaybackRegionHostRef hostRef);

    class Listener  : public ARAListenableModelClass<ARAPlaybackRegion>::Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_BEGIN

        /** Called before the playback region's properties are updated.
            @param playbackRegion The playback region whose properties will be updated. 
            @param newProperties The playback region properties that will be assigned to \p playbackRegion. 
        */
        virtual void willUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion, ARAPlaybackRegion::PropertiesPtr newProperties) {}

        /** Called after the playback region's properties are updated.
            @param playbackRegion The playback region whose properties were updated.
        */
        virtual void didUpdatePlaybackRegionProperties (ARAPlaybackRegion* playbackRegion) {}

        /** Called when the playback region's content changes.
            @param playbackRegion The playback region with updated content
            @param scopeFlags The scope of the content update.

            Note that this may be triggered internally by the plug-in, in which case it may be called
            outside of a host edit cycle - see ARADocumentController::notifyPlaybackRegionContentChanged().
        */
        virtual void didUpdatePlaybackRegionContent (ARAPlaybackRegion* playbackRegion, ARAContentUpdateScopes scopeFlags) {}

        /** Called before the playback region is destroyed.
            @param playbackRegion The playback region that will be destoyed.
        */
        virtual void willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion) {}

       ARA_DISABLE_UNREFERENCED_PARAMETER_WARNING_END
    };

    /** Get the head time (in seconds) before the stat of the playback region */
    double getHeadTime() const { return headTime; }

    /** Get the tail time (in seconds) after the end of the playback region */
    double getTailTime() const { return tailTime; }

    /** Set the head time (in seconds) before the stat of the playback region
        @param newHeadTime The new head time. 
    */
    void setHeadTime (double newHeadTime);

    /** Set the tail time (in seconds) after the end of the playback region
        @param newTailTime The new tail time.
    */
    void setTailTime (double newTailTime);

    /** Set both the head and tail time of the playback region
        @param newHeadTime The new head time. 
        @param newTailTime The new tail time.
    */
    void setHeadAndTailTime (double newHeadTime, double newTailTime);

    /** Returns time range covered by all playback regions in the region sequence
        @param includeHeadAndTail Whether or not the range includes the head and tail 
                                  time of all playback regions in the sequence. 
    */
    Range<double> getTimeRange (bool includeHeadAndTail = false) const;

    /** Notify the ARA host and any listeners of a content update 

        Playback region content changes should be triggered if, for example,
        the user adjusts some analysis parameter and causes the analysis to yield new results.

        @param scopeFlags The scope of the content update. 
    */
    void notifyContentChanged (ARAContentUpdateScopes scopeFlags);

private:
    double headTime = 0.0;
    double tailTime = 0.0;
};


} // namespace juce