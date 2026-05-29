#!/usr/bin/env python3
from pathlib import Path
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
PDF_ROOT = REPO_ROOT / "hardware"
BAD_TOKENS = ("break_temp", "sch_break", "break temp")


def iter_pdfs():
    if not PDF_ROOT.exists():
        return []
    return sorted(PDF_ROOT.rglob("*.pdf"))


def read_pdf_title(path: Path):
    try:
        from pypdf import PdfReader
    except Exception as exc:
        raise RuntimeError("Missing pypdf. Install with: python -m pip install pypdf") from exc

    reader = PdfReader(str(path))
    metadata = reader.metadata or {}
    title = metadata.get("/Title") or metadata.get("Title") or ""
    return str(title)


def main() -> int:
    failures = []
    pdfs = iter_pdfs()

    for pdf in pdfs:
        rel = pdf.relative_to(REPO_ROOT).as_posix()
        rel_lower = rel.lower()

        for token in BAD_TOKENS:
            if token in rel_lower:
                failures.append(f"{rel}: filename/path contains '{token}'")

        try:
            title = read_pdf_title(pdf)
        except Exception as exc:
            failures.append(f"{rel}: cannot read PDF metadata: {exc}")
            continue

        title_lower = title.lower()
        for token in BAD_TOKENS:
            if token in title_lower:
                failures.append(f"{rel}: /Title contains '{token}' -> {title!r}")

    if failures:
        print("PDF metadata check failed:")
        for item in failures:
            print(f"- {item}")
        return 1

    print(f"PDF metadata check passed: {len(pdfs)} PDF file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
