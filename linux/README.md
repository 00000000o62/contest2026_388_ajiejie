# linux/ — AI 大脑侧（Linux / Fedora on LogicPi A1）

墨灵 VelaInk 采用「大脑 + 实时层 + 执行层」三层架构：

```
┌─────────────────────────────────────────────────────────┐
│ Linux（Fedora on LogicPi A1）                            │
│   语音 ASR → 端侧 LLM → 生成 SVG 创意草图                │
│   NPU（adla）跑视觉小模型：笔尖检测 / 纸面定位            │
└───────────────┬─────────────────────────────────────────┘
                │  VELAINK/1（归一化笔迹，纯文本协议）
┌───────────────▼─────────────────────────────────────────┐
│ openvela · velaink（app/velaink）                        │
│   Catmull-Rom 平滑 → 等弧长重采样 → 限位缩放 → G-code    │
└───────────────┬─────────────────────────────────────────┘
                │  G-code（GRBL 1.1f，逐行等待 ok）
┌───────────────▼─────────────────────────────────────────┐
│ GRBL 控制板（MKS DLC V2.0）                              │
│   CoreXY 解算 + 步进驱动 + Z 轴抬落笔                    │
└─────────────────────────────────────────────────────────┘
```

## velaink_brain.py

SVG → 归一化笔迹 → VELAINK/1 协议。支持 M/L/C/Q/Z（含相对指令），
三次/二次贝塞尔采样，等比缩放居中，Y 轴翻转。

```sh
python3 velaink_brain.py --demo                    # 内置示例（一颗心）
python3 velaink_brain.py drawing.svg               # 提取 SVG 中所有 <path>
python3 velaink_brain.py --path "M10 10 L90 90 Z"  # 直接给 path 数据
python3 velaink_brain.py --demo --dev /dev/ttyUSB0 # 发到串口
```

## 本地联调（无需硬件）

```sh
# 终端 1：openvela 侧（用主机编译版验证算法）
gcc -O1 -o velaink_host ../app/velaink/*.c -lm
./velaink_host recv /dev/stdin out.nc < strokes.txt

# 终端 2：大脑侧
python3 velaink_brain.py --demo > strokes.txt
```
