#!/usr/bin/env python3
"""
BookClient: connect to market data server, parse JSON snapshots, and render
per-symbol top-5 bid/ask in the terminal, refreshing on each snapshot.

- Protocol: server sends concatenated JSON objects (no delimiter).
  We use JSONDecoder.raw_decode to parse incrementally from a stream buffer.
- Format expected (tolerant):
  {
    "action": "market_data",
    "event":  "market_data",
    "data": { SYMBOL: { "buy": [...], "sell": [...] }, ... },
    "timestamp": 169...  # optional, unix ms
  }
  Each level supports { "price": int, "quantity"|"volume": int }

Usage:
  python3 client/BookClient.py --host 127.0.0.1 --port 8000
  Optional: --frames N  Exit after N frames (default 0 = infinite)
"""
from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from typing import Any, Dict, List, Tuple

RESET_SCREEN = "\x1b[2J\x1b[H"  # clear screen, move cursor home


def decode_stream(sock: socket.socket):
    """Yield JSON objects parsed from a TCP stream lacking delimiters."""
    decoder = json.JSONDecoder()
    buf = ""
    sock.settimeout(10.0)
    while True:
        # Try to parse existing buffer first
        start = 0
        while True:
            # Skip leading whitespace
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
                # need more data
                break
        try:
            chunk = sock.recv(4096)
            if not chunk:
                return
            buf += chunk.decode("utf-8", errors="ignore")
        except socket.timeout:
            # no data for a while; continue waiting
            continue


def fmt_levels(levels: Any, top: int = 5) -> List[Tuple[int, int]]:
    """Return list of (price, qty) from levels JSON (may be None)."""
    out: List[Tuple[int, int]] = []
    if not isinstance(levels, list):
        return out
    for item in levels[:top]:
        if not isinstance(item, dict):
            continue
        price = item.get("price")
        qty = item.get("quantity", item.get("volume"))
        if isinstance(price, int) and isinstance(qty, int):
            out.append((price, qty))
    return out


def render_frame(snapshot: Dict[str, Any]) -> None:
    data = snapshot.get("data", {})
    ts = snapshot.get("timestamp")
    # optional: format timestamp
    if isinstance(ts, int):
        ts_s = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(ts / 1000))
        ts_ms = f".{ts % 1000:03d}"
        ts_str = f"{ts_s}{ts_ms}"
    else:
        ts_str = "-"

    print(RESET_SCREEN, end="")
    print(f"Market Data  (ts: {ts_str})\n")

    # For stable output, sort symbols
    for symbol in sorted(data.keys()):
        book = data.get(symbol) or {}
        buys = fmt_levels(book.get("buy"), 5)
        sells = fmt_levels(book.get("sell"), 5)

        print(f"Symbol: {symbol}")
        # Make rows aligned by filling shorter side
        rows = max(len(buys), len(sells), 5)
        buys += [(0, 0)] * (rows - len(buys))
        sells += [(0, 0)] * (rows - len(sells))

        # Header
        print("  BUY (px/qty)           |  SELL (px/qty)")
        print("  -----------------------+-----------------------")
        for i in range(rows):
            bp, bq = buys[i]
            sp, sq = sells[i]
            btxt = f"{bp:>6}/{bq:<6}" if bp and bq else "-"
            stxt = f"{sp:>6}/{sq:<6}" if sp and sq else "-"
            print(f"  {btxt:<21} |  {stxt:<21}")
        print()


def run(host: str, port: int, frames: int) -> int:
    try:
        with socket.create_connection((host, port)) as sock:
            count = 0
            for obj in decode_stream(sock):
                # Validate action/event
                if obj.get("action") != "market_data":
                    continue
                render_frame(obj)
                count += 1
                if frames > 0 and count >= frames:
                    break
        return 0
    except KeyboardInterrupt:
        return 130
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Top-5 book client")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--frames", type=int, default=0, help="exit after N frames (0=infinite)")
    args = ap.parse_args(argv)
    return run(args.host, args.port, args.frames)


if __name__ == "__main__":
    raise SystemExit(main())
