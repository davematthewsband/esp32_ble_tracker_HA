"""Run pure resolver/presence tests on macOS using Apple's AES backend."""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="esp32-presence-tests-") as directory:
    binary = str(Path(directory) / "test_presence")
    subprocess.run(
        [
            "clang++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(root / "src"),
            str(root / "src/iphone_presence.cpp"),
            str(root / "tests/test_iphone_presence.cpp"),
            "-o", binary,
        ],
        check=True,
    )
    subprocess.run([binary], check=True)
