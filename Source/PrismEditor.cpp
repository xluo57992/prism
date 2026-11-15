#include "PrismEditor.h"
#include "PrismLog.h"
#include <cmath>

namespace
{
    constexpr int winW = 720;
    constexpr int winH = 720;

    //--------------------------------------------------------------------------
    // 常驻的大「强度」旋钮
    //--------------------------------------------------------------------------
    constexpr int mixKnobSize = 160;
    constexpr int mixKnobX = (winW - mixKnobSize) / 2;
    constexpr int mixKnobY = 352;

    //--------------------------------------------------------------------------
    // 底部常驻的 4 个参数旋钮
    //--------------------------------------------------------------------------
    constexpr int drawerKnobSize = 52;
    constexpr int drawerKnobPitch = 140;
    constexpr int drawerKnobTop = 588;

    //--------------------------------------------------------------------------
    // 棱镜几何（背景绘制与光路共用）
    //--------------------------------------------------------------------------
    constexpr float apexX = 360.0f;
    constexpr float apexY = 150.0f;
    constexpr float baseY = 320.0f;
    constexpr float halfBase = 88.0f;

    const juce::Colour accentColour()  { return juce::Colour (0xFFD8A657); }
    const juce::Colour textColour()    { return juce::Colour (0xFFC6C7D0); }
    const juce::Colour dimTextColour() { return juce::Colour (0xFF7C7D88); }

    // 三角形左右斜边在给定高度上的 x（y 在 apexY..baseY 之间）
    float prismEdgeX (float y, bool leftSide)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (y - apexY) / (baseY - apexY));
        return leftSide ? apexX - t * halfBase : apexX + t * halfBase;
    }
}

