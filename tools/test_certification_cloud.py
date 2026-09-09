#!/usr/bin/env python3
"""
Pruebas automatizadas de la API Cloud y del motor de auditoría de certificación.
"""

import os
import sys
import io
import json
import zlib
import tarfile
import hashlib
import unittest

# Agregar directorios al path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "ABDSharedCode", "Certification")))
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "server")))

from CertificationAuditor import CertificationAuditor

class TestCertificationAuditor(unittest.TestCase):
    def setUp(self):
        self.auditor = CertificationAuditor()

    def create_mock_bundle(self, hw_id="roland_aira_bitrazer", snr=88.5, include_header=True, include_html=True):
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w:gz") as tar:
            # 1. Manifest
            manifest_data = {
                "schema_version": "2.0",
                "timestamp": "2026-09-06T00:00:00Z",
                "sample_rate": 96000,
                "hardware": {
                    "id": hw_id,
                    "name": "Roland AIRA Bitrazer",
                    "device_type": "AUTOMATED_SYSEX"
                },
                "measured_points": [
                    {
                        "point_index": i,
                        "parameters": [{"name": "Cutoff", "normalized_value": i / 16.0}],
                        "metrics": {"snr_db": snr, "thd_percent": 0.02}
                    }
                    for i in range(16)
                ]
            }
            manifest_bytes = json.dumps(manifest_data).encode("utf-8")
            ti = tarfile.TarInfo(name=f"{hw_id}_manifest.json")
            ti.size = len(manifest_bytes)
            tar.addfile(ti, io.BytesIO(manifest_bytes))

            # 2. Header
            if include_header:
                header_str = """#pragma once
#include <cstdint>
struct alignas(16) AbdBatchedPoint { float p1, p2, mu, sigma, sec_mu, sec_sigma, thd, pad; };
static const AbdBatchedPoint test_table[16] = {};
"""
                header_bytes = header_str.encode("utf-8")
                ti_h = tarfile.TarInfo(name=f"{hw_id}_lut.h")
                ti_h.size = len(header_bytes)
                tar.addfile(ti_h, io.BytesIO(header_bytes))

            # 3. HTML
            if include_html:
                html_bytes = b"<!DOCTYPE html><html><body><h1>Certification Report</h1></body></html>"
                ti_html = tarfile.TarInfo(name=f"{hw_id}_certification_report.html")
                ti_html.size = len(html_bytes)
                tar.addfile(ti_html, io.BytesIO(html_bytes))

        return buf.getvalue()

    def test_valid_bundle_approval(self):
        bundle = self.create_mock_bundle(snr=92.0)
        res = self.auditor.audit_package_bytes(bundle)
        self.assertEqual(res["status"], "APPROVED")
        self.assertEqual(res["tier"], "CERTIFIED_GOLD")
        self.assertEqual(res["hardware_id"], "roland_aira_bitrazer")
        self.assertEqual(res["measured_points_count"], 16)
        self.assertGreaterEqual(res["average_snr_db"], 90.0)

    def test_missing_header_rejection(self):
        bundle = self.create_mock_bundle(include_header=False)
        res = self.auditor.audit_package_bytes(bundle)
        self.assertEqual(res["status"], "REJECTED")
        self.assertTrue(any("Cabecera C++" in e for e in res["errors"]))

    def test_low_snr_rejection(self):
        bundle = self.create_mock_bundle(snr=25.0)
        res = self.auditor.audit_package_bytes(bundle)
        self.assertEqual(res["status"], "REJECTED")
        self.assertTrue(any("SNR" in e and "insuficiente" in e.lower() for e in res["errors"]))

    def test_corrupted_gzip_handling(self):
        res = self.auditor.audit_package_bytes(b"NOT_A_GZIP_STREAM")
        self.assertEqual(res["status"], "REJECTED")
        self.assertEqual(res["tier"], "INVALID")

import threading
import urllib.request
from http.server import HTTPServer
from cloud_certification_server import CertificationCloudHandler

class TestCertificationCloudServer(unittest.TestCase):
    server = None
    server_thread = None
    port = 8888

    @classmethod
    def setUpClass(cls):
        cls.server = HTTPServer(("127.0.0.1", cls.port), CertificationCloudHandler)
        cls.server_thread = threading.Thread(target=cls.server.serve_forever)
        cls.server_thread.daemon = True
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        if cls.server:
            cls.server.shutdown()
            cls.server.server_close()

    def test_server_health(self):
        req = urllib.request.Request(f"http://127.0.0.1:{self.port}/api/v1/health")
        with urllib.request.urlopen(req) as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode("utf-8"))
            self.assertEqual(data["status"], "UP")

    def test_e2e_upload_and_catalog(self):
        # 1. Create a bundle
        auditor_test = TestCertificationAuditor()
        bundle_bytes = auditor_test.create_mock_bundle(hw_id="behringer_pro800", snr=89.0)

        # 2. Upload via POST
        req = urllib.request.Request(
            f"http://127.0.0.1:{self.port}/api/v1/certifications/upload",
            data=bundle_bytes,
            headers={"Content-Type": "application/gzip", "Content-Length": str(len(bundle_bytes))},
            method="POST"
        )
        with urllib.request.urlopen(req) as resp:
            self.assertEqual(resp.status, 201)
            res_data = json.loads(resp.read().decode("utf-8"))
            self.assertEqual(res_data["status"], "APPROVED")
            cert_id = res_data["certification"]["certification_id"]

        # 3. Query catalog
        cat_req = urllib.request.Request(f"http://127.0.0.1:{self.port}/api/v1/certifications?manufacturer=pro800")
        with urllib.request.urlopen(cat_req) as resp:
            self.assertEqual(resp.status, 200)
            cat_data = json.loads(resp.read().decode("utf-8"))
            self.assertGreaterEqual(cat_data["total"], 1)

        # 4. Fetch HTML report
        rep_req = urllib.request.Request(f"http://127.0.0.1:{self.port}/api/v1/certifications/{cert_id}/report")
        with urllib.request.urlopen(rep_req) as resp:
            self.assertEqual(resp.status, 200)
            self.assertIn(b"Certification Report", resp.read())

if __name__ == "__main__":
    unittest.main()
