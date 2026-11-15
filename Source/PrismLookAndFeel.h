#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

//==============================================================================
// 界面统一字体：微软雅黑（Windows 自带，覆盖全部中文字形）
// JUCE 默认字体是 Segoe UI，不含汉字，中文会变成方框
//==============================================================================
inline juce::Font prismFont (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions ("Microsoft YaHei UI", height, style));
}

//==============================================================================
// 棱镜的旋钮外观：径向渐变本体 + 数值弧 + 悬停光晕 + 高光
//==============================================================================
class PrismLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PrismLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xFF151518));
    }

    //--------------------------------------------------------------------------
    // 字体：全部走中文字体
    //--------------------------------------------------------------------------
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return prismFont ((float) buttonHeight * 0.40f);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return prismFont ((float) box.getHeight() * 0.40f);
    }

    juce::Font getPopupMenuFont() override { return prismFont (15.0f); }
    juce::Font getLabelFont (juce::Label& label) override
    {
        return prismFont ((float) label.getHeight() * 0.62f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 粗细随旋钮大小走：小旋钮 3px，大 MIX 旋钮自动加粗
        const float arcWidth    = juce::jmax (3.0f, radius * 0.085f);
        const float pointerWidth = juce::jmax (2.4f, radius * 0.085f);
        const float ringRadius  = juce::jmax (6.0f, radius - arcWidth * 0.5f - 1.0f);
        const float bodyRadius  = juce::jmax (4.0f, ringRadius - arcWidth * 1.05f);

        const bool hovered = slider.isMouseOver();
        const bool dragging = slider.isMouseButtonDown (true);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // 拖动光晕（径向渐变，边缘自然消失，不用一个硬边圆）
        if (dragging)
        {
            juce::ColourGradient glow (accent.withAlpha (0.16f), centre,
                                       accent.withAlpha (0.0f), centre.translated (0.0f, radius * 1.15f), true);
            g.setGradientFill (glow);
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        }

        // 背景环
        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                          rotaryStartAngle, rotaryEndAngle, 48);
        g.setColour (juce::Colour (0xFF2B2B31));
        g.strokePath (bg, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // 数值弧
        if (sliderPosProportional > 0.002f)
        {
            juce::Path fg;
            fg.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                              rotaryStartAngle, angle, 48);
            g.setColour (accent);
            g.strokePath (fg, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        // 本体：径向渐变，左上偏亮、右下压暗
        {
            const auto light = centre.translated (-bodyRadius * 0.34f, -bodyRadius * 0.40f);

            juce::ColourGradient body (juce::Colour (0xFF4E4E58), light,
                                       juce::Colour (0xFF141418), centre.translated (bodyRadius * 0.30f, bodyRadius * 0.55f),
                                       false);
            g.setGradientFill (body);
            g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                           bodyRadius * 2.0f, bodyRadius * 2.0f);
        }

        // 镜面高光：贴着左上内缘的一道细弧，比整块椭圆干净得多
        {
            const float hlR = bodyRadius * 0.76f;
            juce::Path hl;
            hl.addCentredArc (centre.x, centre.y, hlR, hlR, 0.0f,
                              -juce::MathConstants<float>::pi * 0.62f,
                              -juce::MathConstants<float>::pi * 0.08f, 24);
            g.setColour (juce::Colour (0x30FFFFFF));
            g.strokePath (hl, juce::PathStrokeType (juce::jmax (1.5f, bodyRadius * 0.10f),
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        // 描边
        g.setColour (hovered ? juce::Colour (0xFF7A7A84) : juce::Colour (0xFF303038));
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius,
                       bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

        // 指针：必须用 getPointOnCircumference（0 = 12 点、顺时针），
        // 和 addCentredArc 的角度约定一致；用 cos/sin 会整体偏 90°。
        const auto tip  = centre.getPointOnCircumference (bodyRadius * 0.82f, angle);
        const auto root = centre.getPointOnCircumference (bodyRadius * 0.06f, angle);

        g.setColour (juce::Colour (0xFFEDEDF2));
        g.drawLine ({ root, tip }, pointerWidth);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = button.getToggleState();

        juce::Colour fill = on ? juce::Colour (0xFF3B3B44) : juce::Colour (0xFF202026);

        if (shouldDrawButtonAsDown)
            fill = fill.brighter (0.12f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter (0.06f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (on ? juce::Colour (0xFF9AA6B8) : juce::Colour (0xFF3E3E47));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }
};

//==============================================================================
// 支持 Ctrl/Alt 精细拖动的旋钮
//==============================================================================
class KnobSlider : public juce::Slider
{
public:
    KnobSlider()
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setVelocityBasedMode (false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        fineMode = e.mods.isCtrlDown() || e.mods.isAltDown();
        dragStartValue = getValue();
        dragStartY = e.position.y;
        juce::Slider::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! fineMode)
        {
            juce::Slider::mouseDrag (e);
            return;
        }

        const double dy = (double) (dragStartY - e.position.y);
        const double range = getMaximum() - getMinimum();
        const double delta = dy * (range / 180.0) * 0.25;

        // 默认通知方式（异步）会通知监听者 → 参数自动同步
        setValue (juce::jlimit (getMinimum(), getMaximum(), dragStartValue + delta));
    }

private:
    bool fineMode = false;
    double dragStartValue = 0.0;
    int dragStartY = 0;
};
