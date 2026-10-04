#include "core/extraction/QueryInterpreter.h"

#include <algorithm>
#include <cmath>

namespace zarlino::source::extraction
{

namespace
{

// Field order used by the (C++20) designated initialisers below:
//   text, multiWord, weight, lowBand, midBand, highBand,
//   attackSharpness, sustainRatio, rhythmicRate, harmonicity,
//   pitchHzHint, pitchToleranceOctaves, stereoSpread
//
// Values are interpretive priors describing what these sounds tend to look
// like acoustically - they steer candidate scoring and refinement; they are
// never a stem lookup and they never select output by themselves.
std::vector<QueryInterpreter::Descriptor> buildLexicon()
{
    using D = QueryInterpreter::Descriptor;

    return std::vector<D> {

        // ----- band descriptors -------------------------------------------------
        D { .text = "low",       .weight = 0.8f,  .lowBand = 1.8f,  .midBand = 0.7f,  .highBand = 0.4f },
        D { .text = "sub",       .weight = 0.9f,  .lowBand = 2.0f,  .midBand = 0.5f,  .highBand = 0.3f },
        D { .text = "deep",      .weight = 0.7f,  .lowBand = 1.7f,  .midBand = 0.8f,  .highBand = 0.5f },
        D { .text = "boomy",     .weight = 0.7f,  .lowBand = 1.8f,  .midBand = 0.7f,  .highBand = 0.4f },
        D { .text = "mid",       .weight = 0.8f,  .lowBand = 0.6f,  .midBand = 1.8f,  .highBand = 0.7f },
        D { .text = "warm",      .weight = 0.8f,  .lowBand = 1.15f, .midBand = 1.5f,  .highBand = 0.7f,
                                                 .harmonicity = 0.7f },
        D { .text = "bright",    .weight = 0.8f,  .lowBand = 0.6f,  .midBand = 0.9f,  .highBand = 1.8f },
        D { .text = "high",      .weight = 0.7f,  .lowBand = 0.4f,  .midBand = 0.8f,  .highBand = 1.7f },
        D { .text = "air",       .weight = 0.7f,  .lowBand = 0.3f,  .midBand = 0.6f,  .highBand = 1.9f },
        D { .text = "treble",    .weight = 0.7f,  .lowBand = 0.3f,  .midBand = 0.7f,  .highBand = 1.8f },
        D { .text = "top",       .weight = 0.6f,  .lowBand = 0.5f,  .midBand = 0.8f,  .highBand = 1.5f },

        // ----- dynamics / temporal descriptors ----------------------------------
        D { .text = "attack",    .weight = 0.7f,  .attackSharpness = 0.85f },
        D { .text = "transient", .weight = 0.8f,  .attackSharpness = 0.9f },
        D { .text = "punch",     .weight = 0.8f,  .attackSharpness = 0.8f },
        D { .text = "click",     .weight = 0.9f,  .highBand = 1.3f, .attackSharpness = 0.95f, .sustainRatio = 0.15f },
        D { .text = "hit",       .weight = 0.7f,  .attackSharpness = 0.85f, .sustainRatio = 0.25f },
        D { .text = "swell",     .weight = 0.8f,  .attackSharpness = 0.15f },
        D { .text = "pad",       .weight = 1.0f,  .attackSharpness = 0.2f, .sustainRatio = 0.95f,
                                                 .harmonicity = 0.6f, .stereoSpread = 0.65f },
        D { .text = "sustained", .weight = 0.8f,  .sustainRatio = 0.9f },
        D { .text = "sustain",   .weight = 0.8f,  .sustainRatio = 0.9f },
        D { .text = "drone",     .weight = 0.9f,  .attackSharpness = 0.1f, .sustainRatio = 0.98f },
        D { .text = "short",     .weight = 0.7f,  .sustainRatio = 0.2f },
        D { .text = "stab",      .weight = 0.8f,  .attackSharpness = 0.8f, .sustainRatio = 0.3f },

        // ----- texture descriptors ----------------------------------------------
        D { .text = "noise",     .weight = 0.9f,  .harmonicity = 0.05f },
        D { .text = "noisy",     .weight = 0.8f,  .harmonicity = 0.1f },
        D { .text = "breath",    .weight = 0.7f,  .highBand = 1.3f, .harmonicity = 0.25f },
        D { .text = "pitched",   .weight = 0.8f,  .harmonicity = 0.9f },
        D { .text = "tonal",     .weight = 0.7f,  .harmonicity = 0.85f },

        // ----- rhythmic descriptor ----------------------------------------------
        D { .text = "rhythmic",  .weight = 0.7f,  .rhythmicRate = 2.0f },
        D { .text = "beat",      .weight = 0.6f,  .rhythmicRate = 2.0f },

        // ----- spatial descriptors ----------------------------------------------
        D { .text = "wide",      .weight = 0.8f,  .stereoSpread = 0.9f },
        D { .text = "stereo",    .weight = 0.5f,  .stereoSpread = 0.7f },
        D { .text = "mono",      .weight = 0.8f,  .stereoSpread = 0.05f },
        D { .text = "centred",   .weight = 0.8f,  .stereoSpread = 0.1f },
        D { .text = "centered",  .weight = 0.8f,  .stereoSpread = 0.1f },

        // ----- drums and percussion ---------------------------------------------
        D { .text = "kick",      .weight = 1.1f,  .lowBand = 1.9f, .midBand = 0.8f,  .highBand = 0.4f,
                                                  .attackSharpness = 0.9f, .sustainRatio = 0.12f,
                                                  .harmonicity = 0.35f, .pitchHzHint = 60.0f,
                                                  .pitchToleranceOctaves = 0.8f, .stereoSpread = 0.05f },
        D { .text = "snare",     .weight = 1.1f,  .lowBand = 0.6f, .midBand = 1.4f,  .highBand = 1.3f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.15f,
                                                  .harmonicity = 0.3f, .stereoSpread = 0.15f },
        D { .text = "clap",      .weight = 1.0f,  .lowBand = 0.5f, .midBand = 1.3f,  .highBand = 1.3f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.12f,
                                                  .harmonicity = 0.1f, .stereoSpread = 0.4f },
        D { .text = "hihat",     .weight = 1.1f,  .lowBand = 0.2f, .midBand = 0.7f,  .highBand = 1.9f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.1f,
                                                  .harmonicity = 0.05f, .stereoSpread = 0.3f },
        D { .text = "hi hat",    .multiWord = true, .weight = 1.1f,
                                                  .lowBand = 0.2f, .midBand = 0.7f,  .highBand = 1.9f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.1f,
                                                  .harmonicity = 0.05f, .stereoSpread = 0.3f },
        D { .text = "hat",       .weight = 0.8f,  .lowBand = 0.2f, .midBand = 0.7f,  .highBand = 1.9f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.1f,
                                                  .harmonicity = 0.05f, .stereoSpread = 0.3f },
        D { .text = "shaker",    .weight = 1.1f,  .lowBand = 0.15f, .midBand = 0.8f, .highBand = 1.8f,
                                                  .attackSharpness = 0.9f, .sustainRatio = 0.2f,
                                                  .rhythmicRate = 6.0f, .harmonicity = 0.05f,
                                                  .stereoSpread = 0.5f },
        D { .text = "tambourine", .weight = 0.9f, .lowBand = 0.2f, .midBand = 0.9f,  .highBand = 1.7f,
                                                  .attackSharpness = 0.9f, .sustainRatio = 0.25f,
                                                  .harmonicity = 0.1f, .stereoSpread = 0.4f },
        D { .text = "cymbal",    .weight = 0.9f,  .lowBand = 0.15f, .midBand = 0.8f, .highBand = 1.8f,
                                                  .attackSharpness = 0.75f, .sustainRatio = 0.45f,
                                                  .harmonicity = 0.05f, .stereoSpread = 0.6f },
        D { .text = "crash",     .weight = 0.9f,  .lowBand = 0.15f, .midBand = 0.8f, .highBand = 1.8f,
                                                  .attackSharpness = 0.75f, .sustainRatio = 0.45f,
                                                  .harmonicity = 0.05f, .stereoSpread = 0.6f },
        D { .text = "ride",      .weight = 0.9f,  .lowBand = 0.2f, .midBand = 0.8f,  .highBand = 1.6f,
                                                  .attackSharpness = 0.7f, .sustainRatio = 0.5f,
                                                  .harmonicity = 0.1f, .stereoSpread = 0.55f },
        D { .text = "percussion", .weight = 0.6f, .attackSharpness = 0.7f, .rhythmicRate = 2.0f,
                                                  .stereoSpread = 0.5f },
        D { .text = "drum",      .weight = 0.6f,  .attackSharpness = 0.7f, .rhythmicRate = 1.5f },
        D { .text = "tom",       .weight = 0.9f,  .lowBand = 1.2f, .midBand = 1.4f,  .highBand = 0.9f,
                                                  .attackSharpness = 0.85f, .sustainRatio = 0.3f,
                                                  .harmonicity = 0.4f, .pitchHzHint = 120.0f,
                                                  .pitchToleranceOctaves = 1.0f, .stereoSpread = 0.2f },
        D { .text = "rim",       .weight = 0.8f,  .lowBand = 0.5f, .midBand = 1.1f,  .highBand = 1.3f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.08f,
                                                  .harmonicity = 0.2f, .stereoSpread = 0.15f },
        D { .text = "woodblock", .weight = 0.8f,  .midBand = 1.4f, .highBand = 1.3f,
                                                  .attackSharpness = 0.95f, .sustainRatio = 0.1f,
                                                  .harmonicity = 0.5f, .stereoSpread = 0.2f },
        D { .text = "808",       .weight = 1.1f,  .lowBand = 2.0f, .midBand = 0.6f,  .highBand = 0.3f,
                                                  .attackSharpness = 0.35f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.5f, .pitchHzHint = 55.0f,
                                                  .pitchToleranceOctaves = 0.6f, .stereoSpread = 0.05f },

        // ----- bass --------------------------------------------------------------
        D { .text = "sub bass",  .multiWord = true, .weight = 1.1f,
                                                  .lowBand = 2.0f, .midBand = 0.5f,  .highBand = 0.3f,
                                                  .sustainRatio = 0.8f, .harmonicity = 0.6f,
                                                  .pitchHzHint = 45.0f, .pitchToleranceOctaves = 0.5f,
                                                  .stereoSpread = 0.1f },
        D { .text = "bass",      .weight = 1.0f,  .lowBand = 1.8f, .midBand = 0.9f,  .highBand = 0.5f,
                                                  .attackSharpness = 0.45f, .sustainRatio = 0.7f,
                                                  .harmonicity = 0.75f, .pitchHzHint = 90.0f,
                                                  .pitchToleranceOctaves = 1.1f, .stereoSpread = 0.25f },
        D { .text = "bass guitar", .multiWord = true, .weight = 1.0f,
                                                  .lowBand = 1.6f, .midBand = 1.0f,  .highBand = 0.6f,
                                                  .attackSharpness = 0.5f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.3f },

        // ----- voices --------------------------------------------------------------
        D { .text = "vocal",     .weight = 1.0f,  .lowBand = 0.7f, .midBand = 1.5f,  .highBand = 0.8f,
                                                  .attackSharpness = 0.5f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.35f },
        D { .text = "vocals",    .weight = 1.0f,  .lowBand = 0.7f, .midBand = 1.5f,  .highBand = 0.8f,
                                                  .attackSharpness = 0.5f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.35f },
        D { .text = "voice",     .weight = 0.9f,  .lowBand = 0.7f, .midBand = 1.5f,  .highBand = 0.8f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.35f },
        D { .text = "vox",       .weight = 0.8f,  .midBand = 1.5f, .harmonicity = 0.8f },
        D { .text = "male vocal", .multiWord = true, .weight = 1.0f,
                                                  .lowBand = 1.1f, .midBand = 1.5f,  .highBand = 0.7f,
                                                  .attackSharpness = 0.5f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.8f, .pitchHzHint = 130.0f,
                                                  .pitchToleranceOctaves = 1.0f, .stereoSpread = 0.2f },
        D { .text = "female vocal", .multiWord = true, .weight = 1.0f,
                                                  .midBand = 1.5f, .highBand = 1.0f,
                                                  .attackSharpness = 0.45f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.8f, .pitchHzHint = 250.0f,
                                                  .pitchToleranceOctaves = 1.0f, .stereoSpread = 0.25f },
        D { .text = "lead vocal", .multiWord = true, .weight = 1.0f,
                                                  .lowBand = 0.7f, .midBand = 1.5f,  .highBand = 0.8f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.12f },
        D { .text = "background vocals", .multiWord = true, .weight = 1.0f,
                                                  .midBand = 1.4f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.8f },
        D { .text = "background vocal", .multiWord = true, .weight = 1.0f,
                                                  .midBand = 1.4f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.8f },

        // ----- guitars --------------------------------------------------------------
        D { .text = "guitar",    .weight = 1.0f,  .lowBand = 0.8f, .midBand = 1.5f, .highBand = 0.9f,
                                                  .attackSharpness = 0.6f, .sustainRatio = 0.55f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.5f },
        D { .text = "electric guitar", .multiWord = true, .weight = 1.1f,
                                                  .lowBand = 0.8f, .midBand = 1.5f, .highBand = 1.0f,
                                                  .attackSharpness = 0.65f, .sustainRatio = 0.55f,
                                                  .harmonicity = 0.65f, .stereoSpread = 0.5f },
        D { .text = "acoustic guitar", .multiWord = true, .weight = 1.1f,
                                                  .lowBand = 0.85f, .midBand = 1.5f, .highBand = 1.05f,
                                                  .attackSharpness = 0.75f, .sustainRatio = 0.5f,
                                                  .harmonicity = 0.75f, .stereoSpread = 0.6f },

        // ----- keys and pianos -------------------------------------------------------
        D { .text = "piano",     .weight = 1.1f,  .lowBand = 0.9f, .midBand = 1.4f, .highBand = 1.2f,
                                                  .attackSharpness = 0.75f, .sustainRatio = 0.55f,
                                                  .harmonicity = 0.85f, .stereoSpread = 0.6f },
        D { .text = "rhodes",    .weight = 1.15f, .lowBand = 0.7f, .midBand = 1.5f, .highBand = 0.8f,
                                                  .attackSharpness = 0.5f, .sustainRatio = 0.75f,
                                                  .harmonicity = 0.85f, .stereoSpread = 0.5f },
        D { .text = "wurlitzer", .weight = 1.1f,  .lowBand = 0.7f, .midBand = 1.4f, .highBand = 1.0f,
                                                  .attackSharpness = 0.55f, .sustainRatio = 0.7f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.45f },
        D { .text = "organ",     .weight = 1.0f,  .midBand = 1.3f,
                                                  .attackSharpness = 0.25f, .sustainRatio = 0.9f,
                                                  .harmonicity = 0.85f, .stereoSpread = 0.5f },
        D { .text = "keys",      .weight = 0.6f,  .midBand = 1.3f,
                                                  .attackSharpness = 0.55f, .sustainRatio = 0.7f,
                                                  .harmonicity = 0.8f },
        D { .text = "clav",      .weight = 0.9f,  .midBand = 1.4f, .highBand = 1.1f,
                                                  .attackSharpness = 0.85f, .sustainRatio = 0.35f,
                                                  .harmonicity = 0.75f, .stereoSpread = 0.4f },

        // ----- synths --------------------------------------------------------------
        D { .text = "synth",     .weight = 0.7f,  .midBand = 1.15f, .harmonicity = 0.6f },
        D { .text = "synth pad", .multiWord = true, .weight = 1.0f,
                                                  .midBand = 1.2f, .attackSharpness = 0.2f,
                                                  .sustainRatio = 0.95f, .harmonicity = 0.6f,
                                                  .stereoSpread = 0.65f },
        D { .text = "synth lead", .multiWord = true, .weight = 1.0f,
                                                  .midBand = 1.3f, .attackSharpness = 0.6f,
                                                  .sustainRatio = 0.6f, .harmonicity = 0.7f,
                                                  .stereoSpread = 0.3f },
        D { .text = "lead",      .weight = 0.7f,  .midBand = 1.3f, .sustainRatio = 0.6f,
                                                  .stereoSpread = 0.3f },

        // ----- strings, brass, winds ------------------------------------------------
        D { .text = "strings",   .weight = 1.0f,  .midBand = 1.3f,
                                                  .attackSharpness = 0.3f, .sustainRatio = 0.85f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.75f },
        D { .text = "string",    .weight = 1.0f,  .midBand = 1.3f,
                                                  .attackSharpness = 0.3f, .sustainRatio = 0.85f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.75f },
        D { .text = "violin",    .weight = 0.9f,  .midBand = 1.4f, .highBand = 1.1f,
                                                  .attackSharpness = 0.4f, .sustainRatio = 0.8f,
                                                  .harmonicity = 0.85f, .stereoSpread = 0.5f },
        D { .text = "cello",     .weight = 0.9f,  .lowBand = 1.1f, .midBand = 1.4f,
                                                  .attackSharpness = 0.4f, .sustainRatio = 0.8f,
                                                  .harmonicity = 0.85f, .stereoSpread = 0.45f },
        D { .text = "brass",     .weight = 1.0f,  .midBand = 1.35f,
                                                  .attackSharpness = 0.6f, .sustainRatio = 0.7f,
                                                  .harmonicity = 0.75f, .stereoSpread = 0.5f },
        D { .text = "brass section", .multiWord = true, .weight = 1.0f,
                                                  .midBand = 1.35f, .attackSharpness = 0.6f,
                                                  .sustainRatio = 0.7f, .harmonicity = 0.75f,
                                                  .stereoSpread = 0.65f },
        D { .text = "trumpet",   .weight = 0.9f,  .midBand = 1.4f, .highBand = 1.1f,
                                                  .attackSharpness = 0.65f, .sustainRatio = 0.65f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.4f },
        D { .text = "trombone",  .weight = 0.9f,  .lowBand = 1.1f, .midBand = 1.4f,
                                                  .attackSharpness = 0.6f, .sustainRatio = 0.7f,
                                                  .harmonicity = 0.8f, .stereoSpread = 0.4f },
        D { .text = "horn",      .weight = 0.8f,  .midBand = 1.35f,
                                                  .attackSharpness = 0.55f, .sustainRatio = 0.7f,
                                                  .harmonicity = 0.8f },
        D { .text = "sax",       .weight = 0.9f,  .midBand = 1.35f, .highBand = 0.9f,
                                                  .attackSharpness = 0.6f, .sustainRatio = 0.65f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.35f },
        D { .text = "saxophone", .weight = 0.9f,  .midBand = 1.35f, .highBand = 0.9f,
                                                  .attackSharpness = 0.6f, .sustainRatio = 0.65f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.35f },
        D { .text = "flute",     .weight = 0.9f,  .midBand = 1.2f, .highBand = 1.1f,
                                                  .attackSharpness = 0.35f, .sustainRatio = 0.75f,
                                                  .harmonicity = 0.7f, .stereoSpread = 0.35f },
        D { .text = "whistle",   .weight = 0.8f,  .highBand = 1.3f,
                                                  .attackSharpness = 0.3f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.9f },

        // ----- other instruments / material -------------------------------------------
        D { .text = "harp",      .weight = 0.9f,  .midBand = 1.3f, .highBand = 1.2f,
                                                  .attackSharpness = 0.7f, .sustainRatio = 0.6f,
                                                  .harmonicity = 0.9f, .stereoSpread = 0.6f },
        D { .text = "bell",      .weight = 0.9f,  .midBand = 1.1f, .highBand = 1.6f,
                                                  .attackSharpness = 0.7f, .sustainRatio = 0.5f,
                                                  .harmonicity = 0.9f, .stereoSpread = 0.5f },
        D { .text = "bells",     .weight = 0.9f,  .midBand = 1.1f, .highBand = 1.6f,
                                                  .attackSharpness = 0.7f, .sustainRatio = 0.5f,
                                                  .harmonicity = 0.9f, .stereoSpread = 0.5f },
        D { .text = "marimba",   .weight = 0.8f,  .midBand = 1.4f, .highBand = 1.1f,
                                                  .attackSharpness = 0.85f, .sustainRatio = 0.3f,
                                                  .harmonicity = 0.8f, .pitchHzHint = 220.0f,
                                                  .pitchToleranceOctaves = 1.0f },
        D { .text = "vinyl",     .weight = 0.8f,  .highBand = 1.1f, .sustainRatio = 0.9f,
                                                  .harmonicity = 0.15f, .stereoSpread = 0.5f },
        D { .text = "crackle",   .weight = 0.8f,  .highBand = 1.4f,
                                                  .attackSharpness = 0.8f, .sustainRatio = 0.3f,
                                                  .harmonicity = 0.05f },
        D { .text = "ambience",  .weight = 0.7f,  .sustainRatio = 0.9f,
                                                  .harmonicity = 0.3f, .stereoSpread = 0.8f },
        D { .text = "ambient",   .weight = 0.7f,  .sustainRatio = 0.9f,
                                                  .harmonicity = 0.3f, .stereoSpread = 0.8f },
        D { .text = "fx",        .weight = 0.7f,  .sustainRatio = 0.9f,
                                                  .harmonicity = 0.3f, .stereoSpread = 0.8f },
        D { .text = "loop",      .weight = 0.4f,  .rhythmicRate = 2.0f },

        // ----- character adjectives ------------------------------------------------------
        D { .text = "acoustic",  .weight = 0.5f,  .attackSharpness = 0.7f, .harmonicity = 0.6f },
        D { .text = "electric",  .weight = 0.5f,  .attackSharpness = 0.6f, .harmonicity = 0.65f },
        D { .text = "distorted", .weight = 0.7f,  .midBand = 1.2f, .highBand = 1.2f,
                                                  .attackSharpness = 0.6f, .harmonicity = 0.6f },
        D { .text = "clean",     .weight = 0.5f,  .harmonicity = 0.7f },
    };
}

bool matchesSingleWord (const juce::String& token, const QueryInterpreter::Descriptor& d)
{
    if (d.multiWord)
        return false;

    if (d.text == token)
        return true;

    if (d.text.length() >= 4 && token.startsWith (d.text))
        return true;

    if (token.length() >= 4 && d.text.startsWith (token))
        return true;

    return false;
}

} // namespace

QueryInterpreter::QueryInterpreter()
    : descriptors_ (buildLexicon())
{
}

Interpretation QueryInterpreter::interpret (const juce::String& description) const
{
    Interpretation result;

    // ---- tokenize ------------------------------------------------------------
    const auto lower = description.toLowerCase();
    juce::String cleaned;

    for (int i = 0; i < lower.length(); ++i)
    {
        const auto c = lower[i];
        cleaned += juce::CharacterFunctions::isLetterOrDigit (c) ? c : juce::juce_wchar (' ');
    }

    juce::StringArray rawTokens;
    rawTokens.addTokens (cleaned, " \t\r\n", "");
    rawTokens.removeEmptyStrings();

    const auto stopWords = juce::StringArray::fromTokens (
        "the a an that this some those these my your of in on to it its is for and or with part sound track thing", " ", "");

    std::vector<juce::String> tokens;

    for (const auto& t : rawTokens)
        if (! stopWords.contains (t))
            tokens.push_back (t);

    // ---- accumulate ----------------------------------------------------------
    struct Dim
    {
        float sum = 0.0f, weight = 0.0f;

        void add (float value, float w) noexcept { sum += value * w; weight += w; }

        float resolve (float fallback) const noexcept
        {
            return weight > 0.0f ? sum / weight : fallback;
        }
    };

    Dim low, mid, high, attack, sustain, rhythmic, harmonicity, pitch, pitchTol, stereo;

    float totalWeight = 0.0f;
    int matchedTokenCount = 0;

    std::vector<juce::String> matched;
    std::vector<bool> consumed (tokens.size(), false);

    auto apply = [&] (const QueryInterpreter::Descriptor& d)
    {
        totalWeight += d.weight;

        if (d.lowBand >= 0.0f)           low.add (d.lowBand, d.weight);
        if (d.midBand >= 0.0f)           mid.add (d.midBand, d.weight);
        if (d.highBand >= 0.0f)          high.add (d.highBand, d.weight);
        if (d.attackSharpness >= 0.0f)   attack.add (d.attackSharpness, d.weight);
        if (d.sustainRatio >= 0.0f)      sustain.add (d.sustainRatio, d.weight);
        if (d.rhythmicRate >= 0.0f)      rhythmic.add (d.rhythmicRate, d.weight);
        if (d.harmonicity >= 0.0f)       harmonicity.add (d.harmonicity, d.weight);
        if (d.pitchHzHint >= 0.0f)       pitch.add (d.pitchHzHint, d.weight);
        if (d.pitchToleranceOctaves >= 0.0f) pitchTol.add (d.pitchToleranceOctaves, d.weight);
        if (d.stereoSpread >= 0.0f)      stereo.add (d.stereoSpread, d.weight);

        if (std::find (matched.begin(), matched.end(), d.text) == matched.end())
            matched.push_back (d.text);
    };

    // ---- match multi-word phrases first (they override the single words) -----
    for (size_t i = 0; i + 1 < tokens.size(); ++i)
    {
        if (consumed[i] || consumed[i + 1])
            continue;

        const auto phrase = tokens[i] + " " + tokens[i + 1];

        const QueryInterpreter::Descriptor* best = nullptr;

        for (const auto& d : descriptors_)
            if (d.multiWord && d.text == phrase)
                if (best == nullptr || d.text.length() > best->text.length())
                    best = &d;

        if (best != nullptr)
        {
            apply (*best);
            consumed[i] = consumed[i + 1] = true;
            matchedTokenCount += 2;
        }
    }

    // ---- then single words ----------------------------------------------------
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (consumed[i])
            continue;

        const auto& token = tokens[i];
        const QueryInterpreter::Descriptor* best = nullptr;

        auto consider = [&] (const juce::String& t)
        {
            for (const auto& d : descriptors_)
                if (matchesSingleWord (t, d))
                    if (best == nullptr || d.text.length() > best->text.length())
                        best = &d;
        };

        consider (token);

        // Plural fallback: "wurlitzers" -> "wurlitzer".
        if (best == nullptr && token.endsWithChar ('s') && token.length() > 3)
            consider (token.dropLastCharacters (1));

        if (best != nullptr)
        {
            apply (*best);
            ++matchedTokenCount;
        }
    }

