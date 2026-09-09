#!/usr/bin/env python3
"""
ABDAudioLab — Servidor Central / API Cloud para Certificación y Preservación Analógica
Recibe paquetes .tar.gz, audita su integridad criptográfica y acústica con JSON Schema
y publica los modelos certificados en el catálogo comunitario.
"""

import os
import sys
import json
import time
import tarfile
import hashlib
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from typing import Dict, Any

# Agregar ABDSharedCode al path para importar el auditor
shared_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "ABDSharedCode", "Certification"))
if shared_dir not in sys.path:
    sys.path.insert(0, shared_dir)

try:
    from CertificationAuditor import CertificationAuditor
except ImportError:
    # Fallback si se ejecuta desde otra raíz
    local_auditor = os.path.abspath(os.path.join(os.path.dirname(__file__), "CertificationAuditor.py"))
    if os.path.exists(local_auditor):
        from CertificationAuditor import CertificationAuditor
    else:
        raise

STORAGE_DIR = os.path.join(os.path.dirname(__file__), "cloud_storage")
PACKAGES_DIR = os.path.join(STORAGE_DIR, "packages")
REPORTS_DIR = os.path.join(STORAGE_DIR, "reports")
LUTS_DIR = os.path.join(STORAGE_DIR, "luts")
MODELS_DIR = os.path.join(STORAGE_DIR, "models")
CATALOG_FILE = os.path.join(STORAGE_DIR, "catalog.json")

for d in [PACKAGES_DIR, REPORTS_DIR, LUTS_DIR, MODELS_DIR]:
    os.makedirs(d, exist_ok=True)

