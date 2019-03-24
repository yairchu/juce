#include "TrackHeaderView.h"

//==============================================================================
TrackHeaderView::TrackHeaderView (ARAEditorView* view, ARARegionSequence* sequence)
    : editorView (view),
      regionSequence (sequence)
{
    regionSequence->addListener (this);

    editorView->addListener (this);
    onNewSelection (editorView->getViewSelection());
}

TrackHeaderView::~TrackHeaderView()
{
    detachFromRegionSequence();
}

void TrackHeaderView::detachFromRegionSequence()
{
    if (regionSequence == nullptr)
        return;

    regionSequence->removeListener (this);

    editorView->removeListener (this);

    regionSequence = nullptr;
}

//==============================================================================
void TrackHeaderView::paint (juce::Graphics& g)
{
    if (regionSequence == nullptr)
        return;

    Colour trackColour = convertOptionalARAColour (regionSequence->getColor());

    auto rect = getLocalBounds();
    g.setColour (isSelected ? Colours::yellow : Colours::black);
    g.drawRect (rect);
    rect.reduce (1, 1);

    g.setColour (trackColour);
    g.fillRect (rect);

    if (const auto& name = regionSequence->getName())
    {
        g.setColour (trackColour.contrasting (1.0f));
        g.setFont (Font (12.0f));
        g.drawText (convertOptionalARAString (name), rect, Justification::centredLeft);
    }
}

//==============================================================================
void TrackHeaderView::onNewSelection (const ARA::PlugIn::ViewSelection& viewSelection)
{
    jassert (regionSequence != nullptr);

    bool selected = ARA::contains (viewSelection.getRegionSequences(), regionSequence);
    if (selected != isSelected)
    {
        isSelected = selected;
        repaint();
    }
}

void TrackHeaderView::didUpdateRegionSequenceProperties (ARARegionSequence* sequence)
{
    jassert (regionSequence == sequence);

    repaint();
}

void TrackHeaderView::willDestroyRegionSequence (ARARegionSequence* sequence)
{
    jassert (regionSequence == sequence);

    detachFromRegionSequence();
}
