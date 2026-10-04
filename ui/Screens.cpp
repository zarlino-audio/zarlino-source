#include "ui/Screens.h"

#include "ui/ZarlinoTheme.h"

#include <cmath>

namespace zarlino::source::ui
{

namespace
{
    juce::String formatSeconds (double seconds)
    {
        const int totalMs = (int) std::llround (seconds * 1000.0);
        const int ms = totalMs % 1000;
        const int totalSec = totalMs / 1000;
        const int sec = totalSec % 60;
        const int min = totalSec / 60;

        return juce::String (min).paddedLeft ('0', 2) + ":"
             + juce::String (sec).paddedLeft ('0', 2) + "."
             + juce::String (ms).paddedLeft ('0', 3);
    }

    const char* const kStageNames[] = {
        "Analyzing source...", "Finding target...", "Separating...", "Refining...", "Finalizing..."
    };
}

//==============================================================================
// ExtractScreen
//==============================================================================

ExtractScreen::ExtractScreen()
{
    openButton_.onClick = [this] { if (onOpenRequested) onOpenRequested(); };
    addAndMakeVisible (openButton_);

    sourceLabel_.setFont (Theme::ui (12.0f));
    sourceLabel_.setColour (juce::Label::textColourId, Theme::textDim());
    sourceLabel_.setText ("NO SOURCE LOADED", juce::dontSendNotification);
    addAndMakeVisible (sourceLabel_);

    waveform_.onSelectionChanged = [this] { refreshRegionHint(); };
    addAndMakeVisible (waveform_);

    regionHint_.setFont (Theme::mono (11.0f));
    regionHint_.setColour (juce::Label::textColourId, Theme::textDim());
    addAndMakeVisible (regionHint_);

    queryEditor_.setFont (Theme::ui (15.0f));
    queryEditor_.setTextToShowWhenEmpty ("What do you want to extract?  e.g. Rhodes, shaker, electric guitar",
                                         Theme::textDim());
    queryEditor_.onReturnKey = [this] { requestExtract(); };
    addAndMakeVisible (queryEditor_);

    qualityBox_.addItem ("Preview", 1);
    qualityBox_.addItem ("High Quality", 2);
    qualityBox_.addItem ("Maximum", 3);
    qualityBox_.setSelectedId (2, juce::dontSendNotification);
    addAndMakeVisible (qualityBox_);

    extractButton_.setColour (juce::TextButton::buttonColourId, Theme::accent());
    extractButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    extractButton_.onClick = [this] { requestExtract(); };
    addAndMakeVisible (extractButton_);

    statusLabel_.setFont (Theme::ui (12.0f));
    statusLabel_.setColour (juce::Label::textColourId, Theme::textDim());
    addAndMakeVisible (statusLabel_);

    refreshRegionHint();
}

void ExtractScreen::setDocument (const audio::AudioDocument* document)
{
    if (document == nullptr || ! document->isValid())
    {
        waveform_.setBuffer (nullptr, 0.0);
        sourceLabel_.setText ("NO SOURCE LOADED", juce::dontSendNotification);
        return;
    }

    waveform_.setBuffer (&document->buffer, document->sampleRate);

    const int totalSeconds = (int) std::llround (document->getLengthSeconds());
    const int mins = totalSeconds / 60;
    const int secs = totalSeconds % 60;

    sourceLabel_.setText (document->name
                              + "   -   " + juce::String (mins) + ":"
                              + juce::String (secs).paddedLeft ('0', 2)
                              + "   -   " + juce::String (document->sampleRate / 1000.0, 1) + " kHz",
                          juce::dontSendNotification);
}

void ExtractScreen::setStatus (const juce::String& message, bool isError)
{
    statusLabel_.setText (message, juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::textColourId, isError ? Theme::error() : Theme::textDim());
}

void ExtractScreen::setBusy (bool busy)
{
    busy_ = busy;

    openButton_.setEnabled (! busy);
    extractButton_.setEnabled (! busy);
    queryEditor_.setEnabled (! busy);
    qualityBox_.setEnabled (! busy);
}

void ExtractScreen::refreshRegionHint()
{
    const auto selection = waveform_.getSelection();

    if (selection.has_value() && selection->isValid())
        regionHint_.setText ("REGION   " + formatSeconds (selection->startTime)
                                 + " - " + formatSeconds (selection->endTime),
                             juce::dontSendNotification);
    else
        regionHint_.setText ("REGION   FULL FILE  (drag on the waveform to limit it - optional)",
                             juce::dontSendNotification);
}

void ExtractScreen::requestExtract()
{
    if (busy_)
        return;

    const auto text = queryEditor_.getText().trim();

    if (text.isEmpty())
    {
        setStatus ("Type what you want to extract first.", true);
        queryEditor_.grabKeyboardFocus();
        return;
    }

    extraction::SourceQuery query;
    query.description = text.toStdString();

    if (const auto selection = waveform_.getSelection(); selection.has_value() && selection->isValid())
    {
        query.startTime = selection->startTime;
        query.endTime = selection->endTime;
    }

    auto quality = inference::SeparationRequest::Quality::highQuality;

    switch (qualityBox_.getSelectedId())
    {
        case 1:  quality = inference::SeparationRequest::Quality::preview;     break;
        case 3:  quality = inference::SeparationRequest::Quality::maximum;     break;
        case 2:
        default: quality = inference::SeparationRequest::Quality::highQuality; break;
    }

    if (onExtract)
        onExtract (query, quality);
}

void ExtractScreen::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background());

    g.setColour (Theme::textDim());
    g.setFont (Theme::mono (11.0f));
    g.drawText ("SOURCE", headerSourceRect_, juce::Justification::centredLeft);
    g.drawText ("WHAT DO YOU WANT TO EXTRACT?", headerQueryRect_, juce::Justification::centredLeft);
    g.drawText ("QUALITY", headerQualityRect_, juce::Justification::centredLeft);
}

