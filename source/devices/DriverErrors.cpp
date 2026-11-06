// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "DriverErrors.h"

namespace aether
{

juce::String explainDriverError (const juce::String& errorCode)
{
    const auto code = errorCode.trim().toUpperCase();

    if (code == "AUDCLNT_E_DEVICE_IN_USE")
        return "another program is using this device right now. Close other programs that might use "
               "it (web browsers, Discord, Teams, Zoom, games, other music software) and try again.";

    if (code == "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED")
        return "Windows doesn't allow exclusive control of this device. Open Windows Sound settings, "
               "choose the device, then Properties > Advanced, and tick \"Allow applications to take "
               "exclusive control of this device\".";

    if (code == "AUDCLNT_E_UNSUPPORTED_FORMAT")
        return "the device can't run at this sample rate in this mode. Try a different sample rate, "
               "or use \"Windows Audio\", which converts automatically.";

    if (code == "AUDCLNT_E_DEVICE_INVALIDATED" || code == "AUDCLNT_E_ENDPOINT_CREATE_FAILED")
        return "the device was unplugged, disabled or switched off. Check that it's connected and "
               "enabled in Windows Sound settings.";

    if (code == "AUDCLNT_E_INVALID_DEVICE_PERIOD" || code == "AUDCLNT_E_BUFFER_SIZE_ERROR"
        || code == "AUDCLNT_E_INCORRECT_BUFFER_SIZE" || code == "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL"
        || code == "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED")
        return "the device can't use this buffer size. Try a larger buffer size.";

    if (code == "AUDCLNT_E_SERVICE_NOT_RUNNING")
        return "the Windows Audio service isn't running. Restarting the computer usually fixes this.";

    if (code == "E_ACCESSDENIED" || code == "80070005")
        return "Windows blocked access to the device. For a microphone, check Windows Settings > "
               "Privacy & security > Microphone, and allow desktop apps to use it.";

    return {};
}

} // namespace aether
