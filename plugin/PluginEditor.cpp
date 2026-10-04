#include "plugin/PluginEditor.h"

#include "core/audio/AudioFileIO.h"
#include "core/sampling/OneShotCreator.h"

namespace zarlino::source
{

ZarlinoSourceAudioProcessorEditor::ZarlinoSourceAudioProcessorEditor (ZarlinoSourceAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      processorRef_ (processor),
      controller_ (processor.getExtractionController())
{
    setLookAndFeel (&lookAndFeel_);

    setOpaque (true);
    setSize (960, 620);
    setResizable (true, true);
    setResizeLimits (800, 560, 2560, 1440);

    addAndMakeVisible (extractScreen_);
    addChildComponent (processingScreen_);
    addChildComponent (resultScreen_);

    extractScreen_.onOpenRequested = [this] { chooseAndLoadFile(); };
    extractScreen_.onExtract = [this] (const extraction::SourceQuery& query,
                                       inference::SeparationRequest::Quality quality)
    {
        processingScreen_.setQuery (juce::String (query.description));
        controller_.startExtraction (query, quality);
        showScreen (ExtractionController::Screen::processing);
    };

    processingScreen_.onCancel = [this] { controller_.cancelExtraction(); };

    resultScreen_.onSaveWav = [this] { saveWav(); };
    resultScreen_.onOneShot = [this] { saveOneShot(); };
    resultScreen_.onNewExtraction = [this] { showScreen (ExtractionController::Screen::extract); };

    controller_.addChangeListener (this);

    extractScreen_.setDocument (controller_.hasDocument() ? &controller_.getDocument() : nullptr);

    refreshFromController();
    showScreen (controller_.getScreen());

    juce::ignoreUnused (processorRef_);
}

ZarlinoSourceAudioProcessorEditor::~ZarlinoSourceAudioProcessorEditor()
{
    controller_.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ZarlinoSourceAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromController();
    showScreen (controller_.getScreen());
}

void ZarlinoSourceAudioProcessorEditor::refreshFromController()
{
    const auto state = controller_.getState();

    extractScreen_.setBusy (state == ExtractionController::State::running);

    switch (state)
    {
        case ExtractionController::State::running:
            processingScreen_.setProgress (controller_.getProgress());
            break;

        case ExtractionController::State::finished:
            lastResult_ = controller_.getResult();
            resultScreen_.setResult (lastResult_);
            break;

        case ExtractionController::State::failed:
        {
            const auto result = controller_.getResult();
            extractScreen_.setStatus (juce::String (result.errorMessage.c_str()), true);
            break;
        }

        case ExtractionController::State::cancelled:
            extractScreen_.setStatus ("Extraction cancelled.", false);
            break;

        case ExtractionController::State::idle:
        default:
            break;
    }
}

void ZarlinoSourceAudioProcessorEditor::showScreen (ExtractionController::Screen screen)
{
    currentScreen_ = screen;

    extractScreen_.setVisible (screen == ExtractionController::Screen::extract);
    processingScreen_.setVisible (screen == ExtractionController::Screen::processing);
    resultScreen_.setVisible (screen == ExtractionController::Screen::result);
}

void ZarlinoSourceAudioProcessorEditor::chooseAndLoadFile()
{
    fileChooser_ = std::make_unique<juce::FileChooser> ("Open a mixed recording",
                                                        juce::File(),
                                                        "*.wav;*.aiff;*.aif;*.flac");

    fileChooser_->launchAsync (juce::FileBrowserComponent::openMode
                                   | juce::FileBrowserComponent::canSelectFiles,
                               [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (file == juce::File())
            return;

        juce::String error;

        if (controller_.loadFile (file, error))
        {
            extractScreen_.setDocument (&controller_.getDocument());
            extractScreen_.setStatus ("Loaded " + file.getFileName(), false);
        }
        else
        {
            extractScreen_.setStatus (error, true);
        }
    });
}

void ZarlinoSourceAudioProcessorEditor::saveWav()
{
    if (! lastResult_.successful || ! lastResult_.hasAudio())
    {
        resultScreen_.setStatus ("There is no extracted audio to save.", true);
        return;
    }

    const auto baseName = juce::String (lastResult_.requestedDescription).isNotEmpty()
                              ? juce::String (lastResult_.requestedDescription)
                              : juce::String ("Extracted Source");

    saveBuffer (lastResult_.audio, lastResult_.metadata.sampleRate,
                juce::File::createLegalFileName (baseName) + ".wav",
                "Saved ");
}

void ZarlinoSourceAudioProcessorEditor::saveOneShot()
{
    if (! lastResult_.successful || ! lastResult_.hasAudio())
    {
        resultScreen_.setStatus ("There is no extracted audio to sample.", true);
        return;
    }

    const auto shot = sampling::OneShotCreator::create (lastResult_);

    if (! shot.success)
    {
        resultScreen_.setStatus ("Could not create a one-shot: " + shot.error, true);
        return;
    }

    const auto baseName = juce::String (lastResult_.requestedDescription).isNotEmpty()
                              ? juce::String (lastResult_.requestedDescription)
                              : juce::String ("Extracted Source");

    saveBuffer (shot.audio, shot.sampleRate,
                juce::File::createLegalFileName (baseName) + " - One-Shot.wav",
                "Saved one-shot: ");
}

void ZarlinoSourceAudioProcessorEditor::saveBuffer (const juce::AudioBuffer<float>& buffer,
                                                    double sampleRate,
                                                    const juce::String& suggestedFileName,
                                                    const juce::String& savedLabel)
{
    if (buffer.getNumSamples() <= 0 || sampleRate <= 0.0)
    {
        resultScreen_.setStatus ("There is no audio to save.", true);
        return;
    }

    pendingSaveBuffer_.makeCopyOf (buffer);
    pendingSaveRate_ = sampleRate;
    pendingSaveLabel_ = savedLabel;

    const auto suggested = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                               .getChildFile (suggestedFileName);

    fileChooser_ = std::make_unique<juce::FileChooser> ("Save audio", suggested, "*.wav");

    fileChooser_->launchAsync (juce::FileBrowserComponent::saveMode
                                   | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting,
                               [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();

        if (file == juce::File())
            return;

        file = file.withFileExtension ("wav");

        juce::String error;

        if (audio::AudioFileIO::writeWav (file, pendingSaveBuffer_, pendingSaveRate_,
                                          audio::AudioFileIO::BitDepth::pcm24, &error))
            resultScreen_.setStatus (pendingSaveLabel_ + file.getFileName(), false);
        else
            resultScreen_.setStatus (error.isNotEmpty() ? error : juce::String ("Could not save the file."),
                                     true);
    });
}

void ZarlinoSourceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::Theme::background());

    auto header = headerRect_;

    g.setColour (ui::Theme::text());
    g.setFont (ui::Theme::mono (15.0f));
    g.drawText ("ZARLINO SOURCE", header.removeFromTop (20), juce::Justification::centredLeft);

    g.setColour (ui::Theme::textDim());
    g.setFont (ui::Theme::mono (11.0f));
    g.drawText ("OFFLINE SOURCE EXTRACTION   v" + juce::String (ZARLINO_SOURCE_VERSION),
                header.removeFromTop (14), juce::Justification::centredLeft);

    g.setColour (ui::Theme::panelEdge());
    g.drawHorizontalLine (headerRect_.getBottom() + 2, 16.0f, (float) getWidth() - 16.0f);
}

void ZarlinoSourceAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56);
    headerRect_ = header.reduced (16, 10);

    extractScreen_.setBounds (r);
    processingScreen_.setBounds (r);
    resultScreen_.setBounds (r);
}

} // namespace zarlino::source
