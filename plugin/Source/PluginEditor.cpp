#include "PluginEditor.h"
#include "Exporter.h"

using namespace juce;
using namespace hicplug;

static constexpr int kBaseHeight = 720, kDetailHeight = 210;

HicEditor::HicEditor(HicProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      transport(p.params(), "Transport"), feelDetail(p.params(), "Feel detail"),
      staticPanel(p.params(), "Static"), duckPanel(p.params(), "Duck"), repeatPanel(p.params(), "Repeat"),
      freezePanel(p.params(), "Freeze"), reverbPanel(p.params(), "Reverb"),
      padEditor(p.params()), bus(p.params()) {
    getLookAndFeel().setColour(ResizableWindow::backgroundColourId, Colour(0xff15171b));

    title.setText("Hic", dontSendNotification);
    title.setFont(Font(FontOptions(22.0f, Font::bold)));
    title.setColour(Label::textColourId, Colours::white.withAlpha(0.9f));
    addAndMakeVisible(title);
    kitButton.onClick = [this] { loadKitMenu(); };
    exportButton.onClick = [this] { exportKit(); };
    loopButton.onClick = [this] { exportLoop(); };
    detailButton.onClick = [this] { toggleDetail(); };
    detailButton.setClickingTogglesState(true);
    addAndMakeVisible(kitButton); addAndMakeVisible(exportButton); addAndMakeVisible(loopButton); addAndMakeVisible(detailButton);

    transport.setCellSize(76, 60);
    transport.rebuild({ { id::play, "Play" }, { id::sync, "Sync" }, { id::bpm, "BPM" }, { id::pattern, "Pattern" },
                        { id::seqEnable, "Seq" }, { id::swing, "Swing" }, { id::out, "Output" } });
    addAndMakeVisible(transport);

    feelDetail.setCellSize(66, 74);
    feelDetail.rebuild({ { id::nudge, "Nudge" }, { id::scatter, "Scatter Max" }, { id::velScatter, "Vel Max" }, { id::seed, "Seed" }, { id::lookahead, "Lookahead" } });
    staticPanel.setCellSize(66, 74);
    staticPanel.rebuild({ { id::stClock, "Clock" }, { id::stLevel, "Level" }, { id::stDensity, "Density" }, { id::stColour, "Colour" },
                          { id::stWarmth, "Warmth" }, { id::stPulse, "Pulse" }, { id::stAtt, "Attack" }, { id::stRel, "Release" } });
    duckPanel.setCellSize(66, 74);
    duckPanel.rebuild({ { id::duckDepth, "Depth" }, { id::duckHold, "Hold" }, { id::duckRel, "Release" } });
    repeatPanel.setCellSize(62, 74);
    repeatPanel.rebuild({ { id::repOn, "On" }, { id::repGrid, "Grid" }, { id::repProb, "Chance" }, { id::repLen, "Length" },
                          { id::repMinBars, "Bars" }, { id::repMix, "Mix" } });
    freezePanel.setCellSize(62, 74);
    freezePanel.rebuild({ { id::frzHold, "Hold" }, { id::frzGrain, "Grain" }, { id::frzDensity, "Density" }, { id::frzSpray, "Spray" },
                          { id::frzJitter, "Jitter" }, { id::frzMix, "Mix" } });
    reverbPanel.setCellSize(62, 74);
    reverbPanel.rebuild({ { id::revType, "Type" }, { id::revDecay, "Decay" }, { id::revDamp, "Damp" }, { id::revPre, "Predelay" } });
    for (auto* c : { &feelDetail, &staticPanel, &duckPanel, &repeatPanel, &freezePanel, &reverbPanel }) { addChildComponent(c); c->setVisible(false); }

    pads.onSelect = [this](int pad) { padEditor.setPad(pad); };
    pads.onAudition = [this](int pad) { proc.auditionPad(pad, 0.85f); };
    addAndMakeVisible(pads); addAndMakeVisible(padEditor); addAndMakeVisible(bus);

    grid.onEdit = [this] { proc.bridge().publishPatterns(); };
    grid.currentStep = [this](int track) { return proc.transportPlaying() ? proc.currentStep(track) : -1; };
    addAndMakeVisible(grid);
    showPattern();
    proc.params().addParameterListener(id::pattern, this);

    setResizable(true, true);
    setResizeLimits(980, 640, 2400, 1600);
    setSize(1160, kBaseHeight);
    startTimerHz(30);

    // Developer hooks for headless checks: HIC_SNAPSHOT=<file.png> writes a picture of
    // the editor; HIC_EXPORT_DIR=<dir> runs the kit and loop exporters. Either quits afterwards.
    const char* snap = std::getenv("HIC_SNAPSHOT");
    const char* exportDir = std::getenv("HIC_EXPORT_DIR");
    if (snap != nullptr || exportDir != nullptr) {
        const juce::String snapPath(snap != nullptr ? snap : "");
        const juce::String dirPath(exportDir != nullptr ? exportDir : "");
        if (std::getenv("HIC_SNAPSHOT_DETAIL") != nullptr) { detailButton.setToggleState(true, dontSendNotification); toggleDetail(); }
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

void HicEditor::toggleDetail() {
    detailVisible = detailButton.getToggleState();
    for (auto* c : { &feelDetail, &staticPanel, &duckPanel, &repeatPanel, &freezePanel, &reverbPanel }) c->setVisible(detailVisible);
    setSize(getWidth(), detailVisible ? getHeight() + kDetailHeight : jmax(640, getHeight() - kDetailHeight));
}

void HicEditor::timerCallback() { grid.repaint(); }

void HicEditor::paint(Graphics& g) { g.fillAll(Colour(0xff15171b)); }

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

void HicEditor::resized() {
    auto r = getLocalBounds().reduced(6);
    auto top = r.removeFromTop(84);
    title.setBounds(top.removeFromLeft(70));
    transport.setBounds(top.removeFromLeft(7 * 76 + 12));
    top.removeFromLeft(6);
    auto buttons = top.removeFromLeft(240);
    auto col1 = buttons.removeFromLeft(120), col2 = buttons;
    kitButton.setBounds(col1.removeFromTop(26).reduced(2));
    exportButton.setBounds(col1.removeFromTop(26).reduced(2));
    loopButton.setBounds(col1.removeFromTop(26).reduced(2));
    detailButton.setBounds(col2.removeFromTop(26).reduced(2));
    r.removeFromTop(6);

    if (detailVisible) {
        auto row2 = r.removeFromBottom(100);
        repeatPanel.setBounds(row2.removeFromLeft(6 * 62 + 12)); row2.removeFromLeft(6);
        freezePanel.setBounds(row2.removeFromLeft(6 * 62 + 12)); row2.removeFromLeft(6);
        reverbPanel.setBounds(row2);
        r.removeFromBottom(6);
        auto row1 = r.removeFromBottom(100);
        staticPanel.setBounds(row1.removeFromLeft(8 * 66 + 12)); row1.removeFromLeft(6);
        duckPanel.setBounds(row1.removeFromLeft(3 * 66 + 12)); row1.removeFromLeft(6);
        feelDetail.setBounds(row1);
        r.removeFromBottom(6);
    }

    bus.setBounds(r.removeFromBottom(126));
    r.removeFromBottom(6);
    padEditor.setBounds(r.removeFromBottom(150));
    r.removeFromBottom(6);

    pads.setBounds(r.removeFromLeft(110));
    r.removeFromLeft(6);
    grid.setBounds(r);
}
