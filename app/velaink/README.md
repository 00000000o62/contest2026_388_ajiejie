# velaink（墨灵笔迹执行引擎）

映射到 openvela `packages/demos/contest2026_388_velaink`。

## 职责

在 openvela 侧完成「笔迹 → G-code」的实时编译与下发，是墨灵
「AI 大脑（Linux）+ 实时小脑（openvela）+ 运动执行（GRBL）」三层架构中的中间层。

数据流：

```
Linux 大脑（LLM/视觉/NPU）
        │  归一化笔迹（0.0~1.0）
        ▼
openvela · velaink
        │  Catmull-Rom 平滑 → 等弧长重采样 → 限位缩放
        ▼
G-code（GRBL 1.1f）
        │  串口逐行下发，等待 ok 应答
        ▼
GRBL 控制板（CoreXY 解算 + 步进驱动）
```

## 命令

```sh
velaink info                    # 打印机器参数（默认 200x196mm 工作区）
velaink demo [step_mm]          # 生成示例笔迹（一颗心）的 G-code
velaink send <file> <dev>       # 下发 G-code 到串口，如 /dev/ttyS1
```

`velaink demo` 同时把结果写入 `/tmp/velaink_demo.nc`。

## 设计要点

- **只输出笛卡尔坐标**：CoreXY 运动学解算交给 GRBL 板，符合"大脑 + 小脑"分工
- **归一化坐标 + 等比缩放居中**：同一套笔迹适配任意幅面
- **等弧长重采样**：保证进给速度均匀，避免抖动与停顿
- **限位保护**：任一点越界立即中止并报错，绝不把机器送出轨
- **GRBL 握手**：逐行发送并等待 `ok`，防止下位机缓冲区溢出丢步

## 编译开关

`CONFIG_LVX_USE_DEMO_CONTEST2026_388_VELAINK`
