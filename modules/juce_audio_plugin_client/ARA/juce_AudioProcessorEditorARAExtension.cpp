#include "juce_AudioProcessorEditorARAExtension.h"

namespace juce
{

AudioProcessorEditorARAExtension::AudioProcessorEditorARAExtension (AudioProcessor* audioProcessor)
: araProcessorExtension (dynamic_cast<AudioProcessorARAExtension*> (audioProcessor))
{
    if (isARAEditorView())
        getARAEditorView()->setEditorOpen (true);
}

AudioProcessorEditorARAExtension::~AudioProcessorEditorARAExtension()
{
    if (isARAEditorView())
        getARAEditorView()->setEditorOpen (false);
}

ARAEditorView* AudioProcessorEditorARAExtension::getARAEditorView() const noexcept
{
    return araProcessorExtension ? static_cast<ARAEditorView*>(araProcessorExtension->getARAEditorView()) : nullptr;
}

} // namespace juce
