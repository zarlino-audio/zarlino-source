#pragma once

#include "core/audio/AudioDocument.h"
#include "core/extraction/Progress.h"
#include "core/extraction/SourceQuery.h"
#include "core/extraction/SourceResult.h"
#include "inference/ISeparator.h"
#include "ui/WaveformView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace zarlino::source::ui
{

/** SCREEN 1 - EXTRACT (spec section 47): source + waveform + optional region,
    free-form query, quality mode, EXTRACT. No reference-audio field. */
class ExtractScreen final : public juce::Component
{
public:
    ExtractScreen();

    std::function<void()> onOpenRequested;
    std::function<void (const extraction::SourceQuery&, inference::SeparationRequest::Quality)> onExtract;

    void setDocument (const audio::AudioDocument* document);
    void setStatus (const juce::String& message, bool isError);
    void setBusy (bool busy);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void requestExtract();
    void refreshRegionHint();

    juce::TextButton openButton_ { "OPEN AUDIO..." };
    juce::Label sourceLabel_;
    WaveformView waveform_;
    juce::Label regionHint_;
    juce::TextEditor queryEditor_;
    juce::ComboBox qualityBox_;
    juce::TextButton extractButton_ { "EXTRACT" };
    juce::Label statusLabel_;

    juce::Rectangle<int> headerSourceRect_, headerQueryRect_, headerQualityRect_;

    bool busy_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtractScreen)
};

/** SCREEN 2 - PROCESSING (spec section 48): real per-stage progress and a
    working CANCEL that stops the worker. */
class ProcessingScreen final : public juce::Component
{
public:
    ProcessingScreen();

    std::function<void()> onCancel;

    void setQuery (const juce::String& description);
    void setProgress (const extraction::ProgressUpdate& update);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextButton cancelButton_ { "CANCEL" };

    juce::String query_;
    std::array<float, 5> stageFills_ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int currentStage_ = -1;
    float overall_ = 0.0f;
    juce::String detail_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessingScreen)
};

/** SCREEN 3 - RESULT (spec section 49). Only real capabilities are enabled;
    not-yet-built transforms are disabled and say so in their tooltips. */
class ResultScreen final : public juce::Component
{
public:
    ResultScreen();

    std::function<void()> onNewExtraction;
    std::function<void()> onSaveWav;
    std::function<void()> onOneShot;

    /** The result must outlive the display of this screen; only its audio
        buffer pointer is retained for the waveform. */
    void setResult (const extraction::SourceResult& result);
    void setStatus (const juce::String& message, bool isError);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    const extraction::SourceResult* result_ = nullptr;
    juce::String name_;
    juce::String qualityLine_;
    juce::String modelLine_;
    juce::String statusMessage_;
    bool statusIsError_ = false;

    WaveformView waveform_;
    juce::TextButton saveButton_ { "SAVE WAV" };
    std::array<juce::TextButton, 4> laterButtons_;
    juce::TextButton newButton_ { "NEW EXTRACTION" };

    juce::Rectangle<int> titleRect_, nameRect_, qualityRect_, modelRect_, statusRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResultScreen)
};

} // namespace zarlino::source::ui
