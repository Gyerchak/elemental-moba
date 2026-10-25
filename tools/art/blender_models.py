"""[U-258] Runs INSIDE Blender (headless):  blender -b -P tools/art/blender_models.py -- <manifest.json> <out dir>

Builds simple low-poly placeholder models from primitives (no artist needed), poses them frame by frame and
renders every animation in 8 directions with the game's camera (orthographic, 51.7° down = the tilted view).
Each model also gets saved as .glb (for a later real 3D renderer).

Manifest: a list of {"id", "kind", "body", "colour", "accent", "front" (picture or ""), "px", "ortho", "anims"}.
Output:   <out>/<id>/<anim>_<dir>_<frame>.png  (the driver packs them into one sheet).
"""
import bpy
import json
import math
import os
import sys

argv = sys.argv[sys.argv.index("--") + 1:]
MANIFEST = json.load(open(argv[0]))
OUT = argv[1]
PITCH = math.radians(51.7)  # game view: ground squash 0.78, height factor 0.62


# ---------------------------------------------------------------- scene
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 16
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.58, 0.65, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.45
    sc.world = world
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 2.4
    so = bpy.data.objects.new("sun", sun)
    so.rotation_euler = (math.radians(40), math.radians(-25), math.radians(-30))
    sc.collection.objects.link(so)
    return sc


def camera(sc, ortho, px, anchor_y):
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = ortho
    co = bpy.data.objects.new("cam", cam)
    sc.collection.objects.link(co)
    sc.camera = co
    d = 30.0
    co.location = (0, -d * math.cos(PITCH), d * math.sin(PITCH))
    co.rotation_euler = (math.pi / 2 - PITCH, 0, 0)
    # the ground point (0,0,0) lands at anchor_y of the picture height (from the top)
    cam.shift_y = anchor_y - 0.5
    sc.render.resolution_x = px
    sc.render.resolution_y = px
    return co


def material(name, rgb, image=None, projector=None, rough=0.75, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Roughness"].default_value = rough
    if emit > 0:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
        bsdf.inputs["Emission Strength"].default_value = emit
    if image is not None and projector is not None:
        # front projection of the ComfyUI character picture: projector-local x → u, z → v
        tc = nt.nodes.new("ShaderNodeTexCoord")
        tc.object = projector
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        mu = nt.nodes.new("ShaderNodeMath")
        mu.operation = "MULTIPLY_ADD"
        mu.inputs[1].default_value = 0.6
        mu.inputs[2].default_value = 0.5
        nt.links.new(sep.outputs["X"], mu.inputs[0])
        mv = nt.nodes.new("ShaderNodeMath")
        mv.operation = "MULTIPLY_ADD"
        mv.inputs[1].default_value = 0.92
        mv.inputs[2].default_value = 0.03
        nt.links.new(sep.outputs["Z"], mv.inputs[0])
        comb = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(mu.outputs[0], comb.inputs["X"])
        nt.links.new(mv.outputs[0], comb.inputs["Y"])
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = image
        tex.extension = "EXTEND"
        nt.links.new(comb.outputs[0], tex.inputs["Vector"])
        # white picture background → fall back to the solid colour
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        lum = nt.nodes.new("ShaderNodeRGBToBW")
        nt.links.new(tex.outputs["Color"], lum.inputs[0])
        ramp = nt.nodes.new("ShaderNodeMapRange")
        ramp.inputs["From Min"].default_value = 0.86
        ramp.inputs["From Max"].default_value = 0.97
        nt.links.new(lum.outputs[0], ramp.inputs["Value"])
        nt.links.new(ramp.outputs[0], mix.inputs["Factor"])
        nt.links.new(tex.outputs["Color"], mix.inputs["A"])
        mix.inputs["B"].default_value = (*rgb, 1)
        nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    return m


def empty(name, loc, parent=None):
    o = bpy.data.objects.new(name, None)
    o.location = loc
    o.parent = parent
    bpy.context.scene.collection.objects.link(o)
    return o


def part(kind, name, parent, loc, scale=(1, 1, 1), mat=None, rot=(0, 0, 0), **kw):
    if kind == "cyl":
        bpy.ops.mesh.primitive_cone_add(vertices=kw.get("v", 10), radius1=kw["r1"], radius2=kw["r2"], depth=kw["d"])
    elif kind == "sphere":
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=kw["r"])
    elif kind == "cube":
        bpy.ops.mesh.primitive_cube_add(size=1)
    elif kind == "ico":
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=kw.get("sub", 1), radius=kw["r"])
    o = bpy.context.active_object
    o.name = name
    o.scale = scale
    o.rotation_euler = rot
    o.parent = parent
    o.location = loc
    if mat:
        o.data.materials.append(mat)
    bpy.ops.object.shade_flat()
    return o


