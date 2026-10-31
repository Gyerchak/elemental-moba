"""[U-485] League of Legends model → our sprite frames (run INSIDE Blender, called by tools/art/lol_skin_make.py).

    blender -b --factory-startup -P tools/art/lol_skin.py -- job.json

Reads the League files directly (no add-ons): .skn (mesh, v1–v4), .skl (new rig format), .anm compressed
(r3d2canm) and uncompressed v4 (r3d2anmd), a texture already converted to PNG. Skinning is done here with numpy:
global joint matrix × inverse bind matrix, weighted per vertex. For every animation of ours (idle / walk / cast /
attack / death) the League animation is sampled at even times; each pose is rendered in the 8 game directions with
the same camera / light as tools/art/blender_models.py, as <anim>_<dir>_<frame>.png.
"""
import json
import math
import os
import struct
import sys

import bpy
import numpy as np

PITCH = math.radians(51.7)  # (same as blender_models.py)
FRAMES = {"idle": 2, "walk": 6, "cast": 3, "attack": 3, "death": 3}


# ---------------------------------------------------------------- League files
def read_skn(path):
    b = open(path, "rb").read()
    magic, major, minor = struct.unpack_from("<IHH", b, 0)
    assert magic == 0x00112233, "not a .skn"
    o = 8
    subs = []
    if major > 0:
        (n,) = struct.unpack_from("<I", b, o)
        o += 4
        for _ in range(n):
            name = b[o:o + 64].split(b"\0")[0].decode(errors="replace")
            sv, vc, si, ic = struct.unpack_from("<4I", b, o + 64)
            subs.append((name, sv, vc, si, ic))
            o += 80
    vtype = 0
    if major == 4:
        o += 4  # flags
    ic, vc = struct.unpack_from("<2I", b, o)
    o += 8
    vsize = 52
    if major == 4:
        vsize, vtype = struct.unpack_from("<2I", b, o)
        o += 8 + 24 + 16  # bounding box + sphere
    idx = np.frombuffer(b, dtype="<u2", count=ic, offset=o).astype(np.int64)
    o += ic * 2
    raw = np.frombuffer(b, dtype=np.uint8, count=vc * vsize, offset=o).reshape(vc, vsize)
    pos = raw[:, 0:12].copy().view("<f4").reshape(vc, 3)
    bones = raw[:, 12:16].astype(np.int64)
    weights = raw[:, 16:32].copy().view("<f4").reshape(vc, 4)
    uv = raw[:, 44:52].copy().view("<f4").reshape(vc, 2)
    return pos, bones, weights, uv, idx.reshape(-1, 3), subs


def trs(t, q, s):
    x, y, z, w = q
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * np.asarray(s)[None, :]
    m[:3, 3] = t
    return m


def read_skl(path):
    b = open(path, "rb").read()
    size, token, version = struct.unpack_from("<3I", b, 0)
    assert token == 0x22FD4FC3, "only the new .skl format is supported"
    flags, jc, infc = struct.unpack_from("<HHI", b, 12)
    joff, jidx, infoff = struct.unpack_from("<3i", b, 20)
    joints = []
    for i in range(jc):
        o = joff + i * 100
        jflags, jid, parent, _pad, nhash, radius = struct.unpack_from("<HhhhIf", b, o)
        lt = struct.unpack_from("<3f", b, o + 16)
        ls = struct.unpack_from("<3f", b, o + 28)
        lr = struct.unpack_from("<4f", b, o + 40)
        it = struct.unpack_from("<3f", b, o + 56)
        is_ = struct.unpack_from("<3f", b, o + 68)
        ir = struct.unpack_from("<4f", b, o + 80)
        (noff,) = struct.unpack_from("<i", b, o + 96)
        name = b[o + 96 + noff:].split(b"\0")[0].decode(errors="replace")
        joints.append(dict(id=jid, parent=parent, hash=nhash, name=name, lt=lt, ls=ls, lr=lr, inv=trs(it, ir, is_)))
    infl = list(struct.unpack_from(f"<{infc}H", b, infoff))
    return joints, infl


def elf_lower(s):
    h = 0
    for c in s.lower().encode():
        h = ((h << 4) + c) & 0xFFFFFFFF
        g = h & 0xF0000000
        if g:
            h ^= g >> 24
        h &= ~g & 0xFFFFFFFF
    return h


def quat_48(v0, v1, v2):
    bits = v0 | (v1 << 16) | (v2 << 32)
    mi = (bits >> 45) & 3
    s2 = 1.41421356237
    a = ((bits >> 30) & 0x7FFF) / 32767.0 * s2 - 1 / s2
    b = ((bits >> 15) & 0x7FFF) / 32767.0 * s2 - 1 / s2
    c = (bits & 0x7FFF) / 32767.0 * s2 - 1 / s2
    d = math.sqrt(max(0.0, 1 - (a * a + b * b + c * c)))
    return [(d, a, b, c), (a, d, b, c), (a, b, d, c), (a, b, c, d)][mi]


