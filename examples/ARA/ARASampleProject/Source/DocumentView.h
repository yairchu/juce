#pragma once

#include "JuceHeader.h"

#include "TimelineViewport/TimelineViewport.h"
#include "RulersView.h"

class TrackHeaderView;
class RegionSequenceView;
class PlaybackRegionView;

//==============================================================================
/**
 DocumentView Class -
    This class provides basic foundation to show the ARA Document as well as
    their current selection state

    It is currently work-in-progress, with the goal of making it a reusable base class
    that is part of the JUCE_ARA framework module, not just example code.
    Any JUCE-based ARA plug-in should be able to utilize this to ease its view implementation.

 TODO JUCE_ARA:
    - provide juce::LookAndFeel mechanism so it could be customized for developer needs.
    - configuration for all sizes: track height, ruler height, track header width etc.
    - refactor RulersViews to have RulersView::RulerBase and subclasses.
      maybe we don't need a shared base class other than Component, that would be preferrable.
    - option to show regions including their head and tail
      (for crossfades mostly, renderer will already provide proper samples,
       but time ranges must be adjusted for this and updated if head/tail change)
    - properly compensate for presentation latency (IAudioPresentationLatency/contextPresentationLatency)
      when drawing play head (requires minor additons to the VST and AU wrapper)
    - replace Viewport with better mechanism to avoid integer overflow with long documents and high zoom level.
 */
class DocumentView  : public Component,
                      private ARAEditorView::Listener,
                      private ARADocument::Listener,
                      private juce::Timer
{
public:
    /** Creation.

     @param editorARAExtension  the editor extension used for viewing the document
     @param positionInfo        the time info to be used for showing the playhead
                                This needs to be updated from the processBlock() method of the
                                audio processor showing the editor. The view code can deal with
                                this struct being updated concurrently from the render thread.
     */
    DocumentView (const AudioProcessorEditorARAExtension& editorARAExtension, const AudioPlayHead::CurrentPositionInfo& positionInfo);

    /** Destructor. */
    ~DocumentView();

    /*
     Creates a new PlaybackRegionView which will be owned.
     This allows customizing PlaybackRegionView Component to desired behavior.
     (for example: showing notes)
     */
    virtual PlaybackRegionView* createViewForPlaybackRegion (ARAPlaybackRegion*);

    /*
     Creates a new RegionSequenceView which will be owned.
     This allows customizing RegionSequenceView Component to desired behavior.
     (for example: allow showing cross-fades or interaction between regions)
     */
    virtual RegionSequenceView* createViewForRegionSequence (ARARegionSequence*);

    /*
     Creates a new TrackHeaderView which will be owned.
     This allows customizing TrackHeaderView Component to desired behavior.
     */
    virtual TrackHeaderView* createHeaderViewForRegionSequence (ARARegionSequence*);

    /*
     Creates a rulers to be shown on rulersView.
     This allows setting custom rulers as required.
     */
    virtual void createRulers();

    template<typename EditorView_t = ARAEditorView>
    EditorView_t* getARAEditorView() const noexcept { return araExtension.getARAEditorView<EditorView_t>(); }

    template<typename DocumentController_t = ARADocumentController>
    DocumentController_t* getARADocumentController() const noexcept { return araExtension.getARADocumentController<DocumentController_t>(); }

    const ARASecondsPixelMapper& getTimeMapper() { return timeMapper; }

    // flag that our view needs to be rebuilt
    void invalidateRegionSequenceViews();

    Component& getTrackHeadersView() { return trackHeadersView; }

    AudioFormatManager& getAudioFormatManger() { return audioFormatManger; }

    const AudioPlayHead::CurrentPositionInfo& getPlayHeadPositionInfo() const { return positionInfo; }

    // DocumentView States
    void setShowOnlySelectedRegionSequences (bool newVal);
    bool isShowingOnlySelectedRegionSequences() { return showOnlySelectedRegionSequences; }

    void setIsTrackHeadersVisible (bool shouldBeVisible);
    bool isTrackHeadersVisible() const { return trackHeadersView.isVisible(); }

    /* Sets if DocumentView should show ARAEditor ViewSelection */
    void setIsViewSelectionVisible (bool isVisible) { timeRangeSelectionView.setVisible (isVisible); }
    /* @return true if DocumentView is showing ARAEditor ViewSelection */
    bool getIsViewSelectionVisible() { return timeRangeSelectionView.isVisible(); }

    int getTrackHeaderWidth() const { return trackHeadersView.getWidth(); }
    int getTrackHeaderMaximumWidth () { return trackHeadersView.getMaximumWidth(); }
    int getTrackHeaderMinimumWidth () { return trackHeadersView.getMinimumWidth(); }
    void setTrackHeaderWidth (int newWidth);
    void setTrackHeaderMaximumWidth (int newWidth);
    void setTrackHeaderMinimumWidth (int newWidth);

    void setScrollFollowsPlayHead (bool followPlayHead) { scrollFollowsPlayHead = followPlayHead; }
    bool isScrollFollowingPlayHead() const { return scrollFollowsPlayHead; }

    void setVisibleTimeRange (Range<double> newRange) { viewport.setVisibleRange (newRange); };
    void zoomBy (double newValue);

    void setTrackHeight (int newHeight);
    int getTrackHeight() const { return trackHeight; }

    void setRulersHeight (int rulersHeight);
    int getRulersHeight() const { return rulersHeight; }
    RulersView& getRulersView() { return rulersView; }

    /** Returns borders of "static" components within the viewport */
    BorderSize<int> getViewportBorders() { return viewport.getViewedComponentBorders(); };

    Range<double> getVisibleTimeRange() { return viewport.getVisibleRange(); }

    /** Get ScrollBar components owned by the viewport, this allows further customization */
    juce::ScrollBar& getScrollBar (bool isVertical) { return viewport.getScrollBar (isVertical); }

    //==============================================================================
    void parentHierarchyChanged() override;
    void paint (Graphics&) override;
    void resized() override;

    // juce::Timer overrides
    void timerCallback() override;

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection) override;
    void onHideRegionSequences (std::vector<ARARegionSequence*> const& regionSequences) override;

    // ARADocument::Listener overrides
    void didEndEditing (ARADocument* document) override;
    void didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* regionSequence) override;
    void didReorderRegionSequencesInDocument (ARADocument* document) override;

    // update region to range (if needed)
    void setRegionBounds (PlaybackRegionView*, Range<double>);

    //==============================================================================
    /**
     A class for receiving events from a DocumentView.

     You can register a DocumentView::Listener with a DocumentView using DocumentView::addListener()
     method, and it will be called on changes.

     @see DocumentView::addListener, DocumentView::removeListener
     */
    class Listener
    {
    public:
        /** Destructor. */
        virtual ~Listener() {}

        /** Called when a DocumentView visible time range is changed.
            This happens when being scrolled or zoomed/scaled on the horizontal axis.

         @param newVisibleTimeRange       the new range of the document that's currently visible.
         @param zoomFactor                current ratio between pixels and timeline baseunit.
         */
        virtual void visibleTimeRangeChanged (Range<double> newVisibleTimeRange, double zoomFactor) = 0;

        /** Called when a trackHeight is changed.

         @param newTrackHeight           new trackHeight in pixels.
         */
        virtual void trackHeightChanged (int newTrackHeight) {};

        /** Called when a rulersHeight is changed.

         @param newRulersHeight           new rulersHeight in pixels.
         */
        virtual void rulersHeightChanged (int newRulersHeight) {};
    };

    /** Registers a listener that will be called for changes of the DocumentView. */
    void addListener (Listener* listener);

    /** Deregisters a previously-registered listener. */
    void removeListener (Listener* listener);

