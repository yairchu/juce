#pragma once

#include "JuceHeader.h"

#include "TimelineViewport/TimelineViewport.h"
#include "RulersView.h"
#include "RegionSequenceView.h"

class TrackHeaderView;
class PlaybackRegionView;
class DocumentView;

//==============================================================================
/**
 Resizable container of TrackHeadersView(s)
 */
class TrackHeadersView    : public Component,
public ComponentBoundsConstrainer
{
public:
    TrackHeadersView ();
    void setIsResizable (bool isResizable);
    void resized() override;
private:
    ResizableEdgeComponent resizeBorder;
};

//==============================================================================
/**
 One of these is used by a DocumentView as the data controller for document view contents.
 
 The virtual methods that you override in this class take care of drawing elements and reacting to events.

 A single controller can be used for multiple DocumentViews (if needed)
 
 @see DcoumentView
 
 @tags{GUI}
 */
class DocumentViewController : public ChangeBroadcaster,
                               private ARAEditorView::Listener,
                               private ARADocument::Listener
{
public:
    //==============================================================================
    /** Creation.

     @param editorARAExtension  the editor extension used for viewing the document
     @param positionInfo        the time info to be used for showing the playhead
     This needs to be updated from the processBlock() method of the
     audio processor showing the editor. The view code can deal with
     this struct being updated concurrently from the render thread.
     */
    DocumentViewController (const AudioProcessorEditorARAExtension& editorARAExtension);

    /** Destructor.
        Make sure you're overriding it!
     */
    virtual ~DocumentViewController();

    /*
     Creates a new Component that will be used to hold all DocumentView components.
     (this might be useful if you need finer control of the parent viewed component).
     */
    virtual Component* createCanvasComponent();
    /*
     Creates a new PlaybackRegionView which will be owned.
     This allows customizing PlaybackRegionView Component to desired behavior.
     (for example: showing notes)
     */
    virtual PlaybackRegionView* createViewForPlaybackRegion (RegionSequenceView* owner, ARAPlaybackRegion*);

    /*
     Creates a new RegionSequenceView which will be owned.
     This allows customizing RegionSequenceView Component to desired behavior.
     (for example: allow showing cross-fades or interaction between regions)
     */
    virtual RegionSequenceView* createViewForRegionSequence (DocumentView& owner, ARARegionSequence*);
    
    /*
     Creates a new TrackHeaderView which will be owned.
     This allows customizing TrackHeaderView Component to desired behavior.
     */
    virtual TrackHeaderView* createHeaderViewForRegionSequence (RegionSequenceView& ownerTrack);

    /*
     Creates a new RulerView which will be owned.
     This allows customizing RulerView to have default rulers on construction.
     */
    virtual RulersView* createRulersView (DocumentView& owner);

    /*
     Creates a new component that will paint playhead above all timeline viewport,
     This allows customizing PlayheadView Component to desired behavior. If nullptr return this will use default component.
     Component will be owned.
     */
    virtual Component* createPlayheadView (DocumentView& owner);

    /*
     Creates a new component that will paint ARA SelectionView above all timeline viewport.
     This allows customizing TimeRangeSelectionView Component to desired behavior. If nullptr return this will use default component.
     Component will be owned.
     */
    virtual Component* createTimeRangeSelectionView (DocumentView& owner);

    /** Returns entire document time range
     Note: host timeline can have different start/end times.
     but this range must guarantee to be within the host timeline.
     **/
    virtual Range<double> getDocumentTimeRange();

    /** Adds border padding to time range **/
    virtual Range<double> padTimeRange (Range<double> timeRange);

    /** Returns the top-most vertical position after setTrackHeight */
    virtual int getTopForCurrentTrackHeight (DocumentView& owner);

    /** Return the RegionSequences that should be visible by this DocumentView.
        Default Implementation returns current ARA ViewSelection.
     */
    virtual std::vector<ARARegionSequence*> getVisibleRegionSequences();

    // flag that our view needs to be rebuilt
    void invalidateRegionSequenceViews (NotificationType notificationType = NotificationType::sendNotification);

    const AudioProcessorEditorARAExtension& getARAEditorExtension() const { return araExtension; }

    template<typename EditorView_t = ARAEditorView>
    EditorView_t* getARAEditorView() const noexcept { return getARAEditorExtension().getARAEditorView<EditorView_t>(); }

    template<typename DocumentController_t = ARADocumentController>
    DocumentController_t* getARADocumentController() const noexcept { return getARAEditorView()->getDocumentController<DocumentController_t>(); }

    // ARAEditorView::Listener overrides
    void onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection) override;
    void onHideRegionSequences (std::vector<ARARegionSequence*> const& regionSequences) override;

    // ARADocument::Listener overrides
    void didEndEditing (ARADocument* document) override;
    void didAddRegionSequenceToDocument (ARADocument* document, ARARegionSequence* regionSequence) override;
    void didReorderRegionSequencesInDocument (ARADocument* document) override;


