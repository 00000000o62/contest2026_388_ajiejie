# tools/ — 项目自用工具

## export_workbuddy_logs.py

大赛要求把 AI 编程过程沉淀到 `logs/`。官方采集器只挂钩
Claude Code / opencode / codex / kiro，用 WorkBuddy 开发时不会自动入仓。
本脚本把 WorkBuddy 的会话记录（`~/.workbuddy/projects/<项目>/*.jsonl`）
转换成大赛 schema 要求的 JSONL，并按日期分文件写入 `logs/<github_login>/`，
同时维护 `manifest.json`。

导出前会**自动脱敏**（GitHub PAT、Bearer token、api_key、password），
脱敏次数记入各事件的 `redacted_count`。

```sh
python3 tools/export_workbuddy_logs.py \
  --project ~/.workbuddy/projects/<项目目录> \
  --out logs/00000000o62 \
  --login 00000000o62 \
  --team-id contest2026_388_ajiejie \
  --since 2026-09-01

# 用官方工具校验
python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/
```
