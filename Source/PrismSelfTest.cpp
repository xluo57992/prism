//==============================================================================
// Prism DSP 自检（色散版）
//
// 全通滤波器理论上不改幅度谱，所以核心判据就一条：
//   湿信号 RMS ≈ 干信号 RMS（误差 < 2%），且任何极端参数下都不出 NaN、不发散。
//==============================================================================
#include "DispersionEngine.h"

#include <juce_dsp/juce_dsp.h>

#include <cstdio>
#include <cmath>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what)
    {
        std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what);

        if (! ok)
            ++failures;
    }

    float rms (const std::vector<float>& v)
    {
        double sum = 0.0;

        for (float x : v)
            sum += (double) x * (double) x;

        return (float) std::sqrt (sum / (double) juce::jmax<size_t> (1, v.size()));
    }

    float peak (const std::vector<float>& v)
    {
        float p = 0.0f;

        for (float x : v)
            p = juce::jmax (p, std::abs (x));

        return p;
    }

    bool finite (const std::vector<float>& v)
    {
        for (float x : v)
            if (! std::isfinite (x))
                return false;

        return true;
    }

    // 生成一段双声道测试信号（正弦 + 一点噪声，模拟真实素材）
    juce::AudioBuffer<float> makeSignal (double sr, int numSamples, float freqHz, float amplitude)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);

        juce::Random rng (12345);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const double t = (double) i / sr;
                const float sine = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * t);
                const float noise = amplitude * 0.05f * (rng.nextFloat() * 2.0f - 1.0f);
                d[i] = sine + noise;
            }
        }

        return buffer;
    }

    std::vector<float> flatten (const juce::AudioBuffer<float>& b)
    {
        std::vector<float> v;
        v.reserve ((size_t) (b.getNumChannels() * b.getNumSamples()));

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            v.insert (v.end(), b.getReadPointer (ch), b.getReadPointer (ch) + b.getNumSamples());

        return v;
    }

    DispersionEngine::Params makeParams (float freq, float q, float intensity, float mix, float outDb = 0.0f)
    {
        DispersionEngine::Params p;
        p.frequencyHz = freq;
        p.q           = q;
        p.intensity   = intensity;
        p.mix         = mix;
        p.outputDb    = outDb;
        return p;
    }
}

