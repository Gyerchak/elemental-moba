"""[U-504] Champion art → a textured 3D model → sprite frames (run INSIDE Blender, called by tools/art/art_models.py).

    blender -b --factory-startup -P tools/art/art_model_blender.py -- job.json

The full-body front picture is cut out (flood fill of the plain background), its silhouette is inflated into a rounded,
closed body (thickness from the distance to the edge), the picture is the texture. Arms, legs, head and torso get soft
weights from the body's proportions; poses rotate them around their joints with numpy. Same camera + light as
blender_models.py; frames <anim>_<dir>_<frame>.png.
"""
import json
import math
import os
import sys

import bpy
import numpy as np

PITCH = math.radians(51.7)


def load_picture(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    return img, px.reshape(h, w, 4)[::-1]  # row 0 = the top of the picture


def silhouette(px, gw):
    """the figure on a grid gw wide: flood fill from the border through background-like pixels, largest piece kept"""
    h, w = px.shape[:2]
    gh = int(gw * h / w)
    ys = (np.arange(gh) + 0.5) * h / gh
    xs = (np.arange(gw) + 0.5) * w / gw
    g = px[ys.astype(int)][:, xs.astype(int), :3]
    if px.shape[2] == 4 and px[..., 3].min() < 0.5:  # a picture with real transparency
        fg = px[ys.astype(int)][:, xs.astype(int), 3] > 0.5
    else:
        bg_col = np.median(np.concatenate([g[0], g[-1], g[:, 0], g[:, -1]]), axis=0)
        near = np.abs(g - bg_col).max(axis=2) < 0.10
        seen = np.zeros((gh, gw), bool)
        stack = [(y, x) for y in range(gh) for x in (0, gw - 1) if near[y, x]] + [(y, x) for x in range(gw) for y in (0, gh - 1) if near[y, x]]
        while stack:
            y, x = stack.pop()
            if seen[y, x] or not near[y, x]:
                continue
            seen[y, x] = True
            for a, b in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= a < gh and 0 <= b < gw and not seen[a, b]:
                    stack.append((a, b))
        fg = ~seen
    # the largest connected piece (drops a signature in a corner)
    label = np.zeros((gh, gw), int)
    best, best_n, cur = 0, 0, 0
    for y0 in range(gh):
        for x0 in range(gw):
            if fg[y0, x0] and not label[y0, x0]:
                cur += 1
                n = 0
                stack = [(y0, x0)]
                label[y0, x0] = cur
                while stack:
                    y, x = stack.pop()
                    n += 1
                    for a, b in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                        if 0 <= a < gh and 0 <= b < gw and fg[a, b] and not label[a, b]:
                            label[a, b] = cur
                            stack.append((a, b))
                if n > best_n:
                    best, best_n = cur, n
    return label == best


def distance(mask):
    """chamfer distance to the outside, in grid cells"""
    h, w = mask.shape
    d = np.where(mask, 1e6, 0.0)
    for y in range(h):
        for x in range(w):
            if d[y, x]:
                up = d[y - 1, x] + 1 if y else 1
                left = d[y, x - 1] + 1 if x else 1
                d[y, x] = min(d[y, x], up, left)
    for y in range(h - 1, -1, -1):
        for x in range(w - 1, -1, -1):
            if d[y, x]:
                down = d[y + 1, x] + 1 if y < h - 1 else 1
                right = d[y, x + 1] + 1 if x < w - 1 else 1
                d[y, x] = min(d[y, x], down, right)
    return d


def build(job):
    img, px = load_picture(job["picture"])
    mask = silhouette(px, job.get("grid", 150))
    gh, gw = mask.shape
    rows = np.where(mask.any(axis=1))[0]
    top, bottom = rows[0], rows[-1]
    H_cells = bottom - top + 1
    d = distance(mask)
    # thickness: round like a tube, limited so wide robes do not become barrels
    dmax = float(d.max())
    thick = 0.85 * np.sqrt(np.clip(d * (2.0 * dmax - d), 0, None)) * np.minimum(1.0, 0.55 + d / max(1.0, dmax))
    scale = job["height"] / float(H_cells)
    # feet centre = x 0
    foot = mask[bottom - int(H_cells * 0.12):bottom + 1].any(axis=0)
    cols = np.where(foot)[0]
    cx = (cols[0] + cols[-1]) / 2 if len(cols) else gw / 2
    index = {}
    verts, uvs, back_flags = [], [], []
    def vert(ix, iy, side):
        key = (ix, iy, side)
        if key in index:
            return index[key]
        # a grid corner: its thickness = the mean of the 4 cells around it (0 on the edge: front and back meet there)
        cells = [(iy - a, ix - b) for a in (0, 1) for b in (0, 1) if 0 <= iy - a < gh and 0 <= ix - b < gw]
        inside = all(mask[c] for c in cells) and len(cells) == 4
        t = float(np.mean([thick[c] for c in cells])) if inside else 0.0
        if t == 0.0 and side == 1:  # the edge: shared with the front
            return vert(ix, iy, 0)
        key = (ix, iy, side)
        index[key] = len(verts)
        verts.append(((ix - cx) * scale, (t if side else -t) * scale, (bottom + 1 - iy) * scale))
        uvs.append(((ix) / gw, 1.0 - iy / gh))
        back_flags.append(side)
        return index[key]
    faces, face_back = [], []
    for y in range(gh):
        for x in range(gw):
            if not mask[y, x]:
                continue
            a, b, c, e = vert(x, y, 0), vert(x + 1, y, 0), vert(x + 1, y + 1, 0), vert(x, y + 1, 0)
            faces.append((e, c, b, a))
            face_back.append(0)
            a, b, c, e = vert(x, y, 1), vert(x + 1, y, 1), vert(x + 1, y + 1, 1), vert(x, y + 1, 1)
            faces.append((a, b, c, e))
            face_back.append(1)
    return img, np.array(verts, dtype=np.float64), np.array(uvs, dtype=np.float32), faces, np.array(face_back), scale, mask, (top, bottom, cx), np.array(back_flags)


def bleed(img, px, mask, rounds=14):
    """the background colour must never reach the texture: outside the figure, repeat the nearest figure colours"""
    h, w = px.shape[:2]
    gh, gw = mask.shape
    # a little inside the silhouette too: its edge pixels are mixed with the background
    m = mask[(np.arange(h) * gh // h)][:, (np.arange(w) * gw // w)].copy()
    white = px[..., :3].min(axis=2) > 0.9
    m &= ~white | np.roll(m, 3, 0) & np.roll(m, -3, 0) & np.roll(m, 3, 1) & np.roll(m, -3, 1)
    c = px[..., :3].copy()
    c[~m] = 0
    for _ in range(rounds):
        n = np.zeros_like(c)
        k = np.zeros((h, w), np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (2, 0), (-2, 0), (0, 2), (0, -2)):
            ok = np.roll(np.roll(m, dy, 0), dx, 1)
            n += np.roll(np.roll(c, dy, 0), dx, 1) * ok[..., None]
            k += ok
        grow = ~m & (k > 0)
        c[grow] = (n[grow] / k[grow][..., None])
        m = m | grow
    out = np.concatenate([c, np.ones((h, w, 1), np.float32)], axis=2)[::-1]
    img.pixels.foreach_set(out.reshape(-1))
    img.update()


def hair_uv(px, mask, info):
    """a texel of the hair: among the head's pixels, the dark end (the back of the head is painted with it)"""
    top, bottom, cx = info
    h, w = px.shape[:2]
    gh, gw = mask.shape
    y0, y1 = int(top * h / gh), int((top + 0.14 * (bottom - top)) * h / gh)
    half = int(0.09 * (bottom - top) * w / gw)
    xc = int(cx * w / gw)
    box = px[y0:y1, max(0, xc - half):xc + half, :3]
    lum = box.mean(axis=2)
    keep = lum < 0.9
    if not keep.any():
        return 0.5, 1.0 - (y0 + 2) / h
    t = np.percentile(lum[keep], 25)
    ys, xs = np.where(keep & (np.abs(lum - t) < 0.02))
    i = len(ys) // 2
    return (max(0, xc - half) + xs[i]) / w, 1.0 - (y0 + ys[i]) / h


def smooth(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def weights(v, height, mask, info, scale):
    """soft body parts from proportions: arms outside the torso, legs under the hips, the head on top"""
    top, bottom, cx = info
    z = v[:, 2] / height
    x = v[:, 0]
    # torso half width: the narrowest figure width between the chest and the waist (the arms are outside it)
    widths = []
    for f in np.linspace(0.58, 0.74, 9):
        row = int(bottom - f * (bottom - top))
        col = np.where(mask[row])[0]
        if len(col):
            # the run of cells around the centre = the torso
            c = int(cx)
            l, r = c, c
            while l > 0 and mask[row, l - 1]:
                l -= 1
            while r < mask.shape[1] - 1 and mask[row, r + 1]:
                r += 1
            widths.append((r - l + 1) / 2 * scale)
    tw = float(np.clip(np.median(widths) if widths else 0.2 * height, 0.10 * height, 0.17 * height))
    w = {}
    arm_zone = smooth(0.40, 0.46, z) * (1 - smooth(0.84, 0.88, z))
    w["armR"] = smooth(tw * 0.9, tw * 1.35, -x) * arm_zone
    w["armL"] = smooth(tw * 0.9, tw * 1.35, x) * arm_zone
    leg = 1 - smooth(0.40, 0.52, z)
    w["legR"] = leg * smooth(0.0, 0.05 * height, -x + 0.02 * height)
    w["legL"] = leg * smooth(0.0, 0.05 * height, x + 0.02 * height)
    w["head"] = smooth(0.84, 0.88, z)
    w["spine"] = smooth(0.45, 0.70, z)
    joints = {"armR": (-tw, 0, 0.82 * height), "armL": (tw, 0, 0.82 * height), "legR": (-0.08 * height, 0, 0.50 * height),
              "legL": (0.08 * height, 0, 0.50 * height), "head": (0, 0, 0.85 * height), "spine": (0, 0, 0.48 * height)}
    return w, joints


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return lambda p: np.stack([p[:, 0], p[:, 1] * c - p[:, 2] * s, p[:, 1] * s + p[:, 2] * c], axis=1)


def rotate(v, weight, joint, ax=0.0, ay=0.0):
    """rotate by weight × angle around a joint (x: swing forward / back; y: out to the side)"""
    out = v.copy()
    j = np.array(joint)
    p = v - j
    a = weight * ax
    if ax:
        c, s = np.cos(a), np.sin(a)
        y, z = p[:, 1] * c - p[:, 2] * s, p[:, 1] * s + p[:, 2] * c
        p = np.stack([p[:, 0], y, z], axis=1)
    if ay:
        b = weight * ay
        c, s = np.cos(b), np.sin(b)
        x, z = p[:, 0] * c + p[:, 2] * s, -p[:, 0] * s + p[:, 2] * c
        p = np.stack([x, p[:, 1], z], axis=1)
    return p + j


def pose(v0, w, j, height, anim, t):
    """a pose; forward = −Y, so a positive x rotation lifts a hanging limb forward"""
    v = v0.copy()
    s = math.sin(2 * math.pi * t)
    r = math.radians
    if anim == "idle":
        v = rotate(v, w["spine"], j["spine"], ax=r(1.5) * s)
        v = rotate(v, w["armR"], j["armR"], ax=r(3) * s)
        v = rotate(v, w["armL"], j["armL"], ax=-r(3) * s)
        v[:, 2] += 0.006 * height * s
    elif anim == "walk":
        v = rotate(v, w["legR"], j["legR"], ax=r(26) * s)
        v = rotate(v, w["legL"], j["legL"], ax=-r(26) * s)
        v = rotate(v, w["armR"], j["armR"], ax=-r(24) * s)
        v = rotate(v, w["armL"], j["armL"], ax=r(24) * s)
        v = rotate(v, w["spine"], j["spine"], ax=-r(4))
        v[:, 2] += 0.012 * height * abs(math.cos(2 * math.pi * t))
    elif anim == "attack":
        q = math.sin(math.pi * min(1.0, t * 1.2))
        v = rotate(v, w["armR"], j["armR"], ax=-r(115) * q + r(60) * q * t)
        v = rotate(v, w["armL"], j["armL"], ax=r(20) * q)
        v = rotate(v, w["spine"], j["spine"], ax=-r(16) * q)
    elif anim == "cast":
        q = math.sin(math.pi * min(1.0, t * 1.1))
        v = rotate(v, w["armR"], j["armR"], ax=-r(95) * q, ay=r(10) * q)
        v = rotate(v, w["armL"], j["armL"], ax=-r(95) * q, ay=-r(10) * q)
        v = rotate(v, w["head"], j["head"], ax=r(8) * q)
    elif anim == "death":  # falls on its back; the last frame = the body lying
        q = min(1.0, t * 1.15)
        q = q * q * (3 - 2 * q)
        v = rotate(v, w["armR"], j["armR"], ay=-r(35) * q)
        v = rotate(v, w["armL"], j["armL"], ay=r(35) * q)
        v = rotate(v, np.ones(len(v)), (0, 0, 0), ax=-r(86) * q)
        v[:, 2] = np.maximum(v[:, 2], 0.0) + 0.03 * height * q
    return v


def main():
    job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    if os.environ.get("EM_GPU"):
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for kind in ("OPTIX", "CUDA"):
            try:
                prefs.compute_device_type = kind
                prefs.get_devices()
                if any(d.type == kind for d in prefs.devices):
                    for d in prefs.devices:
                        d.use = d.type == kind
                    sc.cycles.device = "GPU"
                    break
            except TypeError:
                pass
    sc.cycles.samples = job.get("samples", 24)
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.75, 0.77, 0.82, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.85
    sc.world = world
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 1.8
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

    img, v0, uv, faces, face_back, scale, mask, info, _ = build(job)
    _, px_full = img, np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(px_full)
    px_full = px_full.reshape(img.size[1], img.size[0], 4)[::-1]
    hair_u, hair_v = hair_uv(px_full, mask, info)
    bleed(img, px_full, mask)
    height = job["height"]
    w, j = weights(v0, height, mask, info, scale)
    root = bpy.data.objects.new("root", None)
    sc.collection.objects.link(root)
    me = bpy.data.meshes.new("body")
    me.from_pydata(v0.tolist(), [], faces)
    layer = me.uv_layers.new()
    # the back of the head must not show the face: back faces there read the hair row instead
    top, bottom, _cx = info
    gh = mask.shape[0]
    loop_uv = []
    head_v = 1.0 - (top + 0.15 * (bottom - top)) / gh
    for poly, back in zip(me.polygons, face_back):
        for vi in poly.vertices:
            u, vv = uv[vi]
            if back and vv > head_v:  # (one hair texel, with a little variation down the head)
                u, vv = hair_u, hair_v
            loop_uv += [u, vv]
    layer.data.foreach_set("uv", np.array(loop_uv, dtype=np.float32))
    for p in me.polygons:
        p.use_smooth = True
    # front = the picture; back = the picture a little darker
    for name, dark in (("front", 1.0), ("back", 0.72)):
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        bsdf.inputs["Roughness"].default_value = 0.65
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = img
        tex.interpolation = "Cubic"
        if dark < 1:
            mul = nt.nodes.new("ShaderNodeHueSaturation")
            mul.inputs["Value"].default_value = dark
            nt.links.new(tex.outputs["Color"], mul.inputs["Color"])
            nt.links.new(mul.outputs["Color"], bsdf.inputs["Base Color"])
        else:
            nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        me.materials.append(mat)
    me.polygons.foreach_set("material_index", face_back.astype(np.int32))
    ob = bpy.data.objects.new("body", me)
    ob.parent = root
    sc.collection.objects.link(ob)
    out = job["out"]
    os.makedirs(out, exist_ok=True)
    for anim, n in job["frames"].items():
        for f in range(n):
            t = f / max(1, n - 1) if anim == "death" else f / n
            co_ = pose(v0, w, j, height, anim, t)
            me.vertices.foreach_set("co", co_.astype(np.float32).reshape(-1))
            me.update()
            for d in range(8):
                # the picture has no real side view: "sideways" is drawn as a 3/4 view (the front stays visible)
                ang = (90 - d * 45 + 180) % 360 - 180
                turned = {0: 0, 45: 30, 90: 60, 135: 125, 180: 180}[abs(ang)]
                root.rotation_euler.z = math.radians(turned if ang >= 0 else -turned)
                sc.render.filepath = os.path.join(out, f"{anim}_{d}_{f}.png")
                bpy.ops.render.render(write_still=True)
    open(os.path.join(out, "done"), "w").write("ok")


main()
