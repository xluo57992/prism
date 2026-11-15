#include "SpectrumDisplay.h"
#include "PrismProcessor.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr double lowestHz  = 20.0;
    constexpr double highestHz = 20000.0;

    // 频率 → 0..1 的对数位置
    inline float logPosition (double hz)
    {
        return (float) (std::log (hz / lowestHz) / std::log (highestHz / lowestHz));
    }
}

//==============================================================================
SpectrumDisplay::SpectrumDisplay (PrismProcessor& processorToUse)
    : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
    binLevel.fill (0.0f);
    binPeak.fill (0.0f);
}

//==============================================================================
void SpectrumDisplay::update()
{
    fftData.fill (0.0f);

    // 取最近 2048 个采样；不够就补零
    processor.readSpectrum (fftData.data(), fftSize);

    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double sr = processor.getCurrentSampleRate();
    const double binHz = sr / (double) fftSize;

    //--------------------------------------------------------------------------
    // 先算每个 bin 的 dB
    //--------------------------------------------------------------------------
    static std::array<float, numBins> raw {};
    raw.fill (0.0f);

    for (int b = 1; b < numBins; ++b)
    {
        const double f = (double) b * binHz;

        if (f < lowestHz || f > highestHz)
        {
            raw[(size_t) b] = 0.0f;
            continue;
        }

        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * 2.0f / (float) fftSize, -90.0f);
        raw[(size_t) b] = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);
    }

    //--------------------------------------------------------------------------
    // 按 1/6 倍频程做平滑：低频 bin 稀、高频 bin 密，
    // 半径跟着 bin 序号走，视觉上才不会被高频的锯齿占满
    //--------------------------------------------------------------------------
    for (int b = 1; b < numBins; ++b)
    {
        const double f = (double) b * binHz;

        if (f < lowestHz || f > highestHz)
        {
            binLevel[(size_t) b] = 0.0f;
            continue;
        }

        // ±1/12 倍频程 ≈ ±1 个半音
        const double lo = f / 1.059463;
        const double hi = f * 1.059463;

        int b0 = juce::jlimit (1, numBins - 1, (int) std::floor (lo / binHz));
        int b1 = juce::jlimit (1, numBins - 1, (int) std::ceil  (hi / binHz));

        float sum = 0.0f;
        int count = 0;

        for (int k = b0; k <= b1; ++k) { sum += raw[(size_t) k]; ++count; }

        const float smoothed = count > 0 ? sum / (float) count : 0.0f;

        // 滞留感：上升不瞬间跳到位，下落放慢（30Hz 刷新下，约 1.4 秒衰到 -90%）
        constexpr float attack  = 0.55f;
        constexpr float release = 0.94f;

        auto& level = binLevel[(size_t) b];
        level = smoothed > level ? level + (smoothed - level) * attack
                                 : level * release;

        // 峰值线几乎不掉：每帧只降 0.005，满量程大约 6–7 秒才落到底
        auto& peak = binPeak[(size_t) b];
        peak = juce::jmax (level, peak - 0.005f);
    }
}

//==============================================================================
void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0x55000000));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (juce::Colour (0x22FFFFFF));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    auto plot = bounds.reduced (8.0f, 10.0f);

    if (plot.getHeight() <= 4.0f || plot.getWidth() <= 4.0f)
        return;

    const float maxLevel = *std::max_element (binLevel.begin(), binLevel.end());

    if (maxLevel < 0.015f)
    {
        g.setColour (juce::Colour (0x66FFFFFF));
        g.setFont (juce::Font (juce::FontOptions ("Microsoft YaHei UI", 12.0f, juce::Font::plain)));
        g.drawText (juce::String::fromUTF8 ("等待音频…"), plot, juce::Justification::centred);
    }

    //--------------------------------------------------------------------------
    // 频率刻度线
    //--------------------------------------------------------------------------
    g.setFont (juce::Font (juce::FontOptions (9.5f)));

    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const float x = plot.getX() + logPosition (f) * plot.getWidth();

        g.setColour (juce::Colour (0x1AFFFFFF));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

        g.setColour (juce::Colour (0x66FFFFFF));
        g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 12.0f, plot.getBottom() - 10.0f, 24.0f, 10.0f),
                    juce::Justification::centred);
    }

    //--------------------------------------------------------------------------
    // 曲线：逐 bin 取点，横坐标按对数映射
    //--------------------------------------------------------------------------
    const double sr = processor.getCurrentSampleRate();
    const double binHz = sr / (double) fftSize;

    juce::Path curve;
    juce::Path stroke;
    juce::Path peakPath;

    bool started = false;

    for (int b = 1; b < numBins; ++b)
    {
        const double f = (double) b * binHz;

        if (f < lowestHz || f > highestHz)
            continue;

        const float x = plot.getX() + logPosition (f) * plot.getWidth();
        const float y = plot.getBottom() - binLevel[(size_t) b] * plot.getHeight();
        const float py = plot.getBottom() - binPeak[(size_t) b] * plot.getHeight();

        if (! started)
        {
            curve.startNewSubPath (x, plot.getBottom());
            curve.lineTo (x, y);
            stroke.startNewSubPath (x, y);
            peakPath.startNewSubPath (x, py);
            started = true;
        }
        else
        {
            curve.lineTo (x, y);
            stroke.lineTo (x, y);
            peakPath.lineTo (x, py);
        }
    }

    if (! started)
        return;

    curve.lineTo (plot.getRight(), plot.getBottom());
    curve.closeSubPath();

    {
        juce::ColourGradient fill (juce::Colour (0x88D8A657), plot.getX(), plot.getY(),
                                   juce::Colour (0x05D8A657), plot.getX(), plot.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (curve);
    }

    g.setColour (juce::Colour (0xFFE8C07A));
    g.strokePath (stroke, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0x44FFFFFF));
    g.strokePath (peakPath, juce::PathStrokeType (1.0f));
}
