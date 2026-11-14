"""Exercise private configuration and real OpenSSL output in temp directories."""

import importlib.util
import json
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("setup_network", root / "tools/setup_network.py")
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class NetworkSetupTests(unittest.TestCase):
    def test_private_setup_preserves_keys_and_login(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            irk = project / "include/config.local.h"
            irk.parent.mkdir()
            irk.write_text("// Existing IRK config, untouched by network setup\n")
            config, cert = setup.prepare(project, 'Test "SSID"', 'test-password')
            first = setup.read_settings(config)
            key = (project / ".local-network/server.key").read_bytes()
            setup.prepare(project, None)
            self.assertEqual(first, setup.read_settings(config))
            self.assertEqual(key, (project / ".local-network/server.key").read_bytes())
            self.assertEqual(irk.read_text(), "// Existing IRK config, untouched by network setup\n")
            self.assertGreaterEqual(len(first["DASHBOARD_PASSWORD"]), 24)
            self.assertEqual(stat.S_IMODE(config.stat().st_mode), 0o600)
            self.assertEqual(stat.S_IMODE((project / ".local-network").stat().st_mode), 0o700)
            subprocess.run(["openssl", "verify", "-trusted", str(cert),
                            "-verify_hostname", "ble-tracker.local", str(cert)],
                           check=True, capture_output=True)
            # A hostname change regenerates TLS but preserves the dashboard login.
            setup.prepare(project, None, hostname="bedroom", ip="192.0.2.10")
            self.assertNotEqual(key, (project / ".local-network/server.key").read_bytes())
            self.assertEqual(first["DASHBOARD_PASSWORD"], setup.read_settings(config)["DASHBOARD_PASSWORD"])
            subprocess.run(["openssl", "verify", "-trusted", str(cert),
                            "-verify_ip", "192.0.2.10", str(cert)], check=True, capture_output=True)
            self.assertEqual(json.loads((project / ".local-network/certificate.json").read_text())["hostname"], "bedroom")
            key_before_renew = (project / ".local-network/server.key").read_bytes()
            setup.prepare(project, None, hostname="bedroom", ip="192.0.2.10", renew=True)
            self.assertNotEqual(key_before_renew, (project / ".local-network/server.key").read_bytes())
            self.assertEqual(first["DASHBOARD_PASSWORD"], setup.read_settings(config)["DASHBOARD_PASSWORD"])

    def test_rejects_bad_identity_before_creating_files(self):
        for hostname in ["bad.local", "-bad", "bad\nCN=evil", "BAD", "a" * 64]:
            with self.assertRaises(ValueError): setup.valid_hostname(hostname)
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            for ssid, password in [("", "password"), ("a" * 33, "password"),
                                   ("name\n", "password"), ("test", "short")]:
                with self.assertRaises(ValueError): setup.prepare(project, ssid, password)
            self.assertEqual(list(project.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
