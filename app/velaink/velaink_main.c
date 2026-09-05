/****************************************************************************
 * apps/packages/demos/contest2026_388_velaink/velaink_main.c
 *
 * 墨灵 VelaInk — openvela 侧笔迹执行器
 *
 * 命令：
 *   velaink info                     打印机器参数
 *   velaink demo [step_mm]           生成示例笔迹（一颗心）并输出 G-code
 *   velaink recv <indev> [outdev]    从串口/文件接收笔迹并编译下发
 *   velaink send <gcode> <dev>       把 G-code 文件下发到串口（GRBL 握手）
 *
 * 典型链路：
 *   Linux 大脑 --(VELAINK/1 协议)--> openvela velaink --(G-code)--> GRBL 板
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "stroke.h"
#include "gcode.h"
#include "protocol.h"

static void usage(void)
{
  printf("VelaInk - stroke to G-code engine\n");
  printf("Usage:\n");
  printf("  velaink info\n");
  printf("  velaink demo [step_mm]            # default step 0.5mm\n");
  printf("  velaink recv <indev> [outdev]     # e.g. velaink recv "
         "/dev/ttyS1 /dev/ttyS2\n");
  printf("  velaink send <gcodefile> <dev>    # stream G-code to GRBL\n");
}

static int cmd_info(void)
{
  struct velaink_machine m;

  velaink_machine_default(&m);
  printf("VelaInk machine profile\n");
  printf("  workspace : %.0f x %.0f mm (margin %.0f)\n", (double)m.width_mm,
         (double)m.height_mm, (double)m.margin_mm);
  printf("  pen       : up %.1f mm / down %.1f mm\n", (double)m.pen_up_mm,
         (double)m.pen_down_mm);
  printf("  feedrate  : draw %.0f / plunge %.0f mm/min\n",
         (double)m.feed_mm_min, (double)m.plunge_mm_min);
  printf("  kinematics: cartesian (CoreXY solved by GRBL board)\n");
  return 0;
}

static int cmd_demo(float step_mm)
{
  struct velaink_drawing  raw;
  struct velaink_drawing  drawn;
  struct velaink_machine  m;
  struct velaink_stroke   smoothed;
  struct velaink_stroke   resampled;
  int                     s;
  int                     ret;
  int                     total_pts = 0;

  velaink_machine_default(&m);

  if (velaink_drawing_init(&raw, 8) < 0)
    {
      printf("velaink: out of memory (raw)\n");
      return -1;
    }

  if (velaink_demo_heart(&raw) < 0)
    {
      printf("velaink: failed to build demo stroke\n");
      velaink_drawing_free(&raw);
      return -1;
    }

  if (velaink_drawing_init(&drawn, 8) < 0)
    {
      printf("velaink: out of memory (drawn)\n");
      velaink_drawing_free(&raw);
      return -1;
    }

  for (s = 0; s < raw.nstrokes; s++)
    {
      if (raw.strokes[s].npts < 2)
        {
          continue;
        }

      if (velaink_stroke_smooth(&raw.strokes[s], &smoothed, 6) < 0)
        {
          printf("velaink: smooth failed\n");
          break;
        }

      if (velaink_stroke_resample(&smoothed, &resampled, step_mm,
                                  m.width_mm, m.height_mm) < 0)
        {
          printf("velaink: resample failed\n");
          velaink_stroke_free(&smoothed);
          break;
        }

      if (drawn.nstrokes < drawn.cap)
        {
          drawn.strokes[drawn.nstrokes] = resampled;
          drawn.nstrokes++;
          total_pts += resampled.npts;
        }

      velaink_stroke_free(&smoothed);
    }

  printf("; VelaInk demo: %d stroke(s), %d points, step %.2f mm\n",
         drawn.nstrokes, total_pts, (double)step_mm);

  ret = velaink_gcode_emit(stdout, &drawn, &m);
  if (ret < 0)
    {
      printf("velaink: gcode generation failed (%d)\n", ret);
    }
  else
    {
      printf("; generated %d gcode lines\n", ret);

      if (velaink_gcode_to_file("/tmp/velaink_demo.nc", &drawn, &m) > 0)
        {
          printf("; saved /tmp/velaink_demo.nc\n");
        }
    }

  velaink_drawing_free(&drawn);
  velaink_drawing_free(&raw);
  return ret < 0 ? -1 : 0;
}

/* 接收笔迹 -> 编译 G-code -> 下发 */
static int cmd_recv(const char *indev, const char *outdev)
{
  struct velaink_drawing drawing;
  struct velaink_machine m;
  FILE                  *in;
  FILE                  *out;
  int                    npts = 0;
  int                    ret;
  int                    infd = -1;

  velaink_machine_default(&m);

  infd = open(indev, O_RDONLY);
  if (infd < 0)
    {
      printf("velaink: cannot open input %s\n", indev);
      return -1;
    }

  in = fdopen(infd, "r");
  if (!in)
    {
      printf("velaink: fdopen failed\n");
      close(infd);
      return -1;
    }

  printf("velaink: waiting for strokes on %s ...\n", indev);
  fflush(stdout);

  ret = velaink_recv_stream(in, &drawing, &npts);
  fclose(in);

  if (ret <= 0)
    {
      printf("velaink: no complete drawing received (%d)\n", ret);
      return -1;
    }

  printf("velaink: received %d strokes / %d points\n", drawing.nstrokes, npts);

  out = stdout;
  if (outdev)
    {
      out = fopen(outdev, "w");
      if (!out)
        {
          printf("velaink: cannot open output %s, fallback to stdout\n",
                 outdev);
          out = stdout;
        }
    }

  ret = velaink_gcode_emit(out, &drawing, &m);
  if (out != stdout)
    {
      fflush(out);
      fclose(out);
    }

  if (ret < 0)
    {
      printf("velaink: gcode failed (%d)\n", ret);
    }
  else
    {
      printf("velaink: emitted %d gcode lines\n", ret);
    }

  velaink_drawing_free(&drawing);
  return ret < 0 ? -1 : 0;
}

