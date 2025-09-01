from pathlib import Path
import subprocess
import platform
import datetime

RESULTS_DIR = Path("results")
GNU_FILE = RESULTS_DIR / "benchmark-gnu.csv"
MUSL_FILE = RESULTS_DIR / "benchmark-musl.csv"
OUT_MD = Path("README.md")


def load(path):
    """
    Returns dict: benchmark -> {operations,time_ns,ns_per_op,ops_per_sec}
    Accepts:
      1) Old semicolon format (possibly prefixed with '1,')
      2) New comma CSV:
         - Header: benchmark,operations,time_ns,ns_per_op[,ops_per_sec]
         - Data lines may omit ops_per_sec (will be computed)
    """
    data = {}
    if not path.exists():
        return data

    with path.open("r", encoding="utf-8") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    if not lines:
        return data

    header_line = lines[0]

    # Detect delimiter (prefer ; if present, else ,)
    if ";" in header_line and header_line.count(";") >= header_line.count(","):
        delim = ";"
    else:
        delim = ","

    # Old odd prefix "1," before header
    if header_line.startswith("1,") and delim == ";":
        header_line = header_line.split(",", 1)[1]

    header = [h.strip() for h in header_line.split(delim)]
    # Normalize expected names
    # Accept either 4 or 5 columns
    # Required minimal columns
    if "benchmark" not in header:
        raise ValueError("No 'benchmark' column in header")

    idx = {name: i for i, name in enumerate(header)}

    def get(parts, key, default=None):
        i = idx.get(key)
        if i is None or i >= len(parts):
            return default
        return parts[i].strip()

    def parse_number(s):
        if s is None or s == "":
            return None
        try:
            if "." in s or "e" in s or "E" in s:
                return float(s)
            return int(s)
        except ValueError:
            return None

    # Iterate data lines
    for line in lines[1:]:
        parts = [p.strip() for p in line.split(delim)]
        if len(parts) < 2:
            continue
        name = get(parts, "benchmark")
        if not name:
            continue
        operations = parse_number(get(parts, "operations"))
        time_ns = parse_number(get(parts, "time_ns"))
        ns_per_op = parse_number(get(parts, "ns_per_op"))
        ops_per_sec = parse_number(get(parts, "ops_per_sec"))

        # Derive missing metrics
        if ns_per_op is None and operations and time_ns and operations != 0:
            # time_ns / operations
            ns_per_op = time_ns / operations
        if ops_per_sec is None and ns_per_op and ns_per_op != 0:
            # 1e9 ns per second / ns_per_op
            ops_per_sec = 1e9 / ns_per_op
        if ops_per_sec is None and operations and time_ns and time_ns != 0:
            ops_per_sec = operations / (time_ns / 1e9)

        # Still missing? Skip.
        if ns_per_op is None or ops_per_sec is None:
            continue

        data[name] = {
            "operations": operations if operations is not None else 0,
            "time_ns": time_ns if time_ns is not None else 0,
            "ns_per_op": float(ns_per_op),
            "ops_per_sec": float(ops_per_sec),
        }
    return data


def percent_diff(a, b, lower_is_better=True):
    """
    Percent difference of b vs a.
    If lower_is_better:
        positive means b is faster (smaller) than a
    """
    if a == 0:
        return float("inf")
    if lower_is_better:
        return (a - b) / a * 100.0
    else:
        return (b - a) / a * 100.0


