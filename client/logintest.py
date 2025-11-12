#!/usr/bin/env python3
"""简洁的 login 测试脚本。

发送三条登录请求（用户名空、密码空、有效），解析无分隔的 JSON 流并只打印登录响应，忽略市场数据。
"""
from __future__ import annotations

import argparse
import json
import socket
import time
from typing import Generator, Any


def decode_stream(sock: socket.socket) -> Generator[Any, None, None]:
    """从 TCP 流中解析连续的 JSON 对象（没有分隔符）。"""
    decoder = json.JSONDecoder()
    buf = ""
    sock.settimeout(1.0)
    while True:
        # 先尝试从已有缓冲解析
        start = 0
        while True:
            while start < len(buf) and buf[start].isspace():
                start += 1
            if start >= len(buf):
                buf = ""
                break
            try:
                obj, end = decoder.raw_decode(buf, idx=start)
                yield obj
                buf = buf[end:]
                start = 0
            except json.JSONDecodeError:
                break
        try:
            chunk = sock.recv(4096)
            if not chunk:
                return
            buf += chunk.decode("utf-8", errors="ignore")
        except socket.timeout:
            # no data this cycle; continue
            continue


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="login test client")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--timeout", type=float, default=5.0, help="seconds to wait for replies")
    args = ap.parse_args(argv)

    msgs = [
        {"action": "login", "username": "", "password": "pass"},
        {"action": "login", "username": "user", "password": ""},
        {"action": "login", "username": "alice", "password": "secret"},
    ]

    try:
        with socket.create_connection((args.host, args.port), timeout=2) as sock:
            # 发送所有测试消息
            for m in msgs:
                sock.sendall(json.dumps(m).encode("utf-8"))
                time.sleep(0.05)

            # 读取并只打印与登录相关的响应
            want = len(msgs)
            got = 0
            start = time.time()
            gen = decode_stream(sock)
            while time.time() - start < args.timeout and got < want:
                try:
                    obj = next(gen)
                except StopIteration:
                    break
                except Exception:
                    # parsing/recv timeout - loop and check overall timeout
                    continue

                # 登录回复在服务器端以 "action":"login" 返回，status 存储状态码
                print(json.dumps(obj, ensure_ascii=False))
                """
                if isinstance(obj, dict) and obj.get("action") == "login":
                    print(json.dumps(obj, ensure_ascii=False))
                    got += 1
                # 兼容：如果服务器使用 status+msg/error 返回但没有 action 字段
                elif isinstance(obj, dict) and ("status" in obj) and ("msg" in obj or "error" in obj):
                    print(json.dumps(obj, ensure_ascii=False))
                    got += 1
                """

            return 0
    except Exception as e:
        print(f"error: {e}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