void ExtractScreen::resized()
{
    auto r = getLocalBounds().reduced (16, 12);

    auto bottomRow = r.removeFromBottom (40);
    r.removeFromBottom (12);
    auto qualityBlock = r.removeFromBottom (18 + 28);
    r.removeFromBottom (12);
    auto queryBlock = r.removeFromBottom (18 + 36);
    r.removeFromBottom (12);
    auto regionBlock = r.removeFromBottom (16);

    headerSourceRect_ = r.removeFromTop (20);

    auto topRow = r.removeFromTop (24);
    openButton_.setBounds (topRow.removeFromRight (140));
    topRow.removeFromRight (8);
    sourceLabel_.setBounds (topRow);

    r.removeFromTop (8);
    waveform_.setBounds (r);

    regionHint_.setBounds (regionBlock);

    headerQueryRect_ = queryBlock.removeFromTop (18);
    queryEditor_.setBounds (queryBlock);

    headerQualityRect_ = qualityBlock.removeFromTop (18);
    qualityBox_.setBounds (qualityBlock.removeFromLeft (220));

    extractButton_.setBounds (bottomRow.removeFromRight (180));
    bottomRow.removeFromRight (12);
    statusLabel_.setBounds (bottomRow);
}

//==============================================================================
// ProcessingScreen
//==============================================================================

ProcessingScreen::ProcessingScreen()
{
    cancelButton_.onClick = [this] { if (onCancel) onCancel(); };
    addAndMakeVisible (cancelButton_);
}

void ProcessingScreen::setQuery (const juce::String& description)
{
    query_ = description;
    stageFills_ = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    currentStage_ = -1;
    overall_ = 0.0f;
    detail_.clear();
    repaint();
}

