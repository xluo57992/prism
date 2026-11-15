#include "DispersionEngine.h"
#include <cmath>

//==============================================================================
void DispersionEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;

    freqSmooth.reset (sampleRate, 0.05);   // 50ms 平滑，扫频不咔
    qSmooth.reset (sampleRate, 0.05);

    freqSmooth.setCurrentAndTargetValue (juce::jlimit (20.0f, (float) (sampleRate * 0.45), params.frequencyHz));
    qSmooth.setCurrentAndTargetValue (juce::jmax (0.005f, params.q));

    activeStages = 0;
    reset();
}

void DispersionEngine::reset()
{
    for (auto& s : stagesL) s.reset();
    for (auto& s : stagesR) s.reset();
}

void DispersionEngine::setParams (const Params& p)
{
    params = p;

    freqSmooth.setTargetValue (juce::jlimit (20.0f, (float) (sampleRate * 0.45), p.frequencyHz));
    qSmooth.setTargetValue (juce::jmax (0.005f, p.q));
}

//==============================================================================
void DispersionEngine::recomputeStageCoefficients (float freqHz, float qKnob)
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * (double) freqHz / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);

    // 与参考实现一致：alpha = sin(w0) * 2 * sqrt(2) * q
    // 下限钳一下，避免极低频 + 极小 q 时极点贴到单位圆上（会无限振铃）
    const double alpha = juce::jmax (2.0e-4, sinw0 * (2.0 * std::sqrt (2.0) * (double) qKnob));

    const double a0 = 1.0 + alpha;
    const double b0 = 1.0 - alpha;

    c0 = b0 / a0;
    c1 = (-2.0 * cosw0) / a0;
    c2 = 1.0;                       // (1 + alpha) / a0
    c3 = (-2.0 * cosw0) / a0;
    c4 = b0 / a0;
}

void DispersionEngine::updateCoefficients()
{
    // 注意：这里只负责把平滑后的值算成系数，推进平滑由 process() 里的 chunk 控制
    const float f = juce::jlimit (20.0f, (float) (sampleRate * 0.45), freqSmooth.getCurrentValue());
    const float q = juce::jmax (0.005f, qSmooth.getCurrentValue());

    recomputeStageCoefficients (f, q);

    const int want = juce::jlimit (0, maxStages,
                                   (int) std::lround (juce::jlimit (0.0f, 1.0f, params.intensity)
                                                          * (float) maxStages));

    // 新启用的级必须清状态，否则会带出上一次残留的振铃
    if (want > activeStages)
    {
        for (int i = activeStages; i < want; ++i)
        {
            stagesL[(size_t) i].reset();
            stagesR[(size_t) i].reset();
        }
    }

    activeStages = want;

    for (int i = 0; i < activeStages; ++i)
    {
        stagesL[(size_t) i].setCoeffs (c0, c1, c2, c3, c4);
        stagesR[(size_t) i].setCoeffs (c0, c1, c2, c3, c4);
    }
}

//==============================================================================
void DispersionEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    if (numSamples <= 0 || numCh <= 0)
        return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const float mix     = juce::jlimit (0.0f, 1.0f, params.mix);
    const float dryGain = 1.0f - mix;
    const float outGain = juce::Decibels::decibelsToGain (params.outputDb);

    // 每 32 采样重算一次系数：比"每块一次"顺滑得多，又不用逐采样插值 50 级系数
    constexpr int chunkSize = 32;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - start);

        freqSmooth.skip (n);
        qSmooth.skip (n);
        updateCoefficients();

        const int stages = activeStages;

        for (int i = 0; i < n; ++i)
        {
            const float dryL = left[start + i];
            const float dryR = right != nullptr ? right[start + i] : 0.0f;

            float wl = dryL;
            float wr = dryR;

            for (int s = 0; s < stages; ++s)
            {
                wl = stagesL[(size_t) s].process (wl);
                wr = stagesR[(size_t) s].process (wr);
            }

            left[start + i] = (wl * mix + dryL * dryGain) * outGain;

            if (right != nullptr)
                right[start + i] = (wr * mix + dryR * dryGain) * outGain;
        }
    }
}
