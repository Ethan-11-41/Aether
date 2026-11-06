// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_core/juce_core.h>

namespace aether
{

/** Turns a Windows audio error code (e.g. "AUDCLNT_E_DEVICE_IN_USE") into a sentence explaining
    what it means and what to do. Returns an empty string for codes it doesn't know. */
juce::String explainDriverError (const juce::String& errorCode);

} // namespace aether