void ProcessingScreen::setProgress (const extraction::ProgressUpdate& update)
{
    int index = -1;

    switch (update.stage)
    {
        case extraction::Stage::analyzingSource: index = 0; break;
        case extraction::Stage::findingTarget:   index = 1; break;
        case extraction::Stage::separating:      index = 2; break;
        case extraction::Stage::refining:        index = 3; break;
        case extraction::Stage::finalizing:      index = 4; break;
        case extraction::Stage::idle:
        default: break;
    }

    if (index >= 0)
    {
        for (int i = 0; i < index; ++i)
            stageFills_[(size_t) i] = 1.0f;

        stageFills_[(size_t) index] = juce::jlimit (0.0f, 1.0f, update.stageFraction);

        for (int i = index + 1; i < 5; ++i)
            stageFills_[(size_t) i] = 0.0f;

        currentStage_ = index;
    }

    overall_ = juce::jmax (overall_, juce::jlimit (0.0f, 1.0f, update.overallFraction));
    detail_ = update.detail;
    repaint();
}

void ProcessingScreen::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background());

    auto r = getLocalBounds().reduced (16, 12);

    g.setColour (Theme::text());
    g.setFont (Theme::mono (16.0f));
    g.drawText ("EXTRACTING " + query_.toUpperCase(), r.removeFromTop (26), juce::Justification::centredLeft);

    r.removeFromTop (10);

    // Overall progress bar.
    auto overallArea = r.removeFromTop (10);

    g.setColour (Theme::panel());
    g.fillRoundedRectangle (overallArea.toFloat(), 3.0f);

    if (overall_ > 0.0f)
    {
        g.setColour (Theme::accent());
        g.fillRoundedRectangle (overallArea.toFloat().withWidth ((float) overallArea.getWidth() * overall_),
                                3.0f);
    }

    r.removeFromTop (18);

    for (int i = 0; i < 5; ++i)
    {
        auto row = r.removeFromTop (44);

        g.setColour (i == currentStage_ ? Theme::text() : Theme::textDim());
        g.setFont (Theme::ui (13.0f));
        g.drawText (kStageNames[i], row.removeFromTop (18), juce::Justification::centredLeft);

        auto bar = row.removeFromTop (8).removeFromLeft (juce::jmax (140, getWidth() / 2));

        g.setColour (Theme::panel());
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);

        const float fill = stageFills_[(size_t) i];

        if (fill > 0.0f)
        {
            g.setColour (i < currentStage_ ? Theme::accent().withAlpha (0.55f) : Theme::accent());
            g.fillRoundedRectangle (bar.toFloat().withWidth ((float) bar.getWidth() * fill), 3.0f);
        }

        g.setColour (Theme::textDim());
        g.setFont (Theme::mono (11.0f));
        g.drawText (juce::String ((int) std::lround (fill * 100.0f)).paddedLeft (' ', 3) + " %",
                    juce::Rectangle<int> (bar.getRight() + 10, bar.getY() - 4, 64, 16),
                    juce::Justification::centredLeft);
    }

    // Detail line above the cancel button.
    if (detail_.isNotEmpty())
    {
        auto bottom = getLocalBounds().reduced (16, 12);
        bottom.removeFromBottom (40);

        g.setColour (Theme::textDim());
        g.setFont (Theme::mono (11.0f));
        g.drawFittedText (detail_, bottom.removeFromBottom (16), juce::Justification::centredLeft, 1);
    }
}

void ProcessingScreen::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    cancelButton_.setBounds (r.removeFromBottom (32).removeFromRight (120));
}

//==============================================================================
// ResultScreen
//==============================================================================

ResultScreen::ResultScreen()
{
    saveButton_.setColour (juce::TextButton::buttonColourId, Theme::accent());
    saveButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    saveButton_.onClick = [this] { if (onSaveWav) onSaveWav(); };
    addAndMakeVisible (saveButton_);

    const char* const names[] = { "ONE-SHOT", "PHRASE", "LOOP", "INSTRUMENT" };

    for (int i = 0; i < 4; ++i)
    {
        auto& b = laterButtons_[(size_t) i];
        b.setButtonText (names[i]);
        b.setEnabled (false);
        b.setTooltip ("Arriving in a later phase - not implemented yet");
        addAndMakeVisible (b);
    }

    // One-shot creation is implemented and wired to the editor.
    laterButtons_[0].setEnabled (true);
    laterButtons_[0].setTooltip ("Create a one-shot from this extraction and save it as WAV");
    laterButtons_[0].onClick = [this] { if (onOneShot) onOneShot(); };

    newButton_.onClick = [this] { if (onNewExtraction) onNewExtraction(); };
    addAndMakeVisible (newButton_);

    waveform_.setSelectionEnabled (false);
    addAndMakeVisible (waveform_);
}

