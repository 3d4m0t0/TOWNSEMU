#!/usr/bin/env python3
"""Smoke: legacy absolute-only vs DS-off Game Phys INI contract + resolve helpers."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCAN_CPP = ROOT / "src/towns/mouse_coord_write_scan.cpp"
SCAN_H = ROOT / "src/towns/mouse_coord_write_scan.h"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    hdr = SCAN_H.read_text(encoding="utf-8", errors="replace")
    cpp = SCAN_CPP.read_text(encoding="utf-8", errors="replace")

    for needle in (
        "dsOffX",
        "hasDsOff",
        "ResolveDsRelativePhys",
        "CaptureDsRelativeFromPhys",
        "ResolveActiveProfileDsOffsets",
        "ResolveCoordPairDsOffLocked",
    ):
        if needle not in hdr and needle not in cpp:
            fail(f"missing {needle}")

    for needle in ('"_ds_off_x"==sub', '"_ds_off_y"==sub', '"_ds_sel"==sub'):
        if needle not in cpp:
            fail(f"FromIni missing branch {needle}")

    for needle in ("_ds_off_x=0x", "_ds_off_y=0x", "_ds_sel=0x"):
        if needle not in cpp:
            fail(f"ToIni missing write of {needle}")

    # Sample shapes the loader is expected to accept.
    legacy_ok = "pair0_x=0x00123456" and "pair0_y=0x00123458"
    ds_ok = all(
        k in (
            "pair0_ds_off_x=0x0006EEDC\n"
            "pair0_ds_off_y=0x0006EEDE\n"
            "pair0_ds_sel=0x00000014\n"
        )
        for k in ("pair0_ds_off_x=", "pair0_ds_off_y=", "pair0_ds_sel=")
    )
    if not legacy_ok or not ds_ok:
        fail("sample shapes")

    print(
        "mouse_coord_ds_off smoke: ok "
        "(legacy absolute + DS-off INI keys, resolve helpers present)"
    )


if __name__ == "__main__":
    main()
