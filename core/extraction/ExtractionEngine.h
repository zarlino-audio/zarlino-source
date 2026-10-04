#pragma once

#include "core/audio/AudioDocument.h"
#include "core/extraction/Progress.h"
#include "core/extraction/SourceQuery.h"
#include "core/extraction/SourceResult.h"
#include "inference/ISeparator.h"
#include "inference/ModelManager.h"

#include <vector>

namespace zarlino::source::extraction
{

struct ExtractionJob
{
    audio::AudioDocument source;
    SourceQuery query;

    inference::SeparationRequest::Quality quality = inference::SeparationRequest::Quality::highQuality;
};

/** Orchestrates the extraction pipeline (spec section 25):

        validate -> region -> analyze source -> find target -> separate
                 -> artifact analysis -> refinement (<= 3) -> finalize

    The engine performs real inference through registered ISeparators. When no
    separator is available it fails with an actionable message - it never
    produces placeholder audio (spec sections 57/66). All work happens on the
    calling (worker) thread; the DAW audio thread never runs this. */
class ExtractionEngine
{
public:
    explicit ExtractionEngine (inference::ModelManager& models);

    /** Registers a separator (not owned). Registration order = preference. */
    void addSeparator (inference::ISeparator* separator);

    bool hasAvailableSeparator() const;

    SourceResult run (const ExtractionJob& job,
                      const ProgressCallback& progress,
                      const CancellationToken& cancellation) const;

private:
    inference::ModelManager& models_;
    std::vector<inference::ISeparator*> separators_;
};

} // namespace zarlino::source::extraction
