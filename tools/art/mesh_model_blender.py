"""[U-510] A 3D file → sprite frames (run INSIDE Blender, called by tools/art/model_from_file.py).

    blender -b --factory-startup -P tools/art/mesh_model_blender.py -- job.json

Reads .glb / .gltf / .fbx / .obj, stands it on the ground at the wanted height, renders the same camera and light as the other
models. A file with animations: actions whose names fit (idle, walk, run, attack, cast, death) are played; everything else gets
simple body movement, so a model without a skeleton still walks and attacks.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

PITCH = math.radians(51.7)


def setup(job):
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
    sc.cycles.samples = job.get("samples", 16)
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
    return sc


def import_file(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=path)
    else:
        raise SystemExit(f"unknown model file: {ext}")


def main():
    job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    sc = setup(job)
    import_file(job["model"])
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if not meshes:
        raise SystemExit("no mesh in the file")
    root = bpy.data.objects.new("root", None)
    sc.collection.objects.link(root)
    for o in bpy.data.objects:
        if o not in (root,) and o.parent is None and o.type in ("MESH", "ARMATURE", "EMPTY"):
            o.parent = root
    bpy.context.view_layer.update()
    # stand it on the ground, centred, at the wanted height
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for m in meshes:
        for c in m.bound_box:
            w = m.matrix_world @ Vector(c)
            lo = Vector(min(a, b) for a, b in zip(lo, w))
            hi = Vector(max(a, b) for a, b in zip(hi, w))
    size = max(1e-4, hi.z - lo.z)
    s = job["height"] / size
    root.scale = (s, s, s)
    root.location = (-(lo.x + hi.x) / 2 * s, -(lo.y + hi.y) / 2 * s, -lo.z * s)
    base_loc = root.location.copy()
    # its animations by name
    actions = {a.name.lower(): a for a in bpy.data.actions}
    arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
    def action_for(anim):
        for name, a in actions.items():
            if anim in name:
                return a
        return None
    out = job["out"]
    os.makedirs(out, exist_ok=True)
    for anim, n in job["frames"].items():
        act = action_for(anim) if arm else None
        if act and arm:
            arm.animation_data_create()
            arm.animation_data.action = act
        for f in range(n):
            t = f / max(1, n - 1) if anim == "death" else f / n
            root.location = base_loc.copy()
            root.rotation_euler = (0, 0, 0)
            if act:
                a, b = act.frame_range
                sc.frame_set(int(a + (b - a) * t))
            else:  # simple body movement
                wave = math.sin(t * 2 * math.pi)
                if anim == "idle":
                    root.location.z = base_loc.z + 0.02 * wave
                elif anim == "walk":
                    root.location.z = base_loc.z + 0.05 * abs(wave)
                    root.rotation_euler.y = 0.08 * wave
                elif anim == "attack":
                    root.rotation_euler.x = -0.35 * math.sin(t * math.pi)
                elif anim == "cast":
                    root.location.z = base_loc.z + 0.08 * math.sin(t * math.pi)
                elif anim == "death":
                    root.rotation_euler.x = t * 1.5
            for d in range(8):
                root.rotation_euler.z = math.radians(90 - d * 45)
                sc.render.filepath = os.path.join(out, f"{anim}_{d}_{f}.png")
                bpy.ops.render.render(write_still=True)
    open(os.path.join(out, "done"), "w").write("ok")


main()
