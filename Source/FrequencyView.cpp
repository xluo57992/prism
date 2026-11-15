#include "FrequencyView.h"
#include "PrismProcessor.h"
#include "PrismLookAndFeel.h"

#include <cmath>
#include <algorithm>

namespace
{
    const juce::Colour accentColour  (0xFFD8A657);
    const juce::Colour textColour    (0xFFC6C7D0);
    const juce::Colour dimTextColour (0xFF7C7D88);

    juce::String formatHz (float hz)
    {
        if (hz >= 10000.0f) return juce::String (hz / 1000.0f, 1) + " kHz";
        if (hz >= 1000.0f)  return juce::String (hz / 1000.0f, 2) + " kHz";
        return juce::String (juce::roundToInt (hz)) + " Hz";
    }

    juce::String formatMs (float ms)
    {
        if (ms < 0.0005f) return juce::String::fromUTF8 ("0 ms");
        if (ms >= 100.0f) return juce::String (ms, 0) + " ms";
        if (ms >= 10.0f)  return juce::String (ms, 1) + " ms";
        if (ms >= 1.0f)   return juce::String (ms, 2) + " ms";
        return juce::String (ms, 3) + " ms";
    }

    juce::String formatAxisHz (double hz)
    {
        if (hz >= 1000.0) return juce::String (hz / 1000.0, 0) + "k";
        return juce::String ((int) hz);
    }
}

