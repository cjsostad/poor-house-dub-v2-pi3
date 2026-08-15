#pragma once

#include "Common.h"
#include "DSP/Oscillator.h"
#include "DSP/Envelope.h"
#include "DSP/LFO.h"
#include "DSP/Filter.h"
#include "DSP/Delay.h"
#include "DSP/Reverb.h"
#include <memory>
#include <mutex>

namespace DubSiren {

/**
 * Main Dub Siren Audio Engine.
 * 
 * Integrates all DSP components and provides a thread-safe interface
 * for parameter control from the GPIO controller.
 */
class AudioEngine {
public:
    explicit AudioEngine(int sampleRate = DEFAULT_SAMPLE_RATE, int bufferSize = DEFAULT_BUFFER_SIZE);
    ~AudioEngine() = default;
    
    // Non-copyable, non-movable (audio engine is stateful)
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    
    /**
     * Generate audio samples.
     * Called from the audio callback thread.
     * @param output Buffer to fill with stereo interleaved samples
     * @param numFrames Number of frames (samples per channel)
     */
    void process(float* output, int numFrames);
    
    /**
     * Trigger the siren sound.
     */
    void trigger();
    
    /**
     * Release the siren sound.
     */
    void release();
    
    /**
     * Cycle through pitch envelope modes.
     * @return The new pitch envelope mode name
     */
    const char* cyclePitchEnvelope();
    
    // ========================================================================
    // Parameter Setters (Thread-Safe)
    // ========================================================================
    
    // Master
    void setVolume(float volume);
    
    // Oscillator
    void setFrequency(float freq);
    void setWaveform(Waveform wf);
    void setWaveform(int index);
    
    // Envelope
    void setAttackTime(float seconds);
    void setReleaseTime(float seconds);
    
    // LFO
    void setLfoRate(float rate);
    void setLfoDepth(float depth);
    void setLfoPitchDepth(float depth);  // LFO modulation depth for pitch (0.0-1.0, where 1.0 = ±1 octave)
    void setLfoWaveform(Waveform wf);
    void setLfoWaveform(int index);
    
    // Filter
    void setFilterCutoff(float freq);
    void setFilterResonance(float res);
    
    // Delay
    void setDelayTime(float seconds);
    void setDelayFeedback(float feedback);
    void setDelayMix(float mix);
    
    // Reverb
    void setReverbSize(float size);
    void setReverbMix(float mix);
    void setReverbDamping(float damping);
    
    // Pitch Envelope
    void setPitchEnvelopeMode(PitchEnvelopeMode mode);
    
    // ========================================================================
    // Getters
    // ========================================================================
    
    float getVolume() const { return volume.get(); }
    float getFrequency() const { return baseFrequency.get(); }
    bool isPlaying() const { return envelope.isActive() || envelope.getCurrentValue() > 0.001f; }
    PitchEnvelopeMode getPitchEnvelopeMode() const { return pitchEnvMode.get(); }
    
private:
    int sampleRate;
    int bufferSize;
    
    // DSP Components
    Oscillator oscillator;
    LFO lfo;
    Envelope envelope;
    LowPassFilter filter;
    DCBlocker dcBlocker;
    DelayEffect delay;
    ReverbEffect reverb;
    
    // Thread-safe parameters
    AudioParameter<float> volume;
    AudioParameter<float> baseFrequency;
    AudioParameter<float> lfoPitchDepth;  // LFO pitch modulation depth
    AudioParameter<PitchEnvelopeMode> pitchEnvMode;
    
    // Internal state
    float currentFrequency;
    SmoothedValue frequencySmooth;
    
    // Pitch envelope state
    bool inReleasePhase;
    float pitchEnvStartLevel;  // Envelope level when release started
    
    // Temporary buffers (pre-allocated to avoid allocation in audio thread)
    std::vector<float> oscBuffer;
    std::vector<float> envBuffer;
    std::vector<float> lfoBuffer;
    std::vector<float> filterBuffer;
    std::vector<float> delayBuffer;
    
    // Mutex for trigger/release operations
    std::mutex triggerMutex;
};

} // namespace DubSiren