/* GRBL 逐行下发：每行等待 "ok" 应答，保证下位机缓冲区不溢出 */
static int cmd_send(const char *file, const char *dev)
{
  FILE *fp;
  int   fd;
  char  line[160];
  int   sent = 0;

  fp = fopen(file, "r");
  if (!fp)
    {
      printf("velaink: cannot open %s\n", file);
      return -1;
    }

  fd = open(dev, O_RDWR);
  if (fd < 0)
    {
      printf("velaink: cannot open %s\n", dev);
      fclose(fp);
      return -1;
    }

  /* 唤醒 GRBL */
  write(fd, "\r\n\r\n", 4);
  usleep(200000);

  while (fgets(line, sizeof(line), fp))
    {
      size_t len = strlen(line);

      if (len == 0 || line[0] == ';')
        {
          continue;
        }

      write(fd, line, len);

      {
        char  buf[64];
        int   got = 0;
        int   tries;

        for (tries = 0; tries < 100 && !got; tries++)
          {
            ssize_t n = read(fd, buf, sizeof(buf) - 1);

            if (n > 0)
              {
                buf[n] = '\0';
                if (strstr(buf, "ok"))
                  {
                    got = 1;
                  }
              }
            else
              {
                usleep(10000);
              }
          }

        if (!got)
          {
            printf("velaink: no ok from GRBL at line %d\n", sent + 1);
            close(fd);
            fclose(fp);
            return -1;
          }
      }

      sent++;
    }

  close(fd);
  fclose(fp);
  printf("velaink: sent %d lines to %s\n", sent, dev);
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
    {
      usage();
      return 1;
    }

  if (strcmp(argv[1], "info") == 0)
    {
      return cmd_info();
    }

  if (strcmp(argv[1], "demo") == 0)
    {
      float step = (argc > 2) ? (float)atof(argv[2]) : 0.5f;

      if (step <= 0.0f)
        {
          step = 0.5f;
        }

      return cmd_demo(step);
    }

  if (strcmp(argv[1], "recv") == 0)
    {
      if (argc < 3)
        {
          usage();
          return 1;
        }

      return cmd_recv(argv[2], (argc > 3) ? argv[3] : NULL);
    }

  if (strcmp(argv[1], "send") == 0)
    {
      if (argc < 4)
        {
          usage();
          return 1;
        }

      return cmd_send(argv[2], argv[3]);
    }

  usage();
  return 1;
}
