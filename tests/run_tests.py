"""Run pure resolver/presence tests on macOS using Apple's AES backend."""

from pathlib import Path
import hashlib
import subprocess
import tempfile
import urllib.request

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
    dashboard_binary = str(Path(directory) / "test_dashboard")
    subprocess.run(
        ["clang++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
         "-I", str(root / "src"), str(root / "src/dashboard_protocol.cpp"),
         str(root / "tests/test_dashboard_protocol.cpp"), "-o", dashboard_binary],
        check=True,
    )
    subprocess.run([dashboard_binary], check=True)
    cjson = root / ".pio-core/host-tests/cjson"
    cjson.mkdir(parents=True, exist_ok=True)
    for filename, checksum in {
        "cJSON.c": "298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d",
        "cJSON.h": "25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee",
    }.items():
        source = cjson / filename
        if not source.exists():
            source.write_bytes(urllib.request.urlopen(
                f"https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.19/{filename}", timeout=20
            ).read())
        if hashlib.sha256(source.read_bytes()).hexdigest() != checksum:
            raise RuntimeError("Host cJSON dependency checksum mismatch")
    cjson_object = str(Path(directory) / "cjson.o")
    subprocess.run(["clang", "-std=c11", "-Wno-deprecated-declarations", "-fsanitize=address,undefined", "-I", str(cjson),
                    "-c", str(cjson / "cJSON.c"), "-o", cjson_object], check=True)
    settings_binary = str(Path(directory) / "test_settings")
    subprocess.run(["clang++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(root / "src"),
                    "-I", str(cjson), str(root / "src/device_settings.cpp"),
                    str(root / "src/dashboard_protocol.cpp"), str(root / "tests/test_device_settings.cpp"),
                    cjson_object, "-o", settings_binary], check=True)
    subprocess.run([settings_binary], check=True)
    button_binary = str(Path(directory) / "test_button")
    subprocess.run(["clang++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(root / "src"),
                    str(root / "tests/test_page_button.cpp"), "-o", button_binary], check=True)
    subprocess.run([button_binary], check=True)
    wifi_binary = str(Path(directory) / "test_wifi")
    subprocess.run(["clang++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(root / "src"),
                    str(root / "tests/test_wifi_diagnostics.cpp"), "-o", wifi_binary], check=True)
    subprocess.run([wifi_binary], check=True)
