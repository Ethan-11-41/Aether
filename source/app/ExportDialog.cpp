// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ExportDialog.h"

#include <array>

#include "io/Mp3Encoder.h"
#include "model/TimelineEdits.h"

namespace aether
{

namespace
{
    constexpr int labelWidth = 110, rowHeight = 30, gap = 8;

    const std::array<double, 4> rates { 44100.0, 48000.0, 88200.0, 96000.0 };
    const std::array<int, 4> mp3Rates { 128, 192, 256, 320 };
}

/** Runs the export, so the window stays responsive. */
class ExportDialog::Worker final : public juce::Thread
{
public:
    explicit Worker (ExportJob& jobToRun) : juce::Thread ("Aether export"), job (jobToRun) {}
    ~Worker() override   { job.cancel(); stopThread (30000); }

    void run() override
    {
        result = job.run();
        done = true;
    }

    std::atomic<bool> done { false };
    juce::Result result = juce::Result::ok();

private:
    ExportJob& job;
};

ExportDialog::ExportDialog (const ui::Theme& themeToUse, const Project& projectToExport, const ProjectLocation& where,
                            juce::PropertySet& settingsToUse)
    : theme (themeToUse), project (projectToExport), location (where), settings (settingsToUse),
      folder (where.getFolder().getChildFile ("Exports"))
{
    auto setUpLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, theme.textDim);
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    };

    setUpLabel (whatLabel, "Export");
    setUpLabel (rangeLabel, "Range");
    setUpLabel (formatLabel, "Format");
    setUpLabel (qualityLabel, "Quality");
    setUpLabel (rateLabel, "Sample rate");
    setUpLabel (optionsLabel, "Options");
    setUpLabel (nameLabel, "File name");
    setUpLabel (folderLabel, "Folder");

    mixToggle.setTooltip ("The whole song as one stereo file, as you hear it (mutes and solos included).");
    stemsToggle.setTooltip ("One file per track, each as you'd hear it soloed (with its effects, sends and the master). "
                            "Played together, they add up to the mix. Useful for sending a song to someone to mix.");
    normalizeToggle.setTooltip ("Makes the mix as loud as it can be without clipping: its loudest moment at -1 dB. "
                                "Stems are never normalized, so they still add up.");
    tailToggle.setTooltip ("Keeps recording after the last clip until reverb, echoes and released notes have died away.");

    const auto loop = timeline::getLoop (project);
    const auto tempo = timeline::buildTempoMap (project);
    rangeBox.addItem ("The whole song (to the end of the last clip)", 1);
    rangeBox.addItem ("The loop region (bar " + juce::String (tempo.ticksToBarBeat (loop.start).bar + 1) + " to bar "
                      + juce::String (tempo.ticksToBarBeat (loop.end).bar + 1) + ")", 2);

    formatBox.addItem ("WAV (uncompressed: best for further work)", 1);
    formatBox.addItem ("FLAC (smaller than WAV, same quality)", 2);
    formatBox.addItem (Mp3Encoder::isAvailable() ? juce::String ("MP3 (smallest: for sharing and listening)")
                                                 : juce::String ("MP3 (not available: the LAME encoder is missing)"), 3);
    formatBox.setItemEnabled (3, Mp3Encoder::isAvailable());
    formatBox.onChange = [this] { updateChoices(); };

    for (auto* toggle : { &mixToggle, &stemsToggle, &normalizeToggle, &tailToggle })
        addAndMakeVisible (toggle);

    for (auto* box : { &rangeBox, &formatBox, &qualityBox, &rateBox })
        addAndMakeVisible (box);

    nameEditor.setText (project.getName(), false);
    nameEditor.setTooltip ("The mix's file name. Stems are named after it and their track, in a \"... stems\" folder.");
    addAndMakeVisible (nameEditor);

    folderPath.setColour (juce::Label::textColourId, theme.text);
    folderPath.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (folderPath);
    addAndMakeVisible (chooseFolderButton);
    chooseFolderButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export to which folder?", folder);
        juce::Component::SafePointer<ExportDialog> safeThis (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis] (const juce::FileChooser& c)
        {
            if (safeThis != nullptr && c.getResult() != juce::File())
            {
                safeThis->folder = c.getResult();
                safeThis->folderPath.setText (safeThis->folder.getFullPathName(), juce::dontSendNotification);
            }
        });
    };

    status.setColour (juce::Label::textColourId, theme.text);
    status.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (status);
    addChildComponent (progressBar);

    exportButton.onClick = [this] { startExport(); };
    exportButton.setColour (juce::TextButton::buttonColourId, theme.accent);
    exportButton.setColour (juce::TextButton::textColourOffId, theme.accentText);
    closeButton.onClick = [this]
    {
        if (isExporting())
        {
            job->cancel();
            return;
        }

        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);
    };
    showButton.onClick = [this]
    {
        if (job != nullptr && ! job->getFiles().isEmpty())
            job->getFiles()[0].revealToUser();
        else
            folder.revealToUser();
    };

    addAndMakeVisible (exportButton);
    addAndMakeVisible (closeButton);
    addChildComponent (showButton);

    // What was chosen last time.
    mixToggle.setToggleState (settings.getBoolValue ("exportMix", true), juce::dontSendNotification);
    stemsToggle.setToggleState (settings.getBoolValue ("exportStems", false), juce::dontSendNotification);
    normalizeToggle.setToggleState (settings.getBoolValue ("exportNormalize", false), juce::dontSendNotification);
    tailToggle.setToggleState (settings.getBoolValue ("exportTail", true), juce::dontSendNotification);
    rangeBox.setSelectedId (1, juce::dontSendNotification);
    formatBox.setSelectedId (juce::jlimit (1, 3, settings.getIntValue ("exportFormat", 1)), juce::dontSendNotification);
    if (! Mp3Encoder::isAvailable() && formatBox.getSelectedId() == 3)
        formatBox.setSelectedId (1, juce::dontSendNotification);

    folderPath.setText (folder.getFullPathName(), juce::dontSendNotification);
    status.setText ("The files go in the project's Exports folder unless you choose another.", juce::dontSendNotification);
    updateChoices();
    setSize (620, 470);
}

