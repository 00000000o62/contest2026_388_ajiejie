# 墨灵 VelaInk — AI 创意书写绘画机器人

> 2026 首届 openvela AI 硬件开发者大赛 · AI 硬件产品创新赛道 · 388 队

## 一句话

**过去的写字机是"人在操作机器"，墨灵是"机器理解人"。**

对着它说一句"写一句生日祝福给妈妈"，它就想清楚、写下来、落笔成字。不需要学软件，不需要手工描图。

---

## 一、要解决什么问题

市面上的写字机（如本项目使用的大鱼 DayuWriter V2.21 Pro）本质是**执行设备**——要出成品，用户得先在 PC 上用绘图软件画好、导入专用上位机、调参、下发。整个过程门槛高、链路长，非技术用户基本被劝退。

墨灵把这段链路压缩成一句自然语言。用户只表达"想要什么"，剩下的理解、构图、生成笔迹、控制落笔全部由端侧完成。

| | 传统写字机 | 墨灵 VelaInk |
|---|---|---|
| 输入 | 手工绘制 / 导入矢量图 | 自然语言 / 语音 |
| 依赖 | PC + 专用上位机 | 完全离线，设备本体 |
| 门槛 | 需掌握绘图与上位机软件 | 会说话就会用 |
| 硬件 | 需专用配套 | **存量写字机零改造** |
| 隐私 | 云端处理 | **全离线，不出设备** |

---

## 二、系统架构：大脑 + 实时层 + 执行层

```
┌───────────────────────────────────────────────────────┐
│ ① AI 大脑 · Linux (Fedora 44) on LogicPi A1           │
│    语音识别 → 端侧 LLM 理解意图 → 生成 SVG 创意草图    │
│    NPU (adla) 跑视觉小模型：笔尖检测 / 纸面定位         │
└────────────────────┬──────────────────────────────────┘
                     │  VELAINK/1 协议（归一化笔迹，纯文本）
┌────────────────────▼──────────────────────────────────┐
│ ② 实时层 · openvela（运行于 A1 的 KVM 虚拟机）          │
│    velaink：平滑 → 等弧长重采样 → 限位缩放 → G-code    │
│    硬实时、确定性，不因大模型推理抖动而丢步             │
└────────────────────┬──────────────────────────────────┘
                     │  G-code（GRBL 1.1f，逐行等待 ok）
┌────────────────────▼──────────────────────────────────┐
│ ③ 执行层 · GRBL 控制板 (MKS DLC V2.0)                  │
│    CoreXY 运动学解算 + 步进驱动 + Z 轴抬落笔            │
└───────────────────────────────────────────────────────┘
```

### 为什么是"Linux + openvela"异构，而不是只跑一个

- **NPU SDK 只提供 Linux 版本** —— 要用上 A311Y2 的 NPU 算力，必须有 Linux
- **运动控制要确定性** —— LLM 推理会占用大量 CPU 并带来抖动，若由同一个系统直接驱动步进电机，必然丢步
- **openvela 的设计初衷正是如此** —— 异构多核、RPC、共享内存是 openvela 官方的核心能力，本项目把它用在真实产品场景里

---

## 三、技术亮点

### 1. 笔迹编译器（openvela 侧，`app/velaink`）

从 AI 得到的原始路径不能直接下发给机器——点太密会卡、太疏会失形、速度不均会抖动、越界会撞机。墨灵做了四层处理：

- **Catmull-Rom 样条插值** —— 把稀疏控制点变成平滑笔迹
- **等弧长重采样** —— 保证相邻点间距一致，进给速度恒定，消除抖动与停顿
- **归一化坐标 + 等比缩放居中** —— 同一套笔迹适配任意幅面，不被拉伸
- **越界即中止保护** —— 任一点超出工作区立即报错停机，绝不把机器送出轨

### 2. 松耦合的笔迹协议（VELAINK/1）

纯文本、肉眼可调试、对丢包不敏感，且**能容忍 LLM 的多余输出**（未知行直接忽略）：

```
VELAINK/1
S              ← 抬笔，开始新笔画
P 0.15 0.15    ← 归一化点 (0.0~1.0)
P 0.85 0.15
E              ← 结束，立即编译并下发
```

### 3. 存量硬件零改造

只输出标准 G-code（G21/G90/G94，Z 轴抬落笔），任何 GRBL 1.1f 写字机都能直接用。CoreXY 运动学解算交给下位控制板，符合"大脑负责决策、小脑负责执行"的分工。

