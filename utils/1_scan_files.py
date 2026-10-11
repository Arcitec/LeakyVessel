#!/usr/bin/env python3
"""Scans a Skyrim installation and builds a list of "Data/" directory entries."""

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Scan a Skyrim installation and build a list of Data/ directory entries."
    )
    parser.add_argument(
        "game_dir",
        type=Path,
        help="Path to the game installation directory (parent of 'Data/').",
    )
    args = parser.parse_args()

    game_dir: Path = args.game_dir
    data_dir = game_dir / "Data"

    files_dedup: dict[str, str] = {}

    for p in data_dir.iterdir():
        lp = p.name.casefold()
        files_dedup[lp] = p.name

    files = sorted(files_dedup.values(), key=lambda x: x.casefold())

    raw_json = json.dumps(files, indent=2)
    json_file = Path(__file__).parent / "files.json"
    json_file.write_text(raw_json, encoding="utf-8")


if __name__ == "__main__":
    main()