def hexrgb(h):
    v = int(h, 16)
    return ((v >> 16 & 255) / 255, (v >> 8 & 255) / 255, (v & 255) / 255)


# ---------------------------------------------------------------- builders
def humanoid(m, root, height=1.8, width=1.0, weapon=None):
    col, acc = hexrgb(m["colour"]), hexrgb(m["accent"])
    img = None
    proj = empty("projector", (0, 0, 0), root)
    proj.scale = (1.2 * width, 1, height)
    if m.get("front") and os.path.exists(m["front"]):
        img = bpy.data.images.load(m["front"])
    body = material("body", col, img, proj)
    skin = material("skin", (0.86, 0.68, 0.55))
    trim = material("trim", acc, rough=0.5)
    dark = material("dark", tuple(c * 0.35 for c in col))
    h = height / 1.8
    J = {}
    hip = empty("hip", (0, 0, 0.9 * h), root)
    J["hip"] = hip
    part("cyl", "torso", hip, (0, 0, 0.3 * h), mat=body, r1=0.2 * width, r2=0.24 * width, d=0.62 * h)
    part("cyl", "belt", hip, (0, 0, 0.05 * h), mat=trim, r1=0.215 * width, r2=0.215 * width, d=0.08 * h)
    neck = empty("neck", (0, 0, 0.62 * h), hip)
    J["neck"] = neck
    part("sphere", "head", neck, (0, 0, 0.14 * h), mat=body if img else skin, r=0.135 * h)
    part("cyl", "band", neck, (0, 0, 0.19 * h), mat=trim, r1=0.14 * h, r2=0.14 * h, d=0.04 * h)
    for side, sx in (("R", -1), ("L", 1)):
        sh = empty(f"sh{side}", (sx * 0.27 * width, 0, 0.56 * h), hip)
        J[f"sh{side}"] = sh
        part("cyl", f"arm{side}", sh, (0, 0, -0.3 * h), mat=body, r1=0.05 * width, r2=0.065 * width, d=0.6 * h)
        part("sphere", f"hand{side}", sh, (0, 0, -0.63 * h), mat=skin, r=0.06 * h)
        lg = empty(f"leg{side}", (sx * 0.1 * width, 0, 0), hip)
        J[f"leg{side}"] = lg
        part("cyl", f"thigh{side}", lg, (0, 0, -0.43 * h), mat=body, r1=0.065 * width, r2=0.085 * width, d=0.86 * h)
        part("cube", f"foot{side}", lg, (0, -0.05, -0.87 * h), scale=(0.1 * width, 0.2, 0.06), mat=dark)
    if weapon == "sword":
        part("cube", "sword", J["shR"], (0, -0.2, -0.65 * h), scale=(0.03, 0.5, 0.06), mat=material("steel", (0.75, 0.77, 0.8), rough=0.3))
    elif weapon == "staff":
        part("cyl", "staff", J["shR"], (0, -0.05, -0.55 * h), rot=(math.radians(10), 0, 0), mat=material("wood", (0.45, 0.3, 0.15)),
             r1=0.025, r2=0.025, d=1.4 * h)
        part("ico", "orb", J["shR"], (0, -0.12, 0.13 * h), mat=material("orb", acc, emit=2.0), r=0.08)
    return J


