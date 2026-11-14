"""Prepare private Wi-Fi/dashboard config and a local HTTPS certificate.

Use --prepare to leave the Wi-Fi password blank for editing in the local file.
Otherwise the password prompt never echoes input. Existing credentials and
certificate are preserved unless the hostname changes (which needs new TLS).
"""

import argparse
import getpass
import ipaddress
import json
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import tempfile


def valid_hostname(value):
    if not re.fullmatch(r"[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?", value):
        raise ValueError("Hostname must be a lowercase DNS label, at most 63 characters")
    return value


def read_settings(path):
    if not path.exists():
        return {}
    result = {}
    for name, literal in re.findall(r'^#define (\w+) (".*")$', path.read_text(), re.M):
        result[name] = json.loads(literal)
    return result


def private_write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    # Write with private permissions from creation, then replace atomically.
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as file:
        temp = Path(file.name)
        try:
            file.write(content)
            file.flush()
            temp.chmod(0o600)
            temp.replace(path)
        finally:
            temp.unlink(missing_ok=True)


def prepare(root, ssid, password=None, hostname=None, ip=None, renew=False):
    config = root / "include/config.local.network.h"
    previous = read_settings(config)
    hostname = valid_hostname(hostname or previous.get("TRACKER_HOSTNAME", "ble-tracker"))
    ssid = ssid or previous.get("WIFI_SSID", "")
    password = previous.get("WIFI_PASSWORD", "") if password is None else password
    if not 1 <= len(ssid.encode()) <= 32 or any(ord(c) < 32 for c in ssid):
        raise ValueError("SSID must contain 1–32 UTF-8 bytes without control characters")
    if password and (not 8 <= len(password.encode()) <= 63 or
                     any(ord(c) < 32 for c in password)):
        raise ValueError("WPA personal password must contain 8–63 bytes without control characters")
    if ip:
        ip = str(ipaddress.IPv4Address(ip))
    directory = root / ".local-network"
    directory.mkdir(mode=0o700, exist_ok=True)
    directory.chmod(0o700)
    cert = directory / "server.crt"
    key = directory / "server.key"
    manifest = directory / "certificate.json"
    identity = {"hostname": hostname, "ip": ip}
    if renew or not (cert.exists() and key.exists() and manifest.exists() and
            json.loads(manifest.read_text()) == identity):
        openssl = shutil.which("openssl")
        if not openssl:
            raise RuntimeError("OpenSSL is required to generate the certificate")
        with tempfile.TemporaryDirectory(dir=directory) as work:
            work = Path(work)
            san = f"DNS:{hostname}.local,DNS:{hostname}"
            if ip:
                san += f",IP:{ip}"
            (work / "openssl.cnf").write_text(
                f"[req]\ndistinguished_name=dn\nx509_extensions=server\nprompt=no\n"
                f"[dn]\nCN={hostname}.local\n[server]\nsubjectAltName={san}\n"
                "basicConstraints=critical,CA:FALSE\n"
                "keyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n"
            )
            result = subprocess.run(
                [openssl, "req", "-x509", "-newkey", "ec", "-pkeyopt",
                 "ec_paramgen_curve:prime256v1", "-sha256", "-nodes", "-days", "365",
                 "-config", str(work / "openssl.cnf"),
                 "-keyout", str(work / "server.key"), "-out", str(work / "server.crt")],
                capture_output=True,
            )
            if result.returncode:
                raise RuntimeError("OpenSSL certificate generation failed")
            private_write(key, (work / "server.key").read_text())
            private_write(cert, (work / "server.crt").read_text())
        private_write(manifest, json.dumps(identity) + "\n")
    settings = {
        "WIFI_SSID": ssid, "WIFI_PASSWORD": password, "TRACKER_HOSTNAME": hostname,
        "DASHBOARD_USERNAME": previous.get("DASHBOARD_USERNAME", "admin"),
        "DASHBOARD_PASSWORD": previous.get("DASHBOARD_PASSWORD") or secrets.token_urlsafe(24),
    }
    private_write(config, "#pragma once\n\n// Private installation settings. Do not commit or share.\n" +
                  "".join(f"#define {name} {json.dumps(value, ensure_ascii=False)}\n"
                          for name, value in settings.items()))
    private_write(root / "include/config.local.tls.h",
                  '#pragma once\nstatic const char kTlsCertificate[] = R"PEM(' +
                  cert.read_text() + ')PEM";\nstatic const char kTlsPrivateKey[] = R"PEM(' +
                  key.read_text() + ')PEM";\n')
    return config, cert


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ssid")
    parser.add_argument("--hostname")
    parser.add_argument("--ip", help="Optional reserved IPv4 address for certificate SAN")
    parser.add_argument("--prepare", action="store_true", help="Do not prompt for Wi-Fi password")
    parser.add_argument("--renew", action="store_true", help="Replace TLS key/certificate; keep credentials")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    config = root / "include/config.local.network.h"
    for path in (config, root / "include/config.local.tls.h", root / ".local-network/server.key"):
        if subprocess.run(["git", "check-ignore", "-q", str(path)], cwd=root).returncode:
            parser.error("Private output paths must be ignored by Git before setup")
    password = None if args.prepare else getpass.getpass("IoT Wi-Fi password (hidden): ")
    try:
        config, cert = prepare(root, args.ssid, password, args.hostname, args.ip, args.renew)
    except (ValueError, RuntimeError) as error:
        parser.error(str(error))
    print(f"Private configuration ready: {config}")
    print(f"Server certificate: {cert}")
    print("Read the generated dashboard username/password in the private config file.")
    print("Wi-Fi requires a filled WIFI_PASSWORD and a dashboard password of at least 24 characters.")
    print("Rebuild after changing configuration or renewing the certificate.")


if __name__ == "__main__":
    main()
