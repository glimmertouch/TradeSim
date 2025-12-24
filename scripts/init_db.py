#!/usr/bin/env python3
"""
Init a SQLite database for TradeSim and insert a sample user using salt+hash.

Schema matches C++ Database::init (users table with pass_hash + salt).
Password hashing matches C++ PBKDF2-HMAC-SHA256 (iters=100000, dkLen=32),
and salt is 16 random bytes encoded as lowercase hex.

Usage examples:
  python3 scripts/init_db.py                      # create trade.db and user hello/world
  python3 scripts/init_db.py --db build/trade.db  # choose another path
  python3 scripts/init_db.py -u alice -p secret   # custom user/pass
"""
from __future__ import annotations

import argparse
import hashlib
import secrets
import sqlite3
from pathlib import Path


DEFAULT_DB = "trade.db"


def ensure_schema(conn: sqlite3.Connection) -> None:
    cur = conn.cursor()
    # Create table if not exists (align with C++)
    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS users (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          username TEXT UNIQUE NOT NULL,
          pass_hash TEXT NOT NULL,
          salt TEXT NOT NULL,
          balance REAL DEFAULT 100000.0
        );
        """
    )
    conn.commit()


def pbkdf2_sha256_hex(password: str, salt_hex: str, iters: int = 100000, dk_len: int = 32) -> str:
    salt = bytes.fromhex(salt_hex)
    dk = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, iters, dklen=dk_len)
    return dk.hex()


def insert_user(conn: sqlite3.Connection, username: str, password: str, balance: float) -> int | None:
    cur = conn.cursor()
    salt_hex = secrets.token_bytes(16).hex()  # 16 bytes => 32 hex chars
    hash_hex = pbkdf2_sha256_hex(password, salt_hex)

    try:
        cur.execute(
            """
            INSERT INTO users (username, pass_hash, salt, balance)
            VALUES (?, ?, ?, ?);
            """,
            (username, hash_hex, salt_hex, balance),
        )
        conn.commit()
    except sqlite3.IntegrityError as e:
        print(f"User '{username}' already exists (unique constraint). Skipping. Details: {e}")
        return None

    return cur.lastrowid


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Initialize TradeSim DB and insert a sample user")
    ap.add_argument("--db", default=DEFAULT_DB, help="Path to sqlite database (default: trade.db)")
    ap.add_argument("-u", "--username", default="hello")
    ap.add_argument("-p", "--password", default="world")
    ap.add_argument("--balance", type=float, default=100000.0)
    args = ap.parse_args(argv)

    db_path = Path(args.db)
    db_path.parent.mkdir(parents=True, exist_ok=True)

    conn = sqlite3.connect(str(db_path))
    try:
        ensure_schema(conn)
        uid = insert_user(conn, args.username, args.password, args.balance)
        if uid is not None:
            print(f"Created user id={uid}, username='{args.username}' in {db_path}")
        else:
            print(f"User '{args.username}' already exists in {db_path}")
        return 0
    finally:
        conn.close()


if __name__ == "__main__":
    raise SystemExit(main())
