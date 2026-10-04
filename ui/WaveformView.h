#pragma once

#include "core/extraction/SourceQuery.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace zarlino::source::ui
{

/** Amplitude overview of an AudioBuffer with optional click-drag time
    selection (spec sections 47/49). Flat rendering, no gradients: min/max
    envelope per pixel column in the blue accent.

    The view holds a non-owning pointer to the buffer; the caller must keep it
    alive while the view is displayed. */
class WaveformView final : public juce::Component
{
public:
    WaveformView();

    void setBuffer (const juce::AudioBuffer<float>* buffer, double sampleRate);
    void clearSelection();

    void setSelectionEnabled (bool enabled) { selectionEnabled_ = enabled; }

    std::optional<extraction::TimeRange> getSelection() const { return selection_; }
    double getLengthSeconds() const noexcept;

    /** Called on the message thread whenever the selection changes. */
    std::function<void()> onSelectionChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    float timeToX (double seconds) const;
    double xToTime (float x) const;
    double getDragThresholdSeconds() const noexcept;

    const juce::AudioBuffer<float>* buffer_ = nullptr;
    double sampleRate_ = 0.0;

    bool selectionEnabled_ = true;
    bool dragging_ = false;
    double dragAnchorSeconds_ = 0.0;
    std::optional<extraction::TimeRange> selection_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

} // namespace zarlino::source::ui