### 4. openvela 生态贡献：LogicPi A1 板级适配

LogicPi A1（Amlogic A311Y2 / S905D5）此前未被 openvela 支持。本项目：

- 建立可用的板级配置（官方模板自带的 `configs/nsh` 未指定架构，无法编译）
- 梳理板载资源：UPDATE 按键（SARADC CH3）、SD 卡槽、RGMII 网口、调试串口（uart_B）
- 打通构建与模拟器验证流程
- 沉淀完整构建技能，供后续开发者复用

---

## 四、硬件清单

| 部件 | 型号 | 说明 |
|---|---|---|
| 主控 | 逻极派 LogicPi A1 | Amlogic A311Y2，4×A73 + 2×A53，含 NPU |
| 写字机 | 大鱼 DayuWriter V2.21 Pro | MKS DLC V2.0 控制板 / GRBL 1.1f / CoreXY / Z 轴抬笔 |
| 工作幅面 | 200 × 196 mm | 笔迹编译器按此标定 |
| 供电 | 12V ≥2A（A1） | **与写字机电源严格分离**，避免电机尖峰串扰 USB |

---

## 五、仓库结构

```
contest2026_388_ajiejie/
├── app/velaink/           # openvela 侧笔迹执行引擎（C）
│   ├── stroke.c/h         #   笔迹容器、Catmull-Rom 平滑、等弧长重采样
│   ├── gcode.c/h          #   机器参数、坐标映射、限位保护、G-code 生成
│   ├── protocol.c/h       #   VELAINK/1 协议解析
│   └── velaink_main.c     #   NSH 命令：info / demo / recv / send
├── board/contest_board/   # 板级适配（含可用的 velaink-arm64 配置）
├── linux/                 # AI 大脑侧（Python）
│   ├── velaink_brain.py   #   SVG → 归一化笔迹 → VELAINK/1
│   └── README.md          #   三层架构说明与联调方法
├── tools/
│   └── export_workbuddy_logs.py   # AI 开发日志导出（含自动脱敏）
└── logs/<github_login>/   # AI 编程过程日志（官方校验通过）
```

---

## 六、快速开始

### 编译 openvela 镜像

```bash
cd openvela
./build.sh vendor/openvela/boards/contest2026_388_board/configs/velaink-arm64/ --cmake -j8
./emulator.sh cmake_out/contest2026_388_board_velaink-arm64/ -no-window -no-audio
# 成功标志：出现 NuttShell 与 goldfish-armv8a-ap> 提示符
```

### 在 openvela 中使用

```sh
velaink info                    # 查看机器参数
velaink demo 0.6                # 生成示例笔迹的 G-code（步长 0.6mm）
velaink recv /dev/ttyS1 /dev/ttyS2   # 从串口收笔迹，编译后下发至写字机
velaink send out.nc /dev/ttyS2  # 逐行下发 G-code，等待 GRBL 回 ok
```

### 本地全链路联调（无需硬件）

```bash
# 大脑侧：SVG → 笔迹协议
python3 linux/velaink_brain.py --demo > strokes.txt

# 实时层：笔迹协议 → G-code
gcc -O1 -o velaink_host app/velaink/*.c -lm
./velaink_host recv /dev/stdin out.nc < strokes.txt
```

已验证：内置示例生成 146 个归一化点，编译为 **159 行合法 G-code**，闭合心形，全部坐标落在 200×196mm 工作区内。

---

## 七、开发进度

| 阶段 | 状态 |
|---|---|
| openvela 环境搭建、构建与模拟器跑通 | ✅ |
| 笔迹编译器 + 笔迹协议 + AI 大脑脚本全链路 | ✅ |
| 板级配置（LogicPi A1 适配） | ✅ |
| AI 编程日志归集（官方校验通过） | ✅ |
| 真机 NPU 推理、语音识别接入 | 进行中 |
| openvela 虚拟机与 Linux 的 virtio-serial 通道 | 进行中 |
| 整机联调与演示视频 | 待完成 |

---

## 八、致谢与声明

- 赛事主办：openvela 社区
- 硬件支持：九望科技（逻极派 LogicPi A1）
- 本项目 AI 编程过程完整记录于 `logs/`，可用官方工具 `render-log.py` 查看

Licensed under the Apache License, Version 2.0.