def quadruped(m, root, size=1.0):
    col, acc = hexrgb(m["colour"]), hexrgb(m["accent"])
    fur = material("fur", col)
    eye = material("eye", acc, emit=3.0)
    J = {}
    body = empty("body", (0, 0, 0.55 * size), root)
    J["hip"] = body
    part("cube", "trunk", body, (0, 0, 0), scale=(0.42 * size, 1.0 * size, 0.42 * size), mat=fur)
    neck = empty("neck", (0, -0.5 * size, 0.15 * size), body)
    J["neck"] = neck
    part("cube", "head", neck, (0, -0.2 * size, 0.08 * size), scale=(0.3 * size, 0.38 * size, 0.28 * size), mat=fur)
    for ex in (-0.08, 0.08):
        part("sphere", "eye", neck, (ex * size, -0.39 * size, 0.14 * size), mat=eye, r=0.035 * size)
    for name, x, y in (("legFR", -0.15, -0.38), ("legFL", 0.15, -0.38), ("legBR", -0.15, 0.38), ("legBL", 0.15, 0.38)):
        lg = empty(name, (x * size, y * size, -0.15 * size), body)
        J[name] = lg
        part("cyl", name + "m", lg, (0, 0, -0.2 * size), mat=fur, r1=0.06 * size, r2=0.08 * size, d=0.42 * size)
    part("cyl", "tail", body, (0, 0.6 * size, 0.1 * size), rot=(math.radians(-60), 0, 0), mat=fur, r1=0.02, r2=0.07 * size, d=0.5 * size)
    return J


def structure(m, root):
    k = m["kind"]
    stone = material("stone", hexrgb(m["colour"]))
    acc = material("acc", hexrgb(m["accent"]), rough=0.4)
    fire = material("fire", (1.0, 0.45, 0.1), emit=6.0)
    if k == "turret":
        part("cyl", "base", root, (0, 0, 0.6), mat=stone, r1=1.1, r2=0.95, d=1.2, v=8)
        part("cyl", "tower", root, (0, 0, 2.6), mat=stone, r1=0.8, r2=0.6, d=2.8, v=8)
        part("cyl", "ring", root, (0, 0, 4.05), mat=acc, r1=0.75, r2=0.75, d=0.15, v=8)
        part("cyl", "bowl", root, (0, 0, 4.3), mat=acc, r1=0.35, r2=0.7, d=0.4, v=8)
        part("ico", "flame", root, (0, 0, 4.7), mat=fire, r=0.35, sub=1)
    elif k == "inhibitor":
        part("cyl", "plinth", root, (0, 0, 0.3), mat=stone, r1=1.0, r2=0.9, d=0.6, v=6)
        part("ico", "crystal", root, (0, 0, 1.4), scale=(0.6, 0.6, 1.2), mat=material("cr", hexrgb(m["accent"]), emit=1.5, rough=0.2), r=0.8, sub=1)
    elif k == "nexus":
        part("cube", "floor", root, (0, 0, 0.3), scale=(3.4, 3.4, 0.6), mat=stone)
        for i, (s, z) in enumerate(((2.6, 1.4), (2.0, 2.5), (1.4, 3.4))):
            part("cube", f"wall{i}", root, (0, 0, z - 0.35), scale=(s * 0.8, s * 0.8, 0.7), mat=stone)
            part("cyl", f"roof{i}", root, (0, 0, z + 0.25), mat=acc, r1=s * 0.85, r2=s * 0.35, d=0.5, v=4, rot=(0, 0, math.radians(45)))
        part("ico", "orb", root, (0, 0, 4.2), mat=material("orb", hexrgb(m["accent"]), emit=4.0), r=0.45, sub=2)
    elif k == "tree":
        bark = material("bark", (0.36, 0.24, 0.13))
        leaf = material("leaf", hexrgb(m["colour"]))
        part("cyl", "trunk", root, (0, 0, 0.9), mat=bark, r1=0.22, r2=0.14, d=1.8, v=7)
        for i, (r, z) in enumerate(((1.4, 2.0), (1.1, 2.9), (0.75, 3.7))):
            part("cyl", f"cone{i}", root, (0, 0, z), mat=leaf, r1=r, r2=0.05, d=1.4, v=8)
    elif k == "bush":
        leaf = material("leaf", hexrgb(m["colour"]))
        for i, (x, y, r) in enumerate(((0, 0, 0.55), (0.45, 0.2, 0.42), (-0.4, 0.25, 0.4), (0.1, -0.35, 0.38))):
            part("ico", f"b{i}", root, (x, y, r * 0.8), mat=leaf, r=r, sub=1)
    return {}


