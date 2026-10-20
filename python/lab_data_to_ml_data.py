#!/usr/bin/env python3
"""
Usage: python extract_rms.py input.csv output.csv
"""
import argparse
import csv

COLUMN_NAMES = ["v-RMS", "a-RMS", "revolution"]
START_ROW = 8      # 1-based row where the first block of data starts
SKIP_AFTER = 7     # rows to skip after a non-"2026" marker row
PREFIX = "2026"


def first_column(path):
    """Read only the first column of the CSV (empty string if row is empty)."""
    with open(path, newline="", encoding="utf-8-sig") as f:
        return [row[0].strip() if row else "" for row in csv.reader(f)]


def extract_blocks(items):
    blocks = []
    i = START_ROW - 1  # convert to 0-based index

    for _ in COLUMN_NAMES:
        values = []

        # Collect values until the first column no longer starts with "2026"
        while i < len(items) and items[i].startswith(PREFIX):
            # "2026-10-19T22:54:38.8210000+00:00;0" -> "0"
            values.append(items[i].split(";", 1)[1] if ";" in items[i] else "")
            i += 1
        blocks.append(values)

        # Stop entirely at the end of the file or a blank item
        if i >= len(items) or items[i] == "":
            break

        # items[i] is a non-"2026" marker row: skip it plus 7 more rows
        i += 1 + SKIP_AFTER

    # Pad in case fewer blocks than columns were found
    while len(blocks) < len(COLUMN_NAMES):
        blocks.append([])
    return blocks


def main():
    parser = argparse.ArgumentParser(description="Extract RMS columns from a CSV.")
    parser.add_argument("input", help="input CSV file")
    parser.add_argument("output", help="output CSV file")
    args = parser.parse_args()

    blocks = extract_blocks(first_column(args.input))
    n_rows = max(len(b) for b in blocks)

    with open(args.output, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(COLUMN_NAMES)
        for r in range(n_rows):
            writer.writerow([b[r] if r < len(b) else "" for b in blocks])

    print(f"Wrote {n_rows} rows to {args.output}")


if __name__ == "__main__":
    main()