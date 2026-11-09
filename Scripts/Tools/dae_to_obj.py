"""Converts a simple Collada (.dae) file to OBJ, for models Unreal can't import (it has no Collada importer, and
Blender 5 dropped Collada too). Handles what Assimp-exported files use: <polylist>/<triangles> with VERTEX, NORMAL and
TEXCOORD inputs, node matrices (nested), and material bindings. Everything is merged into ONE object (Unreal's OBJ
import makes a mesh per group) with `usemtl` per material, positions converted from the file's unit to centimetres.
Output is Z-up (Collada's Y-up rotated: x, -z, y), because Unreal's OBJ import reads OBJ as Z-up.

  python dae_to_obj.py <in.dae> <out.obj> [scale] [top]
scale: multiplies positions; default converts the file's <unit> to cm. Check the printed size: Blender exports often
put a x100 into the node matrices already (the worn overhead light: pass 1). "top" moves the pivot to the top centre
(for things hung from a ceiling).
(Runs with any Python 3, e.g. Blender's: blender --background --factory-startup --python dae_to_obj.py -- in out)
"""
import sys
import xml.etree.ElementTree as ET

NS = "{http://www.collada.org/2005/11/COLLADASchema}"


def floats(text):
    return [float(v) for v in text.split()]


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def transform_point(m, p):
    return [m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3)]


def transform_normal(m, n):
    # Rotation part only (the files we convert have no shear); renormalised
    v = [m[i][0] * n[0] + m[i][1] * n[1] + m[i][2] * n[2] for i in range(3)]
    length = sum(c * c for c in v) ** 0.5 or 1.0
    return [c / length for c in v]


def main(src, dst, scale=None, pivot_top=False):
    root = ET.parse(src).getroot()
    unit = root.find(NS + "asset/" + NS + "unit")
    to_cm = float(unit.get("meter", "1")) * 100.0 if unit is not None else 100.0
    if scale is not None:
        to_cm = scale

    materials = {m.get("id"): m.get("name") or m.get("id") for m in root.iter(NS + "material")}
    geometries = {g.get("id"): g for g in root.iter(NS + "geometry")}

    name = dst.replace("\\", "/").split("/")[-1].rsplit(".", 1)[0]
    out = ["# converted from %s by dae_to_obj.py (units: cm, Z-up)" % src.replace("\\", "/").split("/")[-1], "o " + name]
    counts = {"v": 0, "vt": 0, "vn": 0}
    faces = 0

    def visit(node, parent):
        nonlocal faces
        m = parent
        matrix = node.find(NS + "matrix")
        if matrix is not None:
            values = floats(matrix.text)
            m = mat_mul(parent, [values[0:4], values[4:8], values[8:12], values[12:16]])
        for inst in node.findall(NS + "instance_geometry"):
            geometry = geometries[inst.get("url").lstrip("#")]
            binding = {im.get("symbol"): materials.get(im.get("target").lstrip("#"), im.get("target").lstrip("#"))
                       for im in inst.iter(NS + "instance_material")}
            mesh = geometry.find(NS + "mesh")
            sources = {s.get("id"): s for s in mesh.findall(NS + "source")}

            def data(source_id):
                s = sources[source_id.lstrip("#")]
                arr = floats(s.find(NS + "float_array").text)
                stride = int(s.find(NS + "technique_common/" + NS + "accessor").get("stride"))
                return [arr[i:i + stride] for i in range(0, len(arr), stride)]

            # <vertices> maps VERTEX to its POSITION source
            vertices = mesh.find(NS + "vertices")
            position_id = [i.get("source") for i in vertices.findall(NS + "input") if i.get("semantic") == "POSITION"][0]

            for prim in list(mesh.findall(NS + "polylist")) + list(mesh.findall(NS + "triangles")):
                inputs = prim.findall(NS + "input")
                stride = max(int(i.get("offset")) for i in inputs) + 1
                offsets = {}
                arrays = {}
                for i in inputs:
                    semantic = i.get("semantic")
                    if semantic in offsets:
                        continue  # second UV set etc.
                    offsets[semantic] = int(i.get("offset"))
                    arrays[semantic] = data(position_id if semantic == "VERTEX" else i.get("source"))
                base = dict(counts)
                for p in arrays["VERTEX"]:
                    q = transform_point(m, p)
                    out.append("v %.6f %.6f %.6f" % (q[0] * to_cm, -q[2] * to_cm, q[1] * to_cm))
                counts["v"] += len(arrays["VERTEX"])
                if "TEXCOORD" in arrays:
                    for t in arrays["TEXCOORD"]:
                        out.append("vt %.6f %.6f" % (t[0], t[1]))
                    counts["vt"] += len(arrays["TEXCOORD"])
                if "NORMAL" in arrays:
                    for n in arrays["NORMAL"]:
                        nn = transform_normal(m, n)
                        out.append("vn %.6f %.6f %.6f" % (nn[0], -nn[2], nn[1]))
                    counts["vn"] += len(arrays["NORMAL"])
                out.append("usemtl %s" % binding.get(prim.get("material"), prim.get("material")))
                indices = [int(v) for v in prim.find(NS + "p").text.split()]
                vcount_el = prim.find(NS + "vcount")
                sizes = [int(v) for v in vcount_el.text.split()] if vcount_el is not None else [3] * (len(indices) // (3 * stride))
                cursor = 0
                for size in sizes:
                    corners = []
                    for c in range(size):
                        at = (cursor + c) * stride
                        v = indices[at + offsets["VERTEX"]] + base["v"] + 1
                        t = indices[at + offsets["TEXCOORD"]] + base["vt"] + 1 if "TEXCOORD" in offsets else None
                        n = indices[at + offsets["NORMAL"]] + base["vn"] + 1 if "NORMAL" in offsets else None
                        corners.append("%d/%s/%s" % (v, t if t else "", n if n else ""))
                    out.append("f " + " ".join(corners))
                    cursor += size
                    faces += 1
        for child in node.findall(NS + "node"):
            visit(child, m)

    identity = [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for scene in root.iter(NS + "visual_scene"):
        for node in scene.findall(NS + "node"):
            visit(node, identity)
    if pivot_top:
        # Shift every position so the top centre (Z is up) is the origin
        xs, ys, zs = [], [], []
        for line in out:
            if line.startswith("v "):
                x, y, z = (float(c) for c in line.split()[1:4])
                xs.append(x); ys.append(y); zs.append(z)
        offset = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, max(zs))
        out = [("v %.6f %.6f %.6f" % tuple(float(c) - o for c, o in zip(line.split()[1:4], offset))) if line.startswith("v ") else line
               for line in out]
        print("DAE2OBJ pivot moved to the top centre (offset %s)" % (offset,))
    with open(dst, "w") as f:
        f.write("\n".join(out) + "\n")
    print("DAE2OBJ wrote %s: %d vertices, %d faces, materials %s" % (dst, counts["v"], faces, sorted(set(materials.values()))))


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    main(args[0], args[1], float(args[2]) if len(args) > 2 else None, len(args) > 3 and args[3] == "top")
