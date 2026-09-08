#include "PluginEditor.h"

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
    setResizeLimits(960, 640, 2400, 1600);
    setSize(1160, 760);
    startTimerHz(30);
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
    r.removeFromTop(6);

    auto bottom = r.removeFromBottom(100);
    auto fxRow = bottom;
    bedPanel.setBounds(fxRow.removeFromLeft(11 * 66 + 12)); fxRow.removeFromLeft(6);
    duckPanel.setBounds(fxRow.removeFromLeft(3 * 66 + 12)); fxRow.removeFromLeft(6);
    repeatPanel.setBounds(fxRow.removeFromLeft(6 * 66 + 12)); fxRow.removeFromLeft(6);
    freezePanel.setBounds(fxRow.removeFromLeft(6 * 66 + 12)); fxRow.removeFromLeft(6);
    reverbPanel.setBounds(fxRow);
    r.removeFromBottom(6);

    auto padRow = r.removeFromBottom(150);
    padEditor.setBounds(padRow);
    r.removeFromBottom(6);

    pads.setBounds(r.removeFromLeft(110));
    r.removeFromLeft(6);
    grid.setBounds(r);
}