//==============================================================================
// 曲线层：透明，盖在频谱上面（父组件的 paint 在子组件之前，所以曲线必须单独一层）
//==============================================================================
class FrequencyViewComponent::CurveLayer final : public juce::Component
{
public:
    explicit CurveLayer (const FrequencyViewComponent& ownerToUse)
        : owner (ownerToUse)
    {
        setInterceptsMouseClicks (true, false);
        setOpaque (false);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        if (bounds.getWidth() < 8.0f || bounds.getHeight() < 8.0f)
            return;

        const auto& curve = owner.getCurve();
        const bool on = owner.isEffectOn();
        const float vis = on ? 1.0f : 0.45f;

        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        //----------------------------------------------------------------------
        // 网格
        //----------------------------------------------------------------------
        g.setColour (juce::Colour (0x14FFFFFF).withMultipliedAlpha (vis));

        for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            g.drawVerticalLine ((int) xForHz (hz), 0.0f, h);

        for (int i = 0; i <= 4; ++i)
            g.drawHorizontalLine ((int) (h * (float) i / 4.0f), 0.0f, w);

        //----------------------------------------------------------------------
        // 受影响频段（群延迟 ≥ 峰值 25%）
        //----------------------------------------------------------------------
        if (curve.bandValid && curve.peakMs > 0.0f)
        {
            const float x0 = xForHz (curve.bandLowHz);
            const float x1 = xForHz (curve.bandHighHz);
            const float bw = juce::jmax (1.0f, x1 - x0);

            juce::ColourGradient band (accentColour.withAlpha (0.17f * vis), 0.0f, 0.0f,
                                       accentColour.withAlpha (0.03f * vis), 0.0f, h, false);
            g.setGradientFill (band);
            g.fillRect (juce::Rectangle<float> (x0, 0.0f, bw, h));

            const float dashes[] = { 5.0f, 5.0f };
            g.setColour (accentColour.withAlpha (0.55f * vis));
            g.drawDashedLine ({ x0, 0.0f, x0, h }, dashes, 2, 1.0f);
            g.drawDashedLine ({ x1, 0.0f, x1, h }, dashes, 2, 1.0f);
        }

        //----------------------------------------------------------------------
        // 中心频率：细实线。别用虚线 —— 受影响频段的边界也是虚线，两条容易混
        //----------------------------------------------------------------------
        {
            const float xc = xForHz (owner.getCentreHz());

            g.setColour (juce::Colour (0x40FFFFFF).withMultipliedAlpha (vis));
            g.fillRect (xc - 0.5f, 0.0f, 1.0f, h);

            g.setColour (accentColour.withAlpha (0.55f * vis));
            g.fillEllipse (xc - 2.0f, -1.0f, 4.0f, 4.0f);
        }

        //----------------------------------------------------------------------
        // 群延迟曲线
        //----------------------------------------------------------------------
        if (curve.hz.size() >= 2)
        {
            juce::Path path;

            for (size_t i = 0; i < curve.hz.size(); ++i)
            {
                const float x = xForHz (curve.hz[i]);
                const float y = yForMs (curve.delayMs[i]);

                if (i == 0) path.startNewSubPath (x, y);
                else        path.lineTo (x, y);
            }

            juce::Path filled (path);
            filled.lineTo (w, h);
            filled.lineTo (0.0f, h);
            filled.closeSubPath();

            juce::ColourGradient fill (accentColour.withAlpha (0.26f * vis), 0.0f, 0.0f,
                                       accentColour.withAlpha (0.02f * vis), 0.0f, h, false);
            g.setGradientFill (fill);
            g.fillPath (filled);

            g.setColour (accentColour.withAlpha (0.10f * vis));
            g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

            g.setColour (accentColour.withAlpha (0.95f * vis));
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

            // 峰值读数放右上角，和左上角的频段标签错开。
            // 峰值不一定在中心频率上：Q 大时全通过阻尼，峰会掉到低频端。
            size_t peakIndex = 0;

            for (size_t i = 1; i < curve.delayMs.size(); ++i)
                if (curve.delayMs[i] > curve.delayMs[peakIndex])
                    peakIndex = i;

            const float peakHz = curve.hz[peakIndex];

            const auto peakText = juce::String::fromUTF8 ("峰值 ")
                                    + formatHz (peakHz) + juce::String::fromUTF8 (" · ") + formatMs (curve.peakMs);

            g.setColour (accentColour.withAlpha (0.85f * vis));
            g.setFont (prismFont (12.0f));
            g.drawText (peakText, (int) (w - 230.0f), 6, 224, 16, juce::Justification::centredRight);

            // 过阻尼提示：峰明显低于中心频率时，直接说明原因和怎么调回来
            if (on && peakHz < owner.getCentreHz() * 0.5f)
            {
                const auto hint = juce::String::fromUTF8 ("Q 偏大（过阻尼）：延迟堆在低频端，Q 降到 0.1 附近峰才回到中心");
                const juce::Rectangle<float> hintArea (6.0f, h - 30.0f, w - 12.0f, 20.0f);

                g.setColour (juce::Colour (0xB014171E).withMultipliedAlpha (vis));
                g.fillRoundedRectangle (hintArea, 4.0f);

                g.setColour (accentColour.withAlpha (0.75f * vis));
                g.setFont (prismFont (11.5f));
                g.drawFittedText (hint, hintArea.toNearestInt().reduced (7, 0),
                                  juce::Justification::centredLeft, 1);
            }
        }

        //----------------------------------------------------------------------
        // 空状态
        //----------------------------------------------------------------------
        if (curve.stages <= 0)
        {
            g.setColour (dimTextColour);
            g.setFont (prismFont (14.0f));
            g.drawText (juce::String::fromUTF8 ("0 级：旁通，没有任何频率被拖慢"),
                        bounds.reduced (10.0f).toNearestInt(), juce::Justification::centred);
        }
        else if (! on)
        {
            g.setColour (dimTextColour);
            g.setFont (prismFont (14.0f));
            g.drawText (juce::String::fromUTF8 ("总开关关闭：正在直通，下列曲线暂时不生效"),
                        bounds.reduced (10.0f).toNearestInt(), juce::Justification::centred);
        }

        //----------------------------------------------------------------------
        // 鼠标读数
        //----------------------------------------------------------------------
        if (hovering && curve.delayMs.size() >= 2)
        {
            const float x = juce::jlimit (0.0f, w, hoverPos.x);

            const double pos = (double) x / (double) w;
            const double idx = pos * (double) (curve.delayMs.size() - 1);
            const int i0 = juce::jlimit (0, (int) curve.delayMs.size() - 2, (int) std::floor (idx));
            const float t = (float) (idx - (double) i0);
            const float delay = curve.delayMs[(size_t) i0] * (1.0f - t) + curve.delayMs[(size_t) i0 + 1] * t;
            const double hz = FrequencyViewComponent::hzAtPosition (pos);

            const float y = yForMs (delay);

            g.setColour (juce::Colour (0x55FFFFFF));
            g.drawVerticalLine ((int) x, 0.0f, h);

            g.setColour (accentColour);
            g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);

            const auto text = formatHz ((float) hz) + juce::String::fromUTF8 ("  ·  ")
                                + formatMs (delay)
                                + (curve.stages > 0 ? juce::String::fromUTF8 ("  ·  ") + juce::String (curve.stages)
                                                          + juce::String::fromUTF8 (" 级")
                                                    : juce::String());

            const auto font = prismFont (12.5f);
            const int tw = juce::GlyphArrangement::getStringWidthInt (font, text);

            const float boxW = (float) tw + 18.0f;
            const float boxH = 23.0f;
            const float boxX = juce::jlimit (0.0f, juce::jmax (0.0f, w - boxW), x + 12.0f);
            const float boxY = juce::jlimit (0.0f, juce::jmax (0.0f, h - boxH), y - boxH - 12.0f);

            g.setColour (juce::Colour (0xF014171E));
            g.fillRoundedRectangle (boxX, boxY, boxW, boxH, 5.0f);

            g.setColour (accentColour.withAlpha (0.55f));
            g.drawRoundedRectangle (boxX + 0.5f, boxY + 0.5f, boxW - 1.0f, boxH - 1.0f, 5.0f, 1.0f);

            g.setColour (juce::Colour (0xFFEDEDF2));
            g.setFont (font);
            g.drawText (text, (int) (boxX + 9.0f), (int) boxY, tw + 2, (int) boxH,
                        juce::Justification::centredLeft);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        hovering = true;
        hoverPos = e.position;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovering = false;
        repaint();
    }

private:
    float xForHz (double hz) const
    {
        return (float) FrequencyViewComponent::logPosition (hz) * (float) getWidth();
    }

