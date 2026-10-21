"""Assemble the GitHub Pages browser installer from published gateway releases.

Usage: python scripts/build_installer_site.py <output-dir>
Needs the GitHub CLI with GH_TOKEN and GH_REPO set (as in GitHub Actions).
Publishes the Latest stable release and, if newer, the most recent beta pre-release.
Each board's bundle is unpacked into <channel>/<board>/ and their builds are merged into one
manifest, so the installer picks the build that matches the connected chip. A board missing
from a release (older releases are Feather-only) is left out of that channel; releases
without any installer bundle are skipped. UF2_BOARDS are offered as a link to the release's
.uf2 file, since their bootloader appears as a USB drive.
"""
import json
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

BOARDS = ("adafruit_feather_esp32s3", "cytron_maker_esp32")
# Boards installed by copying a .uf2 file to their USB bootloader drive (no browser flashing).
UF2_BOARDS = ("cytron_iriv_io_controller",)
STABLE = re.compile(r"v\d+\.\d+\.\d+")
BETA = re.compile(r"v\d+\.\d+\.\d+-beta\.\d+")


def gh(*args):
    return subprocess.run(["gh", *args], check=True, capture_output=True, text=True).stdout


def pick_channels():
    releases = json.loads(gh("release", "list", "--limit", "100", "--json", "tagName,isLatest,isPrerelease,isDraft"))
    releases = [r for r in releases if not r["isDraft"]]  # newest first
    stable = next((r["tagName"] for r in releases if r["isLatest"] and STABLE.fullmatch(r["tagName"])), None)
    beta = None
    for release in releases:
        if release["tagName"] == stable:
            break  # only offer a beta that is newer than the stable release
        if release["isPrerelease"] and BETA.fullmatch(release["tagName"]):
            beta = release["tagName"]
            break
    return [(name, tag) for name, tag in (("stable", stable), ("beta", beta)) if tag]


def unpack_board(tag, board, target):
    """Extract one board's installer bundle into target; return its manifest, or None if absent."""
    bundle = f"web-installer-{board}.zip"
    with tempfile.TemporaryDirectory() as tmp:
        try:
            gh("release", "download", tag, "--pattern", bundle, "--dir", tmp)
        except subprocess.CalledProcessError:
            return None
        with zipfile.ZipFile(Path(tmp) / bundle) as archive:
            for member in archive.namelist():
                if "/" in member or "\\" in member or member.startswith("."):
                    raise RuntimeError(f"{tag}: unexpected path in installer bundle: {member}")
            archive.extractall(target)
    manifest_path = target / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    manifest_path.unlink()
    return manifest


def main(out):
    root = Path(__file__).resolve().parent.parent
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    shutil.copyfile(root / "installer" / "index.html", out / "index.html")
    repo = gh("repo", "view", "--json", "url", "--jq", ".url").strip()
    channels = []
    for name, tag in pick_channels():
        manifest = None
        for board in BOARDS:
            board_manifest = unpack_board(tag, board, out / name / board)
            if not board_manifest:
                print(f"{tag}: no {board} installer bundle; not offered on the {name} channel")
                continue
            manifest = manifest or {**board_manifest, "builds": []}
            families = {build["chipFamily"] for build in manifest["builds"]}
            for build in board_manifest["builds"]:
                if build["chipFamily"] in families:
                    raise RuntimeError(f"{tag}: two boards share chip family {build['chipFamily']}")
                for part in build["parts"]:
                    part["path"] = f"{board}/{part['path']}"
                manifest["builds"].append(build)
        if not manifest:
            print(f"{tag}: no installer bundle; skipping {name} channel")
            continue
        manifest["version"] = tag
        (out / name / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        assets = {a["name"] for a in json.loads(gh("release", "view", tag, "--json", "assets"))["assets"]}
        downloads = {board: f"{repo}/releases/download/{tag}/firmware-{board}.uf2"
                     for board in UF2_BOARDS if f"firmware-{board}.uf2" in assets}
        channels.append({"channel": name, "tag": tag, "manifest": f"{name}/manifest.json",
                         "notes": f"{repo}/releases/tag/{tag}", "uf2": downloads})
        print(f"{name}: {tag}")
    (out / "channels.json").write_text(json.dumps({"repo": repo, "channels": channels}, indent=2) + "\n")
    if not channels:
        print("No release has an installer bundle yet; the page will say so.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(Path(sys.argv[1]))
