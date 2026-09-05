/****************************************************************************
 * apps/packages/demos/contest2026_388_velaink/stroke.c
 *
 * 墨灵 VelaInk — 笔迹插值与示例生成
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <stdlib.h>
#include <math.h>
#include "stroke.h"

#define VELAINK_PI 3.14159265358979323846f

int velaink_stroke_init(struct velaink_stroke *s, int cap)
{
  if (cap < 2)
    {
      cap = 2;
    }

  s->pts = (float *)calloc((size_t)cap * 2, sizeof(float));
  if (!s->pts)
    {
      s->npts = 0;
      s->cap  = 0;
      return -1;
    }

  s->npts = 0;
  s->cap  = cap;
  return 0;
}

void velaink_stroke_free(struct velaink_stroke *s)
{
  if (s->pts)
    {
      free(s->pts);
    }

  s->pts  = NULL;
  s->npts = 0;
  s->cap  = 0;
}

static int stroke_grow(struct velaink_stroke *s)
{
  int    ncap = s->cap ? s->cap * 2 : 16;
  float *np   = (float *)realloc(s->pts, (size_t)ncap * 2 * sizeof(float));

  if (!np)
    {
      return -1;
    }

  s->pts = np;
  s->cap = ncap;
  return 0;
}

int velaink_stroke_push(struct velaink_stroke *s, float x, float y)
{
  if (s->npts >= s->cap && stroke_grow(s) < 0)
    {
      return -1;
    }

  s->pts[s->npts * 2]     = x;
  s->pts[s->npts * 2 + 1] = y;
  s->npts++;
  return 0;
}

int velaink_drawing_init(struct velaink_drawing *d, int cap)
{
  if (cap < 1)
    {
      cap = 1;
    }

  d->strokes = (struct velaink_stroke *)calloc((size_t)cap,
                                               sizeof(struct velaink_stroke));
  if (!d->strokes)
    {
      d->nstrokes = 0;
      d->cap      = 0;
      return -1;
    }

  d->nstrokes = 0;
  d->cap      = cap;
  return 0;
}

void velaink_drawing_free(struct velaink_drawing *d)
{
  int i;

  if (!d->strokes)
    {
      return;
    }

  for (i = 0; i < d->nstrokes; i++)
    {
      velaink_stroke_free(&d->strokes[i]);
    }

  free(d->strokes);
  d->strokes  = NULL;
  d->nstrokes = 0;
  d->cap      = 0;
}

/* 追加一个点。d->nstrokes == 0 时自动开一笔 */
int velaink_drawing_add(struct velaink_drawing *d, float x, float y)
{
  struct velaink_stroke *cur;

  if (d->nstrokes == 0)
    {
      if (d->nstrokes >= d->cap)
        {
          return -1; /* 容量由 init 决定，示例场景足够 */
        }

      if (velaink_stroke_init(&d->strokes[d->nstrokes], 64) < 0)
        {
          return -1;
        }

      d->nstrokes++;
    }

  cur = &d->strokes[d->nstrokes - 1];
  return velaink_stroke_push(cur, x, y);
}

int velaink_drawing_pen_up(struct velaink_drawing *d)
{
  if (d->nstrokes == 0 || d->nstrokes >= d->cap)
    {
      return -1;
    }

  if (d->strokes[d->nstrokes - 1].npts == 0)
    {
      return 0; /* 空笔画，不新增 */
    }

  if (velaink_stroke_init(&d->strokes[d->nstrokes], 64) < 0)
    {
      return -1;
    }

  d->nstrokes++;
  return 0;
}

/****************************************************************************
 * Catmull-Rom 插值
 ****************************************************************************/

static void catmull_rom(float p0[2], float p1[2], float p2[2], float p3[2],
                        float t, float out[2])
{
  float t2 = t * t;
  float t3 = t2 * t;
  int   k;

  for (k = 0; k < 2; k++)
    {
      out[k] = 0.5f * ((2.0f * p1[k]) +
                       (-p0[k] + p2[k]) * t +
                       (2.0f * p0[k] - 5.0f * p1[k] + 4.0f * p2[k] -
                        p3[k]) * t2 +
                       (-p0[k] + 3.0f * p1[k] - 3.0f * p2[k] + p3[k]) * t3);
    }
}

int velaink_stroke_smooth(const struct velaink_stroke *in,
                          struct velaink_stroke *out, int samples)
{
  int i;
  int j;

  if (!in || !out || in->npts < 2 || samples < 1)
    {
      return -1;
    }

