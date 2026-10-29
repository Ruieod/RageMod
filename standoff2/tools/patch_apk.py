#!/usr/bin/env python3
"""
Patch a Unity/IL2CPP Android game so it loads RageMod - no root, no smali.

  1. libragemod.so is added to lib/arm64-v8a/ and made a DT_NEEDED dependency of libmain.so (LIEF), so the Android linker loads it
     right before the game's own native code starts
  2. classes.dex (UI + glue) is added as the next free classesN.dex, the runtime loads it with the app's class loader
  3. old signatures are dropped, the archive is page-aligned (zipalign -p) and signed with apksigner

Accepts one .apk, several split .apk files (base + config splits), or an .xapk / .apks bundle.
usage: patch_apk.py --lib libragemod.so --dex classes.dex [--out DIR] [--keystore KS --ks-pass PASS --key-alias A] input...
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ABI = "arm64-v8a"
NEEDED = "libragemod.so"


def find_tool(name):
    p = shutil.which(name)
    if p:
        return p
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or "/opt/android-sdk"
    cands = sorted([os.path.join(sdk, "build-tools", v, name) for v in os.listdir(os.path.join(sdk, "build-tools"))] if os.path.isdir(os.path.join(sdk, "build-tools")) else [])
    for c in reversed(cands):
        if os.path.exists(c):
            return c
    sys.exit("cannot find %s (set ANDROID_HOME to an SDK with build-tools)" % name)


def run(cmd, **kw):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)
    if r.returncode != 0:
        sys.exit("command failed: %s\n%s" % (" ".join(cmd), r.stdout))
    return r.stdout


def inject_needed(so_bytes, workdir):
    import lief
    src = os.path.join(workdir, "in.so")
    dst = os.path.join(workdir, "out.so")
    open(src, "wb").write(so_bytes)
    binary = lief.ELF.parse(src)
    if binary is None:
        sys.exit("libmain.so is not a valid ELF")
    have = [str(getattr(e, "name", e)) for e in binary.libraries]
    if NEEDED in have:
        return so_bytes, False
    binary.add_library(NEEDED)
    binary.write(dst)
    out = open(dst, "rb").read()
    check = lief.ELF.parse(dst)
    names = [str(getattr(e, "name", e)) for e in check.libraries]
    if NEEDED not in names:
        sys.exit("LIEF did not persist the DT_NEEDED entry: %s" % names)
    if check.header.machine_type != binary.header.machine_type:
        sys.exit("machine type changed while patching")
    return out, True


def is_sig(name):
    n = name.upper()
    return n.startswith("META-INF/") and (n.endswith((".SF", ".RSA", ".DSA", ".EC")) or n == "META-INF/MANIFEST.MF" or re.match(r"META-INF/[^/]+\.(SF|RSA|DSA|EC)$", n))


def next_dex_name(names):
    top = 1
    for n in names:
        m = re.fullmatch(r"classes(\d*)\.dex", n)
        if m:
            top = max(top, int(m.group(1) or 1))
    return "classes%d.dex" % (top + 1)


def copy_entries(zin, zout, skip, extra):
    for info in zin.infolist():
        if info.filename in skip or is_sig(info.filename):
            continue
        ni = zipfile.ZipInfo(info.filename, info.date_time)
        ni.compress_type = info.compress_type
        ni.external_attr = info.external_attr
        ni.create_system = info.create_system
        if info.is_dir():
            zout.writestr(ni, b"")
            continue
        with zin.open(info) as src, zout.open(ni, "w", force_zip64=info.file_size > 0x7FFFFFFF) as dst:
            shutil.copyfileobj(src, dst, 1 << 20)
    for name, data, ctype in extra:
        ni = zipfile.ZipInfo(name, (2024, 1, 1, 0, 0, 0))
        ni.compress_type = ctype
        ni.external_attr = 0o644 << 16
        zout.writestr(ni, data)


def make_keystore(path):
    run(["keytool", "-genkeypair", "-keystore", path, "-storepass", "android", "-keypass", "android", "-alias", "ragemod",
         "-keyalg", "RSA", "-keysize", "2048", "-validity", "20000", "-dname", "CN=RageMod"])


def patch_apk(path, out_path, lib_bytes, dex_bytes, do_lib, do_dex, target, workdir):
    with zipfile.ZipFile(path) as zin:
        names = zin.namelist()
        extra = []
        skip = set()
        if do_lib:
            tgt = "lib/%s/%s" % (ABI, target)
            so, changed = inject_needed(zin.read(tgt), workdir)
            print("  %s: DT_NEEDED %s %s" % (os.path.basename(path), NEEDED, "added" if changed else "already present"))
            skip.add(tgt)
            extra.append((tgt, so, zipfile.ZIP_STORED))
            extra.append(("lib/%s/%s" % (ABI, NEEDED), lib_bytes, zipfile.ZIP_STORED))
        if do_dex:
            dn = next_dex_name(names)
            print("  %s: adding %s" % (os.path.basename(path), dn))
            extra.append((dn, dex_bytes, zipfile.ZIP_DEFLATED))
        with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED, allowZip64=True) as zout:
            copy_entries(zin, zout, skip, extra)


def align_and_sign(apk_in, apk_out, ks, ks_pass, alias, zipalign, apksigner):
    aligned = apk_out + ".aligned"
    run([zipalign, "-p", "-f", "4", apk_in, aligned])
    run([apksigner, "sign", "--ks", ks, "--ks-pass", "pass:" + ks_pass, "--key-pass", "pass:" + ks_pass, "--ks-key-alias", alias,
         "--v1-signing-enabled", "true", "--v2-signing-enabled", "true", "--v3-signing-enabled", "true", "--out", apk_out, aligned])
    os.remove(aligned)
    run([apksigner, "verify", apk_out])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+")
    ap.add_argument("--lib", required=True)
    ap.add_argument("--dex", required=True)
    ap.add_argument("--out", default="patched")
    ap.add_argument("--target", default="libmain.so", help="game library that gets the DT_NEEDED entry")
    ap.add_argument("--keystore")
    ap.add_argument("--ks-pass", default="android")
    ap.add_argument("--key-alias", default="ragemod")
    a = ap.parse_args()

    lib_bytes = open(a.lib, "rb").read()
    dex_bytes = open(a.dex, "rb").read()
    zipalign, apksigner = find_tool("zipalign"), find_tool("apksigner")
    os.makedirs(a.out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="ragemod-")
    ks = a.keystore
    if not ks:
        bundled = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ragemod.keystore")
        if os.path.exists(bundled):
            ks = bundled          # fixed key: every build is signed the same, so updates install over each other
        else:
            ks = os.path.join(work, "debug.keystore")
            make_keystore(ks)
        ks_pass, alias = "android", "ragemod"
    else:
        ks_pass, alias = a.ks_pass, a.key_alias

    # collect APKs (unpack bundles)
    apks = []
    bundle_meta = []
    for inp in a.inputs:
        if inp.lower().endswith((".xapk", ".apks", ".zip")):
            d = os.path.join(work, "bundle_%d" % len(bundle_meta))
            os.makedirs(d)
            with zipfile.ZipFile(inp) as z:
                z.extractall(d)
            bundle_meta.append((inp, d))
            for root, _, files in os.walk(d):
                for f in sorted(files):
                    if f.lower().endswith(".apk"):
                        apks.append(os.path.join(root, f))
        else:
            apks.append(inp)
    if not apks:
        sys.exit("no .apk found in the input")

    tgt_entry = "lib/%s/%s" % (ABI, a.target)
    lib_apk = dex_apk = None
    for p in apks:
        with zipfile.ZipFile(p) as z:
            n = z.namelist()
            if tgt_entry in n and lib_apk is None:
                lib_apk = p
            if "classes.dex" in n and dex_apk is None:
                dex_apk = p
    if lib_apk is None:
        sys.exit("%s not found in any APK - is this an arm64 build? (32-bit-only builds are not supported)" % tgt_entry)
    if dex_apk is None:
        dex_apk = apks[0]
    print("lib target in: %s\ndex target in: %s" % (os.path.basename(lib_apk), os.path.basename(dex_apk)))

    outputs = {}
    for p in apks:
        do_lib, do_dex = p == lib_apk, p == dex_apk
        tmp_out = os.path.join(work, "p_" + os.path.basename(p))
        if do_lib or do_dex:
            patch_apk(p, tmp_out, lib_bytes, dex_bytes, do_lib, do_dex, a.target, work)
        else:
            with zipfile.ZipFile(p) as zin, zipfile.ZipFile(tmp_out, "w", zipfile.ZIP_DEFLATED, allowZip64=True) as zout:
                copy_entries(zin, zout, set(), [])   # only strips the old signature, split must be re-signed with the same key
        final = os.path.join(work, "f_" + os.path.basename(p))
        align_and_sign(tmp_out, final, ks, ks_pass, alias, zipalign, apksigner)
        outputs[p] = final

    if bundle_meta:
        for inp, d in bundle_meta:
            for p, f in outputs.items():
                if p.startswith(d):
                    shutil.copyfile(f, p)
            base = os.path.splitext(os.path.basename(inp))[0]
            ext = os.path.splitext(inp)[1]
            dest = os.path.join(a.out, base + "-ragemod" + ext)
            with zipfile.ZipFile(dest, "w", zipfile.ZIP_STORED, allowZip64=True) as z:
                for root, _, files in os.walk(d):
                    for f in sorted(files):
                        full = os.path.join(root, f)
                        z.write(full, os.path.relpath(full, d))
            print("wrote", dest)
    else:
        for p, f in outputs.items():
            base = os.path.splitext(os.path.basename(p))[0]
            dest = os.path.join(a.out, base + ("-ragemod" if len(outputs) == 1 or p == lib_apk else "-split") + ".apk")
            shutil.copyfile(f, dest)
            print("wrote", dest)
    print("signed with %s" % ("your keystore" if a.keystore else "the bundled RageMod key (uninstall the original game first: the signature differs)"))
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