    float yForMs (float ms) const
    {
        const float peak = owner.getDisplayPeakMs();

        if (peak <= 0.0f)
            return (float) getHeight();

        return (float) getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, ms / peak));
    }

    const FrequencyViewComponent& owner;

    bool hovering = false;
    juce::Point<float> hoverPos;
};

//==============================================================================
FrequencyViewComponent::FrequencyViewComponent (PrismProcessor& processorToUse)
    : processor (processorToUse),
      spectrum (processorToUse)
{
    spectrum.setMinimalStyle (true);
    spectrum.setAlpha (0.38f);
    addAndMakeVisible (spectrum);

    // 置顶开关：默认开。关掉之后点回宿主，这个窗口就会排到 FX 窗口后面
    pinButton.setButtonText (juce::String::fromUTF8 ("置顶"));
    pinButton.setComponentID ("prismFreqViewPin");
    pinButton.setClickingTogglesState (true);
    pinButton.setToggleState (true, juce::dontSendNotification);
    pinButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xFF1C2028));
    pinButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0x33D8A657));
    pinButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xFF8A8B96));
    pinButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xFFE8C07A));
    pinButton.setTooltip (juce::String::fromUTF8 ("置顶：窗口始终盖在宿主 FX 窗口上面"));
    pinButton.onClick = [this]
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setAlwaysOnTop (pinButton.getToggleState());

        repaint();
    };
    addAndMakeVisible (pinButton);

    curveLayer = std::make_unique<CurveLayer> (*this);
    addAndMakeVisible (*curveLayer);

    sampleRate = processor.getCurrentSampleRate();
    updateFromParams (true);

    startTimerHz (30);
}

//==============================================================================
FrequencyViewComponent::~FrequencyViewComponent() = default;

//==============================================================================
float FrequencyViewComponent::getDisplayPeakMs() const noexcept
{
    return juce::jmax (0.05f, smoothedPeakMs);
}

//==============================================================================
float FrequencyViewComponent::logPosition (double hz)
{
    const double pos = std::log (juce::jlimit (dispersion::lowestHz, dispersion::highestHz, hz) / dispersion::lowestHz)
                         / std::log (dispersion::highestHz / dispersion::lowestHz);

    return (float) pos;
}

double FrequencyViewComponent::hzAtPosition (double pos)
{
    pos = juce::jlimit (0.0, 1.0, pos);

    return dispersion::lowestHz * std::pow (dispersion::highestHz / dispersion::lowestHz, pos);
}

