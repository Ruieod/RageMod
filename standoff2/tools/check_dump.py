#!/usr/bin/env python3
"""Validate every klass()/method()/field() lookup in native/src against an Il2CppDumper dump.cs.
usage: check_dump.py dump.cs [native/src]  -> exit 1 if something does not exist."""
import re, sys, os, glob

def parse_dump(path):
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    hdr = re.compile(r'^(?:public |internal |private |protected )?(?:sealed |abstract |static |readonly )*(class|struct|interface|enum) ([^\s:<,{]+(?:`\d+)?(?:<[^>]*>)?)(?:\s*:\s*([^\s,{]+))?')
    classes = {}   # (ns, name) -> dict(parent, methods{name:set(argc)}, fields:set)
    ns = ""
    i = 0
    n = len(lines)
    while i < n:
        l = lines[i]
        if l.startswith("// Namespace:"):
            ns = l[len("// Namespace:"):].strip()
        m = hdr.match(l)
        if m and not l.startswith("\t") and not l.startswith("//"):
            name = m.group(2).split("<")[0]
            parent = m.group(3)
            if parent: parent = parent.split("<")[0].split(",")[0]
            ent = {"parent": parent, "methods": {}, "fields": set(), "ns": ns}
            j = i + 1
            while j < n and lines[j] != "}":
                s = lines[j]
                if s == "{}" or (s and not s.startswith(("\t", " ", "{", "}", "//", "["))):
                    break
                if s.startswith("\t") and not s.strip().startswith("//"):
                    t = s.strip()
                    mm = re.search(r'([A-Za-z_.\d<>`]+)\((.*)\)\s*\{', t)
                    if mm and "// Fields" not in t:
                        mname = mm.group(1).split(".")[-1] if not mm.group(1).startswith(".") else mm.group(1)
                        args = mm.group(2).strip()
                        depth = 0; cnt = 0; cur = ""
                        for ch in args:
                            if ch == "<": depth += 1
                            elif ch == ">": depth -= 1
                            if ch == "," and depth == 0:
                                if cur.strip(): cnt += 1
                                cur = ""
                            else: cur += ch
                        if cur.strip(): cnt += 1
                        ent["methods"].setdefault(mname, set()).add(cnt)
                    else:
                        fm = re.match(r'(?:public |private |protected |internal )?(?:static |readonly |const )*[^\s]+(?: [^\s]+)* ([A-Za-z_<>\.\d`]+)(?: = .*)?;', t)
                        if fm:
                            ent["fields"].add(fm.group(1))
                j += 1
            key = (ns, name)
            if key not in classes:
                classes[key] = ent
            i = j
        i += 1
    return classes

def lookup(classes, ns, name):
    return classes.get((ns, name))

def has_method(classes, ns, name, meth, argc):
    seen = 0
    ent = lookup(classes, ns, name)
    while ent and seen < 16:
        if meth in ent["methods"] and (argc < 0 or argc in ent["methods"][meth]):
            return True
        p = ent["parent"]
        ent = None
        if p:
            for (n2, c2), e2 in classes.items():
                if c2 == p:
                    ent = e2; break
        seen += 1
    return False

def has_field(classes, ns, name, field):
    seen = 0
    ent = lookup(classes, ns, name)
    while ent and seen < 16:
        if field in ent["fields"] or ("<%s>k__BackingField" % field) in ent["fields"]:
            return True
        p = ent["parent"]
        ent = None
        if p:
            for (n2, c2), e2 in classes.items():
                if c2 == p:
                    ent = e2; break
        seen += 1
    return False

def main():
    dump = sys.argv[1]
    src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "native", "src")
    classes = parse_dump(dump)
    bad = 0; total = 0
    for f in sorted(glob.glob(os.path.join(src, "*.cpp")) + glob.glob(os.path.join(src, "*.h"))):
        txt = open(f, encoding="utf-8").read()
        for m in re.finditer(r'BIND_SIG\(\s*[\w:]+\s*,\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)"', txt):
            total += 1
            ns, cl, me = m.groups()
            if not lookup(classes, ns, cl):
                print("MISSING class  %s.%s   (%s)" % (ns, cl, os.path.basename(f))); bad += 1
            elif not has_method(classes, ns, cl, me, -1):
                print("MISSING method %s.%s::%s   (%s)" % (ns, cl, me, os.path.basename(f))); bad += 1
        for m in re.finditer(r'installHook<\d+>\("([^"]*)",\s*"([^"]*)",\s*"([^"]*)"\)', txt):
            total += 1
            ns, cl, me = m.groups()
            if not lookup(classes, ns, cl):
                print("MISSING class  %s.%s   (%s)" % (ns, cl, os.path.basename(f))); bad += 1
            elif not has_method(classes, ns, cl, me, -1):
                print("MISSING method %s.%s::%s   (%s)" % (ns, cl, me, os.path.basename(f))); bad += 1
        for m in re.finditer(r'il2::klass\("([^"]*)",\s*"([^"]*)"\)', txt):
            total += 1
            if not lookup(classes, m.group(1), m.group(2)):
                print("MISSING class  %s.%s   (%s)" % (m.group(1), m.group(2), os.path.basename(f))); bad += 1
        for m in re.finditer(r'il2::field\("([^"]*)",\s*"([^"]*)",\s*"([^"]*)"\)', txt):
            total += 1
            ns, cl, fi = m.groups()
            if not lookup(classes, ns, cl):
                print("MISSING class  %s.%s   (%s)" % (ns, cl, os.path.basename(f))); bad += 1
            elif not has_field(classes, ns, cl, fi):
                print("MISSING field  %s.%s::%s   (%s)" % (ns, cl, fi, os.path.basename(f))); bad += 1
        for m in re.finditer(r'\bklass\("([^"]*)",\s*"([^"]*)"\)', txt):
            total += 1
            if not lookup(classes, m.group(1), m.group(2)):
                print("MISSING class  %s.%s   (%s)" % (m.group(1), m.group(2), os.path.basename(f))); bad += 1
        for m in re.finditer(r'(?:\bmethod\(|\bBIND\(\s*[\w:]+\s*,)\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",\s*(-?\d+)', txt):
            total += 1
            ns, cl, me, ac = m.group(1), m.group(2), m.group(3), int(m.group(4))
            if not lookup(classes, ns, cl):
                print("MISSING class  %s.%s   (%s)" % (ns, cl, os.path.basename(f))); bad += 1
            elif not has_method(classes, ns, cl, me, -1):
                print("MISSING method %s.%s::%s/%d   (%s)" % (ns, cl, me, ac, os.path.basename(f))); bad += 1
        for m in re.finditer(r'(?:\bfield\(|\bFIELD\(\s*[\w:]+\s*,)\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)"\)', txt):
            total += 1
            ns, cl, fi = m.group(1), m.group(2), m.group(3)
            if not lookup(classes, ns, cl):
                print("MISSING class  %s.%s   (%s)" % (ns, cl, os.path.basename(f))); bad += 1
            elif not has_field(classes, ns, cl, fi):
                print("MISSING field  %s.%s::%s   (%s)" % (ns, cl, fi, os.path.basename(f))); bad += 1
    print("checked %d lookups, %d missing" % (total, bad))
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
