#include "AboutOverlay.h"

//==============================================================================
AboutOverlay::AboutOverlay (PrismProcessor& processorToUse)
    : spectrum (processorToUse)
{
    closeButton.setButtonText (juce::String::fromUTF8 ("✕"));
    closeButton.setTooltip (juce::String::fromUTF8 ("关闭"));
    closeButton.onClick = [this] { setVisible (false); };
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x33FFFFFF));
    closeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0x55FFFFFF));
    closeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE8E8EE));
    closeButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xFFFFFFFF));
    addAndMakeVisible (closeButton);

    addAndMakeVisible (spectrum);
}

//==============================================================================
void AboutOverlay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // 外圈柔光，让面板"浮"在照片上
    {
        juce::ColourGradient glow (juce::Colour (0x88000000), bounds.getCentre(),
                                   juce::Colour (0x00000000), bounds.getCentre().translated (0.0f, bounds.getHeight() * 0.62f), true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (bounds.expanded (18.0f), 22.0f);
    }

    // 半透明面板本体：能透出一点照片，但不能透到影响读字
    g.setColour (juce::Colour (0xEA141418));
    g.fillRoundedRectangle (bounds, 14.0f);

    g.setColour (juce::Colour (0x40D8A657));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.2f);

    //--------------------------------------------------------------------------
    // 标题
    //--------------------------------------------------------------------------
    g.setColour (juce::Colour (0xFFEDEDF2));
    g.setFont (juce::Font (juce::FontOptions ("Microsoft YaHei UI", 26.0f, juce::Font::bold)));
    g.drawText (juce::String::fromUTF8 ("关于"),
                juce::Rectangle<float> (54.0f, 12.0f, 120.0f, 30.0f),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xFF8A8B96));
    g.setFont (juce::Font (juce::FontOptions ("Microsoft YaHei UI", 14.0f, juce::Font::plain)));
    g.drawText (juce::String::fromUTF8 ("PRISM  ·  ALL-PASS DISPERSION  ·  v1.1.1"),
                juce::Rectangle<float> (128.0f, 18.0f, 300.0f, 22.0f),
                juce::Justification::centredLeft);

    //--------------------------------------------------------------------------
    // 正文（占位，之后替换成真正的关于内容）
    //--------------------------------------------------------------------------
    const auto bodyArea = juce::Rectangle<float> (24.0f, 56.0f, bounds.getWidth() - 48.0f,
                                                  bounds.getHeight() - 56.0f - 150.0f);

    g.setColour (juce::Colour (0xCCB9BAC2));
    g.setFont (juce::Font (juce::FontOptions ("Microsoft YaHei UI", 15.0f, juce::Font::plain)));

    g.drawFittedText (juce::String::fromUTF8 (
                          "全通滤波器不改变音量，只让不同频率跑出不同的时间差：\n"
                          "瞬态被抹开，高频跑到低频之前。鼓组上像一层沙，贝斯上像黏稠的拖尾。\n"
                          "\n"
                          "强度　串联级数 0–50（0 = 旁通）　　频率　色散中心\n"
                          "Q　　越小，色散越集中在中心频率　　混音　干湿比（100% 为纯色散）\n"
                          "输出　仅用于 A/B 对齐响度，不影响音色\n"
                          "\n"
                          "双击旋钮复位　·　Ctrl / Alt + 拖动精细调节\n"
                          "右上角「总开关」旁通：射线会收回棱镜\n"
                          "右上角「频率视图」：另开窗口看群延迟分布\n"
                          "\n"
                          "Yunshu_TSK　·　v1.1.1　·　2026　·　GPLv3 开源\n"
                          "space.bilibili.com/1394063758\n"
                          "100% DeepSeek 4.1 Flash 制作\n"
                          "DSP 参考 AllPassPhase by enummusic（按算法重写，未使用其代码）\n"
                          "Built with JUCE 8"),
                      bodyArea.toNearestInt(), juce::Justification::topLeft, 14);

    //--------------------------------------------------------------------------
    // 频谱区标题
    //--------------------------------------------------------------------------
    g.setColour (juce::Colour (0xFF8A8B96));
    g.setFont (juce::Font (juce::FontOptions ("Microsoft YaHei UI", 13.5f, juce::Font::plain)));
    g.drawText (juce::String::fromUTF8 ("FREQUENCY SPECTRUM  ·  输出"),
                juce::Rectangle<float> (24.0f, bounds.getHeight() - 152.0f, 300.0f, 18.0f),
                juce::Justification::centredLeft);
}

//==============================================================================
void AboutOverlay::resized()
{
    closeButton.setBounds (12, 10, 30, 28);

    spectrum.setBounds (20, getHeight() - 150, getWidth() - 40, 132);
}
