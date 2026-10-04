#include "ui/WaveformView.h"

#include "ui/ZarlinoTheme.h"

#include <cmath>

namespace zarlino::source::ui
{

namespace
{
    juce::Rectangle<int> waveformArea (const juce::Rectangle<int>& componentBounds)
    {
        return componentBounds.reduced (6);
    }

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
}

WaveformView::WaveformView()
{
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

void WaveformView::setBuffer (const juce::AudioBuffer<float>* buffer, double sampleRate)
{
    buffer_ = buffer;
    sampleRate_ = sampleRate;
    selection_.reset();
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void WaveformView::clearSelection()
{
    if (! selection_.has_value())
        return;

    selection_.reset();
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

double WaveformView::getLengthSeconds() const noexcept
{
    if (buffer_ == nullptr || sampleRate_ <= 0.0)
        return 0.0;

    return (double) buffer_->getNumSamples() / sampleRate_;
}

float WaveformView::timeToX (double seconds) const
{
    const auto area = waveformArea (getLocalBounds());
    const auto length = getLengthSeconds();

    if (length <= 0.0 || area.getWidth() <= 0)
        return (float) area.getX();

    const double fraction = juce::jlimit (0.0, 1.0, seconds / length);
    return (float) area.getX() + (float) (fraction * (double) area.getWidth());
}

double WaveformView::xToTime (float x) const
{
    const auto area = waveformArea (getLocalBounds());
    const auto length = getLengthSeconds();

    if (length <= 0.0 || area.getWidth() <= 0)
        return 0.0;

    const double fraction = juce::jlimit (0.0, 1.0,
                                          (double) (x - (float) area.getX()) / (double) area.getWidth());
    return fraction * length;
}

double WaveformView::getDragThresholdSeconds() const noexcept
{
    const auto length = getLengthSeconds();
    return juce::jmax (0.005, length * 0.001);
}

void WaveformView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Theme::panel());
    g.fillRoundedRectangle (bounds, Theme::radius);
    g.setColour (Theme::panelEdge());
    g.drawRoundedRectangle (bounds.reduced (0.5f), Theme::radius, 1.0f);

    auto area = waveformArea (getLocalBounds());

    if (buffer_ == nullptr || sampleRate_ <= 0.0 || buffer_->getNumSamples() <= 0)
    {
        g.setColour (Theme::textDim());
        g.setFont (Theme::mono (12.0f));
        g.drawText ("NO AUDIO LOADED", area, juce::Justification::centred);
        return;
    }

    const int numSamples = buffer_->getNumSamples();
    const int numChannels = buffer_->getNumChannels();
    const float midY = (float) area.getCentreY();
    const float halfHeight = juce::jmax (1.0f, (float) area.getHeight() * 0.5f - 3.0f);
    const int width = area.getWidth();

    // Centre line.
    g.setColour (Theme::panelEdge());
    g.drawHorizontalLine ((int) midY, (float) area.getX(), (float) area.getRight());

    // Min/max envelope per pixel column.
    g.setColour (Theme::accent().withAlpha (0.9f));

    for (int x = 0; x < width; ++x)
    {
        const int s0 = (int) ((juce::int64) x * numSamples / width);
        const int s1 = juce::jmin (numSamples, (int) ((juce::int64) (x + 1) * numSamples / width));

        if (s1 <= s0)
            continue;

        float mn = 1.0e9f;
        float mx = -1.0e9f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = buffer_->getReadPointer (ch);

            for (int i = s0; i < s1; ++i)
            {
                mn = juce::jmin (mn, data[i]);
                mx = juce::jmax (mx, data[i]);
            }
        }

        const float top = midY - juce::jlimit (-1.0f, 1.0f, mx) * halfHeight;
        const float bottom = midY - juce::jlimit (-1.0f, 1.0f, mn) * halfHeight;

        g.drawVerticalLine (area.getX() + x, top, juce::jmax (bottom, top + 0.5f));
    }

    // Selection overlay.
    if (selection_.has_value() && selection_->isValid())
    {
        const float x0 = timeToX (selection_->startTime);
        const float x1 = timeToX (selection_->endTime);

        g.setColour (Theme::accent().withAlpha (0.16f));
        g.fillRect (juce::Rectangle<float> (x0, (float) area.getY(),
                                            x1 - x0, (float) area.getHeight()));

        g.setColour (Theme::accent());
        g.drawVerticalLine ((int) x0, (float) area.getY(), (float) area.getBottom());
        g.drawVerticalLine ((int) x1, (float) area.getY(), (float) area.getBottom());

        g.setColour (Theme::text());
        g.setFont (Theme::mono (11.0f));

        auto textArea = area.removeFromBottom (16).removeFromRight (220);
        g.drawText (formatSeconds (selection_->startTime) + " - " + formatSeconds (selection_->endTime),
                    textArea, juce::Justification::centredRight);
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (! selectionEnabled_ || buffer_ == nullptr || ! e.mods.isLeftButtonDown())
        return;

    dragging_ = true;
    dragAnchorSeconds_ = xToTime ((float) e.x);
    selection_ = extraction::TimeRange { dragAnchorSeconds_, dragAnchorSeconds_ };
    repaint();
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging_)
        return;

    const double current = xToTime ((float) e.x);
    selection_ = extraction::TimeRange { juce::jmin (dragAnchorSeconds_, current),
                                         juce::jmax (dragAnchorSeconds_, current) };
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging_)
        return;

    dragging_ = false;
    juce::ignoreUnused (e);

    if (selection_.has_value() && selection_->length() < getDragThresholdSeconds())
        selection_.reset();     // a plain click clears the selection

    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    clearSelection();
}

} // namespace zarlino::source::ui