private:
    void rebuildRegionSequenceViews();
    void updatePlayHeadBounds();

private:
    // TODO JUCE_ARA eventually those should just be LookAndFeel?
    // once RegionSeqeunce will be a view of its own ViewSelection should be
    // refactored/migrated to it.

    // simple utility class to show playhead position
    class PlayHeadView    : public Component
    {
    public:
        PlayHeadView (DocumentView& documentView);
        void paint (Graphics&) override;
    private:
        DocumentView& documentView;
    };

    // simple utility class to show selected time range
    class TimeRangeSelectionView  : public Component
    {
    public:
        TimeRangeSelectionView (DocumentView& documentView);
        void paint (Graphics&) override;
    private:
        DocumentView& documentView;
    };

    // resizable container of TrackHeaderViews
    class TrackHeadersView    : public Component,
                                public ComponentBoundsConstrainer
    {
    public:
        TrackHeadersView (DocumentView& documentView);
        void setIsResizable (bool isResizable);
        void resized() override;
    private:
        DocumentView& documentView;
        ResizableEdgeComponent resizeBorder;
    };

    const AudioProcessorEditorARAExtension& araExtension;

    TrackHeadersView trackHeadersView;
    TimelineViewport viewport;
    const ARASecondsPixelMapper& timeMapper;
    RulersView rulersView;

    OwnedArray<RegionSequenceView> regionSequenceViews;

    PlayHeadView playHeadView;
    TimeRangeSelectionView timeRangeSelectionView;
    AudioFormatManager audioFormatManger;

    // Component View States
    bool scrollFollowsPlayHead = true;
    bool showOnlySelectedRegionSequences = true;

    int trackHeight = 80;
    int rulersHeight = 20;
    bool regionSequenceViewsAreInvalid = true;

    juce::AudioPlayHead::CurrentPositionInfo lastReportedPosition;
    const juce::AudioPlayHead::CurrentPositionInfo& positionInfo;

    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DocumentView)
};
