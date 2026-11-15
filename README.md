# 棱镜 / Prism

> 一条全通滤波器链，不改变音量，只让不同频率跑出不同的时间差。

**VST3 / Standalone · Windows · JUCE 8 · GPLv3**

作者：**Yunshu_TSK** · [Bilibili](https://space.bilibili.com/1394063758)

![界面](docs/ui-freq-1000.png)

## 它做什么

把信号串过一长串双二阶全通滤波器。全通**不改变幅度谱**，只改相位：
每一级都在中心频率附近给出一段群延迟，几十级叠起来，瞬态被抹开、高频跑到低频前面。

听感：

- 鼓组 → 像一层沙，attack 被拉软
- 贝斯 / sub → 黏稠的拖尾
- 合成器 pluck → 金属味的 "zap"

**它不可能既保留瞬态原貌又产生色散**——这是效果的物理本质，不是 bug。

## 参数

| 参数 | 范围 | 说明 |
|---|---|---|
| **强度** | 0–100% | 串联级数 0–50，0 = 旁通 |
| **频率** | 20 Hz–20 kHz | 色散中心。它附近的频率被"捏"得最狠 |
| **Q** | 0.02–1.0 | 越小越集中在中心频率、色散越强 |
| **混音** | 0–100% | 干湿。**默认 100%（纯湿）**；30–70% 之间会与干声抵消出很深的梳状凹陷 |
| **输出增益** | ±24 dB | 仅用于 A/B 对齐响度 |
| **总开关** | on/off | 旁通（逐样本直通）。关闭时界面上的光谱会收回棱镜 |

旋钮：双击复位，Ctrl / Alt + 拖动精细调节。

界面按 720×720 设计稿整体等比缩放，窗口可从 480 拉到 1200，字和控件一起缩放。

## 视觉

背景是**程序化绘制**的，没有图片素材：

- 白光从左射入棱镜，右侧散成 28 条光谱射线
- **频率**决定光束打在棱镜的高度（低频在下、高频在上）
- **强度**决定扇面张角
- **Q** 决定射线粗细
- **混音**决定光谱浓度
- **输出电平**驱动光束亮度与入射光斑大小

右上角「关于」里有一个实时频谱（FFT 2048，对数频率轴）。

## 编译

依赖：

- Windows 10/11
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/)（勾选「使用 C++ 的桌面开发」）
- CMake 已随 VS 一起安装

```powershell
git clone https://github.com/<you>/prism.git
cd prism
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
```

首次配置会通过 `FetchContent` 下载 JUCE 8.0.9（需要网络）。如果本地已有 JUCE：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -JuceDir D:\JUCE
```

产物：

| 目标 | 路径 |
|---|---|
| VST3 | `build/Prism_artefacts/Release/VST3/Prism.vst3` |
| Standalone | `build/Prism_artefacts/Release/Standalone/Prism.exe` |
| DSP 自检 | `build/PrismSelfTest.exe` |
| 界面回归 | `build/PrismEditorProbe.exe` |

装到 DAW：把 `Prism.vst3` 复制到 `C:\Program Files\Common Files\VST3\`，或在 DAW 里把构建目录加进插件扫描路径。

## 测试

```powershell
# DSP：幅度平坦度、极端参数不发散、reset 静音、干湿梳状深度
build\PrismSelfTest.exe

# 界面/旁通回归：宿主 program 回调、打开关闭界面、旁通逐样本直通、频谱数据通路
build\PrismEditorProbe.exe
```

自检的核心判据是**幅度守恒**：全通不改变幅度谱，所以湿信号 RMS 必须等于干信号
（50 级时实测 0.986，差 1.4% 来自起始瞬态）。

## 已知限制

- 参数没有逐样本平滑（混音 / 输出增益 / 强度在块边界瞬跳）
- `getTailLengthSeconds()` 报 0.5s，离线渲染会砍掉更长的尾巴
- 192 kHz 下全通延迟会超出缓冲长度
- 总开关是硬切，没有交叉淡入

## 许可证

**GPLv3**。用 JUCE 8 的免费版本就意味着整个插件必须是 GPLv3——
你可以自由使用、修改、再分发，但衍生作品也必须开源。

Copyright (C) 2026 Yunshu_TSK

## 致谢

- 本插件的代码 100% 由 **DeepSeek 4.1 Flash** 编写（AI 协作，人类负责需求、验收与试听）
- [JUCE](https://juce.com/) — 框架（GPLv3 / 商业双许可）
- [AllPassPhase](https://github.com/enummusic/AllPassPhase) by **enummusic** — 这个插件的 DSP 思路来源。
  本项目的引擎是按算法重写的（RBJ cookbook 的 all-pass 形式），**没有使用其代码**，
  但采样率处理、平滑等细节参考了它的行为。
- RBJ Audio EQ Cookbook — 双二阶滤波器系数公式
