#!/usr/bin/env python3
"""Creates everything from files.json to prepare the test environment."""

import json
import sys
from pathlib import Path


def main() -> None:
    # Read pre-built file list.
    json_file = Path(__file__).parent / "files.json"
    file_names: list[str] = json.loads(json_file.read_text(encoding="utf-8"))

    # Create the subdir "Data" relative to current working dir.
    data_dir = Path("Data")
    if data_dir.exists():
        sys.exit(
            f"Error: Output directory already exists: '{data_dir}'. Stopping to avoid data loss. You must manually delete it before running again."
        )
    data_dir.mkdir(parents=True, exist_ok=True)

    # Loop through every name and create dummy files to represent them.
    count = 0
    for name in file_names:
        count += 1
        file_path = data_dir / name
        file_path.write_text("DUMMYDUMMYDUMMYDUMMY", encoding="utf-8")

    print(f"Created {count} files.")


if __name__ == "__main__":
    main()