    // ---- resolve ---------------------------------------------------------------
    auto& p = result.profile;

    p.lowBandWeight = low.resolve (1.0f);
    p.midBandWeight = mid.resolve (1.0f);
    p.highBandWeight = high.resolve (1.0f);
    p.attackSharpness = attack.resolve (0.5f);
    p.sustainRatio = sustain.resolve (0.5f);
    p.rhythmicRate = rhythmic.resolve (0.0f);
    p.harmonicity = harmonicity.resolve (0.5f);
    p.pitchHzHint = pitch.resolve (0.0f);
    p.pitchToleranceOctaves = pitchTol.resolve (1.0f);
    p.stereoSpread = stereo.resolve (0.5f);

    result.matchedTerms = std::move (matched);

    const float knownRatio = tokens.empty()
                                 ? 0.0f
                                 : (float) matchedTokenCount / (float) tokens.size();

    result.recognised = totalWeight >= 0.4f && ! result.matchedTerms.empty();

    p.interpretationConfidence = juce::jlimit (0.0f, 0.95f,
        0.15f + 0.55f * knownRatio + 0.25f * juce::jmin (1.0f, totalWeight / 2.0f));

    if (! result.recognised)
        p.interpretationConfidence = juce::jmin (p.interpretationConfidence, 0.2f);

