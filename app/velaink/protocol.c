/****************************************************************************
 * apps/packages/demos/contest2026_388_velaink/protocol.c
 *
 * 墨灵 VelaInk — 笔迹协议解析
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

int velaink_parser_init(struct velaink_parser *p)
{
  p->state   = VELAINK_ST_IDLE;
  p->points  = 0;
  p->strokes = 0;

  if (velaink_drawing_init(&p->drawing, 256) < 0)
    {
      return -1;
    }

  return 0;
}

void velaink_parser_free(struct velaink_parser *p)
{
  velaink_drawing_free(&p->drawing);
  p->state   = VELAINK_ST_IDLE;
  p->points  = 0;
  p->strokes = 0;
}

/* 跳过前导空白 */
static const char *skip_ws(const char *s)
{
  while (*s && (unsigned char)*s <= ' ')
    {
      s++;
    }

  return s;
}

int velaink_parser_feed(struct velaink_parser *p, const char *line)
{
  const char *s = skip_ws(line);

  if (*s == '\0' || *s == ';')
    {
      return 0; /* 空行或注释 */
    }

  if (p->state == VELAINK_ST_IDLE)
    {
      if (strncmp(s, "VELAINK/1", 9) == 0)
        {
          p->state = VELAINK_ST_BODY;
          return 0;
        }

      return 0; /* 会话未开始，忽略 */
    }

  if (p->state != VELAINK_ST_BODY)
    {
      return 0;
    }

  if (*s == 'E')
    {
      p->state = VELAINK_ST_DONE;
      return 1;
    }

  if (*s == 'S')
    {
      if (p->strokes == 0 || p->drawing.strokes[p->strokes - 1].npts > 0)
        {
          if (velaink_drawing_pen_up(&p->drawing) == 0)
            {
              p->strokes++;
            }
        }

      return 0;
    }

  if (*s == 'P')
    {
      char  *end = NULL;
      float  x;
      float  y;

      s = skip_ws(s + 1);
      x = strtof(s, &end);
      if (end == s)
        {
          return -1; /* 数字解析失败 */
        }

      s   = skip_ws(end);
      end = NULL;
      y   = strtof(s, &end);
      if (end == s)
        {
          return -1;
        }

      if (x < 0.0f) x = 0.0f;
      if (x > 1.0f) x = 1.0f;
      if (y < 0.0f) y = 0.0f;
      if (y > 1.0f) y = 1.0f;

      if (velaink_drawing_add(&p->drawing, x, y) < 0)
        {
          return -2; /* 内存不足 */
        }

      p->points++;
      return 0;
    }

  return 0; /* 未知指令，宽容处理 */
}

int velaink_recv_stream(FILE *in, struct velaink_drawing *out, int *npts)
{
  struct velaink_parser p;
  char                  line[160];
  int                   rc = 0;

  if (velaink_parser_init(&p) < 0)
    {
      return -1;
    }

  while (fgets(line, sizeof(line), in))
    {
      int r = velaink_parser_feed(&p, line);

      if (r > 0)
        {
          rc = 1;
          break;
        }

      if (r < 0)
        {
          rc = r;
          break;
        }
    }

  if (rc > 0)
    {
      /* 把解析结果移交给调用方 */
      *out = p.drawing;
      if (npts)
        {
          *npts = p.points;
        }
    }
  else
    {
      velaink_parser_free(&p);
    }

  return rc;
}
