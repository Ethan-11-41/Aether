// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Program entry point: creates the settings, audio engine, audio device and main window.

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppSettings.h"
#include "MainComponent.h"
#include "devices/AudioDeviceService.h"
#include "engine/AudioEngine.h"
#include "ui/AetherLookAndFeel.h"

namespace aether
{

//==============================================================================
class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (AudioDeviceService& devices, AudioEngine& engine, const ui::Theme& theme)
        : DocumentWindow ("Aether", theme.background, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (devices, engine, theme), true);
        setResizable (true, true);
        setResizeLimits (860, 600, 10000, 10000);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

//==============================================================================
class AetherApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Aether"; }
    const juce::String getApplicationVersion() override    { return AETHER_VERSION_STRING; }

    bool moreThanOneInstanceAllowed() override
    {
        // Screenshot mode (used by automated checks) may run alongside a normal instance.
        return getCommandLineParameters().contains ("--screenshot");
    }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        settings = std::make_unique<AppSettings>();
        engine = std::make_unique<AudioEngine>();
        devices = std::make_unique<AudioDeviceService> (settings->get());
        devices->addAudioCallback (engine.get());

        mainWindow = std::make_unique<MainWindow> (*devices, *engine, lookAndFeel.getTheme());

        handleScreenshotOption (commandLine);
    }

    void shutdown() override
    {
        mainWindow = nullptr;

        if (devices != nullptr)
        {
            devices->removeAudioCallback (engine.get());
            devices = nullptr;   // also saves the device choice
        }

        engine = nullptr;

        if (settings != nullptr)
            settings->save();

        settings = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        // The user launched Aether again while it was already running: bring this window forward.
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
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

        const auto target = juce::File::getCurrentWorkingDirectory()
                                .getChildFile (args[index + 1].unquoted().isNotEmpty() ? args[index + 1].unquoted()
                                                                                        : "aether-screenshot.png");

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

    ui::AetherLookAndFeel lookAndFeel;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<AudioDeviceService> devices;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace aether

START_JUCE_APPLICATION (aether::AetherApplication)
