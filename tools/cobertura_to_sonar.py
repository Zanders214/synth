#!/usr/bin/env python3
"""Convert an OpenCppCoverage Cobertura report into SonarQube generic coverage.

OpenCppCoverage can emit Cobertura but not SonarQube's generic format, which is
what `sonar.coverageReportPaths` consumes. This does the small translation and
normalises each source path to the repo-relative `src/...` form SonarCloud
indexes (the Cobertura filenames are absolute compile paths).

Usage: python cobertura_to_sonar.py <cobertura.xml> <sonar-coverage.xml>
"""
import sys
import xml.etree.ElementTree as ET


def normalise(filename: str):
    """Return a 'src/...'-relative path, or None to skip (non-project file)."""
    p = filename.replace("\\", "/")
    lower = p.lower()
    idx = lower.rfind("/src/")
    if idx >= 0:
        return p[idx + 1:]          # drop everything before 'src/'
    if lower.startswith("src/"):
        return p
    return None                     # JUCE / system / test sources: not analysed


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: cobertura_to_sonar.py <in-cobertura.xml> <out-sonar.xml>")
        return 2

    src_in, dst_out = sys.argv[1], sys.argv[2]
    root = ET.parse(src_in).getroot()

    # path -> { line_number: covered_bool }, merged across class entries.
    files: dict[str, dict[int, bool]] = {}
    for cls in root.iter("class"):
        path = normalise(cls.get("filename") or "")
        if path is None:
            continue
        per_line = files.setdefault(path, {})
        lines = cls.find("lines")
        if lines is None:
            continue
        for ln in lines.findall("line"):
            num = ln.get("number")
            if num is None:
                continue
            hits = int(ln.get("hits", "0"))
            n = int(num)
            per_line[n] = per_line.get(n, False) or (hits > 0)

    out = ['<coverage version="1">']
    for path in sorted(files):
        out.append(f'  <file path="{path}">')
        for n in sorted(files[path]):
            covered = "true" if files[path][n] else "false"
            out.append(f'    <lineToCover lineNumber="{n}" covered="{covered}"/>')
        out.append("  </file>")
    out.append("</coverage>")

    with open(dst_out, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")

    total = sum(len(v) for v in files.values())
    covered = sum(1 for v in files.values() for c in v.values() if c)
    pct = (100.0 * covered / total) if total else 0.0
    print(f"wrote {dst_out}: {len(files)} files, {covered}/{total} lines covered ({pct:.1f}%)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
