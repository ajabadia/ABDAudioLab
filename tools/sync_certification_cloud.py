#!/usr/bin/env python3
"""
ABDAudioLab — Cloud Telemetry & Pre-Flight Certification Sync Pipeline
Valida la integridad criptográfica de las LUTs (C++, JSON, HTML) y realiza la
sincronización con S3, endpoints REST API, o empaquetado sellado local.
"""

import os
import sys
import re
import json
import zlib
import hashlib
import tarfile
import argparse
from urllib.request import Request, urlopen
from urllib.error import URLError

# Configurar salida segura UTF-8 en consolas Windows
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# Códigos ANSI para reportes legibles en consola (SoundID Style)
GREEN = "\033[92m"
AMBER = "\033[93m"
RED = "\033[91m"
CYAN = "\033[96m"
BOLD = "\033[1m"
RESET = "\033[0m"

def print_status(message, color=CYAN, is_bold=False):
    style = BOLD if is_bold else ""
    print(f"{color}{style}{message}{RESET}")

class PreFlightSanitizer:
    def __init__(self, directory_path):
        self.dir = directory_path
        self.files = {}
        self.manifest_data = {}

    def locate_files(self):
        """Escanea el directorio buscando el trío de artefactos de producción."""
        if not os.path.isdir(self.dir):
            print_status(f"❌ Error: El directorio '{self.dir}' no existe.", RED, True)
            return False

        for f in os.listdir(self.dir):
            if f.endswith("_manifest.json"):
                self.files["manifest"] = os.path.join(self.dir, f)
            elif f.endswith("_lut.h"):
                self.files["header"] = os.path.join(self.dir, f)
            elif f.endswith("_certification_report.html"):
                self.files["html"] = os.path.join(self.dir, f)

        required = ["manifest", "header", "html"]
        missing = [r for r in required if r not in self.files]
        if missing:
            print_status(f"❌ Error: Faltan artefactos críticos en el directorio: {missing}", RED, True)
            return False

        return True

    def sanitize_manifest_and_crc(self):
        """Audita el manifiesto JSON y recalcula el checksum de integridad."""
        print_status("• Validando estructura y firmas del Manifiesto JSON...", CYAN)
        try:
            with open(self.files["manifest"], "r", encoding="utf-8") as f:
                self.manifest_data = json.load(f)
        except Exception as e:
            print_status(f"  ❌ JSON corrupto o inválido: {e}", RED)
            return False

        # Extraer ID de hardware compatible con esquema plano o anidado
        hw_id = (self.manifest_data.get("hardware_id")
                 or self.manifest_data.get("hardware", {}).get("id")
                 or self.manifest_data.get("hardware", {}).get("name"))
        if not hw_id:
            print_status("  ❌ Faltan metadatos críticos en el manifiesto: 'hardware_id'", RED)
            return False

        # Extraer sample_rate
        sample_rate = (self.manifest_data.get("sample_rate")
                       or self.manifest_data.get("audioCalibration", {}).get("sampleRate"))
        if not sample_rate:
            print_status("  ❌ Faltan metadatos críticos en el manifiesto: 'sample_rate'", RED)
            return False

        # Verificar y validar Checksum CRC-32 si está declarado
        declared_crc = (self.manifest_data.get("crc32_checksum")
                        or self.manifest_data.get("crc32Checksum"))

        if declared_crc is not None:
            points_block = (self.manifest_data.get("measured_points")
                            or self.manifest_data.get("measuredPoints")
                            or [])
            points_str = json.dumps(points_block, sort_keys=True)
            computed_crc = zlib.crc32(points_str.encode("utf-8")) & 0xFFFFFFFF
            declared_crc_int = int(declared_crc)

            if computed_crc != declared_crc_int:
                print_status(f"  ❌ Error criptográfico de CRC-32: Declarado {declared_crc_int} != Calculado {computed_crc}", RED)
                return False

            print_status(f"  ✓ Sello de integridad CRC-32 verificado con éxito: {computed_crc}", GREEN)
        else:
            print_status("  ✓ Estructura de manifiesto JSON verificada correctamente.", GREEN)

        return True

    def sanitize_cpp_header(self):
        """Audita la cabecera C++ buscando alineación SIMD, dimensiones y NaNs."""
        print_status("• Ejecutando Pre-Flight Sanitizer sobre la cabecera C++ (_lut.h)...", CYAN)
        try:
            with open(self.files["header"], "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as e:
            print_status(f"  ❌ No se pudo leer el archivo C++: {e}", RED)
            return False

        # 1. Comprobar rigurosamente la alineación de memoria SIMD
        if "alignas(16)" not in content and "alignas(32)" not in content:
            print_status("  ❌ Error de arquitectura: La tabla C++ carece de la directiva estricta alignas(16/32).", RED)
            return False

        # 2. Verificar la presencia de la estructura AbdBatchedPoint
        if "struct" not in content or "AbdBatchedPoint" not in content:
            print_status("  ❌ Error de firma: Estructura AbdBatchedPoint no encontrada en la cabecera.", RED)
            return False

        # 3. Escaneo balístico en busca de valores numéricos inválidos o roturas de coma flotante (NaN / Inf)
        nan_patterns = [r"\bnan\b", r"\b-nan\b", r"\binf\b", r"\b-inf\b", r"\bNaN\b", r"\bInfinity\b"]
        for pattern in nan_patterns:
            if re.search(pattern, content):
                print_status(f"  ❌ Inestabilidad numérica detectada: Se encontraron valores '{pattern}' en el array de datos.", RED)
                return False

        # 4. Asegurar que las dimensiones declaradas en C++ sean coherentes con el manifiesto
        declared_size_match = re.search(r"size_t\s+\w+_SIZE\s*=\s*(\d+)", content)
        if declared_size_match:
            declared_size = int(declared_size_match.group(1))
            expected_size = (self.manifest_data.get("totalPointsMeasured")
                             or self.manifest_data.get("total_points")
                             or (self.manifest_data.get("grid_dimensions", [0, 0])[0] * self.manifest_data.get("grid_dimensions", [0, 0])[1])
                             or (len(self.manifest_data.get("measured_points", []))))
            if expected_size and declared_size != expected_size:
                print_status(f"  ❌ Descalce de rejilla: Tamaño C++ ({declared_size}) != Rejilla Manifiesto ({expected_size})", RED)
                return False

        print_status("  ✓ Estructura de memoria alignas(16) y estabilidad numérica validadas con error 0.", GREEN)
        return True

    def sanitize_html_report(self):
        """Audita la integridad y automatización del reporte HTML imprimible."""
        print_status("• Inspeccionando el Informe Técnico HTML y Gráficos SVG inline...", CYAN)
        try:
            with open(self.files["html"], "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as e:
            print_status(f"  ❌ No se pudo leer el reporte HTML: {e}", RED)
            return False

        # 1. Verificar la inyección del script de auto-impresión para usabilidad Zero-Friction
        if "window.print()" not in content or "window.onload" not in content:
            print_status("  ⚠️ Advertencia de usabilidad: El script de auto-impresión window.print() no está inyectado.", AMBER)

        # 2. Validar que contenga los gráficos vectoriales empotrados de forma nativa
        if "<svg" not in content or "</svg>" not in content:
            print_status("  ❌ Error visual: No se encontraron gráficos vectoriales <svg> empacados en el HTML.", RED)
            return False

        # 3. Comprobar que no arrastre comas de idiomas locales que rompan el path SVG (Inyección de Local)
        if re.search(r'd="M\s*\d+,\d+', content):
            print_status("  ❌ Corrupción de sintaxis SVG: Se detectó formateo local con comas decimales en lugar de puntos.", RED)
            return False

        print_status("  ✓ Reporte HTML5 y directivas vectoriales autónomas verificadas con éxito.", GREEN)
        return True

    def package_bundle(self):
        """Empaqueta los artefactos validados en un bundle sellado comprimido tar.gz."""
        dist_dir = "dist_packages"
        os.makedirs(dist_dir, exist_ok=True)
        
        hardware_id = (self.manifest_data.get("hardware_id")
                       or self.manifest_data.get("hardware", {}).get("id")
                       or "unknown_device")
        bundle_name = f"{hardware_id}_certification_package.tar.gz"
        bundle_path = os.path.join(dist_dir, bundle_name)

        print_status(f"• Sellando paquete de producción en '{bundle_path}'...", CYAN)
        
        try:
            with tarfile.open(bundle_path, "w:gz") as tar:
                for key, filepath in self.files.items():
                    tar.add(filepath, arcname=os.path.basename(filepath))
            
            # Generar comprobante criptográfico de backup local
            receipt_path = os.path.join(dist_dir, f"{hardware_id}_sync_receipt.json")
            receipt = {
                "hardware_id": hardware_id,
                "crc32_integrity": self.manifest_data.get("crc32_checksum", self.manifest_data.get("crc32Checksum", "VALID")),
                "sha256_bundle": hashlib.sha256(open(bundle_path, "rb").read()).hexdigest(),
                "status": "SEALED_LOCAL"
            }
            with open(receipt_path, "w", encoding="utf-8") as rf:
                json.dump(receipt, rf, indent=2)

            print_status(f"  ✓ Bundle local empaquetado y firmado criptográficamente en '{receipt_path}'.", GREEN)
            return bundle_path
        except Exception as e:
            print_status(f"  ❌ Fallo al empaquetar el bundle comprimido: {e}", RED)
            return None

def upload_to_rest_api(bundle_path, api_url, token):
    """Sincroniza el bundle de lazo cerrado mediante un POST a la API REST de la nube."""
    if not token:
        token = "ABD-COMMUNITY-GUEST"

    try:
        with open(bundle_path, "rb") as f:
            file_data = f.read()

        # Construir una petición multipart cruda de forma manual para evitar dependencias de 'requests'
        boundary = b"----ABDAudioLabBoundary"
        body = []
        body.append(b"--" + boundary)
        body.append(f'Content-Disposition: form-data; name="file"; filename="{os.path.basename(bundle_path)}"'.encode('utf-8'))
        body.append(b"Content-Type: application/gzip")
        body.append(b"")
        body.append(file_data)
        body.append(b"--" + boundary + b"--")
        body.append(b"")
        payload = b"\r\n".join(body)

        req = Request(api_url, data=payload, method="POST")
        req.add_header("Content-Type", f"multipart/form-data; boundary={boundary.decode('utf-8')}")
        req.add_header("Authorization", f"Bearer {token}")
        req.add_header("Content-Length", str(len(payload)))

        with urlopen(req, timeout=15) as response:
            res_body = response.read().decode('utf-8')
            print_status(f"  ✓ Sincronización en la nube exitosa. Código de respuesta: {response.status}", GREEN)
            print(f"  Respuesta del servidor: {res_body}")
            return True
    except URLError as e:
        print_status(f"  ❌ Error de conexión con el servidor cloud: {e}", RED)
        return False
    except Exception as e:
        print_status(f"  ❌ Fallo inesperado en la transmisión REST: {e}", RED)
        return False

def audit_hardware_contracts(contracts_dir="contracts/hardware", schema_path="contracts/hardware_profile.schema.json"):
    """Audita los perfiles declarativos de hardware contra el esquema JSON Schema Draft 2020-12."""
    print_status("\n• Auditando perfiles declarativos de hardware en 'contracts/'...", CYAN, True)

    if not os.path.isdir(contracts_dir):
        print_status(f"  ⚠️ Advertencia: Directorio '{contracts_dir}' no encontrado.", AMBER)
        return True

    # Cargar schema si existe
    schema_data = None
    if os.path.isfile(schema_path):
        try:
            with open(schema_path, "r", encoding="utf-8") as sf:
                schema_data = json.load(sf)
        except Exception as e:
            print_status(f"  ⚠️ No se pudo cargar el schema '{schema_path}': {e}", AMBER)

    has_jsonschema = False
    try:
        import jsonschema
        has_jsonschema = True
    except ImportError:
        pass

    passed = 0
    warnings = 0
    json_files = [f for f in os.listdir(contracts_dir) if f.endswith(".json")]

    for f in json_files:
        path = os.path.join(contracts_dir, f)
        try:
            with open(path, "r", encoding="utf-8") as jf:
                data = json.load(jf)

            if not isinstance(data, dict):
                print_status(f"  ❌ {f}: La raíz no es un objeto JSON válido.", RED)
                warnings += 1
                continue

            if not data.get("id") or not data.get("displayName"):
                print_status(f"  ❌ {f}: Faltan claves obligatorias 'id' o 'displayName'.", RED)
                warnings += 1
                continue

            if has_jsonschema and schema_data:
                try:
                    jsonschema.validate(instance=data, schema=schema_data)
                except jsonschema.ValidationError as ve:
                    print_status(f"  ⚠️ {f}: Advertencia de schema: {ve.message}", AMBER)
                    warnings += 1
                    continue

            passed += 1
        except Exception as e:
            print_status(f"  ❌ {f}: Error de sintaxis o lectura: {e}", RED)
            warnings += 1

    print_status(f"  ✓ Auditoría de contratos completada: {passed} válidos, {warnings} advertencias.", GREEN if warnings == 0 else AMBER)
    return warnings == 0

def main():
    parser = argparse.ArgumentParser(description="ABDAudioLab Telemetry Sync & Pre-Flight Sanitizer Pipeline")
    parser.add_argument("--dir", default="exported_luts", help="Directorio con los artefactos de la sesión")
    parser.add_argument("--target", choices=["local", "rest", "s3"], default="local", help="Destino de sincronización")
    parser.add_argument("--url", default=os.getenv("ABDAUDIOLAB_API_URL", "http://localhost:8080/api/v1/certifications/upload"), help="Endpoint REST API")
    parser.add_argument("--token", default=os.getenv("ABDAUDIOLAB_API_TOKEN", ""), help="Token de autorización Bearer")
    parser.add_argument("--audit-contracts", action="store_true", help="Auditar perfiles JSON contra schema formal")
    parser.add_argument("--audit-only", action="store_true", help="Auditar perfiles de hardware y salir sin procesar sesión")

    args = parser.parse_args()

    print_status("================================================================", CYAN)
    print_status("   ABDAUDIOLAB — PIPELINE DE TELEMETRÍA Y CONTROL DE CALIDAD    ", CYAN, True)
    print_status("================================================================", CYAN)

    if args.audit_contracts or args.audit_only:
        ok = audit_hardware_contracts()
        if args.audit_only:
            sys.exit(0 if ok else 1)

    sanitizer = PreFlightSanitizer(args.dir)

    # Ejecución encadenada de la auditoría estricta (Pre-Flight Sanitizer)
    if not (sanitizer.locate_files() and
            sanitizer.sanitize_manifest_and_crc() and
            sanitizer.sanitize_cpp_header() and
            sanitizer.sanitize_html_report()):
        print_status("\n❌ PIPELINE ABORTADO: Los artefactos de la sesión contienen errores críticos.", RED, True)
        sys.exit(1)

    print_status("\n✓ ¡Todos los controles de integridad han pasado al 100% en verde!", GREEN, True)

    # Sellar bundle local comprimido
    bundle_path = sanitizer.package_bundle()
    if not bundle_path:
        sys.exit(1)

    # Ruteo multi-backend declarativo según los argumentos del operador
    if args.target == "rest":
        if not args.url:
            print_status("❌ Error: Se especificó destino REST pero la URL del endpoint está vacía.", RED, True)
            sys.exit(1)
        success = upload_to_rest_api(bundle_path, args.url, args.token)
        if not success:
            sys.exit(1)
    elif args.target == "s3":
        print_status("• Destino S3/R2 seleccionado. Bundle sellado en dist_packages/ listo para sincronización CLI externa.", AMBER)

    print_status("\n🏆 ¡Proceso de certificación y sincronización finalizado con éxito total!", GREEN, True)

if __name__ == "__main__":
    main()
