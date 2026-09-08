#include "PluginEditor.h"
#include "Exporter.h"

using namespace juce;
using namespace hicplug;

HicEditor::HicEditor(HicProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      transport(p.params(), "Transport"), feel(p.params(), "Feel"),
      bedPanel(p.params(), "Bed"), duckPanel(p.params(), "Duck"), repeatPanel(p.params(), "Repeat"),
      freezePanel(p.params(), "Freeze"), reverbPanel(p.params(), "Reverb"),
      padEditor(p.params()) {
    setLookAndFeel(nullptr);
    getLookAndFeel().setColour(ResizableWindow::backgroundColourId, Colour(0xff15171b));

    title.setText("Hic", dontSendNotification);
    title.setFont(Font(FontOptions(22.0f, Font::bold)));
    title.setColour(Label::textColourId, Colours::white.withAlpha(0.9f));
    addAndMakeVisible(title);
    kitButton.onClick = [this] { loadKitMenu(); };
    exportButton.onClick = [this] { exportKit(); };
    loopButton.onClick = [this] { exportLoop(); };
    addAndMakeVisible(kitButton); addAndMakeVisible(exportButton); addAndMakeVisible(loopButton);

    transport.setCellSize(76, 60);
    transport.rebuild({ { id::play, "Play" }, { id::sync, "Sync" }, { id::bpm, "BPM" }, { id::pattern, "Pattern" },
                        { id::seqEnable, "Seq" }, { id::out, "Output" } });
    feel.setCellSize(76, 60);
    feel.rebuild({ { id::swing, "Swing" }, { id::nudge, "Nudge" }, { id::scatter, "Scatter" }, { id::velScatter, "Vel Scat" },
                   { id::seed, "Seed" }, { id::lookahead, "Lookahead" } });
    addAndMakeVisible(transport); addAndMakeVisible(feel);

    bedPanel.setCellSize(66, 74);
    bedPanel.rebuild({ { id::bedType, "Type" }, { id::bedLevel, "Level" }, { id::bedDensity, "Density" }, { id::bedWarmth, "Warmth" },
                       { id::bedPop, "Pops" }, { id::bedColor, "Color" }, { id::bedHiss, "Hiss" }, { id::bedGate, "Gate" },
                       { id::bedGateAtt, "Gate Att" }, { id::bedGateRel, "Gate Rel" }, { id::bedGateDuty, "Gate Open" } });
    duckPanel.setCellSize(66, 74);
    duckPanel.rebuild({ { id::duckDepth, "Depth" }, { id::duckHold, "Hold" }, { id::duckRel, "Release" } });
    repeatPanel.setCellSize(66, 74);
    repeatPanel.rebuild({ { id::repOn, "On" }, { id::repGrid, "Grid" }, { id::repProb, "Chance" }, { id::repLen, "Length" },
                          { id::repMinBars, "Bars" }, { id::repMix, "Mix" } });
    freezePanel.setCellSize(66, 74);
    freezePanel.rebuild({ { id::frzHold, "Hold" }, { id::frzGrain, "Grain" }, { id::frzDensity, "Density" }, { id::frzSpray, "Spray" },
                          { id::frzJitter, "Jitter" }, { id::frzMix, "Mix" } });
    reverbPanel.setCellSize(66, 74);
    reverbPanel.rebuild({ { id::revType, "Type" }, { id::revDecay, "Decay" }, { id::revDamp, "Damp" }, { id::revPre, "Predelay" }, { id::revMix, "Return" } });
    addAndMakeVisible(bedPanel); addAndMakeVisible(duckPanel); addAndMakeVisible(repeatPanel);
    addAndMakeVisible(freezePanel); addAndMakeVisible(reverbPanel);

    pads.onSelect = [this](int pad) { padEditor.setPad(pad); };
    pads.onAudition = [this](int pad) { proc.auditionPad(pad, 0.85f); };
    addAndMakeVisible(pads); addAndMakeVisible(padEditor);

    grid.onEdit = [this] { proc.bridge().publishPatterns(); };
    grid.currentStep = [this](int track) { return proc.transportPlaying() ? proc.currentStep(track) : -1; };
    addAndMakeVisible(grid);
    showPattern();
    proc.params().addParameterListener(id::pattern, this);

    setResizable(true, true);
    setResizeLimits(960, 720, 2400, 1600);
    setSize(1160, 860);
    startTimerHz(30);

    // Developer hooks for headless checks: HIC_SNAPSHOT=<file.png> writes a picture of
    // the editor; HIC_EXPORT_DIR=<dir> runs the kit and loop exporters. Either quits afterwards.
    const char* snap = std::getenv("HIC_SNAPSHOT");
    const char* exportDir = std::getenv("HIC_EXPORT_DIR");
    if (snap != nullptr || exportDir != nullptr) {
        const juce::String snapPath(snap != nullptr ? snap : "");
        const juce::String dirPath(exportDir != nullptr ? exportDir : "");
        Timer::callAfterDelay(1500, [sp = Component::SafePointer<HicEditor>(this), snapPath, dirPath] {
            if (sp == nullptr) return;
            if (snapPath.isNotEmpty()) {
                juce::Image img = sp->createComponentSnapshot(sp->getLocalBounds(), true, 1.0f);
                juce::File f(snapPath); f.deleteFile();
                if (auto out = f.createOutputStream()) { juce::PNGImageFormat png; png.writeImageToStream(img, *out); }
            }
            if (dirPath.isNotEmpty()) {
                juce::File dir(dirPath);
                Exporter::Options opt; String err;
                const int n = Exporter::exportKit(sp->proc.bridge(), dir, opt, err);
                const bool ok = Exporter::exportLoop(sp->proc.bridge(), dir.getChildFile("Hic_loop.wav"), 92.0, opt, err);
                std::printf("HIC_EXPORT: %d one-shots, loop %s %s\n", n, ok ? "ok" : "failed", err.toRawUTF8());
                std::fflush(stdout);
            }
            juce::JUCEApplicationBase::quit();
        });
    }
}

void HicEditor::loadKitMenu() {
    PopupMenu m;
    const StringArray names = kitNames();
    for (int i = 0; i < names.size(); ++i) m.addItem(i + 1, names[i]);
    m.showMenuAsync(PopupMenu::Options().withTargetComponent(kitButton), [this](int r) { if (r > 0) proc.bridge().loadKit(r - 1); });
}

void HicEditor::exportKit() {
    chooser = std::make_unique<FileChooser>("Choose a folder for the one-shots", File::getSpecialLocation(File::userMusicDirectory));
    chooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this](const FileChooser& fc) {
        const File dir = fc.getResult();
        if (dir == File()) return;
        Exporter::Options opt; opt.sampleRate = proc.getSampleRate() > 1000.0 ? proc.getSampleRate() : 48000.0;
        String err;
        const int n = Exporter::exportKit(proc.bridge(), dir, opt, err);
        AlertWindow::showMessageBoxAsync(err.isEmpty() ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon, "Export kit",
                                         err.isEmpty() ? String(n) + " one-shots written to " + dir.getFullPathName() : err);
    });
}