ExportDialog::~ExportDialog()
{
    stopTimer();
    worker = nullptr;   // cancels and waits
    saveChoices();
}

ExportFormat ExportDialog::selectedFormat() const
{
    switch (formatBox.getSelectedId())
    {
        case 2:  return ExportFormat::flac;
        case 3:  return ExportFormat::mp3;
        default: return ExportFormat::wav;
    }
}

void ExportDialog::updateChoices()
{
    const auto format = selectedFormat();
    qualityBox.clear (juce::dontSendNotification);
    rateBox.clear (juce::dontSendNotification);

    if (format == ExportFormat::mp3)
    {
        for (size_t i = 0; i < mp3Rates.size(); ++i)
            qualityBox.addItem (juce::String (mp3Rates[i]) + " kbps" + (mp3Rates[i] == 320 ? " (best)" : mp3Rates[i] == 128 ? " (smallest)" : ""),
                                mp3Rates[i]);
        qualityBox.setSelectedId (settings.getIntValue ("exportKbps", 320), juce::dontSendNotification);
    }
    else
    {
        qualityBox.addItem ("16-bit (CD quality)", 16);
        qualityBox.addItem ("24-bit (recommended)", 24);

        if (format == ExportFormat::wav)
            qualityBox.addItem ("32-bit floating point (for further mixing)", 32);

        qualityBox.setSelectedId (settings.getIntValue (format == ExportFormat::wav ? "exportWavBits" : "exportFlacBits", 24),
                                  juce::dontSendNotification);
    }

    if (qualityBox.getSelectedId() == 0)
        qualityBox.setSelectedItemIndex (qualityBox.getNumItems() - 1, juce::dontSendNotification);

    // MP3 only goes up to 48 kHz.
    for (size_t i = 0; i < rates.size(); ++i)
        if (format != ExportFormat::mp3 || rates[i] <= 48000.0)
            rateBox.addItem (juce::String (rates[i] / 1000.0, rates[i] == 44100.0 || rates[i] == 88200.0 ? 1 : 0)
                                 + " kHz" + (rates[i] == 44100.0 ? " (CD)" : ""),
                             (int) rates[i]);

    rateBox.setSelectedId (settings.getIntValue ("exportRate", 48000), juce::dontSendNotification);
    if (rateBox.getSelectedId() == 0)
        rateBox.setSelectedId (48000, juce::dontSendNotification);

}

void ExportDialog::saveChoices()
{
    settings.setValue ("exportMix", mixToggle.getToggleState());
    settings.setValue ("exportStems", stemsToggle.getToggleState());
    settings.setValue ("exportNormalize", normalizeToggle.getToggleState());
    settings.setValue ("exportTail", tailToggle.getToggleState());
    settings.setValue ("exportFormat", formatBox.getSelectedId());
    settings.setValue ("exportRate", rateBox.getSelectedId());

    const auto format = selectedFormat();
    settings.setValue (format == ExportFormat::mp3 ? "exportKbps" : format == ExportFormat::wav ? "exportWavBits" : "exportFlacBits",
                       qualityBox.getSelectedId());
}

