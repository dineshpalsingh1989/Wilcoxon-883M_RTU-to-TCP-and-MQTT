"""Assemble the GitHub Pages browser installer from published gateway releases.

Usage: python scripts/build_installer_site.py <output-dir>
Needs the GitHub CLI with GH_TOKEN and GH_REPO set (as in GitHub Actions).
Publishes the Latest stable release and, if newer, the most recent beta pre-release.
Releases without the installer bundle (made before it existed) are skipped.
"""
import json
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

BUNDLE = "web-installer-adafruit_feather_esp32s3.zip"
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


def main(out):
    root = Path(__file__).resolve().parent.parent
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    shutil.copyfile(root / "installer" / "index.html", out / "index.html")
    repo = gh("repo", "view", "--json", "url", "--jq", ".url").strip()
    channels = []
    for name, tag in pick_channels():
        with tempfile.TemporaryDirectory() as tmp:
            try:
                gh("release", "download", tag, "--pattern", BUNDLE, "--dir", tmp)
            except subprocess.CalledProcessError:
                print(f"{tag}: no {BUNDLE}; skipping {name} channel")
                continue
            target = out / name
            with zipfile.ZipFile(Path(tmp) / BUNDLE) as archive:
                for member in archive.namelist():
                    if "/" in member or "\\" in member or member.startswith("."):
                        raise RuntimeError(f"{tag}: unexpected path in installer bundle: {member}")
                archive.extractall(target)
        manifest_path = target / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["version"] = tag
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        channels.append({"channel": name, "tag": tag, "manifest": f"{name}/manifest.json",
                         "notes": f"{repo}/releases/tag/{tag}"})
        print(f"{name}: {tag}")
    (out / "channels.json").write_text(json.dumps({"repo": repo, "channels": channels}, indent=2) + "\n")
    if not channels:
        print("No release has an installer bundle yet; the page will say so.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(Path(sys.argv[1]))
