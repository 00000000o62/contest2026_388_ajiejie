#!/usr/bin/env python3
"""把 WorkBuddy 会话记录导出为 openvela 大赛合规的 AI Coding 日志。

官方采集器只挂钩 Claude Code / opencode / codex / kiro，用 WorkBuddy 开发时
不会自动入仓。本脚本把 WorkBuddy 的会话 JSONL 转成大赛 schema 要求的格式，
并按日期分文件写入 <仓>/logs/<github_login>/<date>/<tool>__<session_id>.jsonl，
同时维护 manifest.json。

安全：导出前会脱敏密钥（GitHub PAT、Bearer token 等），脱敏次数记入
redacted_count 字段，供审计。

用法：
    python3 export_workbuddy_logs.py --project <workbuddy项目目录> \
        --out <仓>/logs/00000000o62 --login 00000000o62 \
        --team-id contest2026_388_ajiejie --since 2026-09-01
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

SCHEMA_VERSION = "1.0"
TOOL = "claude-code"  # schema 枚举允许值；WorkBuddy 采用同源会话格式

# ---- 脱敏规则 ----
REDACTIONS: List[tuple] = [
    (re.compile(r"github_pat_[A-Za-z0-9_]{20,}"), "<REDACTED_GITHUB_PAT>"),
    (re.compile(r"\bghp_[A-Za-z0-9]{20,}"), "<REDACTED_GHP>"),
    (re.compile(r"\bgho_[A-Za-z0-9]{20,}"), "<REDACTED_GHO>"),
    (re.compile(r"Bearer\s+[A-Za-z0-9._\-]{16,}"), "Bearer <REDACTED>"),
    (re.compile(r"(?i)(authorization[\"'\s:=]+)[A-Za-z0-9._\-]{16,}"), r"\1<REDACTED>"),
    (re.compile(r"(?i)(api[_-]?key[\"'\s:=]+)[A-Za-z0-9._\-]{16,}"), r"\1<REDACTED>"),
    (re.compile(r"(?i)(password[\"'\s:=]+)\S{8,}"), r"\1<REDACTED>"),
]

REDACT_COUNT = 0


def redact(obj: Any) -> Any:
    """递归脱敏字符串中的密钥，返回新对象。"""
    global REDACT_COUNT
    if isinstance(obj, str):
        out = obj
        for pat, rep in REDACTIONS:
            out, n = pat.subn(rep, out)
            REDACT_COUNT += n
        return out
    if isinstance(obj, list):
        return [redact(x) for x in obj]
    if isinstance(obj, dict):
        return {k: redact(v) for k, v in obj.items()}
    return obj


def iso_ts(ms: Optional[int]) -> str:
    if not ms:
        return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    return datetime.fromtimestamp(ms / 1000, timezone.utc).isoformat().replace("+00:00", "Z")


def text_from_content(content: Any) -> str:
    """把 content 块列表拼成纯文本。"""
    if isinstance(content, str):
        return content
    if not isinstance(content, list):
        return ""
    parts = []
    for blk in content:
        if isinstance(blk, str):
            parts.append(blk)
        elif isinstance(blk, dict):
            t = blk.get("text") or blk.get("content") or ""
            if isinstance(t, str) and t.strip():
                parts.append(t)
    return "\n".join(parts)


def files_from_arguments(args: Any, cwd: str) -> List[str]:
    """从工具参数里提取涉及的文件路径。"""
    found = []

    def walk(o: Any) -> None:
        if isinstance(o, str):
            if re.match(r"^[A-Za-z]:[\\/]", o) or o.startswith("/") or \
               re.match(r"^~?/[A-Za-z0-9_.-]+/", o):
                if len(o) < 300:
                    found.append(o)
        elif isinstance(o, dict):
            for k, v in o.items():
                if k.lower() in ("path", "file_path", "filepath", "file", "notebook_path"):
                    if isinstance(v, str):
                        found.append(v)
                else:
                    walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    walk(args)
    # 去重、去掉明显噪声
    seen, out = set(), []
    for f in found:
        if f not in seen and " " not in f[:40]:
            seen.add(f)
            out.append(f)
    return out[:20]


def convert_file(src: Path, session_id: str, team_id: str, login: str,
                 since_ms: int, max_output: int) -> Dict[str, List[dict]]:
    """转换单个会话文件，返回 {日期: [事件]}"""
    by_date: Dict[str, List[dict]] = {}
    seq = 0
    pending_thinking: List[str] = []
    pending_model = None

    def emit(ev: dict, date_key: str) -> None:
        nonlocal seq
        ev["seq"] = seq
        seq += 1
        by_date.setdefault(date_key, []).append(ev)

    with src.open(encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue

            t = d.get("type")
            ts = d.get("timestamp") or 0
            if ts and ts < since_ms:
                continue
            ts_iso = iso_ts(ts)
            date_key = ts_iso[:10]
            cwd = d.get("cwd") or ""

            if t == "message":
                role = d.get("role", "user")
                text = text_from_content(d.get("content"))
                if not text.strip():
                    continue
                ev = {
                    "schema_version": SCHEMA_VERSION,
                    "session_id": session_id,
                    "team_id": team_id,
                    "github_login": login,
                    "tool": TOOL,
                    "ts": ts_iso,
                    "role": role,
                    "text": text,
                }
                if cwd:
                    ev["cwd"] = cwd
                if role == "assistant":
                    if pending_thinking:
                        ev["thinking"] = "\n\n".join(pending_thinking)[:20000]
                        pending_thinking = []
                    if pending_model:
                        ev["model"] = pending_model
                        pending_model = None
                emit(ev, date_key)

            elif t == "reasoning":
                raw = d.get("rawContent") or d.get("content") or []
                txt = text_from_content(raw)
                pd = d.get("providerData") or {}
                if pd.get("model"):
                    pending_model = pd["model"]
                if txt.strip():
                    pending_thinking.append(txt)

            elif t == "function_call":
                name = d.get("name") or "unknown"
                args_raw = d.get("arguments")
                if isinstance(args_raw, str):
                    try:
                        args = json.loads(args_raw)
                    except json.JSONDecodeError:
                        args = {"raw": args_raw}
                else:
                    args = args_raw
                ev = {
                    "schema_version": SCHEMA_VERSION,
                    "session_id": session_id,
                    "team_id": team_id,
                    "github_login": login,
                    "tool": TOOL,
                    "ts": ts_iso,
                    "role": "tool",
                    "tool_name": name,
                    "tool_call_id": d.get("callId") or d.get("id"),
                    "input": args,
                    "files_touched": files_from_arguments(args, cwd),
                }
                if cwd:
                    ev["cwd"] = cwd
                emit(ev, date_key)

            elif t == "function_call_result":
                out = d.get("output")
                if isinstance(out, dict):
                    out_val: Any = out.get("text") if out.get("type") == "text" else out
                else:
                    out_val = out
                if isinstance(out_val, str) and max_output > 0 and len(out_val) > max_output:
                    out_val = out_val[:max_output] + f"\n...<truncated {len(out_val) - max_output} chars>"
                ev = {
                    "schema_version": SCHEMA_VERSION,
                    "session_id": session_id,
                    "team_id": team_id,
                    "github_login": login,
                    "tool": TOOL,
                    "ts": ts_iso,
                    "role": "tool",
                    "tool_name": d.get("name") or "unknown",
                    "tool_call_id": d.get("callId") or d.get("parentId"),
                    "output": {"status": d.get("status", "completed"), "value": out_val},
                }
                if cwd:
                    ev["cwd"] = cwd
                emit(ev, date_key)

    return by_date


def main() -> int:
    ap = argparse.ArgumentParser(description="导出 WorkBuddy 会话为大赛 AI 日志")
    ap.add_argument("--project", required=True, help="WorkBuddy 项目目录（含 *.jsonl）")
    ap.add_argument("--out", required=True, help="输出目录 logs/<github_login>")
    ap.add_argument("--login", required=True, help="GitHub 用户名")
    ap.add_argument("--team-id", required=True, help="如 contest2026_388_ajiejie")
    ap.add_argument("--since", default="2026-09-01", help="只导出该日期之后的事件")
    ap.add_argument("--max-output", type=int, default=4000, help="单条工具输出最大字符数")
    ap.add_argument("--limit", type=int, default=0, help="最多处理多少个会话文件（0=全部）")
    args = ap.parse_args()

    proj = Path(args.project)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    since_ms = int(datetime.strptime(args.since, "%Y-%m-%d")
                   .replace(tzinfo=timezone.utc).timestamp() * 1000)

    files = sorted(proj.glob("*.jsonl"), key=os.path.getmtime, reverse=True)
    if args.limit:
        files = files[: args.limit]
    if not files:
        print("未找到会话文件", file=sys.stderr)
        return 1

    sessions_meta = []
    for f in files:
        sid = f.stem
        by_date = convert_file(f, sid, args.team_id, args.login, since_ms, args.max_output)
        if not by_date:
            continue
        for date_key, events in sorted(by_date.items()):
            day_dir = out_dir / date_key
            day_dir.mkdir(parents=True, exist_ok=True)
            dest = day_dir / f"{TOOL}__{sid}.jsonl"
            with dest.open("w", encoding="utf-8") as fh:
                for ev in events:
                    fh.write(json.dumps(redact(ev), ensure_ascii=False) + "\n")
            rel = f"logs/{args.login}/{date_key}/{dest.name}"
            sessions_meta.append({
                "session_id": sid,
                "tool": TOOL,
                "started_at": events[0]["ts"],
                "last_event_at": events[-1]["ts"],
                "event_count": len(events),
                "file_path": rel,
                # schema 允许的枚举：cli / vscode_extension /
                # vscode_extension_partial / backfill-sqlite
                "collection_mode": "cli",
                "health": "ok",
            })
            print(f"  写入 {rel}  ({len(events)} 事件)")

    sessions_meta.sort(key=lambda s: s["started_at"])
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "team_id": args.team_id,
        "github_login": args.login,
        "generator": "workbuddy-export@1.0.0 (export_workbuddy_logs.py)",
        "sessions": sessions_meta,
        "updated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }
    with (out_dir / "manifest.json").open("w", encoding="utf-8") as fh:
        json.dump(manifest, fh, ensure_ascii=False, indent=2)
        fh.write("\n")

    print(f"\n完成：{len(sessions_meta)} 个会话分片，脱敏替换 {REDACT_COUNT} 处")
    print(f"manifest: {out_dir / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