def read_anm(path):
    """→ duration, {joint hash: {"r": [(t, q)], "t": [(t, v)], "s": [(t, v)]}}"""
    b = open(path, "rb").read()
    magic = b[:8]
    tracks = {}
    if magic == b"r3d2canm":
        (jc, fc, jcc, dur, fps) = struct.unpack_from("<iiiff", b, 24)
        tmin = np.array(struct.unpack_from("<3f", b, 68)); tmax = np.array(struct.unpack_from("<3f", b, 80))
        smin = np.array(struct.unpack_from("<3f", b, 92)); smax = np.array(struct.unpack_from("<3f", b, 104))
        foff, jcoff, hoff = struct.unpack_from("<3i", b, 116)
        hashes = struct.unpack_from(f"<{jc}I", b, hoff + 12)
        for h in hashes:
            tracks[h] = {"r": [], "t": [], "s": []}
        for i in range(fc):
            t16, ji, v0, v1, v2 = struct.unpack_from("<5H", b, foff + 12 + i * 10)
            t = t16 / 65535.0 * dur
            joint, kind = ji & 0x3FFF, ji >> 14
            if joint >= jc:
                continue
            tr = tracks[hashes[joint]]
            if kind == 0:
                tr["r"].append((t, quat_48(v0, v1, v2)))
            elif kind == 1:
                tr["t"].append((t, tmin + (tmax - tmin) * np.array([v0, v1, v2]) / 65535.0))
            else:
                tr["s"].append((t, smin + (smax - smin) * np.array([v0, v1, v2]) / 65535.0))
        return dur, tracks
    if magic == b"r3d2anmd":
        (version,) = struct.unpack_from("<I", b, 8)
        if version != 4:
            raise ValueError(f"r3d2anmd v{version} not supported (v4 only)")
        # v4 (read from the files): track count, frame count, frame time, offsets (+12) of vectors / quaternions / frames;
        # every frame has one entry per track: joint hash, translation / scale / rotation index
        tc, fc, fdur = struct.unpack_from("<iif", b, 28)
        voff, qoff, foff = struct.unpack_from("<3i", b, 52)
        vec = np.frombuffer(b, dtype="<f4", count=(qoff - voff) // 4, offset=voff + 12).reshape(-1, 3)
        quats = np.frombuffer(b, dtype="<f4", count=(foff - qoff) // 4, offset=qoff + 12).reshape(-1, 4)
        for f in range(fc):
            for k in range(tc):
                h, ti, si, ri, _ = struct.unpack_from("<I4H", b, foff + 12 + (f * tc + k) * 12)
                tr = tracks.setdefault(h, {"r": [], "t": [], "s": []})
                t = f * fdur
                tr["t"].append((t, vec[ti])); tr["s"].append((t, vec[si])); tr["r"].append((t, quats[ri]))
        return fc * fdur, tracks
    raise ValueError(f"unknown animation format {magic!r}")


def sample(keys, t, default, quat=False):
    if not keys:
        return default
    keys = sorted(keys, key=lambda k: k[0])
    if t <= keys[0][0]:
        return keys[0][1]
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if t0 <= t <= t1:
            u = 0.0 if t1 <= t0 else (t - t0) / (t1 - t0)
            a = np.asarray(a, dtype=float); b = np.asarray(b, dtype=float)
            if quat:
                if np.dot(a, b) < 0:
                    b = -b
                q = a * (1 - u) + b * u
                return q / max(1e-9, np.linalg.norm(q))
            return a * (1 - u) + b * u
    return keys[-1][1]


def pose(joints, tracks, t):
    """skin matrices (joint index → 4×4) at time t; joints without a track keep the bind pose"""
    by_id = {j["id"]: i for i, j in enumerate(joints)}
    glob = [None] * len(joints)
    for i, j in enumerate(joints):  # parents come first in .skl
        tr = tracks.get(j["hash"]) or tracks.get(elf_lower(j["name"])) or {}
        lt = sample(tr.get("t"), t, j["lt"]); ls = sample(tr.get("s"), t, j["ls"]); lr = sample(tr.get("r"), t, j["lr"], quat=True)
        m = trs(lt, lr, ls)
        p = j["parent"]
        glob[i] = glob[by_id[p]] @ m if p >= 0 and p in by_id else m
    return [glob[i] @ joints[i]["inv"] for i in range(len(joints))]


# ---------------------------------------------------------------- Blender
def scene(job):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = job.get("samples", 12)
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.58, 0.65, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    sc.world = world
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 2.6
    so = bpy.data.objects.new("sun", sun)
    so.rotation_euler = (math.radians(40), math.radians(-25), math.radians(-30))
    sc.collection.objects.link(so)
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = job["ortho"]
    co = bpy.data.objects.new("cam", cam)
    sc.collection.objects.link(co)
    sc.camera = co
    co.location = (0, -30 * math.cos(PITCH), 30 * math.sin(PITCH))
    co.rotation_euler = (math.pi / 2 - PITCH, 0, 0)
    cam.shift_y = job["anchor"] - 0.5
    sc.render.resolution_x = sc.render.resolution_y = job["px"]
    return sc


def main():
    job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    pos, bones, weights, uv, tris, _subs = read_skn(job["skn"])
    joints, infl = read_skl(job["skl"])
    jb = np.array([infl[b] if b < len(infl) else 0 for b in bones.reshape(-1)]).reshape(bones.shape)
    by_id = {j["id"]: i for i, j in enumerate(joints)}
    jb = np.vectorize(lambda x: by_id.get(int(x), 0))(jb)
    # League (y up, left-handed) → Blender (z up): (x, y, z) → (−x, −z, y); the model faces −Y like ours
    def to_blender(p):
        return np.stack([-p[:, 0], -p[:, 2], p[:, 1]], axis=1)
    bind = to_blender(pos)
    lo, hi = bind.min(axis=0), bind.max(axis=0)
    scale = job["height"] / max(1e-6, hi[2] - lo[2])
    centre = np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]])

    sc = scene(job)
    root = bpy.data.objects.new("root", None)
    sc.collection.objects.link(root)
    me = bpy.data.meshes.new("lol")
    me.from_pydata(((bind - centre) * scale).tolist(), [], tris.tolist())
    uvl = me.uv_layers.new()
    loop_uv = np.array([(uv[v][0], 1.0 - uv[v][1]) for poly in me.polygons for v in poly.vertices], dtype=np.float32)
    uvl.data.foreach_set("uv", loop_uv.reshape(-1))
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("lol", me)
    ob.parent = root
    sc.collection.objects.link(ob)
    # textures: one for everything, or per sub-mesh ({"default": png, "<sub-mesh name>": png})
    texs = job["texture"] if isinstance(job["texture"], dict) else {"default": job["texture"]}
    mats = {}
    def material_for(path):
        if path not in mats:
            mat = bpy.data.materials.new(os.path.basename(path))
            mat.use_nodes = True
            nt = mat.node_tree
            bsdf = nt.nodes["Principled BSDF"]
            bsdf.inputs["Roughness"].default_value = 0.7
            img = nt.nodes.new("ShaderNodeTexImage")
            img.image = bpy.data.images.load(path)
            nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
            me.materials.append(mat)
            mats[path] = len(me.materials) - 1
        return mats[path]
    tri_mat = np.zeros(len(tris), dtype=np.int32)
    for name, _sv, _vc, si, ic in _subs or [("", 0, 0, 0, len(tris) * 3)]:
        tri_mat[si // 3:(si + ic) // 3] = material_for(texs.get(name, texs["default"]))
    me.polygons.foreach_set("material_index", tri_mat)

    out = job["out"]
    os.makedirs(out, exist_ok=True)
    done = {}  # key → (frames, fps), for packing
    for anim, spec in job["anims"].items():
        path = spec["file"] if isinstance(spec, dict) else spec
        dur, tracks = read_anm(path)
        # frames: by length (about 5 per second), at least 2, at most job["max_frames"]; fps so it plays at real speed
        n = FRAMES.get(anim) if job.get("fixed_frames") else max(2, min(job.get("max_frames", 5), round(dur * 5)))
        hold = anim in ("death",) or anim.startswith("cast") or anim.startswith("form_cast") or "attack" in anim or anim == "crit"
        done[anim] = (n, max(1, round(n / max(0.1, dur))))
        for f in range(n):
            t = dur * (f / max(1, n - 1) if hold else f / n)  # one-shot animations end on their last pose
            skin = pose(joints, tracks, t)
            v4 = np.concatenate([pos, np.ones((len(pos), 1))], axis=1)
            res = np.zeros((len(pos), 3))
            for k in range(4):  # 4 influences per vertex
                w = weights[:, k:k + 1]
                if not np.any(w):
                    continue
                mats = np.stack([skin[j] for j in jb[:, k]])  # (V, 4, 4)
                res += w * np.einsum("vij,vj->vi", mats, v4)[:, :3]
            co = (to_blender(res) - centre) * scale
            me.vertices.foreach_set("co", co.astype(np.float32).reshape(-1))
            me.update()
            for d in range(8):
                root.rotation_euler.z = math.radians(90 - d * 45)
                sc.render.filepath = os.path.join(out, f"{anim}_{d}_{f}.png")
                bpy.ops.render.render(write_still=True)
        print("ANIM DONE", anim, flush=True)
    json.dump(done, open(os.path.join(out, "frames.json"), "w"))
    open(os.path.join(out, "done"), "w").write("ok")


main()
