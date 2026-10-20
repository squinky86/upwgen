"""Exercise the public executable contract without assuming random outputs."""
import json
from pathlib import Path
import subprocess
import sys
import unicodedata

binary = str(Path(sys.argv[1]).resolve())


def run(*args, success=True):
    result = subprocess.run(
        [binary, *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        encoding="utf-8", errors="strict", timeout=30, check=False,
    )
    if success:
        assert result.returncode == 0, (args, result.stderr)
        assert not result.stderr, (args, result.stderr)
    else:
        assert result.returncode != 0, args
        assert result.stderr.strip(), args
        assert not result.stdout, (args, "failure unexpectedly emitted output")
    return result


default = run().stdout.removesuffix("\n")
assert len(default) == 24
assert all(0x21 <= ord(c) <= 0x7E for c in default)

for profile in ("portable", "unicode", "unspeakable"):
    for length in (1, 9, 24, 65):
        records = [
            json.loads(line) for line in
            run("--profile", profile, "--length", str(length), "--count", "4", "--format", "jsonl").stdout.splitlines()
        ]
        assert len(records) == 4
        for index, record in enumerate(records, 1):
            value = record["value"]
            assert record["index"] == index
            assert len(value) == record["code_points"] == length
            assert len(value.encode("utf-8")) == record["utf8_bytes"]
            assert record["mode"] == "generate"
            assert record["unicode_version"] == "16.0.0"
            assert [f"U+{ord(c):04X}" for c in value] == record["codepoint_sequence"]
            if profile == "unicode":
                assert value == unicodedata.normalize("NFC", value)
            if profile == "portable":
                assert value.isascii()

raw = run("--include", "upper,lower,digits", "--require", "upper,lower,digits",
          "--length", "24", "--count", "20", "--max-bytes", "24").stdout.splitlines()
assert len(raw) == 20
for value in raw:
    assert len(value) == 24
    assert value.isalnum() and value.isascii()
    assert any(c.isupper() for c in value)
    assert any(c.islower() for c in value)
    assert any(c.isdigit() for c in value)

unicode_bytes = run("--profile", "unspeakable", "--include", "emoji",
                    "--length", "3", "--max-bytes", "12").stdout.removesuffix("\n")
assert len(unicode_bytes) == 3 and len(unicode_bytes.encode("utf-8")) == 12
assert all("a" <= c <= "z" for c in run("-scd", "8", "2").stdout.replace("\n", ""))

corpus_args = ("corpus", "--seed", "18446744073709551615", "--count", "8")
assert run(*corpus_args).stdout == run(*corpus_args).stdout
assert all(json.loads(line)["mode"] == "corpus" for line in run(*corpus_args).stdout.splitlines())

for args in (
    ("--length", "0"), ("--length", "-1"), ("--length", "1junk"),
    ("--length", "18446744073709551616"), ("--length", "4097"),
    ("--count", "0"), ("--count", "-1"), ("--count", "10001"),
    ("--length", "4096", "--count", "10000"), ("--unknown",),
    ("--seed", "42"), ("generate", "--seed", "42"),
    ("--max-bytes", "0"), ("--profile", "invalid"),
    ("--include", "tags"), ("--include", "lower,,upper"),
    ("--require", "tags"), ("--length", "1", "--require", "upper,lower"),
    ("--profile", "unicode", "--include", "tags"),
    ("--profile", "unspeakable", "--include", "emoji", "--length", "3", "--max-bytes", "11"),
    ("-scldnexgmo",), ("--length", "20", "--length", "24"),
):
    run(*args, success=False)

assert "0.1.0" in run("--version").stdout
assert "Usage:" in run("--help").stdout
assert "historic" in run("sets").stdout
print("CLI integration: defaults, profiles, metadata, constraints, corpus, and invalid inputs passed")