//==============================================================================
juce::Rectangle<float> FrequencyViewComponent::plotArea() const
{
    return getLocalBounds().toFloat()
             .withTrimmedLeft (66.0f)
             .withTrimmedRight (24.0f)
             .withTrimmedTop (88.0f)
             .withTrimmedBottom (64.0f);
}

//==============================================================================
juce::String FrequencyViewComponent::headerInfo() const
{
    juce::String info = juce::String (stages) + juce::String::fromUTF8 (" 级  ·  中心 ") + formatHz (centreHz)
                          + "  ·  Q " + juce::String (qValue, 2);

    if (! effectOn)
        info += juce::String::fromUTF8 ("  ·  旁通");

    return info;
}

//==============================================================================
float FrequencyViewComponent::rawParam (PrismProcessor& p, const char* id, float fallback)
{
    if (auto* v = p.parameters.getRawParameterValue (id))
        return v->load();

    return fallback;
}

//==============================================================================
void FrequencyViewComponent::updateFromParams (bool force)
{
    const float f       = rawParam (processor, PrismProcessor::pFrequency, 1000.0f);
    const float q       = rawParam (processor, PrismProcessor::pQ, 0.50f);
    const float inten   = rawParam (processor, PrismProcessor::pIntensity, 0.35f);
    const float mix     = rawParam (processor, PrismProcessor::pMix, 1.00f);
    const double sr     = processor.getCurrentSampleRate();
    const int stagesNow = dispersion::stageCountForIntensity (inten);
    const bool on       = processor.isEffectOn();

    const bool cosmeticChanged = on != effectOn || std::abs (mix - mixValue) > 1.0e-6f;

    mixValue = mix;
    effectOn = on;

    const bool audioChanged = stagesNow != stages
                                || std::abs (f - centreHz) > 0.01f
                                || std::abs (q - qValue) > 1.0e-6f
                                || std::abs (sr - sampleRate) > 1.0e-6;

    if (! audioChanged && ! force)
    {
        if (cosmeticChanged)
            repaint();

        // 级数没变时曲线不动，但层级显示（旁通）要跟着变
        if (cosmeticChanged)
            curveLayer->repaint();

        return;
    }

    stages     = stagesNow;
    centreHz   = f;
    qValue     = q;
    sampleRate = sr;

    curve = dispersion::computeCurve (sr, f, q, stagesNow, 720);

    repaint();
    curveLayer->repaint();
}

//==============================================================================
void FrequencyViewComponent::timerCallback()
{
    updateFromParams (false);

    // 实时频谱：每帧都要刷
    spectrum.update();
    spectrum.repaint();

    // 纵轴峰值平滑（扫 Q / 扫强度时坐标轴不跳）。差得很小时直接吸附，
    // 免得坐标轴顶部和「峰值」读数长期差一位小数。
    const float target = juce::jmax (0.05f, curve.peakMs);
    const float diff = std::abs (target - smoothedPeakMs);

    if (diff > 0.001f * juce::jmax (1.0f, target))
    {
        smoothedPeakMs += (target - smoothedPeakMs) * 0.18f;
        curveLayer->repaint();
    }
    else if (diff > 0.0f)
    {
        smoothedPeakMs = target;
        curveLayer->repaint();
    }
}

//==============================================================================
void FrequencyViewComponent::resized()
{
    const auto plot = plotArea();

    spectrum.setBounds (plot.toNearestInt());
    curveLayer->setBounds (plot.toNearestInt());

    pinButton.setBounds (getWidth() - 24 - 56, 15, 56, 24);
}