    // ---- summary ----------------------------------------------------------------
    if (tokens.empty())
    {
        p.summary = "no description - nothing to look for";
        p.interpretationConfidence = 0.0f;
        return result;
    }

    if (! result.recognised)
    {
        p.summary = "unrecognised description - broad, unweighted search";
        return result;
    }

    juce::StringArray parts;

    const float loDev = p.lowBandWeight - 1.0f;
    const float miDev = p.midBandWeight - 1.0f;
    const float hiDev = p.highBandWeight - 1.0f;

    if (miDev > 0.35f && miDev >= loDev && miDev >= hiDev)
        parts.add ("mid-range focus");
    else if (hiDev > 0.35f && hiDev >= loDev && hiDev >= miDev)
        parts.add ("high-frequency focus");
    else if (loDev > 0.35f && loDev >= miDev && loDev >= hiDev)
        parts.add ("low-frequency focus");

    if (p.attackSharpness >= 0.65f)        parts.add ("sharp attack");
    else if (p.attackSharpness <= 0.35f)   parts.add ("soft attack");

    if (p.sustainRatio >= 0.65f)           parts.add ("sustained");
    else if (p.sustainRatio <= 0.35f)      parts.add ("short decay");

    if (p.harmonicity >= 0.65f)            parts.add ("pitched");
    else if (p.harmonicity <= 0.35f)       parts.add ("noise-like");

    if (p.stereoSpread >= 0.65f)           parts.add ("wide stereo");
    else if (p.stereoSpread <= 0.35f)      parts.add ("centred");

    if (p.rhythmicRate > 0.5f)             parts.add ("rhythmic");

    if (parts.isEmpty())
        parts.add ("broad-band");

    auto summary = parts.joinIntoString (", ");

    juce::StringArray termList;

    for (const auto& m : result.matchedTerms)
        termList.add (m);

    summary << "  [" << termList.joinIntoString (", ") << "]";

    p.summary = summary;

    return result;
}

} // namespace zarlino::source::extraction
