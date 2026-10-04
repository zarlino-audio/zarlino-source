#include "TestRunner.h"

#include <juce_events/juce_events.h>

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return zstest::runAll();
}