ExportOptions ExportDialog::getOptions() const
{
    ExportOptions options;
    options.format = selectedFormat();
    options.bitDepth = options.format == ExportFormat::mp3 ? 16 : qualityBox.getSelectedId();
    options.mp3Kbps = options.format == ExportFormat::mp3 ? qualityBox.getSelectedId() : 320;
    options.sampleRate = (double) rateBox.getSelectedId();
    options.mix = mixToggle.getToggleState();
    options.stems = stemsToggle.getToggleState();
    options.loopRegionOnly = rangeBox.getSelectedId() == 2;
    options.includeTail = tailToggle.getToggleState();
    options.normalize = normalizeToggle.getToggleState();
    options.folder = folder;
    options.name = nameEditor.getText().trim();
    return options;
}

bool ExportDialog::isExporting() const
{
    return worker != nullptr && ! worker->done;
}

void ExportDialog::setControlsEnabled (bool enabled)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &mixToggle, &stemsToggle, &normalizeToggle, &tailToggle,
                                                                        &rangeBox, &formatBox, &qualityBox, &rateBox, &nameEditor,
                                                                        &chooseFolderButton, &exportButton })
        c->setEnabled (enabled);

    closeButton.setButtonText (enabled ? "Close" : "Cancel");
}

void ExportDialog::startExport()
{
    if (isExporting())
        return;

    saveChoices();
    const auto options = getOptions();

    if (! options.mix && ! options.stems)
    {
        status.setText ("Tick \"The mix\" or \"Stems\" (or both).", juce::dontSendNotification);
        return;
    }

    job = std::make_unique<ExportJob> (project, location, options);   // copies the project now
    worker = std::make_unique<Worker> (*job);
    progress = 0.0;
    progressBar.setVisible (true);
    showButton.setVisible (false);
    setControlsEnabled (false);
    status.setColour (juce::Label::textColourId, theme.text);
    status.setText ("Exporting...", juce::dontSendNotification);
    worker->startThread();
    startTimerHz (10);
}

void ExportDialog::timerCallback()
{
    if (job == nullptr || worker == nullptr)
        return;

    progress = job->getProgress();
    status.setText ("Exporting: " + job->getCurrentStep(), juce::dontSendNotification);

    if (worker->done)
        finished();
}

void ExportDialog::finished()
{
    stopTimer();
    const auto result = worker->result;
    worker = nullptr;
    setControlsEnabled (true);
    progressBar.setVisible (false);

    if (result.failed())
    {
        status.setText (result.getErrorMessage() == "Cancelled." ? juce::String ("Cancelled (the files written so far are kept).")
                                                                 : "Couldn't export: " + result.getErrorMessage(),
                        juce::dontSendNotification);
        showButton.setVisible (! job->getFiles().isEmpty());
        return;
    }

    juce::String text = "Done: " + juce::String (job->getFiles().size()) + (job->getFiles().size() == 1 ? " file" : " files")
                      + " in " + folder.getFullPathName() + ".";

    for (const auto& warning : job->getWarnings())
        text << "\n\nNote: " << warning;

    status.setText (text, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, job->getWarnings().isEmpty() ? theme.text : theme.warning);
    showButton.setVisible (true);
    juce::Logger::writeToLog ("Exported " + juce::String (job->getFiles().size()) + " file(s) to " + folder.getFullPathName());
}

void ExportDialog::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);
}

void ExportDialog::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto row = [&] (juce::Label& label)
    {
        auto r = area.removeFromTop (rowHeight);
        area.removeFromTop (gap);
        label.setBounds (r.removeFromLeft (labelWidth));
        return r;
    };

    {
        auto r = row (whatLabel);
        mixToggle.setBounds (r.removeFromLeft (r.getWidth() / 2));
        stemsToggle.setBounds (r);
    }

    rangeBox.setBounds (row (rangeLabel));
    formatBox.setBounds (row (formatLabel));
    qualityBox.setBounds (row (qualityLabel));
    rateBox.setBounds (row (rateLabel).removeFromLeft (200));

    {
        auto r = row (optionsLabel);
        normalizeToggle.setBounds (r);
        auto r2 = area.removeFromTop (rowHeight).withTrimmedLeft (labelWidth);
        area.removeFromTop (gap);
        tailToggle.setBounds (r2);
    }

    nameEditor.setBounds (row (nameLabel));

    {
        auto r = row (folderLabel);
        chooseFolderButton.setBounds (r.removeFromRight (90));
        r.removeFromRight (6);
        folderPath.setBounds (r);
    }

    auto buttons = area.removeFromBottom (34);
    exportButton.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (8);
    closeButton.setBounds (buttons.removeFromRight (90));
    showButton.setBounds (buttons.removeFromLeft (130));

    area.removeFromBottom (gap);
    progressBar.setBounds (area.removeFromBottom (22));
    status.setBounds (area);
}

} // namespace aether
