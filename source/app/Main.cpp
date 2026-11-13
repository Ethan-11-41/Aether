// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Program entry point: creates the settings, audio engine, audio device, project session and
// main window, and handles the command line.

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"
#include "AppSettings.h"
#include "CrashReporter.h"
#include "MainWindow.h"
#include "devices/AudioDeviceService.h"
#include "engine/AudioEngine.h"
#include "io/AppFolders.h"
#include "io/AppLog.h"
#include "io/ProjectFormat.h"
#include "io/ProjectSession.h"
#include "io/RecentProjects.h"
#include "ui/AetherLookAndFeel.h"
#include "ui/TransportBar.h"

namespace aether
{

namespace
{
    constexpr const char* lastLaunchKey = "lastLaunchTime";

    /** A project file named on the command line, e.g. when double-clicking a .aether file. */
    std::optional<juce::File> findProjectFileArgument (const juce::String& commandLine)
    {
        for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
        {
            const auto path = arg.unquoted();

            if (path.endsWithIgnoreCase (projectformat::fileExtension))
            {
                const auto file = juce::File::isAbsolutePath (path) ? juce::File (path)
                                                                    : juce::File::getCurrentWorkingDirectory().getChildFile (path);
                return file;
            }
        }

        return std::nullopt;
    }

    bool isScreenshotMode (const juce::String& commandLine)
    {
        return commandLine.contains ("--screenshot");
    }
}

//==============================================================================
class AetherApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Aether"; }
    const juce::String getApplicationVersion() override    { return AETHER_VERSION_STRING; }

    bool moreThanOneInstanceAllowed() override
    {
        // Screenshot mode (used by automated checks) may run alongside a normal instance.
        return isScreenshotMode (getCommandLineParameters());
    }

    void initialise (const juce::String& commandLine) override
    {
        crashreporter::install();

        appLog = std::make_unique<AppLog> (appfolders::logs().getChildFile ("Aether.log"));
        juce::Logger::setCurrentLogger (appLog.get());

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        settings = std::make_unique<AppSettings>();
        auto& props = settings->get();

        // When did Aether last start? Used to spot crash reports from the previous run.
        const auto previousLaunch = juce::Time (props.getValue (lastLaunchKey, "0").getLargeIntValue());
        props.setValue (lastLaunchKey, juce::String (juce::Time::currentTimeMillis()));

        engine = std::make_unique<AudioEngine>();
        engine->setMetronomeEnabled (props.getBoolValue ("metronomeEnabled", true));   // on until there is music to hear
        engine->setMetronomeLevelDb ((float) props.getDoubleValue (ui::TransportBar::metronomeLevelKey, -6.0));
        engine->setCountInBars (props.getIntValue (ui::TransportBar::countInKey, 0));
        devices = std::make_unique<AudioDeviceService> (props);
        devices->setDriverErrorSource ([log = appLog.get()] { return log->getRecentDriverError(); });
        devices->addAudioCallback (engine.get());
        devices->addMidiCallback (engine.get());
        engine->setKeyboardTouch (devices->getKeyboardTouch());
        juce::Logger::writeToLog ("MIDI inputs: " + (devices->getEnabledMidiInputNames().isEmpty()
                                                        ? juce::String ("none")
                                                        : devices->getEnabledMidiInputNames().joinIntoString (", ")));
        juce::Logger::writeToLog ("Audio device: " + (devices->describeCurrentDevice().isNotEmpty()
                                                          ? devices->describeCurrentDevice()
                                                          : "none (" + devices->getStartupError() + ")"));

        recentProjects = std::make_unique<RecentProjects> (props);
        session = std::make_unique<ProjectSession> (props, *recentProjects);
        commandManager = std::make_unique<juce::ApplicationCommandManager>();

        audioFiles = std::make_unique<AudioFileCache>();
        context = std::make_unique<AppContext> (AppContext { lookAndFeel.getTheme(), *settings, *engine, *audioFiles, *devices,
                                                             *recentProjects, *session, *commandManager });
        mainWindow = std::make_unique<MainWindow> (*context);

        if (const auto file = findProjectFileArgument (commandLine))
            mainWindow->getMainContent().openProjectFile (*file);
        else if (! isScreenshotMode (commandLine))
            mainWindow->getMainContent().checkForRecoveryAtStartup (previousLaunch);

        handleScreenshotOption (commandLine);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        context = nullptr;
        audioFiles = nullptr;
        commandManager = nullptr;
        session = nullptr;          // a normal exit: no longer counts as "left open"
        recentProjects = nullptr;

        if (devices != nullptr)
        {
            devices->removeMidiCallback (engine.get());
            devices->removeAudioCallback (engine.get());
            devices = nullptr;      // also saves the device choice
        }

        engine = nullptr;

        if (settings != nullptr)
            settings->save();

        settings = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);

        juce::Logger::writeToLog ("Aether closed normally");
        juce::Logger::setCurrentLogger (nullptr);
        appLog = nullptr;
    }

    void systemRequestedQuit() override
    {
        // Ask about unsaved changes first (the main window calls quit() when it's OK).
        if (mainWindow != nullptr && ! screenshotMode)
            mainWindow->getMainContent().requestQuit();
        else
            quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // Aether was started again (e.g. by double-clicking a project) while already running.
        if (mainWindow == nullptr)
            return;

        mainWindow->toFront (true);

        if (const auto file = findProjectFileArgument (commandLine))
            mainWindow->getMainContent().openProjectFile (*file);
    }

private:
    /** "--screenshot <file.png>": saves a picture of the main window and quits.
        Used by automated checks to see the interface without a person looking at the screen. */
    void handleScreenshotOption (const juce::String& commandLine)
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        const auto index = args.indexOf ("--screenshot");

        if (index < 0)
            return;

        screenshotMode = true;
        const auto name = args[index + 1].unquoted();
        const auto target = juce::File::getCurrentWorkingDirectory()
                                .getChildFile (name.isNotEmpty() ? name : juce::String ("aether-screenshot.png"));

        juce::Timer::callAfterDelay (1500, [this, target]
        {
            bool ok = false;

            if (mainWindow != nullptr)
                if (auto* content = mainWindow->getContentComponent())
                {
                    auto image = content->createComponentSnapshot (content->getLocalBounds());
                    target.deleteFile();
                    juce::FileOutputStream stream (target);
                    juce::PNGImageFormat png;
                    ok = stream.openedOk() && png.writeImageToStream (image, stream);
                }

            setApplicationReturnValue (ok ? 0 : 1);
            quit();
        });
    }

    std::unique_ptr<AppLog> appLog;
    ui::AetherLookAndFeel lookAndFeel;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<AudioDeviceService> devices;
    std::unique_ptr<AudioFileCache> audioFiles;
    std::unique_ptr<RecentProjects> recentProjects;
    std::unique_ptr<ProjectSession> session;
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    std::unique_ptr<AppContext> context;
    std::unique_ptr<MainWindow> mainWindow;
    bool screenshotMode = false;
};

} // namespace aether

START_JUCE_APPLICATION (aether::AetherApplication)