void HicEditor::exportLoop() {
    chooser = std::make_unique<FileChooser>("Save the pattern loop as", File::getSpecialLocation(File::userMusicDirectory).getChildFile("Hic_loop.wav"), "*.wav");
    chooser->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting, [this](const FileChooser& fc) {
        const File file = fc.getResult();
        if (file == File()) return;
        Exporter::Options opt; opt.sampleRate = proc.getSampleRate() > 1000.0 ? proc.getSampleRate() : 48000.0;
        String err;
        const double bpm = static_cast<double>(proc.params().getRawParameterValue(id::bpm)->load());
        const bool ok = Exporter::exportLoop(proc.bridge(), file.withFileExtension("wav"), bpm, opt, err);
        AlertWindow::showMessageBoxAsync(ok ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon, "Bounce loop",
                                         ok ? "Loop written to " + file.getFullPathName() : err);
    });
}

HicEditor::~HicEditor() { proc.params().removeParameterListener(id::pattern, this); }

void HicEditor::parameterChanged(const String&, float) {
    MessageManager::callAsync([sp = Component::SafePointer<HicEditor>(this)] { if (sp) sp->showPattern(); });
}

void HicEditor::showPattern() {
    const int idx = jlimit(0, hic::kNumPatterns - 1, static_cast<int>(proc.params().getRawParameterValue(id::pattern)->load() + 0.5f) - 1);
    if (idx == shownPattern) return;
    shownPattern = idx;
    grid.setPattern(&proc.bridge().editPattern(idx));
}

void HicEditor::timerCallback() { grid.repaint(); }

void HicEditor::paint(Graphics& g) { g.fillAll(Colour(0xff15171b)); }

void HicEditor::resized() {
    auto r = getLocalBounds().reduced(6);
    auto top = r.removeFromTop(84);
    title.setBounds(top.removeFromLeft(70));
    transport.setBounds(top.removeFromLeft(6 * 76 + 12));
    top.removeFromLeft(6);
    feel.setBounds(top.removeFromLeft(6 * 76 + 12));
    top.removeFromLeft(6);
    auto buttons = top.removeFromLeft(120);
    kitButton.setBounds(buttons.removeFromTop(26).reduced(2));
    exportButton.setBounds(buttons.removeFromTop(26).reduced(2));
    loopButton.setBounds(buttons.removeFromTop(26).reduced(2));
    r.removeFromTop(6);

    auto fxRow2 = r.removeFromBottom(100);
    repeatPanel.setBounds(fxRow2.removeFromLeft(6 * 66 + 12)); fxRow2.removeFromLeft(6);
    freezePanel.setBounds(fxRow2.removeFromLeft(6 * 66 + 12)); fxRow2.removeFromLeft(6);
    reverbPanel.setBounds(fxRow2);
    r.removeFromBottom(6);
    auto fxRow = r.removeFromBottom(100);
    bedPanel.setBounds(fxRow.removeFromLeft(11 * 66 + 12)); fxRow.removeFromLeft(6);
    duckPanel.setBounds(fxRow);
    r.removeFromBottom(6);

    auto padRow = r.removeFromBottom(150);
    padEditor.setBounds(padRow);
    r.removeFromBottom(6);

    pads.setBounds(r.removeFromLeft(110));
    r.removeFromLeft(6);
    grid.setBounds(r);
}
