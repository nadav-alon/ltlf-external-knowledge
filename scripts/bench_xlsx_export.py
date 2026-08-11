#!/usr/bin/env python3
"""Spreadsheet export for ltlf-ek-bench (docs/prd/benchmark-suite.md B6).

Shelled out to by src/ltlf_ek_bench.cpp (never imported): reads the runner's
committed JSON report and writes a 5-sheet workbook -- summary, timings,
structural, ltlfsynt, provenance -- via openpyxl. If openpyxl is absent this
falls back to five CSV files (stdlib csv, always available) rather than
losing the sweep, and says so loudly (PRD B6: "the measurements are the
expensive part and must never be lost to a formatting dependency").

Usage:
    python3 scripts/bench_xlsx_export.py <report.json> <out.xlsx>

Exit codes: 0 = wrote the xlsx workbook. 2 = openpyxl unavailable, wrote CSV
fallback instead (still a success from the sweep-preservation standpoint --
the caller prints its own loud warning on this code). 1 = fatal error (bad
JSON, unwritable output path); the caller's --out JSON is untouched either
way, so no measurement is ever lost even on exit 1.
"""

import csv
import json
import os
import sys

# One (sheet name, row-source key, column order) triple per B6 sheet. Column
# order is fixed here (not "whatever keys happen to be in the dict") so a
# reader can rely on it across runs.
SHEET_COLUMNS = {
    "summary": [
        "family", "subject", "tier", "largest_n_completed", "best_time_ns",
        "construction_ns", "speedup_vs_mtdfa_product", "product_states",
        "product_mtdfa_roots",
    ],
    "timings": ["family", "n", "realizable", "subject", "stage", "ns", "timed_out"],
    "structural": ["family", "n", "realizable", "subject", "metric", "value"],
    "ltlfsynt": [
        "family", "n", "realizable", "tier", "psi_in", "status",
        "ek_verdict", "ltlfsynt_verdict", "ltlfsynt_ns", "verdict_mismatch",
    ],
}


def cell(value):
    """None -> "" for CSV/xlsx display; everything else passes through."""
    return "" if value is None else value


def load_report(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def provenance_rows(report):
    prov = report.get("provenance", {})
    rows = []
    for key in sorted(prov.keys()):
        value = prov[key]
        if isinstance(value, (list, dict)):
            value = json.dumps(value, sort_keys=True)
        rows.append((key, value))
    return rows


def write_csv_fallback(report, out_xlsx_path):
    stem, _ = os.path.splitext(out_xlsx_path)
    written = []
    for sheet, columns in SHEET_COLUMNS.items():
        path = f"{stem}_{sheet}.csv"
        with open(path, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(columns)
            for row in report.get(sheet, []):
                w.writerow([cell(row.get(c)) for c in columns])
        written.append(path)
    prov_path = f"{stem}_provenance.csv"
    with open(prov_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["key", "value"])
        for k, v in provenance_rows(report):
            w.writerow([k, v])
    written.append(prov_path)
    return written


def write_xlsx(report, out_xlsx_path):
    import openpyxl  # noqa: E402  (deferred: this is the fallback boundary)

    wb = openpyxl.Workbook()
    # First auto-created sheet is renamed to the first real sheet rather than
    # left as a stray empty "Sheet".
    sheet_names = list(SHEET_COLUMNS.keys()) + ["provenance"]
    first = True
    for sheet in sheet_names:
        ws = wb.active if first else wb.create_sheet()
        ws.title = sheet
        first = False
        if sheet == "provenance":
            ws.append(["key", "value"])
            for k, v in provenance_rows(report):
                ws.append([k, v])
            continue
        columns = SHEET_COLUMNS[sheet]
        ws.append(columns)
        for row in report.get(sheet, []):
            ws.append([cell(row.get(c)) for c in columns])
    wb.save(out_xlsx_path)


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} <report.json> <out.xlsx>", file=sys.stderr)
        return 1
    report_path, out_xlsx_path = argv[1], argv[2]
    try:
        report = load_report(report_path)
    except Exception as e:  # noqa: BLE001 -- fatal, report and exit 1
        print(f"bench_xlsx_export: could not read {report_path}: {e}",
              file=sys.stderr)
        return 1

    try:
        write_xlsx(report, out_xlsx_path)
    except ImportError:
        written = write_csv_fallback(report, out_xlsx_path)
        print(
            "bench_xlsx_export: FALLBACK -- openpyxl is not importable from "
            "this python3, so the workbook was NOT written. Wrote CSV "
            "instead so the sweep is not lost:\n  "
            + "\n  ".join(written),
            file=sys.stderr,
        )
        return 2
    except Exception as e:  # noqa: BLE001 -- fatal at the write step
        print(f"bench_xlsx_export: could not write {out_xlsx_path}: {e}",
              file=sys.stderr)
        return 1

    print(f"bench_xlsx_export: wrote {out_xlsx_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