//==============================================================================
PrismEditor::PrismEditor (PrismProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      processor (processorToUse)
{
    setLookAndFeel (&lookAndFeel);

    PrismLog ("--- editor ctor begin, intensity=" + juce::String (rawParam (PrismProcessor::pIntensity, -1.0f), 4) + " ---");

    addAndMakeVisible (canvas);

    // 默认尺寸；保持 1:1，可自由缩放
    setResizable (true, true);
    setResizeLimits (480, 480, 1200, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (1.0);

    setSize (680, 680);

    //--------------------------------------------------------------------------
    // 常驻：大「强度」旋钮
    //--------------------------------------------------------------------------
    mixSlider.setLookAndFeel (&lookAndFeel);
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, accentColour());
    mixSlider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF34343D));
    mixSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFEDEDF2));
    mixSlider.setTooltip (juce::String::fromUTF8 ("色散强度：串联的全通级数（0–50）。越大相位被抹得越开。双击复位。"));
    canvas.addAndMakeVisible (mixSlider);

    if (auto* param = processor.parameters.getParameter (PrismProcessor::pIntensity))
    {
        mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                            (processor.parameters, PrismProcessor::pIntensity, mixSlider);

        if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (param))
        {
            juce::AudioProcessorParameter* base = floatParam;
            mixSlider.setDoubleClickReturnValue (true, floatParam->convertFrom0to1 (base->getDefaultValue()));
        }
    }

    mixValueLabel.setJustificationType (juce::Justification::centred);
    mixValueLabel.setFont (prismFont (34.0f, juce::Font::bold));
    mixValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFEDEDF2));
    canvas.addAndMakeVisible (mixValueLabel);

    mixStateLabel.setJustificationType (juce::Justification::centredLeft);
    mixStateLabel.setFont (prismFont (16.0f));
    mixStateLabel.setColour (juce::Label::textColourId, textColour());
    canvas.addAndMakeVisible (mixStateLabel);

    //--------------------------------------------------------------------------
    // 底部 4 个参数旋钮（常驻，不做抽屉）
    //--------------------------------------------------------------------------
    const std::pair<const char*, juce::String> paramDefs[] = {
        { PrismProcessor::pFrequency, juce::String::fromUTF8 ("频率")     },
        { PrismProcessor::pQ,         juce::String::fromUTF8 ("Q")        },
        { PrismProcessor::pMix,       juce::String::fromUTF8 ("混音")     },
        { PrismProcessor::pOutputDb,  juce::String::fromUTF8 ("输出增益") }
    };

    for (const auto& def : paramDefs)
        drawerKnobs.push_back (createKnob (def.first, def.second, def.second));

    //--------------------------------------------------------------------------
    // 关于（右上角）
    //--------------------------------------------------------------------------
    aboutButton.setButtonText (juce::String::fromUTF8 ("关于"));
    aboutButton.setComponentID ("prismAboutButton");
    aboutButton.setLookAndFeel (&lookAndFeel);
    aboutButton.onClick = [this]
    {
        if (aboutOverlay == nullptr)
            return;

        const bool show = ! aboutOverlay->isVisible();
        aboutOverlay->setVisible (show);

        PrismLog ("--- about click: visible=" + juce::String ((int) show) + " ---");

        if (show)
        {
            aboutOverlay->toFront (false);
            aboutOverlay->updateSpectrum();
            aboutOverlay->repaint();
        }
    };
    canvas.addAndMakeVisible (aboutButton);

    //--------------------------------------------------------------------------
    // 光效开关（右上角）：关闭时射线收回棱镜，再打开时反向展开
    //--------------------------------------------------------------------------
    visualButton.setButtonText (juce::String::fromUTF8 ("总开关"));
    visualButton.setLookAndFeel (&lookAndFeel);
    visualButton.setClickingTogglesState (true);
    canvas.addAndMakeVisible (visualButton);

    // 绑定到「效果」参数：按钮状态与宿主自动化双向同步
    effectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                           (processor.parameters, PrismProcessor::pEffect, visualButton);

    //--------------------------------------------------------------------------
    // 频率视图（右上角）：另开一个窗口，看群延迟分布
    //--------------------------------------------------------------------------
    freqButton.setButtonText (juce::String::fromUTF8 ("频率视图"));
    freqButton.setComponentID ("prismFreqViewButton");
    freqButton.setLookAndFeel (&lookAndFeel);
    freqButton.setTooltip (juce::String::fromUTF8 ("另开一个窗口：看哪些频率被拖慢、拖慢了多少毫秒"));
    freqButton.onClick = [this] { toggleFrequencyView(); };
    canvas.addAndMakeVisible (freqButton);

    //--------------------------------------------------------------------------
    // 关于面板最后添加，保证盖在其它控件上面
    //--------------------------------------------------------------------------
    aboutOverlay = std::make_unique<AboutOverlay> (processor);
    canvas.addAndMakeVisible (*aboutOverlay);
    aboutOverlay->setBounds (60, 88, 600, 474);
    aboutOverlay->setVisible (juce::SystemStats::getEnvironmentVariable ("PRISM_ABOUT", "0") == "1");

    // 调试用：PRISM_BYPASS=1 启动即旁通（截图用）
    if (juce::SystemStats::getEnvironmentVariable ("PRISM_BYPASS", "0") == "1")
        if (auto* prm = processor.parameters.getParameter (PrismProcessor::pEffect))
            prm->setValueNotifyingHost (0.0f);

    // 调试用：PRISM_FREQVIEW=1 启动即打开频率视图（截图用）
    if (juce::SystemStats::getEnvironmentVariable ("PRISM_FREQVIEW", "0") == "1")
        toggleFrequencyView();

    refreshValues();

    lastStateEditMs = juce::Time::getMillisecondCounter();
    stateAlpha = 1.0f;

    // 关键：resized() 在构造早期被 setSize 触发过一次，那时旋钮/面板还不存在。
    // 这里再显式布局一次，保证不依赖宿主之后是否再 resize。
    resized();

    PrismLog ("--- editor ctor end, intensity=" + juce::String (rawParam (PrismProcessor::pIntensity, -1.0f), 4) + " ---");

    startTimerHz (30);
}

PrismEditor::~PrismEditor()
{
    PrismLog ("--- editor dtor ---");

    stopTimer();

    // 频率视图是独立顶层窗口，必须比编辑器先死（它持有 processor 引用）
    frequencyWindow.reset();

    setLookAndFeel (nullptr);
}

