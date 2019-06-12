#include "TrackHeaderView.h"

//==============================================================================
TrackHeaderView::TrackHeaderView (ARAEditorView* view, RegionSequenceView& ownerTrack)
    : editorView (view),
      owner (ownerTrack)
{
    owner.getRegionSequence()->addListener (this);

    editorView->addListener (this);
    onNewSelection (editorView->getViewSelection());
}

TrackHeaderView::~TrackHeaderView()
{
    detachFromRegionSequence();
}

void TrackHeaderView::detachFromRegionSequence()
{
    if (owner.getRegionSequence() == nullptr)
        return;

    owner.getRegionSequence()->removeListener (this);

    editorView->removeListener (this);
}

//==============================================================================
void TrackHeaderView::paint (juce::Graphics& g)
{
    const auto regionSequence = owner.getRegionSequence();
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
    jassert (owner.getRegionSequence() != nullptr);

    bool selected = ARA::contains (viewSelection.getRegionSequences(), owner.getRegionSequence());
    if (selected != isSelected)
    {
        isSelected = selected;
        repaint();
    }
}

void TrackHeaderView::didUpdateRegionSequenceProperties (ARARegionSequence* sequence)
{
    jassert (owner.getRegionSequence() == sequence);

    repaint();
}

void TrackHeaderView::willDestroyRegionSequence (ARARegionSequence* sequence)
{
    jassert (owner.getRegionSequence() == sequence);

    detachFromRegionSequence();
}
