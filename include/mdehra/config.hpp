#pragma once

// MDE-HRA — analysis framework configuration header.
//
// This header defines the project version constants and the
// AnalysisConfiguration enumeration that identifies the three experimental
// configurations described in the research design:
//
//   A = HRA                        (baseline)
//   B = Liveness + HRA             (liveness-pruned baseline)
//   C = Liveness + HRA + MDE       (full proposed system)
//
// IMPORTANT: This file defines *identifiers* for the configurations.
// None of the corresponding analyses (HRA, liveness, MDE) are implemented
// at this stage. Those belong to later phases.

namespace mdehra {

// ---------------------------------------------------------------------------
// Project version
// ---------------------------------------------------------------------------
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;

constexpr const char* PROJECT_NAME = "MDE-HRA";
constexpr const char* PROJECT_FULL =
    "Deduplication-Accelerated Heap Reference Analysis";

// ---------------------------------------------------------------------------
// AnalysisConfiguration
//
// Identifies one of the three experimental configurations.  The enum values
// carry no analysis semantics — they are labels used for configuration
// selection and reporting only.
//
// Research design reference: docs/project.md §16 / docs/decisions.md D1
// ---------------------------------------------------------------------------
enum class AnalysisConfiguration {
    HRA,              // A: Heap Reference Analysis (baseline, no liveness, no MDE)
    LIVENESS_HRA,     // B: Liveness-pruned HRA
    LIVENESS_HRA_MDE, // C: Liveness-pruned HRA with MDE structural deduplication
    HRA_MDE           // D: Baseline HRA with MDE structural deduplication (no liveness)
};

// Returns a stable, human-readable label for the given configuration.
// Used for logging, test output, and future experiment reporting.
const char* configurationName(AnalysisConfiguration config);

// Returns the single-letter label (A / B / C) for a configuration.
char configurationLabel(AnalysisConfiguration config);

} // namespace mdehra
