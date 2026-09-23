import os
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOW_FILE = ROOT / "scripts" / "dead-code-allow.txt"
CPPCHECK_BUILD_DIR = ROOT / "out" / "cppcheck"

CPP_SUFFIXES = (".cpp", ".cc", ".c", ".h", ".hpp")
FOREIGN_SUFFIXES = (".mm", ".m", ".swift", ".kt")
NON_PRODUCTION_DIRS = ("tests", "fuzz", "perf", "build", ".cxx")
ENTRY_POINTS = {"main", "wWinMain", "WinMain", "JNI_OnLoad", "JNI_OnUnload"}
CALL_KEYWORDS = {"return", "else", "case", "throw", "co_return", "co_await", "not", "if", "switch", "while"}
FFI_PREFIX = re.compile(r"\b(dhb?_[a-z0-9_]+)\s*\(")
IDENTIFIER_CALL = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
UNUSED_FUNCTION = re.compile(r"^(.*?):(\d+):unusedFunction:The function '([^']+)' is never used")
STRING_ID = re.compile(r"\b(DHStr[A-Za-z0-9]+)\s*=\s*(\d+)")
KOTLIN_STRING_ID = re.compile(r"const val (STR_[A-Z0-9_]+)\s*=\s*(\d+)")
KOTLIN_CONST = re.compile(r"\bconst val ([A-Za-z_][A-Za-z0-9_]*)\b")


def tracked_files():
    listing = subprocess.run(
        ["git", "ls-files", "core", "platform", "client"],
        cwd=ROOT, check=True, capture_output=True, text=True,
    ).stdout
    return [path for path in listing.splitlines() if path]


def is_production(path):
    parts = Path(path).parts
    return not any(part in NON_PRODUCTION_DIRS for part in parts)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def strip_comments_and_strings(text):
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"', " ", text, flags=re.S)


def strip_comments(text):
    return re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.S)


def cppcheck_cache_args():
    if not sys.platform.startswith("linux"):
        return []
    CPPCHECK_BUILD_DIR.mkdir(parents=True, exist_ok=True)
    return [f"-j{os.cpu_count() or 2}", f"--cppcheck-build-dir={CPPCHECK_BUILD_DIR}"]


def run_cppcheck(cppcheck, sources):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as listing:
        listing.write("\n".join(sources))
        listing_path = listing.name
    try:
        result = subprocess.run(
            [
                cppcheck,
                "--enable=unusedFunction",
                "--std=c++20",
                "--language=c++",
                "-Icore/include",
                "-Iplatform/include",
                "--quiet",
                *cppcheck_cache_args(),
                "--template={file}:{line}:{id}:{message}",
                f"--file-list={listing_path}",
            ],
            cwd=ROOT, capture_output=True, text=True,
        )
    finally:
        os.unlink(listing_path)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"dead-code: cppcheck exited with {result.returncode}")
    unused = {}
    for line in (result.stdout + result.stderr).splitlines():
        match = UNUSED_FUNCTION.match(line.strip())
        if match:
            unused.setdefault(match.group(3), f"{match.group(1).replace(os.sep, '/')}:{match.group(2)}")
    return unused


def called_from_foreign_code(files):
    names = set()
    for path in files:
        if path.endswith(FOREIGN_SUFFIXES):
            names.update(IDENTIFIER_CALL.findall(strip_comments_and_strings(read(path))))
    return names


def read_allowlist():
    if not ALLOW_FILE.exists():
        return set(), []
    allowed = set()
    problems = []
    lines = ALLOW_FILE.read_text(encoding="utf-8").splitlines()
    for line_no, line in enumerate(lines, 1):
        if not line.strip():
            continue
        name, _, reason = line.partition(":")
        if not reason.strip():
            problems.append(
                f"{ALLOW_FILE.relative_to(ROOT).as_posix()}:{line_no}: '{name.strip()}' has no reason - "
                "write it as '<name>: <which test needs it and what it proves>'"
            )
        allowed.add(name.strip())
    return allowed, problems


