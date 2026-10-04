#pragma once

#include <juce_core/juce_core.h>

#include <optional>
#include <string>

namespace zarlino::source::extraction
{

/** Optional time selection inside the source document (seconds). */
struct TimeRange
{
    double startTime = 0.0;
    double endTime = 0.0;

    double length() const noexcept { return juce::jmax (0.0, endTime - startTime); }
    bool isValid() const noexcept { return endTime > startTime; }
};

/** The core customer input: a free-form description of the source to extract,
    with an optional region of interest.

    The description is free text ("Rhodes", "the electric guitar", "shaker")
    and is never restricted to a fixed enumeration - interpretation happens in
    QueryInterpreter, which maps language to a TargetProfile rather than to a
    hard-coded stem. */
struct SourceQuery
{
    std::string description;

    std::optional<double> startTime;
    std::optional<double> endTime;

    bool hasRegion() const noexcept
    {
        return startTime.has_value() && endTime.has_value() && *endTime > *startTime;
    }

    std::optional<TimeRange> getRegion() const
    {
        if (! hasRegion())
            return std::nullopt;

        return TimeRange { *startTime, *endTime };
    }
};

} // namespace zarlino::source::extraction