//==============================================================================
void FrequencyViewComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    //--------------------------------------------------------------------------
    // 底色
    //--------------------------------------------------------------------------
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF080A0F), 0.0f, 0.0f,
                                             juce::Colour (0xFF141821), 0.0f, bounds.getHeight(), false));
    g.fillAll();

    //--------------------------------------------------------------------------
    // 顶栏
    //--------------------------------------------------------------------------
    g.setColour (juce::Colour (0xFFE8E8EE));
    g.setFont (prismFont (24.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("频率视图"),
                juce::Rectangle<int> (22, 12, 160, 32), juce::Justification::centredLeft);

    g.setColour (textColour);
    g.setFont (prismFont (13.5f));
    {
        // 右侧留给「置顶」按钮
        const int infoRight = (int) bounds.getWidth() - 24 - 56 - 10;
        const int infoWidth = juce::jlimit (120, 340, infoRight - 200);

        g.drawText (headerInfo(),
                    juce::Rectangle<int> (infoRight - infoWidth, 16, infoWidth, 22),
                    juce::Justification::centredRight);
    }

    g.setColour (dimTextColour);
    g.setFont (prismFont (12.5f));
    g.drawText (juce::String::fromUTF8 ("全通链的群延迟 —— 它不改任何频段的音量，只把中心频率附近「跑慢」一点"),
                juce::Rectangle<int> (22, 46, juce::jmax (120, (int) bounds.getWidth() - 44), 18),
                juce::Justification::centredLeft);

    const auto plot = plotArea();

    //--------------------------------------------------------------------------
    // 画布底
    //--------------------------------------------------------------------------
    g.setColour (juce::Colour (0xFF0B0E14));
    g.fillRect (plot);

    //--------------------------------------------------------------------------
    // 纵轴（毫秒）：峰值 / 75% / 50% / 25% / 0
    //--------------------------------------------------------------------------
    const float peak = getDisplayPeakMs();

    g.setFont (prismFont (11.5f));

    for (int i = 0; i <= 4; ++i)
    {
        const float ms = peak * (1.0f - (float) i / 4.0f);
        const float y = plot.getY() + plot.getHeight() * (float) i / 4.0f;

        g.setColour (i == 0 ? accentColour.withAlpha (0.75f) : dimTextColour);
        g.drawText (formatMs (ms),
                    juce::Rectangle<float> (10.0f, y - 8.0f, plot.getX() - 20.0f, 16.0f),
                    juce::Justification::centredRight);
    }

    //--------------------------------------------------------------------------
    // 横轴（对数频率）
    //--------------------------------------------------------------------------
    for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = plot.getX() + logPosition (hz) * plot.getWidth();

        g.setColour (dimTextColour);
        g.drawText (formatAxisHz (hz),
                    juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 4.0f, 40.0f, 16.0f),
                    juce::Justification::centred);
    }

    //--------------------------------------------------------------------------
    // 底部说明
    //--------------------------------------------------------------------------
    juce::String bandText = juce::String::fromUTF8 ("无");

    if (curve.bandValid)
        bandText = formatHz (curve.bandLowHz) + juce::String::fromUTF8 (" – ") + formatHz (curve.bandHighHz);

    g.setColour (dimTextColour.withAlpha (0.85f));
    g.setFont (prismFont (11.5f));
    g.drawText (juce::String::fromUTF8 ("阴影 = 群延迟 ≥ 峰值 25% 的频段：") + bandText,
                juce::Rectangle<int> (22, (int) bounds.getHeight() - 26, juce::jmax (120, (int) bounds.getWidth() - 44), 16),
                juce::Justification::centredLeft);

    g.setColour (dimTextColour.withAlpha (0.55f));
    g.drawText (juce::String::fromUTF8 ("鼠标移到曲线上读数 · 背景是实时输出频谱（全通不改幅度，频谱看着几乎不变是正常的）"),
                juce::Rectangle<int> (22, (int) bounds.getHeight() - 13, juce::jmax (120, (int) bounds.getWidth() - 44), 14),
                juce::Justification::centredLeft);
}

//==============================================================================
FrequencyViewWindow::FrequencyViewWindow (PrismProcessor& processorToUse)
    : DocumentWindow (juce::String::fromUTF8 ("棱镜 · 频率视图"),
                      juce::Colour (0xFF0B0D12),
                      DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (560, 380, 2000, 1400);

    // 默认置顶：否则点回宿主 FX 窗口，这个窗口会被盖到后面去（看起来像被关掉了）
    setAlwaysOnTop (true);

    viewComponent = new FrequencyViewComponent (processorToUse);
    setContentOwned (viewComponent, true);

    centreWithSize (920, 560);
    setVisible (true);
}