def check_cpp_functions(cppcheck, files, allowed):
    sources = [path for path in files if path.endswith(CPP_SUFFIXES)]
    unused = run_cppcheck(cppcheck, sources)
    foreign = called_from_foreign_code(files)
    problems = []
    for name, where in sorted(unused.items()):
        if name in ENTRY_POINTS or name.startswith("Java_") or name in foreign or name in allowed:
            continue
        problems.append(f"{where}: '{name}' is never called by production code")
    stale = sorted(name for name in allowed if name not in unused)
    for name in stale:
        problems.append(
            f"{ALLOW_FILE.relative_to(ROOT).as_posix()}: '{name}' is allowed to be test-only "
            "but production calls it now (or it is gone) - drop it from the list"
        )
    return problems


def is_call_site(line, start):
    prefix = line[:start].strip()
    if not prefix:
        return True
    if any(ch in prefix for ch in "=(),?:!<>&|+-[{"):
        return True
    return prefix.split()[-1] in CALL_KEYWORDS


def check_ffi_functions(files):
    declared = {}
    uses = defaultdict(int)
    for path in files:
        if not path.endswith(CPP_SUFFIXES + FOREIGN_SUFFIXES) or path.endswith("-Bridging-Header.h"):
            continue
        for line_no, line in enumerate(strip_comments_and_strings(read(path)).splitlines(), 1):
            for match in FFI_PREFIX.finditer(line):
                name = match.group(1)
                if path.endswith(FOREIGN_SUFFIXES) or is_call_site(line, match.start()):
                    uses[name] += 1
                elif path.endswith(".h") and line.rstrip().endswith(";"):
                    declared.setdefault(name, f"{path}:{line_no}")
    return [
        f"{where}: FFI function '{name}' is declared but no app calls it"
        for name, where in sorted(declared.items())
        if uses[name] == 0
    ]


def check_string_ids(files):
    header = "platform/include/deskhubp/ffi/ClientFfi.h"
    ids = STRING_ID.findall(strip_comments_and_strings(read(header)))
    swift = " ".join(read(path) for path in files if path.endswith(".swift"))
    kotlin_files = [path for path in files if path.endswith(".kt")]
    kotlin = " ".join(strip_comments(read(path)) for path in kotlin_files)
    kotlin_by_value = {int(value): name for name, value in KOTLIN_STRING_ID.findall(kotlin)}
    problems = []
    for name, value in ids:
        if re.search(rf"\b{name}\b", swift):
            continue
        kotlin_name = kotlin_by_value.get(int(value))
        if kotlin_name and len(re.findall(rf"\b{kotlin_name}\b", kotlin)) > 1:
            continue
        problems.append(f"{header}: string id '{name}' ({value}) is shown by neither the Apple nor the Android app")
    return problems


def check_kotlin_constants(files):
    kotlin_files = [path for path in files if path.endswith(".kt")]
    texts = {path: strip_comments(read(path)) for path in kotlin_files}
    combined = " ".join(texts.values())
    problems = []
    for path, text in texts.items():
        for match in KOTLIN_CONST.finditer(text):
            name = match.group(1)
            if len(re.findall(rf"\b{name}\b", combined)) < 2:
                line_no = text.count("\n", 0, match.start()) + 1
                problems.append(f"{path}:{line_no}: Kotlin constant '{name}' is never read")
    return problems


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: dead-code.py <path-to-cppcheck>")
    files = [path for path in tracked_files() if is_production(path)]
    allowed, problems = read_allowlist()
    problems += check_cpp_functions(sys.argv[1], files, allowed)
    problems += check_ffi_functions(files)
    problems += check_string_ids(files)
    problems += check_kotlin_constants(files)
    if problems:
        print("\n".join(problems))
        print(
            f"\ndead-code: {len(problems)} finding(s). Delete what nothing calls. A function that "
            "only tests call counts as dead too; keep one only when a test genuinely cannot "
            f"observe the behaviour any other way, and then add '<name>: <which test needs it and "
            f"what it proves>' to {ALLOW_FILE.relative_to(ROOT).as_posix()}."
        )
        raise SystemExit(1)
    print("  OK")


if __name__ == "__main__":
    main()