def load_catalog() -> Dict[str, Any]:
    if os.path.exists(CATALOG_FILE):
        try:
            with open(CATALOG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {"items": {}}

def save_catalog(catalog: Dict[str, Any]):
    with open(CATALOG_FILE, "w", encoding="utf-8") as f:
        json.dump(catalog, f, indent=2)

class CertificationCloudHandler(BaseHTTPRequestHandler):
    auditor = CertificationAuditor()

    def _send_json(self, status_code: int, data: Any):
        body = json.dumps(data, indent=2).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")
        query = parse_qs(parsed.query)

        if path == "/api/v1/health":
            catalog = load_catalog()
            self._send_json(200, {
                "status": "UP",
                "service": "ABDAudioLab Analog Preservation Cloud API",
                "version": "1.0.0",
                "certified_models_count": len(catalog.get("items", {}))
            })
            return

        if path == "/api/v1/certifications":
            catalog = load_catalog()
            items = list(catalog.get("items", {}).values())

            # Filtros por query params
            if "manufacturer" in query:
                m_filter = query["manufacturer"][0].lower()
                items = [it for it in items if m_filter in it.get("hardware_name", "").lower() or m_filter in it.get("hardware_id", "").lower()]
            if "tier" in query:
                t_filter = query["tier"][0].upper()
                items = [it for it in items if it.get("tier") == t_filter]

            self._send_json(200, {
                "total": len(items),
                "certifications": items
            })
            return

        # /api/v1/certifications/<id>
        if path.startswith("/api/v1/certifications/"):
            parts = path.split("/")[4:]
            cert_id = parts[0]
            catalog = load_catalog()
            item = catalog.get("items", {}).get(cert_id)

            if not item:
                self._send_json(404, {"error": f"Certification ID '{cert_id}' not found."})
                return

            if len(parts) == 1:
                self._send_json(200, item)
                return

            sub = parts[1]
            if sub == "report":
                report_path = os.path.join(REPORTS_DIR, f"{cert_id}.html")
                if os.path.exists(report_path):
                    with open(report_path, "rb") as f:
                        html_data = f.read()
                    self.send_response(200)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.send_header("Content-Length", str(len(html_data)))
                    self.end_headers()
                    self.wfile.write(html_data)
                    return
                self._send_json(404, {"error": "Report HTML file not found."})
                return

            if sub == "package":
                pkg_path = os.path.join(PACKAGES_DIR, f"{cert_id}.tar.gz")
                if os.path.exists(pkg_path):
                    with open(pkg_path, "rb") as f:
                        pkg_data = f.read()
                    self.send_response(200)
                    self.send_header("Content-Type", "application/gzip")
                    self.send_header("Content-Disposition", f'attachment; filename="{cert_id}.tar.gz"')
                    self.send_header("Content-Length", str(len(pkg_data)))
                    self.end_headers()
                    self.wfile.write(pkg_data)
                    return
                self._send_json(404, {"error": "Package .tar.gz not found."})
                return

        self._send_json(404, {"error": "Endpoint not found."})

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")

        if path == "/api/v1/certifications/upload":
            content_length = int(self.headers.get("Content-Length", 0))
            if content_length <= 0:
                self._send_json(400, {"error": "Content-Length missing or zero."})
                return

            raw_body = self.rfile.read(content_length)

            # Si es multipart/form-data, extraer el payload binario del archivo
            content_type = self.headers.get("Content-Type", "")
            package_bytes = raw_body

            if "multipart/form-data" in content_type and b"boundary=" in content_type.encode():
                boundary = content_type.split("boundary=")[1].encode()
                parts = raw_body.split(b"--" + boundary)
                for part in parts:
                    if b"filename=" in part:
                        header_end = part.find(b"\r\n\r\n")
                        if header_end != -1:
                            package_bytes = part[header_end + 4:].rstrip(b"\r\n")
                            break

            # 1. Auditar el bundle recibido
            audit_result = self.auditor.audit_package_bytes(package_bytes)

            if audit_result.get("status") != "APPROVED":
                self._send_json(422, {
                    "status": "REJECTED",
                    "message": "Package failed cryptographic or acoustic certification audit.",
                    "audit_details": audit_result
                })
                return

            # 2. Generar ID único de certificación
            hw_id = audit_result["hardware_id"]
            short_hash = audit_result["bundle_sha256"][:8]
            cert_id = f"{hw_id}_{short_hash}"

            # 3. Guardar el paquete .tar.gz
            pkg_path = os.path.join(PACKAGES_DIR, f"{cert_id}.tar.gz")
            with open(pkg_path, "wb") as f:
                f.write(package_bytes)

            # 4. Extraer artefactos para servido directo (reporte HTML, LUT, NAM)
            try:
                import io
                tar_stream = io.BytesIO(package_bytes)
                with tarfile.open(fileobj=tar_stream, mode="r:gz") as tar:
                    for m in tar.getmembers():
                        if m.name.endswith(".html"):
                            f_rep = tar.extractfile(m)
                            if f_rep:
                                with open(os.path.join(REPORTS_DIR, f"{cert_id}.html"), "wb") as out_rep:
                                    out_rep.write(f_rep.read())
                        elif m.name.endswith("_lut.h") or m.name.endswith("_LUT.h"):
                            f_lut = tar.extractfile(m)
                            if f_lut:
                                with open(os.path.join(LUTS_DIR, f"{cert_id}_lut.h"), "wb") as out_lut:
                                    out_lut.write(f_lut.read())
                        elif m.name.endswith(".nam"):
                            f_nam = tar.extractfile(m)
                            if f_nam:
                                with open(os.path.join(MODELS_DIR, f"{cert_id}.nam"), "wb") as out_nam:
                                    out_nam.write(f_nam.read())
            except Exception as e:
                print(f"Warning extracting sub-artifacts: {e}")

            # 5. Indexar en catálogo
            catalog = load_catalog()
            entry = {
                "certification_id": cert_id,
                "hardware_id": hw_id,
                "hardware_name": audit_result["hardware_name"],
                "sample_rate": audit_result["sample_rate"],
                "measured_points": audit_result["measured_points_count"],
                "average_snr_db": audit_result["average_snr_db"],
                "average_thd_percent": audit_result["average_thd_percent"],
                "tier": audit_result["tier"],
                "sha256": audit_result["bundle_sha256"],
                "has_nam_model": audit_result["has_nam_model"],
                "published_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "report_url": f"/api/v1/certifications/{cert_id}/report",
                "package_url": f"/api/v1/certifications/{cert_id}/package"
            }
            catalog["items"][cert_id] = entry
            save_catalog(catalog)

            self._send_json(201, {
                "status": "APPROVED",
                "message": "Analog hardware certification verified and published.",
                "certification": entry
            })
            return

        self._send_json(404, {"error": "Endpoint not found."})

def run_server(port: int = 8080):
    server_address = ("", port)
    httpd = HTTPServer(server_address, CertificationCloudHandler)
    print(f"ABDAudioLab Cloud Certification Server listening on port {port}...")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        httpd.server_close()

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    run_server(port)