//==============================================================================
void PrismEditor::toggleFrequencyView()
{
    // 第一次点：建窗口并显示；之后再点就是显示/隐藏的切换
    if (frequencyWindow == nullptr)
    {
        frequencyWindow = std::make_unique<FrequencyViewWindow> (processor);

        // 挂到宿主插件窗口上：跟着它显示/隐藏，但不会盖住宿主的对话框
        frequencyWindow->setHostWindow (*this);

        frequencyWindow->toFront (true);

        PrismLog ("--- frequency view opened ---");
        return;
    }

    const bool show = ! frequencyWindow->isVisible();
    frequencyWindow->setVisible (show);

    if (show)
    {
        frequencyWindow->toFront (true);
        frequencyWindow->view().repaint();
    }

    PrismLog ("--- frequency view visible=" + juce::String ((int) show) + " ---");
}

//==============================================================================
std::unique_ptr<PrismEditor::Knob> PrismEditor::createKnob (const juce::String& paramId, const juce::String& label, const juce::String& tip)
{
    auto knob = std::make_unique<Knob>();
    knob->paramId = paramId;
    auto& slider = knob->slider;
    slider.setLookAndFeel (&lookAndFeel);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour());
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF2B2B31));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFEDEDF2));
    slider.setTooltip (tip);
    canvas.addAndMakeVisible (slider);

    knob->nameLabel.setText (label, juce::dontSendNotification);
    knob->nameLabel.setJustificationType (juce::Justification::centred);
    knob->nameLabel.setFont (prismFont (16.0f));
    knob->nameLabel.setColour (juce::Label::textColourId, textColour());
    canvas.addAndMakeVisible (knob->nameLabel);

    knob->valueLabel.setJustificationType (juce::Justification::centred);
    knob->valueLabel.setFont (prismFont (15.0f));
    knob->valueLabel.setColour (juce::Label::textColourId, dimTextColour());
    canvas.addAndMakeVisible (knob->valueLabel);

    if (auto* param = processor.parameters.getParameter (paramId))
    {
        knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                               (processor.parameters, paramId, slider);

        if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (param))
        {
            juce::AudioProcessorParameter* base = floatParam;
            slider.setDoubleClickReturnValue (true, floatParam->convertFrom0to1 (base->getDefaultValue()));
        }
    }

    return knob;
}

//==============================================================================
juce::String PrismEditor::formatValue (const juce::String& id, float value) const
{
    if (id == PrismProcessor::pFrequency)
    {
        if (value >= 1000.0f)
            return juce::String (value / 1000.0f, 2) + " kHz";

        return juce::String (juce::roundToInt (value)) + " Hz";
    }

    if (id == PrismProcessor::pQ)         return juce::String (value, 2);
    if (id == PrismProcessor::pIntensity) return juce::String (juce::roundToInt (value * 100.0f)) + " %";

    if (id == PrismProcessor::pMix)
    {
        if (value <= 0.0001f)
            return juce::String::fromUTF8 ("干声");

        return juce::String (juce::roundToInt (value * 100.0f)) + " %";
    }

    if (id == PrismProcessor::pOutputDb) return juce::String (value, 1) + " dB";

    return juce::String (value, 2);
}

//==============================================================================
float PrismEditor::rawParam (const char* id, float fallback) const
{
    if (auto* v = processor.parameters.getRawParameterValue (id))
        return v->load();

    return fallback;
}

//==============================================================================
void PrismEditor::refreshValues()
{
    for (const auto& k : drawerKnobs)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (processor.parameters.getParameter (k->paramId)))
            k->valueLabel.setText (formatValue (k->paramId, p->get()), juce::dontSendNotification);
    }

    const float intensity = rawParam (PrismProcessor::pIntensity, 0.35f);

    mixValueLabel.setText (juce::String (juce::roundToInt (intensity * 100.0f)) + " %",
                           juce::dontSendNotification);
}

