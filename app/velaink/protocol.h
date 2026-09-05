/****************************************************************************
 * apps/packages/demos/contest2026_388_velaink/protocol.h
 *
 * 墨灵 VelaInk — 大脑(openvela/Linux) 与 实时层 之间的笔迹传输协议
 *
 * 设计原则：纯文本、可肉眼调试、无依赖、对丢包不敏感。
 *
 *   VELAINK/1              ; 会话开始
 *   S                      ; 开始新笔画（抬笔后落笔）
 *   P <x> <y>              ; 追加一个归一化点 (0.0 ~ 1.0)
 *   E                      ; 会话结束，立即编译并输出 G-code
 *
 * 例：
 *   VELAINK/1
 *   S
 *   P 0.10 0.10
 *   P 0.90 0.10
 *   S
 *   P 0.50 0.90
 *   E
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __VELAINK_PROTOCOL_H
#define __VELAINK_PROTOCOL_H

#include <stdio.h>
#include "stroke.h"

/* 解析状态机 */
enum velaink_parse_state
{
  VELAINK_ST_IDLE = 0,   /* 等待 VELAINK/1 */
  VELAINK_ST_BODY,       /* 接收笔画数据 */
  VELAINK_ST_DONE,       /* 收到 E，作品就绪 */
  VELAINK_ST_ERROR
};

struct velaink_parser
{
  enum velaink_parse_state state;
  struct velaink_drawing   drawing;
  int                      points;   /* 已接收点数 */
  int                      strokes;  /* 已接收笔画数 */
};

int  velaink_parser_init(struct velaink_parser *p);
void velaink_parser_free(struct velaink_parser *p);

/* 喂入一行文本。返回 0 表示继续；返回 1 表示作品就绪（收到 E）；
 * 返回负值表示协议错误
 */
int velaink_parser_feed(struct velaink_parser *p, const char *line);

/* 从文件/流中完整读取一幅作品（阻塞直到 E 或 EOF） */
int velaink_recv_stream(FILE *in, struct velaink_drawing *out, int *npts);

#endif /* __VELAINK_PROTOCOL_H */
