#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include "hic/Engine.h"
#include "EngineBridge.h"

namespace hicplug {

/// Offline bounces for building your own sample packs: every pad as a set of
/// one-shots (several seeds and velocities, so morph and feel give real
/// variations), and the active pattern as a loop. Runs on the message
/// thread with its own Engine; the audio thread is never involved.
class Exporter {
public:
    struct Options {
        int    variations  = 4;
        float  velocities[3] = { 0.55f, 0.8f, 1.0f };
        int    numVelocities = 3;
        double sampleRate  = 48000.0;
        int    loopRepeats = 2;
        bool   oneShotsWithReverb = true;
    };

    /// Returns the number of files written.
    static int exportKit(EngineBridge& bridge, const juce::File& dir, const Options& opt, juce::String& error) {
        auto engine = std::make_unique<hic::Engine>();
        bridge.apply(*engine);
        engine->bed.type = hic::BedType::Off;
        engine->repeat.enabled = false;
        engine->freeze.hold = false;
        engine->feel.lookaheadMs = 0.0f;
        if (!opt.oneShotsWithReverb) engine->reverb.mix = 0.0f;
        const juce::String kit = engine->global.internalPlay ? "" : "";
        (void)kit;
        dir.createDirectory();
        int written = 0;
        const float sr = static_cast<float>(opt.sampleRate);
        const uint32_t baseSeed = engine->feel.seed;
        for (int pad = 0; pad < hic::kNumPads; ++pad) {
            if (engine->kit.pads[pad].level <= 0.0f) continue;
            const int note = pad == hic::PadGlock ? 72 : (pad == hic::PadThumb ? 45 : 36);
            for (int v = 0; v < opt.variations; ++v) {
                for (int k = 0; k < opt.numVelocities; ++k) {
                    engine->feel.seed = baseSeed + static_cast<uint32_t>(v * 7919);
                    engine->prepare(sr);
                    engine->queueHit(pad, opt.velocities[k], note);
                    juce::AudioBuffer<float> buf(2, static_cast<int>(sr * (pad == hic::PadGlock ? 4.0f : 2.5f)));
                    render(*engine, buf);
                    const int len = trimmedLength(buf, sr);
                    juce::String name = juce::String::formatted("Hic_%02d_%s_v%d_%03d.wav", pad, sanitize(hic::defaultPadName(pad)).toRawUTF8(), v + 1,
                                                                static_cast<int>(opt.velocities[k] * 127.0f));
                    if (!write(dir.getChildFile(name), buf, len, sr, error)) return written;
                    ++written;
                }
            }
        }
        engine->feel.seed = baseSeed;
        return written;
    }

    /// Bounces the active pattern on the internal clock at the given tempo.
    static bool exportLoop(EngineBridge& bridge, const juce::File& file, double bpm, const Options& opt, juce::String& error) {
        auto engine = std::make_unique<hic::Engine>();
        bridge.apply(*engine);
        engine->feel.lookaheadMs = 0.0f;
        engine->global.seqEnabled = true;
        engine->global.internalPlay = true;
        engine->global.internalBpm = bpm;
        const float sr = static_cast<float>(opt.sampleRate);
        engine->prepare(sr);
        const hic::Pattern& p = engine->activePattern();
        int maxLen = 1;
        for (auto& t : p.tracks) maxLen = juce::jmax(maxLen, static_cast<int>(t.length));
        const double loopBeats = static_cast<double>(maxLen) / static_cast<double>(juce::jmax<int>(1, p.stepsPerBeat));
        const int frames = static_cast<int>(loopBeats * opt.loopRepeats * 60.0 / bpm * sr);
        juce::AudioBuffer<float> buf(2, frames);
        render(*engine, buf);
        return write(file, buf, frames, sr, error);
    }

private:
    static juce::String sanitize(const juce::String& s) { return s.replaceCharacter(' ', '_'); }

    static void render(hic::Engine& e, juce::AudioBuffer<float>& buf) {
        const int n = buf.getNumSamples();
        hic::TransportInfo t;
        for (int pos = 0; pos < n; pos += 256) {
            const int len = juce::jmin(256, n - pos);
            e.process(nullptr, 0, t, buf.getWritePointer(0) + pos, buf.getWritePointer(1) + pos, len);
        }
    }

    static int trimmedLength(const juce::AudioBuffer<float>& buf, float sr) {
        const float thr = 1e-4f;   // -80 dB
        int last = 0;
        for (int ch = 0; ch < 2; ++ch) {
            const float* x = buf.getReadPointer(ch);
            for (int i = buf.getNumSamples() - 1; i > last; --i) if (std::fabs(x[i]) > thr) { last = i; break; }
        }
        return juce::jmin(buf.getNumSamples(), last + static_cast<int>(sr * 0.02f) + 1);
    }

    static bool write(const juce::File& file, const juce::AudioBuffer<float>& buf, int frames, float sr, juce::String& error) {
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
        if (out == nullptr) { error = "Could not write " + file.getFullPathName(); return false; }
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(out.get(), static_cast<double>(sr), 2, 24, {}, 0));
        if (w == nullptr) { error = "Could not create a WAV writer"; return false; }
        out.release();
        w->writeFromAudioSampleBuffer(buf, 0, frames);
        return true;
    }
};

} // namespace hicplug