//==============================================================================
void PrismEditor::resized()
{
    // 画布永远按 720×720 排版，再整体缩放到窗口大小
    const float scale = (float) getWidth() / (float) winW;

    canvas.setBounds (0, 0, winW, winH);
    canvas.setTransform (juce::AffineTransform::scale (scale));

    // 常驻：强度主旋钮
    mixSlider.setBounds (mixKnobX, mixKnobY, mixKnobSize, mixKnobSize);
    mixValueLabel.setBounds (mixKnobX - 40, mixKnobY + mixKnobSize + 8, mixKnobSize + 80, 34);
    mixStateLabel.setBounds (24, 74, 320, 24);

    // 顶栏：三个按钮，右对齐（频率视图 / 总开关 / 关于）
    constexpr int topButtonW = 76;
    constexpr int topButtonGap = 8;

    aboutButton.setBounds  (winW - 16 - topButtonW, 16, topButtonW, 28);
    visualButton.setBounds (winW - 16 - topButtonW * 2 - topButtonGap, 16, topButtonW, 28);
    freqButton.setBounds   (winW - 16 - topButtonW * 3 - topButtonGap * 2, 16, topButtonW, 28);

    // 底部 4 个参数旋钮：一行居中
    const int count = (int) drawerKnobs.size();
    const int totalW = (count - 1) * drawerKnobPitch + drawerKnobSize;
    const int firstX = (winW - totalW) / 2;

    for (int i = 0; i < count; ++i)
    {
        const int x = firstX + i * drawerKnobPitch;

        drawerKnobs[(size_t) i]->nameLabel.setBounds (x - 40, drawerKnobTop, drawerKnobSize + 80, 22);
        drawerKnobs[(size_t) i]->slider.setBounds (x, drawerKnobTop + 20, drawerKnobSize, drawerKnobSize);
        drawerKnobs[(size_t) i]->valueLabel.setBounds (x - 40, drawerKnobTop + 80, drawerKnobSize + 80, 22);
    }
}

//==============================================================================
void PrismEditor::visibilityChanged()
{
    // 宿主把插件窗口藏起来 / 切到别的插件时，别让频率视图还孤零零挂在桌面上
    if (frequencyWindow != nullptr && ! isVisible())
        frequencyWindow->setVisible (false);
}

//==============================================================================
void PrismEditor::timerCallback()
{
    refreshValues();

    const juce::uint32 nowMs = juce::Time::getMillisecondCounter();

    // 关于面板开着的时候刷频谱
    if (aboutOverlay != nullptr && aboutOverlay->isVisible())
    {
        aboutOverlay->updateSpectrum();
        aboutOverlay->repaint();
    }

    const float intensity = rawParam (PrismProcessor::pIntensity, 0.35f);

    // 音频电平（快起慢落已在音频线程做过，这里再做一次显示平滑）
    const float meter = juce::jlimit (0.0f, 1.0f, processor.meterLevel.load (std::memory_order_relaxed));
    meterSmoothed += (meter - meterSmoothed) * 0.35f;

    // 光效开关的缓动：目标是「效果」参数（旁通时收回射线），指数逼近 = 非线性
    visualTarget = processor.isEffectOn() ? 1.0f : 0.0f;

    if (std::abs (visualAmount - visualTarget) > 0.0015f)
    {
        const float rate = visualTarget > visualAmount ? 0.14f : 0.09f;
        visualAmount += (visualTarget - visualAmount) * rate;

        if (std::abs (visualAmount - visualTarget) <= 0.0015f)
            visualAmount = visualTarget;

        canvas.repaint();
    }

    if (std::abs (meterSmoothed - lastMeter) > 0.004f)
    {
        lastMeter = meterSmoothed;
        canvas.repaint();
    }

    // 画面跟着参数走（强度变化直接影响光谱张角，必须重绘）
    if (std::abs (intensity - lastMaster) > 0.001f)
    {
        lastMaster = intensity;
        lastStateEditMs = nowMs;
        canvas.repaint();
    }
    else if (nowMs - lastStateEditMs < 2400)
    {
        canvas.repaint();      // 状态文字淡出期间也刷
    }

    // 状态文字淡入淡出：刚调过 1.2 秒内显示，之后淡掉
    {
        const float elapsed = (float) (nowMs - lastStateEditMs) / 1000.0f;
        const float targetAlpha = elapsed < 1.2f ? 1.0f : 0.0f;

        if (std::abs (stateAlpha - targetAlpha) > 0.002f)
        {
            stateAlpha += (targetAlpha - stateAlpha) * 0.28f;

            if (stateAlpha < 0.01f) stateAlpha = 0.0f;
            if (stateAlpha > 0.99f) stateAlpha = 1.0f;

            mixStateLabel.setAlpha (stateAlpha);
        }
    }

    const int stages = juce::roundToInt (intensity * 50.0f);

    const juce::String state = stages == 0 ? juce::String::fromUTF8 ("旁通")
                                : (stages <= 12 ? juce::String::fromUTF8 ("轻微")
                                : (stages <= 30 ? juce::String::fromUTF8 ("明显")
                                                : juce::String::fromUTF8 ("彻底散开")));

    mixStateLabel.setText (juce::String::fromUTF8 ("色散度 · ") + state
                               + "  " + juce::String (stages) + juce::String::fromUTF8 (" 级"),
                           juce::dontSendNotification);
}

