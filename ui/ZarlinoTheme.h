#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace zarlino::source::ui
{

/** Zarlino Audio design language (spec section 51):

      near-black background, steel secondary surfaces, white typography,
      blue accent, Inter for UI text, IBM Plex Mono for numerics,
      4 px radius, 8 px spacing, no gradients, no effects.

    Fonts fall back to the system's default sans/mono when Inter or
    IBM Plex Mono are not installed. */
struct Theme
{
    static juce::Colour background() noexcept { return juce::Colour (0xff0b0d10); }
    static juce::Colour panel()      noexcept { return juce::Colour (0xff161a20); }
    static juce::Colour panelEdge()  noexcept { return juce::Colour (0xff262d37); }
    static juce::Colour text()       noexcept { return juce::Colour (0xfff2f4f7); }
    static juce::Colour textDim()    noexcept { return juce::Colour (0xff8f97a3); }
    static juce::Colour accent()     noexcept { return juce::Colour (0xff3b82f6); }
    static juce::Colour warning()    noexcept { return juce::Colour (0xffe0a23c); }
    static juce::Colour error()      noexcept { return juce::Colour (0xffe5484d); }

    static constexpr float spacing = 8.0f;
    static constexpr float radius = 4.0f;

    static juce::Font ui (float height)
    {
        return juce::Font (juce::FontOptions().withName ("Inter").withHeight (height));
    }

    static juce::Font mono (float height)
    {
        return juce::Font (juce::FontOptions().withName ("IBM Plex Mono").withHeight (height));
    }
};

/** Shared look-and-feel: colour scheme only; drawing stays custom so the
    product keeps the flat, professional-instrument feel. */
class ZarlinoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ZarlinoLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Theme::background());
        setColour (juce::TextButton::buttonColourId, Theme::panel());
        setColour (juce::TextButton::buttonOnColourId, Theme::panel());
        setColour (juce::TextButton::textColourOffId, Theme::text());
        setColour (juce::TextButton::textColourOnId, Theme::text());
        setColour (juce::ComboBox::backgroundColourId, Theme::panel());
        setColour (juce::ComboBox::textColourId, Theme::text());
        setColour (juce::ComboBox::outlineColourId, Theme::panelEdge());
        setColour (juce::ComboBox::arrowColourId, Theme::textDim());
        setColour (juce::PopupMenu::backgroundColourId, Theme::panel());
        setColour (juce::PopupMenu::textColourId, Theme::text());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent());
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::backgroundColourId, Theme::panel());
        setColour (juce::TextEditor::textColourId, Theme::text());
        setColour (juce::TextEditor::outlineColourId, Theme::panelEdge());
        setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent());
        setColour (juce::TextEditor::highlightColourId, Theme::accent().withAlpha (0.35f));
        setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour (juce::CaretComponent::caretColourId, Theme::accent());
        setColour (juce::Label::textColourId, Theme::text());
        setColour (juce::ScrollBar::thumbColourId, Theme::panelEdge());
        setColour (juce::TooltipWindow::backgroundColourId, Theme::panel());
        setColour (juce::TooltipWindow::textColourId, Theme::text());
        setColour (juce::TooltipWindow::outlineColourId, Theme::panelEdge());
    }
};

} // namespace zarlino::source::ui
