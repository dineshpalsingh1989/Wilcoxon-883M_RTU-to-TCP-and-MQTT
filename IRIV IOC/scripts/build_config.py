"""Embed selected Mozilla trust roots for certificate-verified GitHub downloads and define build IDs."""
import os
import re
import ssl
import tempfile
from pathlib import Path
import certifi

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
certs = []
for pem in re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", Path(certifi.where()).read_text(), re.S):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".pem", delete=False) as cert:
        cert.write(pem)
        cert_path = cert.name
    try:
        subject = str(ssl._ssl._test_decode_cert(cert_path)["subject"])
        if any(name in subject for name in ("DigiCert", "Sectigo", "USERTrust", "ISRG")):
            certs.append(pem)
    finally:
        os.unlink(cert_path)
if len(certs) < 4:
    raise RuntimeError("GitHub HTTPS trust roots unavailable")
target = Path(env.subst("$PROJECT_INCLUDE_DIR")) / "generated/github_roots.h"
target.parent.mkdir(parents=True, exist_ok=True)
content = '#pragma once\nstatic const char GITHUB_ROOTS[] = R"certs(\n' + '\n'.join(certs) + '\n)certs";\n'
if not target.exists() or target.read_text() != content:
    target.write_text(content)
tag = os.environ.get("GITHUB_REF_NAME", "")
if os.environ.get("GITHUB_REF_TYPE") == "tag" and re.fullmatch(r"v\d+\.\d+\.\d+(-beta\.\d+)?", tag):
    env.Append(CPPDEFINES=[("GATEWAY_RELEASE_VERSION", env.StringifyMacro(tag[1:]))])
# Release asset names and /firmware_info use the PlatformIO environment name as the board ID.
env.Append(CPPDEFINES=[("GATEWAY_BOARD", env.StringifyMacro(env["PIOENV"]))])