//==============================================================================
// 背景：程序化棱镜光谱
//
//   强度  → 光谱扇的张角（色散强弱）
//   频率  → 光束在棱镜上的入射高度 + 扇面倾角
//   Q     → 光束的粗细与亮度（越小越“捏”越锐）
//   混音  → 光谱整体浓度
//   输出  → 环境光亮度
//==============================================================================
void PrismEditor::drawBackground (juce::Graphics& g)
{
    const float intensity = juce::jlimit (0.0f, 1.0f, rawParam (PrismProcessor::pIntensity, 0.35f));
    const float freq      = juce::jmax (20.0f, rawParam (PrismProcessor::pFrequency, 1200.0f));
    const float qValue    = juce::jlimit (0.02f, 1.0f, rawParam (PrismProcessor::pQ, 0.5f));
    const float mixValue  = juce::jlimit (0.0f, 1.0f, rawParam (PrismProcessor::pMix, 1.0f));
    const float outDb     = juce::jlimit (-24.0f, 24.0f, rawParam (PrismProcessor::pOutputDb, 0.0f));

    const float freqNorm = juce::jlimit (0.0f, 1.0f,
                                         std::log (freq / 20.0f) / std::log (1000.0f));

    // 音频能量（电平一般到不了 1，放大一点再用）
    const float energy = juce::jlimit (0.0f, 1.0f, meterSmoothed * 1.5f);
    const float vis    = juce::jlimit (0.0f, 1.0f, visualAmount);
    const float qNorm = juce::jlimit (0.0f, 1.0f, (qValue - 0.02f) / 0.98f);

    //--------------------------------------------------------------------------
    // 底色
    //--------------------------------------------------------------------------
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF080A0F), 0.0f, 0.0f,
                                             juce::Colour (0xFF151821), 0.0f, (float) winH, false));
    g.fillRect (0, 0, winW, winH);

    //--------------------------------------------------------------------------
    // 光路几何：入射点 / 出射点严格落在两条斜边上
    //--------------------------------------------------------------------------
    // 低频打在棱镜下部、高频打在上部（和频谱图一致：低在下）
    const float entryY = juce::jmap (freqNorm, 282.0f, 236.0f);
    const float exitY  = entryY + 20.0f;

    const juce::Point<float> entryPoint (prismEdgeX (entryY, true),  entryY);
    const juce::Point<float> exitPoint  (prismEdgeX (exitY,  false), exitY);

    // 射线必须正好从棱镜表面出发。往里挪会在玻璃内部露出一截（0% 强度时特别明显）。
    const juce::Point<float> rayOrigin (exitPoint.x, exitPoint.y);

    // 背景不再画大范围环境光：之前那层 200px 半径的柔光在 100% 强度时
    // 会糊成一团白，而且线性渐变的终点会留下一圈看得见的边界。
    // 现在只靠光束自身的光晕 + 入射点的小光斑，画面干净很多。

    //--------------------------------------------------------------------------
    // 出射光谱扇
    //--------------------------------------------------------------------------
    const float spread = (juce::jmap (intensity, 0.0f, 0.40f) + energy * 0.05f) * vis;
    const float tilt   = juce::jmap (freqNorm, 0.12f, -0.06f);
    const int   numRays = 28;
    const float rayLength = 800.0f * (0.15f + 0.85f * vis);

    const float rayWidth = juce::jmap (qNorm, 1.4f, 3.6f);        // Q 越大越发散
    const float rayBoost = juce::jmap (qNorm, 1.0f, 0.55f);       // Q 越大越柔
    const float mixAmount = juce::jmap (mixValue, 0.20f, 1.0f) * (0.80f + 0.55f * energy) * vis;

    // 先铺一层整体扇形的柔光（一个路径，不会像逐条描边那样在起点叠成胶囊），
    // 再把细射线画在上面。
    {
        const float a0 = tilt - spread;
        const float a1 = tilt + spread;

        const juce::Point<float> endA (exitPoint.x + std::cos (a0) * rayLength,
                                       exitPoint.y + std::sin (a0) * rayLength);
        const juce::Point<float> endB (exitPoint.x + std::cos (a1) * rayLength,
                                       exitPoint.y + std::sin (a1) * rayLength);

        juce::Path wedge;
        wedge.startNewSubPath (exitPoint);
        wedge.lineTo (endA);
        wedge.lineTo (endB);
        wedge.closeSubPath();

        juce::ColourGradient fanGlow (juce::Colour (0xFFFFFFFF).withAlpha (0.10f * mixAmount),
                                      exitPoint,
                                      juce::Colour (0x00FFFFFF).withAlpha (0.02f * mixAmount),
                                      exitPoint.translated (rayLength * 0.75f, 0.0f), false);
        g.setGradientFill (fanGlow);
        g.fillPath (wedge);
    }

    for (int i = 0; i < numRays; ++i)
    {
        const float t = (float) i / (float) (numRays - 1);
        const float hue = 0.72f - 0.72f * t;
        const auto colour = juce::Colour::fromHSV (hue, 0.80f, 1.0f, 1.0f);

        const float angle = tilt + (t - 0.5f) * 2.0f * spread;
        const juce::Point<float> end (exitPoint.x + std::cos (angle) * rayLength,
                                      exitPoint.y + std::sin (angle) * rayLength);

        // 扇面边缘更淡，避免两条硬直线边界
        const float envelope = 0.30f + 0.70f * std::sin (juce::MathConstants<float>::pi * t);

        juce::Path ray;
        ray.startNewSubPath (rayOrigin);
        ray.lineTo (end);

        const juce::PathStrokeType thin (rayWidth,
                                         juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded);

        g.setColour (colour.withAlpha ((0.26f + 0.20f * intensity) * mixAmount * rayBoost * envelope));
        g.strokePath (ray, thin);
    }
    // 出射点的柔光：弥合 28 条射线汇聚处的硬点
    {
        const float r = 26.0f + 46.0f * energy;
        juce::ColourGradient exitGlow (juce::Colour (0xFFFFF2D8).withAlpha ((0.10f + 0.14f * energy) * vis), exitPoint,
                                       juce::Colour (0x00FFF2D8), exitPoint.translated (r, 0.0f), true);
        g.setGradientFill (exitGlow);
        g.fillEllipse (exitPoint.x - r, exitPoint.y - r, r * 2.0f, r * 2.0f);
    }

    //--------------------------------------------------------------------------
    // 棱镜本体
    //--------------------------------------------------------------------------
    juce::Path prism;
    prism.addTriangle (apexX, apexY, apexX - halfBase, baseY, apexX + halfBase, baseY);

    {
        juce::ColourGradient glass (juce::Colour (0x12FFFFFF), apexX, apexY,
                                    juce::Colour (0x06FFFFFF), apexX, baseY, false);
        g.setGradientFill (glass);
        g.fillPath (prism);
    }

    g.setColour (juce::Colour (0x77BFD4FF).withAlpha (0.45f + 0.45f * energy));
    g.strokePath (prism, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    //--------------------------------------------------------------------------
    // 入射白光 → 棱镜内折射段
    //--------------------------------------------------------------------------
    {
        // 光束：等宽（不做渐尖，不然像漏斗），垂直渐变截面保证上下无硬边。
        // 末端停在离棱镜表面 16px 处，由入射光斑盖住断口 —— 既没有硬切也不变漏斗。
        const float beamHalf = (6.0f + 15.0f * energy) * vis;

        // 芯层和光晕层错开结束：芯层提前 44px、光晕层提前 14px，
        // 两级硬边错位后看起来就是一段渐隐，配合入射光斑完全看不出断口。
        const float glowEnd = entryPoint.x + 2.0f;
        const float coreEnd = entryPoint.x + 2.0f;

        if (glowEnd > 0.0f)
        {
            juce::ColourGradient grad (juce::Colour (0x00FFFFFF), 0.0f, entryY - beamHalf,
                                       juce::Colour (0x00FFFFFF), 0.0f, entryY + beamHalf, false);
            grad.addColour (0.5, juce::Colour (0xFFFFFFFF).withAlpha ((0.07f + 0.10f * energy) * vis));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (0.0f, entryY - beamHalf, glowEnd, beamHalf * 2.0f));
        }

        if (coreEnd > 0.0f)
        {
            const float coreHalf = beamHalf * 0.26f;
            juce::ColourGradient core (juce::Colour (0x00FFFFFF), 0.0f, entryY - coreHalf,
                                       juce::Colour (0x00FFFFFF), 0.0f, entryY + coreHalf, false);
            core.addColour (0.5, juce::Colour (0xFFFFFFFF).withAlpha ((0.55f + 0.22f * energy) * vis));
            g.setGradientFill (core);
            g.fillRect (juce::Rectangle<float> (0.0f, entryY - coreHalf, coreEnd, coreHalf * 2.0f));
        }

        // 光打在棱镜上的光斑：一直存在（低电平也有一点），电平越高越大
        {
            const float r = 26.0f + 34.0f * energy;
            juce::ColourGradient flash (juce::Colour (0xFFFFFFFF).withAlpha ((0.16f + 0.12f * energy) * vis), entryPoint,
                                        juce::Colour (0x00FFFFFF), entryPoint.translated (r, 0.0f), true);
            g.setGradientFill (flash);
            g.fillEllipse (entryPoint.x - r, entryPoint.y - r, r * 2.0f, r * 2.0f);
        }

        // 棱镜内部那一小段：从入射点走到出射点
        g.setColour (juce::Colour (0x88FFFFFF).withAlpha (0.53f * vis));
        g.drawLine ({ entryPoint, exitPoint }, 1.6f);
    }

    //--------------------------------------------------------------------------
    // 压暗 / 暗角
    //--------------------------------------------------------------------------
    juce::ColourGradient bottomFade (juce::Colour (0x00000000), juce::Point<float> (0.0f, (float) winH * 0.62f),
                                     juce::Colour (0xC8000000), juce::Point<float> (0.0f, (float) winH), false);
    g.setGradientFill (bottomFade);
    g.fillRect (0, (int) ((float) winH * 0.62f), winW, (int) ((float) winH * 0.38f));

    juce::ColourGradient topFade (juce::Colour (0x99000000), juce::Point<float> (0.0f, 0.0f),
                                  juce::Colour (0x00000000), juce::Point<float> (0.0f, 120.0f), false);
    g.setGradientFill (topFade);
    g.fillRect (0, 0, winW, 120);
}

