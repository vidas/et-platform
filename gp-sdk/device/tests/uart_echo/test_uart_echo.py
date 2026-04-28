#!/usr/bin/env python3
# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0
"""
End-to-end test for the uart_echo kernel.

Drives the kernel's UART through the host-side erbium_run dispatch
script using --uart-stdin / --uart-stdout. Sends a known string
terminated with ASCII EOT (0x04), captures what the kernel echoes
back, and asserts equality.

Defaults pick up /opt/et/bin/erbium_run + /opt/et/kernels/uart_echo.*
so a stock install runs:

    test_uart_echo.py --device erbium_emu
    test_uart_echo.py --device soc1sim
    test_uart_echo.py --device erbium_emu --message 'hello world'
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_LAUNCHER = "erbium_run"

DEFAULT_ELFS = {
    "soc1sim":    Path("/opt/et/kernels/uart_echo.erbium-soc1sim.elf"),
    "sys_emu":    Path("/opt/et/kernels/uart_echo.erbium-soc1sim.elf"),
    "erbium_emu": Path("/opt/et/kernels/uart_echo.erbium.elf"),
}

DEFAULT_MESSAGE = "Hello, UART!"
EOT = b"\x04"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device",
                    choices=["erbium_emu", "soc1sim", "sys_emu"],
                    default="soc1sim",
                    help="target backend (default: soc1sim)")
    ap.add_argument("--elf", type=Path, default=None,
                    help="override path to uart_echo.elf")
    ap.add_argument("--launcher", type=str, default=DEFAULT_LAUNCHER,
                    help=f"erbium_run dispatch script (default: {DEFAULT_LAUNCHER} on PATH)")
    ap.add_argument("--message", default=DEFAULT_MESSAGE,
                    help=f"text to send through the UART (default: {DEFAULT_MESSAGE!r})")
    ap.add_argument("--output-dir", type=Path, default=Path("./uart-echo-results"),
                    help="directory for input/output artefacts (default: ./uart-echo-results)")
    ap.add_argument("--linger", type=float, default=5.0,
                    help="seconds between sending the message and sending EOT (default: 5.0)")
    ap.add_argument("--kernel-timeout", type=int, default=60,
                    help="kernel timeout passed to the launcher (default: 60s)")
    args = ap.parse_args()

    elf = args.elf or DEFAULT_ELFS[args.device]
    if not elf.exists():
        print(f"error: {elf} not found — build uart_echo first "
              f"(target: {elf.name})", file=sys.stderr)
        return 1
    if not shutil.which(args.launcher):
        print(f"error: launcher '{args.launcher}' not found on PATH", file=sys.stderr)
        return 1

    outdir = args.output_dir
    outdir.mkdir(parents=True, exist_ok=True)

    in_path  = outdir / "uart_in.fifo"
    out_path = outdir / "uart_out.bin"
    if in_path.exists() or in_path.is_symlink():
        in_path.unlink()
    os.mkfifo(in_path)
    if out_path.exists():
        out_path.unlink()

    cmd = [args.launcher,
           "--device", args.device,
           "--elf-load", str(elf),
           "--timeout", str(args.kernel_timeout),
           "--uart-stdin",  str(in_path),
           "--uart-stdout", str(out_path)]

    print(f"+ {' '.join(cmd)}  (linger={args.linger}s)", flush=True)

    # Spawn the launcher first; it opens the FIFO read end O_NONBLOCK
    # so our write-end open below succeeds without blocking the test.
    proc = subprocess.Popen(cmd)

    # Open the writer end with retry: if the launcher hasn't opened the
    # read end yet, O_WRONLY|O_NONBLOCK returns ENXIO -- retry briefly.
    msg = args.message.encode("utf-8")
    overall_timeout = args.kernel_timeout + args.linger + 30
    deadline = time.monotonic() + overall_timeout
    write_fd = -1
    while time.monotonic() < deadline:
        try:
            write_fd = os.open(in_path, os.O_WRONLY | os.O_NONBLOCK)
            break
        except OSError:
            if proc.poll() is not None:
                print(f"FAIL: launcher exited {proc.returncode} before reading FIFO",
                      file=sys.stderr)
                return 1
            time.sleep(0.05)
    if write_fd < 0:
        proc.kill(); proc.wait()
        print("FAIL: timed out waiting for launcher to open UART FIFO", file=sys.stderr)
        return 1

    try:
        # Switch back to blocking writes for simplicity; the kernel reads
        # bytes promptly so the bridge ring rarely backpressures.
        os.set_blocking(write_fd, True)
        os.write(write_fd, msg)
        time.sleep(args.linger)
        os.write(write_fd, EOT)
    finally:
        os.close(write_fd)

    try:
        r = proc.wait(timeout=overall_timeout)
    except subprocess.TimeoutExpired:
        proc.kill(); proc.wait()
        print(f"FAIL: launcher wallclock timeout after {overall_timeout}s", file=sys.stderr)
        return 1

    if r != 0:
        print(f"FAIL: launcher exited {r}", file=sys.stderr)
        return 1

    if not out_path.exists():
        print(f"FAIL: launcher did not produce {out_path}", file=sys.stderr)
        return 1

    actual   = out_path.read_bytes()
    expected = msg  # kernel echoes everything except the EOT

    if actual != expected:
        print(f"FAIL: echo mismatch", file=sys.stderr)
        print(f"  expected ({len(expected)}): {expected!r}", file=sys.stderr)
        print(f"  actual   ({len(actual)}):   {actual!r}", file=sys.stderr)
        return 1

    print(f"PASS: echoed {len(actual)} bytes verbatim (kernel ran ~{args.linger:.1f}s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