private:
    const AudioProcessorEditorARAExtension& araExtension;

    // simple utility class to show playhead position
    class PlayHeadView    : public Component
    {
    public:
        PlayHeadView (DocumentView&);
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
};

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
                      private juce::Timer,
                      private AsyncUpdater,
                      private ChangeListener,
                      private ComponentListener
{
public:
    /** Creation.

     @param controllerToOwn     Controller object to manage DocumentView.
                                DocumentView takes the ownership of this component.
     @param positionInfo        Positional data for this view (for rulers/playhead/etc)
     */
    DocumentView (DocumentViewController* controllerToOwn, const AudioPlayHead::CurrentPositionInfo& positionInfo);

    /** Destructor. */
    ~DocumentView();

    DocumentViewController& getController() { return *viewController; }

    const AudioPlayHead::CurrentPositionInfo& getPlayHeadPositionInfo() const { return lastReportedPosition; }

    void setIsTrackHeadersVisible (bool shouldBeVisible);
    bool isTrackHeadersVisible() const;

    /* Sets if DocumentView should show ARAEditor ViewSelection */
    void setIsViewSelectionVisible (bool isVisible) { timeRangeSelectionView->setVisible (isVisible); }
    /* @return true if DocumentView is showing ARAEditor ViewSelection */
    bool getIsViewSelectionVisible() { return timeRangeSelectionView->isVisible(); }

    int getTrackHeaderWidth() const;
    int getTrackHeaderMaximumWidth();
    int getTrackHeaderMinimumWidth();
    void setTrackHeaderWidth (int newWidth);
    void setTrackHeaderMaximumWidth (int newWidth);
    void setTrackHeaderMinimumWidth (int newWidth);

    void setScrollFollowsPlayHead (bool followPlayHead) { scrollFollowsPlayHead = followPlayHead; }
    bool isScrollFollowingPlayHead() const { return scrollFollowsPlayHead; }

    /* Sets the current visible area by range.
       Note: This would only work if DocumentView bounds.height is greater than 0.
     */
    void setVisibleTimeRange (Range<double> newRange) { viewport.setVisibleRange (newRange); };

    /* Sets the current visible area by start position and zoom ratio.
     */
    void setVisibleTimeRange (double startPos, double ratio) { viewport.setVisibleRange(startPos, ratio); }
    void zoomBy (double newValue, bool relativeToPlayhead = true);

    void setFitTrackHeight (bool shouldFit);
    void setFitTrackWidth (bool shouldFit);
    void setTrackHeight (int newHeight);
    void setMinTrackHeight (int);
    int getTrackHeight() const { return trackHeight; }

    void setRulersHeight (int rulersHeight);
    int getRulersHeight() const { return rulersHeight; }
    RulersView& getRulersView() { return *rulersView; }

    /** Returns borders of "static" components within the viewport */
    BorderSize<int> getViewportBorders() { return viewport.getViewedComponentBorders(); };

    Range<double> getVisibleTimeRange() { return viewport.getVisibleRange(); }

    TimelineViewport& getViewport() { return viewport; }
    TrackHeadersView& getTrackHeadersView() { return *trackHeadersView; }

    const ARASecondsPixelMapper& getTimeMapper() const { return timeMapper; }

    /** Get ScrollBar components owned by the viewport, this allows further customization */
    juce::ScrollBar& getScrollBar (bool isVertical) { return viewport.getScrollBar (isVertical); }

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

    // juce::ComponentListener overrides
    void componentMovedOrResized (Component& component,
                                          bool wasMoved,
                                          bool wasResized) override;
    // juce::Timer overrides
    void timerCallback() override;

    // Listen for DocumentViewController invalidation
    void changeListenerCallback (ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;

    // update region bounds based on new range (if needed)
    void setRegionBounds (PlaybackRegionView*, Range<double>, BorderSize<int> regionSequenceBorders);

    const RegionSequenceView& getRegionSequenceView (int idx) const { return *regionSequenceViews[idx]; }
    int getNumOfTracks() const { return regionSequenceViews.size(); }
    bool canVerticalZoomOutFurther() const;

    /* Tries to set visible range based on playhead.
       this will 'jump' to playhead if playhead is out of current visible range.
    */
    void followPlayheadIfNeeded();

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
        virtual void trackHeightChanged (int /*newTrackHeight*/) {};

        /** Called when a rulersHeight is changed.

         @param newRulersHeight           new rulersHeight in pixels.
         */
        virtual void rulersHeightChanged (int /*newRulersHeight*/) {};
    };

    /** Registers a listener that will be called for changes of the DocumentView. */
    void addListener (Listener* listener);

    /** Deregisters a previously-registered listener. */
    void removeListener (Listener* listener);

protected:
    /** Non-const protected getter to allow subclasses to modify or repaint specific region sequences views */
    RegionSequenceView& getRegionSequenceViewProtected (int idx) { return *regionSequenceViews[idx]; }

private:
    void updatePlayHeadBounds();
    int calcSingleTrackFitHeight() const;
private:
    std::unique_ptr<DocumentViewController> viewController;

    TimelineViewport viewport;
    const ARASecondsPixelMapper& timeMapper;

    OwnedArray<RegionSequenceView> regionSequenceViews;

    std::unique_ptr<RulersView> rulersView;
    std::unique_ptr<Component> playHeadView;
    std::unique_ptr<Component> timeRangeSelectionView;
    std::unique_ptr<TrackHeadersView> trackHeadersView;

    // Component View States
    bool scrollFollowsPlayHead = false;
    bool fitTrackHeight = true;
    bool fitTrackWidth = true;

    int trackHeight = 80;
    int rulersHeight = 20;
    int minTrackHeight = 36;

    const AudioPlayHead::CurrentPositionInfo& positionInfo;
    juce::AudioPlayHead::CurrentPositionInfo lastReportedPosition;
    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DocumentView)
};