  if (velaink_stroke_init(out, (in->npts - 1) * samples + 1) < 0)
    {
      return -1;
    }

  for (i = 0; i < in->npts - 1; i++)
    {
      float p0[2];
      float p1[2];
      float p2[2];
      float p3[2];
      int   i0 = (i > 0) ? (i - 1) : 0;
      int   i2 = (i + 1 < in->npts) ? (i + 1) : (in->npts - 1);
      int   i3 = (i + 2 < in->npts) ? (i + 2) : (in->npts - 1);

      p1[0] = in->pts[i * 2];
      p1[1] = in->pts[i * 2 + 1];
      p2[0] = in->pts[i2 * 2];
      p2[1] = in->pts[i2 * 2 + 1];
      p0[0] = in->pts[i0 * 2];
      p0[1] = in->pts[i0 * 2 + 1];
      p3[0] = in->pts[i3 * 2];
      p3[1] = in->pts[i3 * 2 + 1];

      for (j = 0; j < samples; j++)
        {
          float o[2];

          catmull_rom(p0, p1, p2, p3, (float)j / (float)samples, o);
          if (velaink_stroke_push(out, o[0], o[1]) < 0)
            {
              return -1;
            }
        }
    }

  /* 收尾点 */

  velaink_stroke_push(out, in->pts[(in->npts - 1) * 2],
                      in->pts[(in->npts - 1) * 2 + 1]);
  return 0;
}

/****************************************************************************
 * 等弧长重采样
 ****************************************************************************/

int velaink_stroke_resample(const struct velaink_stroke *in,
                            struct velaink_stroke *out, float step_mm,
                            float width_mm, float height_mm)
{
  float acc = 0.0f;
  float px;
  float py;
  int   i;

  if (!in || !out || in->npts < 2 || step_mm <= 0.0f)
    {
      return -1;
    }

  if (velaink_stroke_init(out, in->npts * 4 + 16) < 0)
    {
      return -1;
    }

  px = in->pts[0];
  py = in->pts[1];
  velaink_stroke_push(out, px, py);

  for (i = 1; i < in->npts; i++)
    {
      float cx = in->pts[i * 2];
      float cy = in->pts[i * 2 + 1];
      float dx = (cx - px) * width_mm;
      float dy = (cy - py) * height_mm;
      float seg = sqrtf(dx * dx + dy * dy);

      if (seg < 1e-6f)
        {
          continue;
        }

      acc += seg;
      if (acc >= step_mm)
        {
          velaink_stroke_push(out, cx, cy);
          acc = 0.0f;
        }

      px = cx;
      py = cy;
    }

  /* 保证末点完整 */

  if (out->npts > 0)
    {
      float lx = in->pts[(in->npts - 1) * 2];
      float ly = in->pts[(in->npts - 1) * 2 + 1];

      if (out->pts[(out->npts - 1) * 2] != lx ||
          out->pts[(out->npts - 1) * 2 + 1] != ly)
        {
          velaink_stroke_push(out, lx, ly);
        }
    }

  return 0;
}

/****************************************************************************
 * 内置示例：一颗心（参数方程）
 *   x = 16 sin^3 t
 *   y = 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t
 ****************************************************************************/

int velaink_demo_heart(struct velaink_drawing *d)
{
  const int   n = 96;
  int         i;
  float       minx = 1e9f;
  float       maxx = -1e9f;
  float       miny = 1e9f;
  float       maxy = -1e9f;
  static float raw[96 * 2];

  for (i = 0; i < n; i++)
    {
      float t = 2.0f * VELAINK_PI * (float)i / (float)n;
      float st = sinf(t);
      float x  = 16.0f * st * st * st;
      float y  = 13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                 2.0f * cosf(3.0f * t) - cosf(4.0f * t);

      raw[i * 2]     = x;
      raw[i * 2 + 1] = y;

      if (x < minx) minx = x;
      if (x > maxx) maxx = x;
      if (y < miny) miny = y;
      if (y > maxy) maxy = y;
    }

  /* Y 轴翻转（屏幕坐标向下为 +Y，机器坐标向上为 +Y）并归一化到 0..1 */

  for (i = 0; i < n; i++)
    {
      float nx = (raw[i * 2] - minx) / (maxx - minx);
      float ny = (maxy - raw[i * 2 + 1]) / (maxy - miny);

      if (velaink_drawing_add(d, nx, ny) < 0)
        {
          return -1;
        }
    }

  /* 闭合 */

  velaink_drawing_add(d, (raw[0] - minx) / (maxx - minx),
                      (maxy - raw[1]) / (maxy - miny));
  return 0;
}
