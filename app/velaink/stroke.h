/****************************************************************************
 * apps/packages/demos/contest2026_388_velaink/stroke.h
 *
 * 墨灵 VelaInk — 笔迹数据结构与插值
 * 大赛：2026 首届 openvela AI 硬件开发者大赛 · 388 队
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __VELAINK_STROKE_H
#define __VELAINK_STROKE_H

#include <stdint.h>

/* 单个笔画（一次落笔到抬笔之间的连续轨迹） */
struct velaink_stroke
{
  float *pts;        /* [x0,y0, x1,y1, ...] 归一化坐标 0.0~1.0 */
  int    npts;       /* 点的个数 */
  int    cap;        /* 已分配容量（点数） */
};

/* 一幅作品 = 若干笔画 */
struct velaink_drawing
{
  struct velaink_stroke *strokes;
  int                    nstrokes;
  int                    cap;
};

/* ---------- 生命周期 ---------- */

int  velaink_stroke_init(struct velaink_stroke *s, int cap);
void velaink_stroke_free(struct velaink_stroke *s);
int  velaink_stroke_push(struct velaink_stroke *s, float x, float y);

int  velaink_drawing_init(struct velaink_drawing *d, int cap);
void velaink_drawing_free(struct velaink_drawing *d);
int  velaink_drawing_add(struct velaink_drawing *d, float x, float y);
/* 结束当前笔画（抬笔），开始下一笔 */
int  velaink_drawing_pen_up(struct velaink_drawing *d);

/* ---------- 平滑 ---------- */

/* Catmull-Rom 样条插值：把稀疏控制点变成平滑笔迹
 * samples: 每两个控制点之间插入的采样数（越大越平滑，点也越多）
 * 返回 0 成功；失败返回负值
 */
int velaink_stroke_smooth(const struct velaink_stroke *in,
                          struct velaink_stroke *out, int samples);

/* 按弧长重采样，保证相邻点间距均匀（避免进给速度抖动） */
int velaink_stroke_resample(const struct velaink_stroke *in,
                            struct velaink_stroke *out, float step_mm,
                            float width_mm, float height_mm);

/* ---------- 内置示例 ---------- */

/* 生成一个示例作品（一颗心），用于自检与演示 */
int velaink_demo_heart(struct velaink_drawing *d);

#endif /* __VELAINK_STROKE_H */
