"""OTA manifest készítése (platformfüggetlen, a GitHub Actions is ezt használja).

Használat:
  python scripts/make_manifest.py --bin build/poolkeeper_ui/poolkeeper_ui.ino.bin \
      --base-url https://github.com/OWNER/REPO/releases/download/v0.4.0 --out ota [--devices PK-71D040,PK-123456]

A build számot és a verziót a firmware/poolkeeper_ui/src/app_config.h-ból olvassa.
"""
import argparse
import hashlib
import json
import os
import re
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--base-url", required=True)
    ap.add_argument("--out", default="ota")
    ap.add_argument("--devices", default="*")
    a = ap.parse_args()

    cfg = open(os.path.join(ROOT, "firmware", "poolkeeper_ui", "src", "app_config.h"), encoding="utf-8").read()
    build = int(re.search(r"#define PK_FW_BUILD (\d+)", cfg).group(1))
    version = re.search(r'#define PK_FW_VERSION "([^"]+)"', cfg).group(1)

    os.makedirs(a.out, exist_ok=True)
    name = "poolkeeper_ui-%d.bin" % build
    dst = os.path.join(a.out, name)
    shutil.copyfile(a.bin, dst)
    sha = hashlib.sha256(open(dst, "rb").read()).hexdigest()
    manifest = {
        "build": build,
        "version": version,
        "url": a.base_url.rstrip("/") + "/" + name,
        "sha256": sha,
        "devices": [d.strip() for d in a.devices.split(",") if d.strip()],
    }
    with open(os.path.join(a.out, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, separators=(",", ":"))
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