# ---------------------------------------------------------------- poses
def pose(J, kind, anim, t):
    """Set the joints for animation `anim` at phase t (0..1)."""
    for o in J.values():
        o.rotation_euler = (0, 0, 0)
    hip = J.get("hip")
    if hip is not None:  # back to the rest position (z0 is stored once after building)
        hip.location.z = hip["z0"]
        hip.location.y = 0
    s = math.sin(2 * math.pi * t)
    if kind == "quad":
        if anim == "walk":
            for n, ph in (("legFR", 0), ("legBL", 0), ("legFL", 0.5), ("legBR", 0.5)):
                J[n].rotation_euler.x = math.radians(28) * math.sin(2 * math.pi * (t + ph))
        elif anim in ("attack", "cast"):
            J["neck"].rotation_euler.x = math.radians(-25) * math.sin(math.pi * t)
            J["hip"].location.y = -0.15 * math.sin(math.pi * t)
        elif anim == "death":
            J["hip"].rotation_euler.y = math.radians(90) * min(1, t * 1.3)
            J["hip"].location.z = J["hip"]["z0"] - 0.25 * t
        else:
            J["neck"].rotation_euler.x = math.radians(4) * s
        return
    if not J:
        return
    arm_out = math.radians(12)
    J["shR"].rotation_euler.y = arm_out
    J["shL"].rotation_euler.y = -arm_out
    if anim == "idle":
        J["hip"].location.z = J["hip"]["z0"] + 0.012 * s
        J["shR"].rotation_euler.x = math.radians(4) * s
        J["shL"].rotation_euler.x = -math.radians(4) * s
    elif anim == "walk":
        a = math.radians(32) * s
        J["legR"].rotation_euler.x = a
        J["legL"].rotation_euler.x = -a
        J["shR"].rotation_euler.x = -a * 0.8
        J["shL"].rotation_euler.x = a * 0.8
        J["hip"].location.z = J["hip"]["z0"] + 0.03 * abs(math.cos(2 * math.pi * t))
    elif anim == "cast":  # both hands thrust forward, body leans in
        k = math.sin(math.pi * min(1, t * 1.4))
        J["shR"].rotation_euler.x = -math.radians(95) * k
        J["shL"].rotation_euler.x = -math.radians(70) * k
        J["hip"].rotation_euler.x = -math.radians(10) * k
    elif anim == "attack":  # right arm swing from behind to the front
        J["shR"].rotation_euler.x = math.radians(40) - math.radians(140) * t
        J["hip"].rotation_euler.z = math.radians(15) - math.radians(30) * t
    elif anim == "death":
        k = min(1, t * 1.25)
        J["hip"].rotation_euler.x = math.radians(85) * k
        J["hip"].location.z = J["hip"]["z0"] - 0.7 * k
        J["shR"].rotation_euler.x = math.radians(40) * k
        J["shL"].rotation_euler.x = math.radians(30) * k


# ---------------------------------------------------------------- main
FRAMES = {"idle": 2, "walk": 6, "cast": 3, "attack": 3, "death": 3}

for m in MANIFEST:
    out = os.path.join(OUT, m["id"])
    if os.path.exists(os.path.join(out, "done")):
        continue
    os.makedirs(out, exist_ok=True)
    sc = reset()
    root = empty("root", (0, 0, 0))
    k = m["kind"]
    if k == "humanoid":
        J = humanoid(m, root, height=m.get("height", 1.8), width={"slim": 0.85, "broad": 1.3}.get(m["body"], 1.0), weapon=m.get("weapon"))
        kind = "human"
    elif k == "quad":
        J = quadruped(m, root, size=m.get("size", 1.0))
        kind = "quad"
    else:
        J = structure(m, root)
        kind = "static"
    for o in J.values():
        o["z0"] = o.location.z
    camera(sc, m["ortho"], m["px"], m.get("anchor", 0.8))
    bpy.ops.export_scene.gltf(filepath=os.path.join(out, "model.glb"), export_format="GLB")  # for a future 3D renderer
    dirs = m.get("dirs", 8)
    for anim in m.get("anims", ["idle"]):
        n = FRAMES.get(anim, 1) if kind != "static" else 1
        for d in range(dirs):
            # direction d: facing the world angle d × 45° (0 = screen right, 2 = towards the camera)
            root.rotation_euler.z = math.radians(90 - d * 45) if dirs > 1 else 0
            for f in range(n):
                pose(J, kind, anim, f / n)
                sc.render.filepath = os.path.join(out, f"{anim}_{d}_{f}.png")
                bpy.ops.render.render(write_still=True)
    open(os.path.join(out, "done"), "w").write("ok")
    print("MODEL DONE", m["id"], flush=True)
