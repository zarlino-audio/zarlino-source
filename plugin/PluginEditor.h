#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/Screens.h"
#include "ui/ZarlinoTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace zarlino::source
{

/** Editor shell: hosts the Extract / Processing / Result screens, follows the
    offline ExtractionController and renders real worker progress. */
class ZarlinoSourceAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::ChangeListener
{
public:
    explicit ZarlinoSourceAudioProcessorEditor (ZarlinoSourceAudioProcessor& processor);
    ~ZarlinoSourceAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void showScreen (ExtractionController::Screen screen);
    void refreshFromController();
    void chooseAndLoadFile();
    void saveWav();
    void saveOneShot();

    /** Shared save path: copies the audio into the pending members, opens a
        save chooser, writes 24-bit WAV and reports into the result screen's
        status line. */
    void saveBuffer (const juce::AudioBuffer<float>& buffer,
                     double sampleRate,
                     const juce::String& suggestedFileName,
                     const juce::String& savedLabel);

    ZarlinoSourceAudioProcessor& processorRef_;
    ExtractionController& controller_;

    ui::ZarlinoLookAndFeel lookAndFeel_;
    ui::ExtractScreen extractScreen_;
    ui::ProcessingScreen processingScreen_;
    ui::ResultScreen resultScreen_;
    juce::TooltipWindow tooltips_ { this, 700 };

    ExtractionController::Screen currentScreen_ = ExtractionController::Screen::extract;

    /** Kept alive for the result screen's waveform pointer (see setResult). */
    extraction::SourceResult lastResult_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Save pipeline: the async chooser callback reads these.
    juce::AudioBuffer<float> pendingSaveBuffer_;
    double pendingSaveRate_ = 0.0;
    juce::String pendingSaveLabel_;

    juce::Rectangle<int> headerRect_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZarlinoSourceAudioProcessorEditor)
};

} // namespace zarlino::source