//==============================================================================
void PrismEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF06070A));
}

void PrismEditor::paintCanvas (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF06070A));

    drawBackground (g);

    // 状态文字底衬
    if (stateAlpha > 0.001f)
    {
        const juce::Rectangle<float> pill (18.0f, 70.0f, 250.0f, 32.0f);
        g.setColour (juce::Colour (0x66000000).withMultipliedAlpha (stateAlpha));
        g.fillRoundedRectangle (pill, 6.0f);
    }


    // 顶部标题
    g.setColour (juce::Colour (0xFFE8E8EE));
    g.setFont (prismFont (30.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("棱镜"), juce::Rectangle<float> (24.0f, 12.0f, 140.0f, 36.0f),
                juce::Justification::centredLeft);

    g.setColour (dimTextColour());
    g.setFont (prismFont (15.0f));
    g.drawText (juce::String::fromUTF8 ("PRISM  ·  相位色散"),
                juce::Rectangle<float> (86.0f, 18.0f, 300.0f, 26.0f),
                juce::Justification::centredLeft);


    // 底部参数区的一条极细分隔线
    g.setColour (juce::Colour (0x14FFFFFF));
    g.drawLine (24.0f, (float) drawerKnobTop - 26.0f, (float) winW - 24.0f, (float) drawerKnobTop - 26.0f, 1.0f);
}