def main():
    gnu = load(GNU_FILE)
    musl = load(MUSL_FILE)

    all_benchmarks = sorted(set(gnu) | set(musl))

    def _get_compiler_version():
        zig_ver = subprocess.check_output(
            ["zig", "version"],
            stderr=subprocess.STDOUT,
            text=True,
            timeout=5,
        ).strip()

        cc_out = subprocess.check_output(
            ["zig", "cc", "--version"],
            stderr=subprocess.STDOUT,
            text=True,
            timeout=5,
        )
        cc_ver = cc_out.strip().splitlines()[0].split(" (")[0]

        return [zig_ver, cc_ver]

    def _get_kernel_version():
        try:
            u = platform.uname()
            return f"{u.system} {u.release} ({u.version})"
        except Exception:
            return "unknown"

    def _get_cpu_model():
        try:
            if platform.system() == "Linux":
                with open("/proc/cpuinfo", "r", encoding="utf-8") as f:
                    for line in f:
                        if line.lower().startswith("model name"):
                            return line.split(":", 1)[1].strip()
            elif platform.system() == "Darwin":
                sys_prof = subprocess.check_output(
                    ["sysctl", "-n", "machdep.cpu.brand_string"],
                    stderr=subprocess.STDOUT,
                    text=True,
                    timeout=5,
                ).strip()
                return sys_prof
            elif platform.system() == "Windows":
                return platform.processor()
        except Exception:
            pass
        return "unknown"

    lines = []

    # Optional header content from header.md
    header_file = Path("header.md")
    if header_file.exists():
        try:
            header_content = header_file.read_text(encoding="utf-8")
            # Preserve existing formatting, strip only trailing whitespace lines
            header_lines = header_content.splitlines()
            # Avoid leading BOM issues
            if header_lines and header_lines[0].startswith("\ufeff"):
                header_lines[0] = header_lines[0].lstrip("\ufeff")
            lines.extend(header_lines)
            if header_lines and header_lines[-1].strip() != "":
                lines.append("")  # ensure a blank line after header
        except Exception as exc:
            lines.append(f"(Failed to read header.md: {exc})")
            lines.append("")
    lines.append(
        "## Benchmark Results: glibc vs musl\n\n")

    compiler_version = _get_compiler_version()
    kernel_version = _get_kernel_version()
    lines.extend(
        [
            f"Compiler: zig {compiler_version[0]} using {compiler_version[1]}",
            f"Kernel: {kernel_version}",
            f"CPU: {_get_cpu_model()}",
            f"Date: {datetime.date.today().isoformat()}",
            ""
        ]
    )
    lines.append(
        "Each row compares musl against the glibc baseline (lower ns/op is better).")
    lines.append("")
    lines.append(
        "| Benchmark | glibc ns/op | musl ns/op | musl vs glibc | Winner |")
    lines.append(
        "|-----------|-------------|------------|---------------|--------|")

    glibc_faster = 0
    musl_faster = 0
    ties = 0

    total_time_glibc = 0
    total_time_musl = 0

    for name in all_benchmarks:
        g = gnu.get(name)
        m = musl.get(name)
        if not g or not m:
            continue  # only compare benchmarks present in both
        g_ns = g["ns_per_op"]
        m_ns = m["ns_per_op"]

        total_time_glibc += g.get("time_ns", 0)
        total_time_musl += m.get("time_ns", 0)

        # Define tie threshold (percent difference)
        TIE_THRESHOLD_PCT = 0.5  # musl within ±0.5% of glibc counts as tie

        if g_ns == 0 and m_ns == 0:
            winner = "tie"
            ties += 1
            rel_pct = 0.0
            rel_display = "0.00% (tie)"
        else:
            ratio = m_ns / g_ns
            # + => musl slower, - => musl faster
            rel_pct = (ratio - 1.0) * 100.0
            abs_rel_pct = abs(rel_pct)

            if abs_rel_pct <= TIE_THRESHOLD_PCT:
                winner = "tie"
                ties += 1
                rel_display = f"{rel_pct:+.2f}% (tie ≤{TIE_THRESHOLD_PCT:.2f}%)"
            elif rel_pct > 0:
                winner = "glibc"
                glibc_faster += 1
                rel_display = f"-{rel_pct:.2f}% slower"
            else:
                winner = "musl"
                musl_faster += 1
                rel_display = f"+{-rel_pct:.2f}% faster"

        lines.append(
            f"| {name} | {g_ns:.2f} | {m_ns:.2f} | {rel_display} | {winner} |")

    lines.append("")
    lines.append("### Summary\n")
    lines.append(f"- Benchmarks compared: {len(all_benchmarks)}")
    lines.append(f"- glibc faster (ns/op): {glibc_faster}")
    lines.append(f"- musl faster (ns/op): {musl_faster}")
    lines.append(f"- Ties (ns/op): {ties}")
    if glibc_faster > musl_faster:
        lines.append("Overall (by count): glibc wins more benchmarks.")
    elif musl_faster > glibc_faster:
        lines.append("Overall (by count): musl wins more benchmarks.")
    else:
        lines.append("Overall (by count): tie.")

    OUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_MD}")


if __name__ == "__main__":
    main()
