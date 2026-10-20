"""Collect the flash images and an ESP Web Tools manifest for the browser installer.

Run with `pio run -t webinstaller`; normal builds are unaffected. Offsets come from the
board's own upload layout, so the browser installs exactly what `pio run -t upload` writes.
"""
import json
import os
import shutil
from pathlib import Path

Import("env")


def bundle(target, source, env):
    out = Path(env.subst("$BUILD_DIR")) / "web-installer"
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), env.subst("$BUILD_DIR/${PROGNAME}.bin")))
    parts = []
    for offset, path in sorted(images, key=lambda image: int(image[0], 16)):
        name = Path(path).name
        if (out / name).exists():
            raise RuntimeError(f"Duplicate flash image name: {name}")
        shutil.copyfile(path, out / name)
        parts.append({"path": name, "offset": int(offset, 16)})
    manifest = {
        "name": "Wilcoxon 883M Gateway",
        "version": os.environ.get("GITHUB_REF_NAME", "local build"),
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Web installer bundle: {out}")
    for part in parts:
        print(f"  0x{part['offset']:06x}  {part['path']}")


env.AddCustomTarget(
    name="webinstaller",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=bundle,
    title="Web installer bundle",
    description="Collect flash images and an ESP Web Tools manifest",
)
