/****************************************************************************
 * apps/packages/demos/contest2026_388_velaink/gcode.h
 *
 * 墨灵 VelaInk — G-code 生成（面向 GRBL 1.1f / CoreXY 写字机）
 *
 * 设计要点：
 *  - 只输出笛卡尔坐标，CoreXY 运动学转换交给 GRBL 控制板（"大脑+小脑"架构）
 *  - 归一化坐标按工作区等比缩放并居中，天然适配不同幅面
 *  - 抬笔/落笔用 Z 轴（舵机抬笔结构），带 plungy 进给与延时
 *  - 所有数值参与限位保护，越界即报错，绝不把机器送出轨
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __VELAINK_GCODE_H
#define __VELAINK_GCODE_H

#include <stdio.h>
#include "stroke.h"

/* 机器参数：大鱼 DayuWriter V2.21 Pro（200 x 196 mm 有效工作区） */
struct velaink_machine
{
  float width_mm;      /* X 行程 */
  float height_mm;     /* Y 行程 */
  float margin_mm;     /* 四周留白 */
  float pen_up_mm;     /* 抬笔高度（Z） */
  float pen_down_mm;   /* 落笔高度（Z） */
  float feed_mm_min;   /* 绘制进给速度 */
  float plunge_mm_min; /* Z 轴进给速度 */
  float safe_z_mm;     /* 快速移动时的安全高度 */
};

void velaink_machine_default(struct velaink_machine *m);

/* 归一化坐标 -> 机器坐标（等比缩放 + 居中），结果写入 mx 与 my */
void velaink_map_point(const struct velaink_machine *m,
                       float nx, float ny, float *mx, float *my);

/* 生成完整 G-code 到 fp
 * 返回生成的行数，出错返回负值
 */
int velaink_gcode_emit(FILE *fp, const struct velaink_drawing *d,
                       const struct velaink_machine *m);

/* 生成 G-code 到文件路径（便于在板子上落盘 .nc 文件） */
int velaink_gcode_to_file(const char *path, const struct velaink_drawing *d,
                          const struct velaink_machine *m);

#endif /* __VELAINK_GCODE_H */
