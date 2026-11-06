// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Exporter.h"
#include "ui/Theme.h"

namespace aether
{

/** File > Export audio: the mix and/or stems, to WAV, FLAC or MP3.

    The export runs on a background thread from a copy of the project, so you can keep working
    (or close this window, which cancels it). The choices are remembered for next time. */
class ExportDialog final : public juce::Component,
                           private juce::Timer
{
public:
    ExportDialog (const ui::Theme& theme, const Project& project, const ProjectLocation& location, juce::PropertySet& settings);
    ~ExportDialog() override;

    /** The options the controls describe. */
    ExportOptions getOptions() const;

    /** Starts exporting (as the Export button does). */
    void startExport();
    bool isExporting() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Worker;

    void timerCallback() override;
    void updateChoices();
    void saveChoices();
    void finished();
    void setControlsEnabled (bool enabled);
    ExportFormat selectedFormat() const;

    const ui::Theme& theme;
    const Project& project;
    ProjectLocation location;
    juce::PropertySet& settings;
    juce::File folder;

    juce::Label whatLabel, rangeLabel, formatLabel, qualityLabel, rateLabel, optionsLabel, nameLabel, folderLabel;
    juce::ToggleButton mixToggle { "The mix (one file)" }, stemsToggle { "Stems (one file per track)" };
    juce::ToggleButton normalizeToggle { "Normalize the mix (loudest peak at -1 dB)" };
    juce::ToggleButton tailToggle { "Let effects and notes ring out at the end" };
    juce::ComboBox rangeBox, formatBox, qualityBox, rateBox;
    juce::TextEditor nameEditor;
    juce::Label folderPath, status;
    juce::TextButton chooseFolderButton { "Choose..." }, exportButton { "Export" }, closeButton { "Close" },
                     showButton { "Show in folder" };
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };

    std::unique_ptr<ExportJob> job;
    std::unique_ptr<Worker> worker;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportDialog)
};

} // namespace aether
