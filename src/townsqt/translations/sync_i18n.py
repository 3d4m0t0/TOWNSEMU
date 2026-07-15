#!/usr/bin/env python3
"""Sync TownsQt translation JSON files to the en/ja key set and ordering (m88-qt style)."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LOCALES = ("en", "ja", "de", "fr", "es", "ko", "zh_CN", "zh_TW")


def load_entries(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8") as handle:
        data = json.load(handle)
    return data["translations"]


def entry_map(entries: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, str]]:
    return {(entry["context"], entry["source"]): entry for entry in entries}


def merged_key_order(
    en_entries: list[dict[str, str]], ja_entries: list[dict[str, str]]
) -> list[tuple[str, str]]:
    order: list[tuple[str, str]] = []
    seen: set[tuple[str, str]] = set()
    for entries in (ja_entries, en_entries):
        for entry in entries:
            key = (entry["context"], entry["source"])
            if key not in seen:
                order.append(key)
                seen.add(key)
    return order


def sync_locale(
    locale: str,
    order: list[tuple[str, str]],
    en_map: dict[tuple[str, str], dict[str, str]],
    locale_map: dict[tuple[str, str], dict[str, str]],
) -> list[dict[str, str]]:
    synced: list[dict[str, str]] = []
    for key in order:
        context, source = key
        en_entry = en_map.get(key)
        existing = locale_map.get(key)
        if locale == "en":
            translation = source
        elif existing is not None and existing.get("translation"):
            translation = existing["translation"]
        elif en_entry is not None:
            # Fall back to English until translated.
            translation = en_entry["translation"]
        else:
            translation = source
        synced.append(
            {
                "context": context,
                "source": source,
                "translation": translation,
            }
        )
    return synced


def main() -> None:
    en_entries = load_entries(ROOT / "townsqt_en.json")
    ja_entries = load_entries(ROOT / "townsqt_ja.json")
    en_map = entry_map(en_entries)
    ja_map = entry_map(ja_entries)
    order = merged_key_order(en_entries, ja_entries)

    for locale in LOCALES:
        path = ROOT / f"townsqt_{locale}.json"
        locale_map = entry_map(load_entries(path)) if path.exists() else {}
        if locale == "ja":
            locale_map = ja_map
        synced = sync_locale(locale, order, en_map, locale_map)
        with path.open("w", encoding="utf-8") as handle:
            json.dump({"translations": synced}, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
        print(f"wrote {path.name} ({len(synced)} entries)")


if __name__ == "__main__":
    main()