void ResultScreen::setResult (const extraction::SourceResult& result)
{
    result_ = &result;

    name_ = juce::String (result.requestedDescription).isNotEmpty()
                ? juce::String (result.requestedDescription)
                : juce::String ("Extracted source");

    const auto& q = result.quality;

    qualityLine_ = "PRESERVATION " + juce::String (q.targetPreservation, 2)
                 + "    LEAKAGE " + juce::String (q.leakageEstimate, 2)
                 + "    TRANSIENTS " + juce::String (q.transientIntegrity, 2)
                 + "    ARTIFACTS " + juce::String (q.artifactScore, 2)
                 + "    CONFIDENCE " + juce::String (q.confidence, 2);

    const juce::String modelName = result.metadata.modelName.empty()
                                       ? juce::String ("n/a")
                                       : juce::String (result.metadata.modelName);

    modelLine_ = "MODEL " + modelName + " " + juce::String (result.metadata.modelVersion)
               + "    SEPARATION " + juce::String (result.diagnostics.separationSeconds, 2) + " s"
               + "    REFINEMENT PASSES " + juce::String (result.diagnostics.refinementPasses);

    const auto& diagnostics = result.diagnostics;

    modelLine_ << "    TEMPO "
               << (diagnostics.tempoBpm > 0.0 ? juce::String (diagnostics.tempoBpm, 1) + " BPM"
                                              : juce::String ("uncertain"))
               << "    KEY " << juce::String (diagnostics.keyDescription);

    if (diagnostics.meanPitchHz > 0.0)
        modelLine_ << "    PITCH " << juce::String (diagnostics.meanPitchHz, 1) << " Hz";

    waveform_.setBuffer (&result.audio, result.metadata.sampleRate);
    statusMessage_.clear();
    repaint();
}

void ResultScreen::setStatus (const juce::String& message, bool isError)
{
    statusMessage_ = message;
    statusIsError_ = isError;
    repaint();
}

void ResultScreen::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background());

    g.setColour (Theme::textDim());
    g.setFont (Theme::mono (11.0f));
    g.drawText ("EXTRACTED SOURCE", titleRect_, juce::Justification::centredLeft);

    g.setColour (Theme::text());
    g.setFont (Theme::ui (22.0f));
    g.drawText (name_, nameRect_, juce::Justification::centredLeft);

    g.setColour (Theme::textDim());
    g.setFont (Theme::mono (11.0f));
    g.drawText (qualityLine_, qualityRect_, juce::Justification::centredLeft);
    g.drawText (modelLine_, modelRect_, juce::Justification::centredLeft);

    if (statusMessage_.isNotEmpty())
    {
        g.setColour (statusIsError_ ? Theme::error() : Theme::textDim());
        g.setFont (Theme::ui (12.0f));
        g.drawText (statusMessage_, statusRect_, juce::Justification::centredLeft);
    }
}

void ResultScreen::resized()
{
    auto r = getLocalBounds().reduced (16, 12);

    titleRect_ = r.removeFromTop (16);
    r.removeFromTop (2);
    nameRect_ = r.removeFromTop (30);
    r.removeFromTop (6);

    auto buttons = r.removeFromBottom (36);
    r.removeFromBottom (10);
    statusRect_ = r.removeFromBottom (16);
    r.removeFromBottom (6);
    modelRect_ = r.removeFromBottom (18);
    r.removeFromBottom (4);
    qualityRect_ = r.removeFromBottom (18);

    waveform_.setBounds (r);

    saveButton_.setBounds (buttons.removeFromLeft (120));
    buttons.removeFromLeft (8);

    for (auto& b : laterButtons_)
    {
        b.setBounds (buttons.removeFromLeft (110));
        buttons.removeFromLeft (8);
    }

    newButton_.setBounds (buttons.removeFromRight (170));
}

} // namespace zarlino::source::ui
