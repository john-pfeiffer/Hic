#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "hic/seq/Pattern.h"
#include "hic/Kit.h"

namespace hicplug {

/// The pattern editor. One row per pad, one cell per step.
///  click        toggle a step
///  drag up/down velocity      alt-drag: nudge      shift-drag: probability
///  right-click  ratchet, accent, reverse, static pulse, note offset, clear
///  row name     right-click for track length and mute
class StepGrid : public juce::Component {
public:
    std::function<void()> onEdit;                       // called after any change
    std::function<int(int)> currentStep;                // playhead per track, -1 when stopped

    void setPattern(hic::Pattern* p) { pattern = p; repaint(); }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1c1f24));
        if (!pattern) return;
        const int cols = visibleCols();
        const float cw = cellWidth(cols), rh = rowHeight();
        for (int r = 0; r < hic::kNumPads; ++r) {
            const hic::Track& t = pattern->tracks[r];
            const float y = r * rh;
            g.setColour(juce::Colour(0xff272b31));
            g.fillRect(0.0f, y, (float)kHeader, rh - 1.0f);
            g.setColour(t.mute ? juce::Colours::grey : juce::Colours::white.withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(hic::defaultPadName(r), 4, (int)y, kHeader - 8, (int)rh, juce::Justification::centredLeft);
            g.drawText(juce::String(t.length), 4, (int)y, kHeader - 8, (int)rh, juce::Justification::centredRight);
            const int play = currentStep ? currentStep(r) : -1;
            for (int c = 0; c < cols; ++c) {
                const float x = kHeader + c * cw;
                juce::Rectangle<float> cell(x + 1.0f, y + 1.0f, cw - 2.0f, rh - 2.0f);
                const bool inLen = c < t.length;
                g.setColour(inLen ? ((c / 4) % 2 ? juce::Colour(0xff23272d) : juce::Colour(0xff2a2f36)) : juce::Colour(0xff1f2226));
                g.fillRect(cell);
                if (c == play) { g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRect(cell); }
                if (!inLen) continue;
                const hic::Step& s = t.steps[c];
                if (!s.on) continue;
                const float v = s.vel / 127.0f;
                const float alpha = 0.35f + 0.65f * (s.prob / 100.0f);
                juce::Colour col = (s.flags & hic::StepAccent) ? juce::Colour(0xffe8c170) : juce::Colour(0xff7fb0d8);
                if (s.flags & hic::StepReverse) col = juce::Colour(0xffc98cd8);
                g.setColour(col.withAlpha(alpha));
                const float nudgePx = juce::jlimit(-cw * 0.4f, cw * 0.4f, s.nudgeMs / 20.0f * cw * 0.4f);
                juce::Rectangle<float> bar(cell.getX() + 2.0f + nudgePx, cell.getBottom() - 3.0f - (cell.getHeight() - 6.0f) * v,
                                           juce::jmax(3.0f, cell.getWidth() - 6.0f), (cell.getHeight() - 6.0f) * v + 1.0f);
                g.fillRect(bar);
                if (s.ratchet >= 2) {
                    g.setColour(juce::Colours::black.withAlpha(0.5f));
                    for (int k = 1; k < s.ratchet; ++k) g.fillRect(bar.getX() + bar.getWidth() * k / s.ratchet, bar.getY(), 1.0f, bar.getHeight());
                }
                if (s.flags & hic::StepBedGate) { g.setColour(juce::Colour(0xffa0d8a0)); g.fillRect(cell.getX(), cell.getBottom() - 2.0f, cell.getWidth(), 2.0f); }
                if (s.noteOffset != 0) {
                    g.setColour(juce::Colours::white.withAlpha(0.8f));
                    g.setFont(juce::Font(juce::FontOptions(9.0f)));
                    g.drawText((s.noteOffset > 0 ? "+" : "") + juce::String(s.noteOffset), cell.toNearestInt(), juce::Justification::topRight);
                }
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (!pattern) return;
        const int r = rowAt(e.y);
        if (r < 0) return;
        hic::Track& t = pattern->tracks[r];
        if (e.x < kHeader) { if (e.mods.isPopupMenu()) trackMenu(r); return; }
        const int c = colAt(e.x);
        if (c < 0 || c >= t.length) return;
        hic::Step& s = t.steps[c];
        if (e.mods.isPopupMenu()) { stepMenu(r, c); return; }
        dragRow = r; dragCol = c; dragged = false;
        startVel = s.vel; startNudge = s.nudgeMs; startProb = s.prob;
        if (!s.on) { s.on = 1; s.vel = static_cast<uint8_t>(juce::jlimit(1, 127, lastVel)); s.prob = 100; s.nudgeMs = 0; s.ratchet = 0; s.flags = 0; s.noteOffset = 0; startVel = s.vel; turnedOn = true; changed(); }
        else turnedOn = false;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!pattern || dragRow < 0) return;
        const int dy = e.getDistanceFromDragStartY();
        if (std::abs(dy) < 3 && !dragged) return;
        dragged = true;
        hic::Step& s = pattern->tracks[dragRow].steps[dragCol];
        if (e.mods.isAltDown())        s.nudgeMs = static_cast<int8_t>(juce::jlimit(-20, 20, startNudge - dy / 3));
        else if (e.mods.isShiftDown()) s.prob = static_cast<uint8_t>(juce::jlimit(0, 100, startProb - dy));
        else { s.vel = static_cast<uint8_t>(juce::jlimit(1, 127, startVel - dy)); lastVel = s.vel; }
        changed();
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (!pattern || dragRow < 0) return;
        if (!dragged && !turnedOn) { pattern->tracks[dragRow].steps[dragCol].on = 0; changed(); }
        dragRow = dragCol = -1;
    }

private:
    static constexpr int kHeader = 92;
    int visibleCols() const {
        int m = 16;
        if (pattern) for (auto& t : pattern->tracks) m = juce::jmax(m, (int)t.length);
        return ((m + 3) / 4) * 4;
    }
    float cellWidth(int cols) const { return (getWidth() - kHeader) / (float)cols; }
    float rowHeight() const { return getHeight() / (float)hic::kNumPads; }
    int rowAt(int y) const { const int r = (int)(y / rowHeight()); return (r >= 0 && r < hic::kNumPads) ? r : -1; }
    int colAt(int x) const { const int c = (int)((x - kHeader) / cellWidth(visibleCols())); return (c >= 0 && c < visibleCols()) ? c : -1; }
    void changed() { repaint(); if (onEdit) onEdit(); }

    void stepMenu(int r, int c) {
        hic::Step& s = pattern->tracks[r].steps[c];
        juce::PopupMenu m, ratchet, note;
        m.addItem(1, "Accent", true, (s.flags & hic::StepAccent) != 0);
        m.addItem(2, "Reverse (swell into step)", true, (s.flags & hic::StepReverse) != 0);
        m.addItem(3, "Static pulse", true, (s.flags & hic::StepBedGate) != 0);
        for (int k = 1; k <= 4; ++k) ratchet.addItem(10 + k, k == 1 ? "Single" : juce::String(k) + " hits", true, (s.ratchet <= 1 ? 1 : s.ratchet) == k);
        m.addSubMenu("Ratchet", ratchet);
        for (int n = -12; n <= 12; ++n) note.addItem(100 + n + 12, (n > 0 ? "+" : "") + juce::String(n), true, s.noteOffset == n);
        m.addSubMenu("Note offset", note);
        m.addSeparator();
        m.addItem(200, "Clear step");
        m.showMenuAsync(juce::PopupMenu::Options(), [this, r, c](int result) {
            if (!pattern || result == 0) return;
            hic::Step& st = pattern->tracks[r].steps[c];
            if (result == 1) st.flags ^= hic::StepAccent;
            else if (result == 2) st.flags ^= hic::StepReverse;
            else if (result == 3) st.flags ^= hic::StepBedGate;
            else if (result >= 11 && result <= 14) st.ratchet = static_cast<uint8_t>(result - 10 == 1 ? 0 : result - 10);
            else if (result >= 100 && result < 125) st.noteOffset = static_cast<int8_t>(result - 112);
            else if (result == 200) st = hic::Step{};
            changed();
        });
    }

    void trackMenu(int r) {
        hic::Track& t = pattern->tracks[r];
        juce::PopupMenu m, len;
        for (int l : { 4, 6, 8, 12, 16, 24, 32, 48, 64 }) len.addItem(300 + l, juce::String(l) + " steps", true, t.length == l);
        m.addSubMenu("Length", len);
        m.addItem(400, "Mute", true, t.mute != 0);
        m.addSeparator();
        m.addItem(401, "Clear track");
        m.showMenuAsync(juce::PopupMenu::Options(), [this, r](int result) {
            if (!pattern || result == 0) return;
            hic::Track& tr = pattern->tracks[r];
            if (result >= 300 && result < 400) tr.length = static_cast<uint8_t>(result - 300);
            else if (result == 400) tr.mute = tr.mute ? 0 : 1;
            else if (result == 401) for (auto& s : tr.steps) s = hic::Step{};
            changed();
        });
    }

    hic::Pattern* pattern = nullptr;
    int dragRow = -1, dragCol = -1, startVel = 100, startNudge = 0, startProb = 100, lastVel = 100;
    bool dragged = false, turnedOn = false;
};

} // namespace hicplug