//==============================================================================
int main()
{
    constexpr double sr = 48000.0;
    constexpr int numSamples = 48000;   // 1 秒

    std::printf ("棱镜 / Prism 色散自检  (sr=%.0f)\n\n", sr);

    //--------------------------------------------------------------------------
    // 1. mix = 0 必须精确直通
    //--------------------------------------------------------------------------
    {
        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (1000.0f, 0.5f, 1.0f, 0.0f));

        auto buffer = makeSignal (sr, numSamples, 440.0f, 0.5f);
        const auto before = flatten (buffer);

        engine.process (buffer);
        const auto after = flatten (buffer);

        float maxDiff = 0.0f;

        for (size_t i = 0; i < before.size(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (before[i] - after[i]));

        std::printf ("mix=0 直通              peak=%.5f  最大偏差=%.8f\n", peak (after), maxDiff);
        check (maxDiff < 1.0e-6f, "mix=0 输出与输入逐样本一致");
    }

    //--------------------------------------------------------------------------
    // 2. 全通幅度守恒：湿信号 RMS 应该 ≈ 干信号 RMS
    //--------------------------------------------------------------------------
    {
        auto buffer = makeSignal (sr, numSamples, 440.0f, 0.5f);
        const float rmsBefore = rms (flatten (buffer));

        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (1200.0f, 0.5f, 1.0f, 1.0f));
        engine.process (buffer);

        const float rmsAfter = rms (flatten (buffer));
        const float ratio = rmsAfter / juce::jmax (1.0e-9f, rmsBefore);

        std::printf ("全通幅度守恒 (50 级)    rms 前=%.5f  后=%.5f  比=%.4f\n", rmsBefore, rmsAfter, ratio);
        check (std::abs (ratio - 1.0f) < 0.02f, "湿信号 RMS 与干信号一致（±2%）");
    }

    //--------------------------------------------------------------------------
    // 3. 极端参数：50 级 + 最低频 + 最小 Q
    //--------------------------------------------------------------------------
    {
        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (20.0f, 0.02f, 1.0f, 1.0f));

        auto buffer = makeSignal (sr, numSamples, 60.0f, 0.7f);
        engine.process (buffer);

        const auto v = flatten (buffer);
        const float p = peak (v);

        std::printf ("极端 20Hz/Q0.02/50级    peak=%.4f  finite=%d\n", p, (int) finite (v));
        check (finite (v), "极端参数下无 NaN / Inf");
        check (p < 8.0f, "极端参数下不发散");
    }

    //--------------------------------------------------------------------------
    // 4. 最高频 + 最小 Q
    //--------------------------------------------------------------------------
    {
        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (19000.0f, 0.02f, 1.0f, 1.0f));

        auto buffer = makeSignal (sr, numSamples, 1000.0f, 0.7f);
        engine.process (buffer);

        const auto v = flatten (buffer);

        std::printf ("极端 19kHz/Q0.02/50级   peak=%.4f\n", peak (v));
        check (finite (v) && peak (v) < 8.0f, "极高频也不发散");
    }

    //--------------------------------------------------------------------------
    // 5. 强度 = 0 时级数为 0，应为纯直通
    //--------------------------------------------------------------------------
    {
        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (1000.0f, 0.5f, 0.0f, 1.0f));

        auto buffer = makeSignal (sr, numSamples, 440.0f, 0.5f);
        const auto before = flatten (buffer);

        engine.process (buffer);
        const auto after = flatten (buffer);

        float maxDiff = 0.0f;

        for (size_t i = 0; i < before.size(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (before[i] - after[i]));

        std::printf ("强度=0（0 级）          级数=%d  最大偏差=%.8f\n", engine.getActiveStages(), maxDiff);
        check (engine.getActiveStages() == 0, "强度=0 时级数为 0");
        check (maxDiff < 1.0e-6f, "0 级时输出等于输入");
    }

    //--------------------------------------------------------------------------
    // 6. reset 之后必须彻底静音（不留上一段的振铃）
    //--------------------------------------------------------------------------
    {
        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (800.0f, 0.05f, 1.0f, 1.0f));

        auto loud = makeSignal (sr, numSamples, 200.0f, 0.8f);
        engine.process (loud);

        engine.reset();

        juce::AudioBuffer<float> silence (2, 2048);
        silence.clear();
        engine.process (silence);

        std::printf ("reset 后静音            peak=%.10f\n", peak (flatten (silence)));
        check (peak (flatten (silence)) == 0.0f, "reset 后输出精确为 0");
    }

    //--------------------------------------------------------------------------
    // 7. 输出增益生效
    //--------------------------------------------------------------------------
    {
        DispersionEngine engine;
        engine.prepare (sr);
        engine.setParams (makeParams (1000.0f, 0.5f, 0.5f, 1.0f, -6.0f));

        auto buffer = makeSignal (sr, numSamples, 440.0f, 0.5f);
        engine.process (buffer);

        const float p = peak (flatten (buffer));

        std::printf ("输出增益 -6dB           peak=%.4f\n", p);
        check (p > 0.1f && p < 0.5f, "输出增益生效");
    }

    //--------------------------------------------------------------------------
    // 8. 幅度响应必须是一条平线（全通的铁律）
    //
    // 做法：喂一个脉冲，取冲激响应做 FFT —— |H(f)| 就是幅度响应。
    // 任何一个环节写错（系数符号、归一化、级联方式），这里都会露馅。
    //--------------------------------------------------------------------------
    {
        const int fftOrder = 13;
        const int fftSize  = 1 << fftOrder;      // 8192

        struct Case { const char* name; float freq, q, intensity; };

        const Case cases[] = {
            { "1kHz Q0.5 18级", 1000.0f, 0.50f, 0.36f },
            { "300Hz Q0.2 30级", 300.0f, 0.20f, 0.60f },
            { "5kHz Q0.1 50级", 5000.0f, 0.10f, 1.00f }
        };

        for (const auto& c : cases)
        {
            DispersionEngine engine;
            engine.prepare (sr);
            engine.setParams (makeParams (c.freq, c.q, c.intensity, 1.0f));

            // 关键：先跑一段静音，让 50ms 的系数平滑走完。
            // 否则脉冲响应测到的是"正在扫频"的时变滤波器，幅度当然不守恒。
            {
                juce::AudioBuffer<float> settle (1, (int) sr / 2);
                settle.clear();
                engine.process (settle);
            }

            juce::AudioBuffer<float> impulse (1, fftSize);
            impulse.clear();
            impulse.setSample (0, 0, 1.0f);

            engine.process (impulse);

            std::vector<float> data ((size_t) fftSize * 2, 0.0f);

            for (int i = 0; i < fftSize; ++i)
                data[(size_t) i] = impulse.getSample (0, i);

            juce::dsp::FFT fft (fftOrder);
            fft.performFrequencyOnlyForwardTransform (data.data());

            // 50 Hz – 15 kHz 内找最大偏差
            const double binHz = sr / (double) fftSize;
            const int b0 = (int) (50.0 / binHz);
            const int b1 = (int) (15000.0 / binHz);

            float minDb = 1.0e9f, maxDb = -1.0e9f;

            for (int b = b0; b <= b1; ++b)
            {
                const float db = juce::Decibels::gainToDecibels (data[(size_t) b], -200.0f);
                minDb = juce::jmin (minDb, db);
                maxDb = juce::jmax (maxDb, db);
            }

            const float spread = maxDb - minDb;

            std::printf ("幅度平坦度 %-16s 起伏=%.3f dB\n", c.name, spread);
            check (spread < 0.5f, "幅度响应是平的（起伏 < 0.5 dB）");
        }
    }

    //--------------------------------------------------------------------------
    // 9. 干湿混合的代价：mix<1 时必然出现梳状凹陷（信息性，不算失败）
    //--------------------------------------------------------------------------
    {
        const int fftOrder = 13;
        const int fftSize  = 1 << fftOrder;

        for (float mixValue : { 1.0f, 0.5f, 0.3f })
        {
            DispersionEngine engine;
            engine.prepare (sr);
            engine.setParams (makeParams (1200.0f, 0.50f, 0.36f, mixValue));

            {
                juce::AudioBuffer<float> settle (1, (int) sr / 2);
                settle.clear();
                engine.process (settle);
            }

            juce::AudioBuffer<float> impulse (1, fftSize);
            impulse.clear();
            impulse.setSample (0, 0, 1.0f);
            engine.process (impulse);

            std::vector<float> data ((size_t) fftSize * 2, 0.0f);

            for (int i = 0; i < fftSize; ++i)
                data[(size_t) i] = impulse.getSample (0, i);

            juce::dsp::FFT fft (fftOrder);
            fft.performFrequencyOnlyForwardTransform (data.data());

            const double binHz = sr / (double) fftSize;
            const int b0 = (int) (100.0 / binHz);
            const int b1 = (int) (10000.0 / binHz);

            float worst = 0.0f;

            for (int b = b0; b <= b1; ++b)
                worst = juce::jmax (worst, -juce::Decibels::gainToDecibels (data[(size_t) b], -200.0f));

            std::printf ("干湿混合 mix=%.1f      最深凹陷 = %.1f dB\n", mixValue, worst);
        }
    }

    //--------------------------------------------------------------------------
    std::printf ("\n");
    if (failures == 0)
    {
        std::printf ("=== 自检全部通过 ===\n");
        return 0;
    }

    std::printf ("=== 自检失败 %d 项 ===\n", failures);
    return 1;
}
