// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace aether
{

/** Aether's settings file (not project data: things like the chosen audio device).

    On Windows it is stored at  %APPDATA%\Aether\Aether.settings
*/
class AppSettings
{
public:
    AppSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Aether";
        options.filenameSuffix = ".settings";
        options.folderName = "Aether";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        properties.setStorageParameters (options);
    }

    juce::PropertiesFile& get()   { return *properties.getUserSettings(); }

    void save()                   { properties.saveIfNeeded(); }

private:
    juce::ApplicationProperties properties;
};

} // namespace aether
