"""Compile the enabled HTTPS path using throwaway credentials, never upload.

The normal checkout may intentionally have incomplete private configuration.
This fixture ensures compile-time constants cannot eliminate the HTTPS path
and hide link/resource failures. Output is restricted to build diagnostics.
"""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("setup", root / "tools/setup_network.py")
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)
pio = shutil.which("pio") or str(Path.home() / ".platformio/penv/bin/platformio")
with tempfile.TemporaryDirectory(prefix="heltec-network-build-") as temp:
    project = Path(temp)
    shutil.copy(root / "platformio.ini", project / "platformio.ini")
    shutil.copytree(root / "src", project / "src")
    shutil.copytree(root / "web", project / "web")
    setup.prepare(project, "fixture-network", "fixture-password-not-real")
    # Reuse already installed libraries; no private IRK/config is copied.
    (project / ".pio").mkdir()
    (project / ".pio/libdeps").symlink_to(root / ".pio/libdeps", target_is_directory=True)
    environment = dict(os.environ, PLATFORMIO_CORE_DIR=str(root / ".pio-core"))
    result = subprocess.run([pio, "run", "--project-dir", str(project)], env=environment,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for line in result.stdout.splitlines():
        if any(marker in line for marker in ("error:", "Error", "undefined reference", "RAM:", "Flash:", "SUCCESS", "FAILED")):
            print(line)
    if result.returncode:
        print("Enabled-network fixture build failed. No board was accessed.")
        raise SystemExit(result.returncode)
    nm = root / ".pio-core/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm"
    symbols = subprocess.check_output([str(nm), str(project / ".pio/build/heltec_v3/firmware.elf")], text=True)
    assert "httpd_ssl_start" in symbols, "HTTPS path was not linked"
    print("PASS: enabled Wi-Fi/HTTPS fixture compiled and linked, with throwaway credentials")
