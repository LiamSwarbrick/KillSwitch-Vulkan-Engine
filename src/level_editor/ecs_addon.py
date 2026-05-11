bl_info = {
    "name": "Adventure Engine Level Editor",
    "author": "Hamish Adventurers",
    "version": (1, 0, 0),
    "blender": (5, 0, 0),
    "category": "Object",
}

import bpy
from bpy_extras.io_utils import ExportHelper, ImportHelper
import bmesh

import math
import mathutils

import inspect


# ------------------------
# Helpers for colliders (important) (COMPONENTS ARE NEXT SECTION)
# ------------------------
def build_capsule(obj, radius, height):

    print(f"Build Capsule")

    height = max(height, 2*radius)
    offset = (height / 2) - radius
    
    mesh = obj.data
    bm = bmesh.new()

    # create the uv sphere
    bmesh.ops.create_uvsphere(
        bm,
        u_segments=32,
        v_segments=16,
        radius=radius
    )

    bm.verts.ensure_lookup_table()
    for vert in bm.verts:
        if vert.co[2] < 0:
            vert.co[2] -= offset
        elif vert.co[2] > 0:
            vert.co[2] += offset

    bm.to_mesh(mesh)
    bm.free()

def build_sphere(obj, radius):
    bm = bmesh.new()

    print(f"Build Sphere")

    bmesh.ops.create_uvsphere(
        bm,
        u_segments=32,
        v_segments=16,
        radius=radius
    )

    bm.to_mesh(obj.data)
    bm.free()


def build_box(obj, size):
    bm = bmesh.new()

    print(f"Build Box")

    bmesh.ops.create_cube(bm, size=1.0)

    for v in bm.verts:
        v.co.x *= size[0]*2
        v.co.y *= size[1]*2
        v.co.z *= size[2]*2

    bm.to_mesh(obj.data)
    bm.free()

def build_convex_hull(collider, parent):

    if parent.type != 'MESH':
        return

    mesh = collider.data
    mesh.clear_geometry()

    bm = bmesh.new()

    print(f"Build Convex Hull")

    # add the vertices manually from the parent mesh
    for v in parent.data.vertices:
        bm.verts.new(v.co)
    bm.verts.ensure_lookup_table()
    
    # create the convex hull
    if bm.verts:
        bmesh.ops.convex_hull(bm, input=bm.verts)
        print(f"Convex hull supposedly created")

    bm.to_mesh(mesh)
    bm.free()

def fit_sphere_collider(parent, rigidbody_comp):
    mesh = parent.data
    if mesh is None:
        return

    # no need to use parent's rotation

    max_dist = 0
    for v in mesh.vertices:
        dist = v.co.length
        if dist > max_dist:
            max_dist = dist

    rigidbody_comp.radius = max_dist

def fit_box_collider(parent, rigidbody_comp):
    mesh = parent.data
    if mesh is None:
        return

    # Create rotation matrix from collider_rotation_offset

    parent_rot = parent.rotation_quaternion.copy()

    # Compute bounding box in rotated local space
    min_co = mathutils.Vector((float('inf'), float('inf'), float('inf')))
    max_co = mathutils.Vector((float('-inf'), float('-inf'), float('-inf')))
    for v in mesh.vertices:
        # Apply object's rotation in case it is NOT 0,0,0 (should do the same with scale BUT please use scale 1,1,1)
        # local_co = parent_rot @ v.co
        local_co = v.co
        # (not needed) Apply inverse rotation to vertex so we get its position in the collider's local rotated space 
        # local_co = rb_rot_inv @ local_co

        min_co.x = min(min_co.x, local_co.x)
        min_co.y = min(min_co.y, local_co.y)
        min_co.z = min(min_co.z, local_co.z)
        max_co.x = max(max_co.x, local_co.x)
        max_co.y = max(max_co.y, local_co.y)
        max_co.z = max(max_co.z, local_co.z)

    rigidbody_comp.half_widths = (
        (max_co.x - min_co.x)/2,
        (max_co.y - min_co.y)/2,
        (max_co.z - min_co.z)/2
    )

def fit_capsule_collider(parent, rigidbody_comp):
    mesh = parent.data
    if mesh is None:
        return

    # same as fit_box
    parent_rot = parent.rotation_quaternion.copy()

    min_z = float('inf')
    max_z = float('-inf')
    max_radius = 0
    for v in mesh.vertices:
        # local_co = parent_rot @ v.co
        local_co = v.co

        # Distance in XY plane from origin
        dist = math.hypot(local_co.x, local_co.y)
        max_radius = max(max_radius, dist)
        min_z = min(min_z, local_co.z)
        max_z = max(max_z, local_co.z)

    rigidbody_comp.radius = max_radius
    rigidbody_comp.height = max_z - min_z

def fit_collider(parent, rigidbody_comp, context):
    if rigidbody_comp.collider_type == "COL_TYPE_SPHERE":
        fit_sphere_collider(parent, rigidbody_comp)
    elif rigidbody_comp.collider_type == "COL_TYPE_BOX":
        fit_box_collider(parent, rigidbody_comp)
    elif rigidbody_comp.collider_type == "COL_TYPE_CAPSULE":
        fit_capsule_collider(parent, rigidbody_comp) 

    # rebuild collider after fitting
    update_collider(rigidbody_comp, context)

def remove_collider(parent):
    if parent is None:
        return

    colliders = [c for c in parent.children if c.get("is_collider")]

    for obj in colliders:
        mesh = obj.data
        # remove the object from the scene
        bpy.data.objects.remove(obj, do_unlink=True)
        # remove orphan mesh datablock if unused
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)


def update_collider(self, context):
    parent = context.object

    print(f"Update Collider")

    # if not obj or obj.type != 'MESH':
    #     return

    collider = get_or_create_collider(parent)

    mesh = collider.data
    mesh.clear_geometry()

    if self.collider_type == 'COL_TYPE_SPHERE':
        build_sphere(collider, self.radius)
    elif self.collider_type == 'COL_TYPE_BOX':
        build_box(collider, self.half_widths)
    elif self.collider_type == 'COL_TYPE_CAPSULE':
        build_capsule(collider, self.radius, self.height)
    elif self.collider_type == 'COL_TYPE_CONVEX_HULL':
        build_convex_hull(collider, parent)
    elif self.collider_type == 'COL_TYPE_NONE':
        remove_collider(parent)
        return

    collider.location = parent.location.copy() + self.collider_position_offset

    if collider.rotation_mode != 'QUATERNION':
        collider.rotation_mode = 'QUATERNION'
    if parent.rotation_mode != 'QUATERNION':
        parent.rotation_mode = 'QUATERNION'
    collider.rotation_quaternion = mathutils.Euler(self.collider_rotation_offset, 'XYZ').to_quaternion() @ parent.rotation_quaternion.copy()
    collider.hide_select = True
    collider.show_in_front = True

def get_collider(parent):
    # Try to find existing collider child
    for child in parent.children:
        if child.get("is_collider"):
            return child

def create_collider(parent):
    # Create new collider
    mesh = bpy.data.meshes.new(parent.name + "_Collider")
    obj = bpy.data.objects.new(parent.name + "_Collider", mesh)
    bpy.context.collection.objects.link(obj)

    # Parent it
    obj.parent = parent
    obj.matrix_parent_inverse = parent.matrix_world.inverted()

    # Tag it
    obj["is_collider"] = True

    # Display as wireframe
    obj.display_type = 'WIRE'
    obj.hide_render = True

    return obj


def get_or_create_collider(parent):
    import bpy

    collider = get_collider(parent)
    if not collider is None:
        return collider

    return create_collider(parent)

# ------------------------
# Components
# ------------------------

class BaseComponent(bpy.types.PropertyGroup):
    bl_label = "Base Component"

    def draw(self, layout):
        for prop in self.bl_rna.properties.keys():
            if prop != "rna_type" and prop != "bl_label":
                layout.prop(self, prop)

class PlayerInput(BaseComponent):
    bl_label = "PlayerInput"

class ZombieInput(BaseComponent):
    bl_label = "ZombieInput"

class WeaponComponent(BaseComponent):
    bl_label = "Weapon"
    damage: bpy.props.IntProperty(default=100)

class HealthComponent(BaseComponent):
    bl_label = "Health"
    max_health: bpy.props.IntProperty(default=200)
    current_health: bpy.props.IntProperty(default=100)
    initial_health: bpy.props.IntProperty(default=100)

class TransformComponent(BaseComponent):
    bl_label = "Transform"
    position: bpy.props.FloatVectorProperty(size=3, default=(0,0,0), subtype='XYZ')
    rotation: bpy.props.FloatVectorProperty(size=3, default=(0,0,0), subtype='XYZ')
    scale: bpy.props.FloatVectorProperty(size=3, default=(1,1,1), subtype='XYZ')

class RigidbodyComponent(BaseComponent):
    bl_label = "Rigidbody"

    mass: bpy.props.FloatProperty(name="Mass", default=1.0, min=0.01)
    gravity_scale: bpy.props.FloatProperty(name="Gravity Scale", default=1.0)
    damping: bpy.props.FloatProperty(name="Damping", default=0.99)

    force_layers: bpy.props.EnumProperty(
        name="Force Layers",
        items=[
            ("FORCE_TYPE_NONE", "None", "Force doesn't affect this rigidbody", 0),
            ("FORCE_TYPE_DEFAULT", "Default", "Default forces: gravity for now", 1),
            ("FORCE_TYPE_WIND", "Wind", "Wind", 2),
            ("FORCE_TYPE_MAGNETIC_TRAP", "Magnetic Trap", "not implemented yet", 4)
        ],
        options={"ENUM_FLAG"},
        default={"FORCE_TYPE_DEFAULT"}
    )

    collider_position_offset: bpy.props.FloatVectorProperty(
        name="Collider's position offset", 
        size=3, 
        default=(0,0,0), 
        subtype='XYZ', 
        update=update_collider
    )
    # to be changed to a quaternion when exporting / importing
    collider_rotation_offset: bpy.props.FloatVectorProperty(
        name="Collider Orientation Offset",
        size=3,
        default=(0.0, 0.0, 0.0),
        subtype='EULER',
        update=update_collider
    )
    collider_type: bpy.props.EnumProperty(
        name="Collider Type",
        items=[
            ("COL_TYPE_NONE", "None", ""),
            ("COL_TYPE_SPHERE","Sphere",""),
            ("COL_TYPE_BOX","Box",""),
            ("COL_TYPE_CAPSULE","Capsule",""),
            ("COL_TYPE_CONVEX_HULL","Convex_Hull","Not yet implemented")
        ],
        default="COL_TYPE_BOX",
        update=update_collider
    )
    radius: bpy.props.FloatProperty(
        name="Radius",
        default=1.0,
        min=0.01,
        update=update_collider
    )
    half_widths: bpy.props.FloatVectorProperty(
        name="Half Widths",
        size=3, 
        default=(1,1,1),
        min=0.01,
        subtype='XYZ', 
        update=update_collider
    )
    height: bpy.props.FloatProperty(
        name="Height",
        default=4.0,
        min=0.01,
        update=update_collider
    )


    # we could get these defined by other components...
    # like: TriggerComponent (if exists then rigidbody is trigger)
    is_static: bpy.props.BoolProperty(name="Is Static", default=True)
    is_kinematic: bpy.props.BoolProperty(name="Is Kinematic",default=False)
    is_character: bpy.props.BoolProperty(name="Is Character",default=False)
    is_trigger: bpy.props.BoolProperty(name="Is Trigger",default=False)


    def draw(self, layout):

        layout.prop(self, "mass")
        layout.prop(self, "gravity_scale")
        layout.prop(self, "damping")

        layout.separator()
        force_box = layout.box()
        force_box.label(text="Force Layers")
        force_box.prop(self, "force_layers")

        collider_box = layout.box()
        collider_box.label(text="Collider info")
        collider_box.prop(self, "collider_position_offset")
        collider_box.prop(self, "collider_rotation_offset")

        collider_box.prop(self, "collider_type")
        collider_box.operator("object.fit_collider_to_mesh", text="Fit collider to mesh")

        if self.collider_type == "COL_TYPE_SPHERE":
            collider_box.prop(self, "radius")
        elif self.collider_type == "COL_TYPE_BOX":
            collider_box.prop(self, "half_widths")
        elif self.collider_type == "COL_TYPE_CAPSULE":
            collider_box.prop(self, "radius")
            collider_box.prop(self, "height")

        layout.prop(self, "is_static")
        layout.prop(self, "is_kinematic")
        layout.prop(self, "is_character")
        layout.prop(self, "is_trigger")


# ------------------------
# ECS Registry
# ------------------------

COMPONENT_CLASSES = {}

def discover_components():
    COMPONENT_CLASSES.clear()
    for name, cls in inspect.getmembers(__import__(__name__)):
        if inspect.isclass(cls) and issubclass(cls, BaseComponent) and cls is not BaseComponent:
            COMPONENT_CLASSES[name] = cls
    print(f"[ECS DEBUG] Discovered components: {list(COMPONENT_CLASSES.keys())}")

def register_components():
    for name, cls in COMPONENT_CLASSES.items():
        bpy.utils.register_class(cls)
        setattr(bpy.types.Object, name.lower(), bpy.props.PointerProperty(type=cls))
    print(f"[ECS DEBUG] Registered components: {list(COMPONENT_CLASSES.keys())}")

def unregister_components():
    for name, cls in COMPONENT_CLASSES.items():
        delattr(bpy.types.Object, name.lower())
        bpy.utils.unregister_class(cls)
    print(f"[ECS DEBUG] Unregistered components: {list(COMPONENT_CLASSES.keys())}")

# ------------------------
# ECS Properties & UI
# ------------------------

class ComponentTag(bpy.types.PropertyGroup):
    name: bpy.props.StringProperty()

class ECSProperties(bpy.types.PropertyGroup):
    def component_enum_items(self, context):
        items = []
        for i,(name,cls) in enumerate(COMPONENT_CLASSES.items()):
            items.append((name, getattr(cls,"bl_label",name),"",i))
        return items
    component_to_add: bpy.props.EnumProperty(name="Component", items=component_enum_items)

def has_component(obj, name):
    return any(c.name == name for c in obj.ecs_components)

# ------------------------
# Operators with debug reporting
# ------------------------

class OBJECT_OT_add_component(bpy.types.Operator):
    bl_idname = "object.add_component"
    bl_label = "Add Component"

    def execute(self, context):
        obj = context.object
        props = context.scene.ecs_props
        comp_name = props.component_to_add

        if obj is None:
            self.report({'WARNING'}, "No object selected")
            return {'CANCELLED'}

        if has_component(obj, comp_name):
            self.report({'WARNING'}, f"{comp_name} already added")
            print(f"[ECS DEBUG] Tried to add {comp_name} to {obj.name}, already exists")
            return {'CANCELLED'}

        item = obj.ecs_components.add()
        item.name = comp_name

        # extra logic to create a collider by default on adding a rigidbody component
        if comp_name == "RigidbodyComponent":
            rigidbody_comp = getattr(obj, "rigidbodycomponent", None)
            if rigidbody_comp:
                # it implicitly does get_or_create_collider(obj)
                fit_collider(obj, rigidbody_comp, context) 

        self.report({'INFO'}, f"Added {comp_name} to {obj.name}")
        print(f"[ECS DEBUG] Added {comp_name} to {obj.name}")
        return {'FINISHED'}

class OBJECT_OT_remove_component(bpy.types.Operator):
    bl_idname = "object.remove_component"
    bl_label = "Remove Component"

    component: bpy.props.StringProperty()

    def execute(self, context):
        obj = context.object
        if obj is None:
            self.report({'WARNING'}, "No object selected")
            return {'CANCELLED'}

        found = False
        for i,c in enumerate(obj.ecs_components):
            if c.name == self.component:

                # extra logic to remove the collider from rigidbody
                if self.component == "RigidbodyComponent":
                    remove_collider(obj)

                obj.ecs_components.remove(i)
                found = True
                self.report({'INFO'}, f"Removed {self.component} from {obj.name}")
                print(f"[ECS DEBUG] Removed {self.component} from {obj.name}")
                break
        if not found:
            self.report({'WARNING'}, f"{self.component} not found on {obj.name}")
            print(f"[ECS DEBUG] Tried to remove {self.component} from {obj.name}, not found")
        return {'FINISHED'}


class OBJECT_OT_toggle_colliders(bpy.types.Operator):
    bl_idname = "object.toggle_colliders"
    bl_label = "Toggle Colliders"
    bl_description = "Toggle visibility of all colliders in the scene"

    def execute(self, context):
        # Find all collider objects
        colliders = [obj for obj in bpy.data.objects if obj.get("is_collider")]

        if not colliders:
            self.report({'INFO'}, "No colliders found")
            return {'CANCELLED'}

        # Determine new visibility: toggle based on first collider
        new_visibility = not colliders[0].hide_viewport

        for obj in colliders:
            obj.hide_viewport = new_visibility
            if new_visibility:
                obj.display_type = 'WIRE'
                obj.hide_render = True

        return {'FINISHED'}

class OBJECT_OT_fit_collider_to_mesh(bpy.types.Operator):
    bl_idname = "object.fit_collider_to_mesh"
    bl_label = "Fit Collider To Mesh"
    bl_description = "Fits the current collider to the mesh"

    def execute(self, context):
        obj = context.object
        if obj is None or obj.type != 'MESH':
            self.report({'WARNING'}, "Select a mesh object")
            return {'CANCELLED'}
        
        rigidbody_comp = getattr(obj, "rigidbodycomponent", None)
        if rigidbody_comp is None:
            self.report({'WARNING'}, "Object has no RigidbodyComponent")
            return {'CANCELLED'}

        # Calculate farthest vertex from origin in local space
        mesh = obj.data
        if mesh is None:
            self.report({'WARNING'}, "Object has no mesh data")
            return {'CANCELLED'}

        if rigidbody_comp.collider_type == "COL_TYPE_NONE":
            return {'FINISHED'}
        else:
            fit_collider(obj, rigidbody_comp, context)

        self.report({'INFO'}, f"Collider Fitted to mesh")
        return {'FINISHED'}

# -----------------------------------------
# --- EXPORT TO GLTF ----------------------
# -----------------------------------------

# serializer from BaseComponent / ECSProperty to gltf extra
def serialize_ecs(obj):
    data = {}
    print(f"--- Serializing Object {obj.name} ---")
    for comp in getattr(obj, "ecs_components", []):
        name = comp.name
        print(f"   Component {obj.name}")
        attr_name = name.lower()
        comp_data = getattr(obj, attr_name, None)

        if not comp_data:
            continue

        comp_dict = {}

        for prop in comp_data.bl_rna.properties:
            if prop.identifier == "rna_type":
                continue

            value = getattr(comp_data, prop.identifier)

            # Convert Blender types → JSON-friendly
            if isinstance(value, (list, tuple)):
                value = list(value)

            # Handle ENUM_FLAG properties (i have been stuck for 3 hours here), (make it 4 hours)
            if getattr(prop, "is_enum_flag", False):
                value = list(value)
                # instead of returning the current value, convert directly to bitmask
                bitmask = 0
                for flag in value:  # value is a set of strings
                    bitmask |= prop.enum_items[flag].value
                value = bitmask

            # Special case: convert Euler to quaternion
            if prop.identifier == "collider_rotation_offset":
                # X Y Z to
                # Z X Y
                euler = mathutils.Euler([value[0], value[1], value[2]], "XYZ")
                quat_xyz = euler.to_quaternion()
                # Axis conversion: -90° around X
                q_axis = mathutils.Quaternion((1, 0, 0), math.radians(-90))
                q_final = q_axis @ quat_xyz @ q_axis.inverted()
                print(f"XYZ: {euler}, Quat: {quat_xyz}")
                print(f"XYZ: {euler}, Quat: {q_final}")
                value = list(q_final)  # [w, x, y, z]
                
            if prop.identifier == "half_widths":
                # Convert from Blender's XYZ to engine's XZY
                value = list(value)  # ensure it's a list
                if len(value) == 3:
                    value = [value[0], value[2], value[1]]  # X,Z,-Y
            if prop.identifier == "collider_position_offset":
                value = list(value)  # ensure it's a list
                if len(value) == 3:
                    value = [value[0], value[2], -value[1]]  # X,Z,Y

            print(f"      Property {prop.identifier}: {value}")

            comp_dict[prop.identifier] = value

        data[name] = comp_dict

    return data


def bake_ecs_to_custom_properties():
    for obj in bpy.data.objects:
        ecs_data = serialize_ecs(obj)

        # if we do not have any ecs_data BUT have the boolean "is_ecs_entity" on, create a dummy _ecs json
        if not ecs_data:
            if obj.is_ecs_entity:
                ecs_data = {} 
                ecs_data["_is_ecs_entity"] = True
                obj["_ecs"] = ecs_data

        if ecs_data:
            obj["_ecs"] = ecs_data


def strip_ecs_runtime_properties():
    ECS_RUNTIME_KEYS = {
        "ecs_components",
        *[name.lower() for name in COMPONENT_CLASSES.keys()]
    }

    print("Stripping ECS RUNTIME")

    for obj in bpy.data.objects:
        for key in list(obj.keys()):
            print(f"--Trying to delete {key} of {obj.name}")
            if key in ECS_RUNTIME_KEYS:
                try:
                    del obj[key]
                    print(f"----Succesfully deleted {key} in {obj.name}")
                except Exception as e:
                    print(f"----Failed deleting {key} in {obj.name}: {e}")
                    pass


def apply_ecs_to_object(obj, ecs_data):
    # Clear existing components
    obj.ecs_components.clear()

    obj.is_ecs_entity = ecs_data.get("_is_ecs_entity", False)

    for comp_name, comp_values in ecs_data.items():

        cls = COMPONENT_CLASSES.get(comp_name)
        if not cls:
            print(f"[ECS IMPORT] Unknown component: {comp_name}")
            continue

        # Add component tag
        item = obj.ecs_components.add()
        item.name = comp_name

        attr_name = comp_name.lower()
        comp_data = getattr(obj, attr_name, None)

        if not comp_data:
            print(f"[ECS IMPORT] Missing PropertyGroup on object: {comp_name}")
            continue

        # Apply each field
        for key, value in comp_values.items():

            if key == "name":
                continue  # ignore unused field

            if not hasattr(comp_data, key):
                print(f"[ECS IMPORT] Skipping unknown field {comp_name}.{key}")
                continue

            try:
                prop_meta = comp_data.bl_rna.properties.get(key)

                # handle if it is 
                if getattr(prop_meta, "is_enum_flag", False):
                    flags = set()
                    for item in prop_meta.enum_items:
                        if value & item.value:
                            flags.add(item.identifier)
                    value = flags
                elif prop_meta.identifier == "half_widths":
                    # Convert from Engine's XZY to Blender's XYZ
                    print(f"--- Converting HalfWidths of {obj.name}")
                    value = list(value)  # ensure it's a list
                    if len(value) == 3:
                        value = [value[0], value[2], value[1]]  # X,Z,Y
                elif prop_meta.identifier == "collider_position_offset":
                    value = list(value)  # ensure it's a list
                    if len(value) == 3:
                        value = [value[0], -value[2], value[1]]  # X,-Z,Y
                elif prop_meta and prop_meta.type == 'FLOAT' and prop_meta.is_array:
                    value = tuple(value)
                    if len(value) == 4 and prop_meta.subtype == 'EULER':
                        quat = mathutils.Quaternion(value)
                        q_axis = mathutils.Quaternion((1, 0, 0), math.radians(-90))
                        q_final = q_axis.inverted() @ quat @ q_axis
                        euler_xyz = q_final.to_euler('XYZ')
                        print(f"XYZ: {euler_xyz}, Quat: {q_final}")
                        value = euler_xyz

                print(f"[ECS IMPORT] Imported {key}: {value}")
                setattr(comp_data, key, value)

            except Exception as e:
                print(f"[ECS IMPORT] Failed to set {comp_name}.{key}: {e}")



def import_ecs_from_scene():
    for obj in bpy.data.objects:
        ecs_data = obj.get("_ecs")

        if not ecs_data:
            continue

        if hasattr(ecs_data, "items"):
            print(f"[ECS IMPORT] ECS data is IDPropertyGroup on {obj.name}. {type(ecs_data)}")
            ecs_data = dict(ecs_data)

        if not isinstance(ecs_data, dict):
            print(f"[ECS IMPORT] Invalid ECS data on {obj.name}. {type(ecs_data)}")
            continue

        apply_ecs_to_object(obj, ecs_data)

        print(f"[ECS IMPORT] Applied ECS to {obj.name}")


def prepare_lights_for_export():
    for obj in bpy.data.objects:
        if obj.type != 'LIGHT':
            continue

        light = obj.data

        # Force the actual color into extras so we can read it directly
        # Blender light colors are usually Linear RGB. (For some reason it always exports as white otherwise on glTF)
        light["engine_color"] = [light.color[0], light.color[1], light.color[2]]

        light["engine_intensity"] = light.energy

        # if light.type == 'POINT':
        radius = light.shadow_soft_size

        if radius <= 0.0:
            continue  # or handle error

        light.use_custom_distance = True
        light.cutoff_distance = radius

# the exporter and importer operator
class EXPORT_OT_level_glb(bpy.types.Operator, ExportHelper):
    bl_idname = "export.level_glb"
    bl_label = "Build Level (.glb)"
    filename_ext = ".glb"

    def execute(self, context):

        # Make sure all lights have a radius, cuz we use this to set the cut off distance
        for obj in bpy.data.objects:
            if obj.type == 'LIGHT':
                light = obj.data
                if light.type == 'POINT' and light.shadow_soft_size == 0.0:
                    self.report({'ERROR'}, f"Light '{obj.name}' has radius = 0")
                    return {'CANCELLED'}

        # Clean all BlenderKit metadata from nodes custom properties
        for entity in bpy.data.objects:
            for key in list(entity.keys()):
                if key == "_RNA_UI":
                    del entity[key]
            
        # Same for material custom properties
        for material in bpy.data.materials:
            for key in list (material.keys()):
                if key == "_RNA_UI":
                    del material[key]
                    
        # Same for scene custom properties
        for scene in bpy.data.scenes:
            for key in list (scene.keys()):
                if key == "_RNA_UI":
                    del scene[key]


        prepare_lights_for_export()
        bake_ecs_to_custom_properties()

        # strip_ecs_runtime_properties()


        bpy.ops.export_scene.gltf(
            filepath=self.filepath,
            
            # Exports separate .gltf, .bin, .png files
            export_format='GLTF_SEPARATE',    # NOTE: <- CHANGE THIS WHEN WE WANT GLB to 'GBL'
            export_image_format='AUTO', 
            export_texture_dir='textures', 
            
            # Change export config
            use_visible=False,
            export_extras=True,
            
            # Apply all modifiers before export
            export_apply=True,

            export_lights=True
        )
        
        self.report({'INFO'}, f"Level exported successfully to {self.filepath}")
        return {'FINISHED'}


class IMPORT_OT_level_glb(bpy.types.Operator, ImportHelper):
    bl_idname = "import.level_glb"
    bl_label = "Import Level (.glb/.gltf)"

    filename_ext = ".glb"
    filter_glob: bpy.props.StringProperty(
        default="*.glb;*.gltf",
        options={'HIDDEN'}
    )

    def execute(self, context):
        # Import the glTF file
        bpy.ops.import_scene.gltf(filepath=self.filepath)

        # Rebuild ECS from extras
        import_ecs_from_scene()

        # hide all colliders
        colliders = [obj for obj in bpy.data.objects if obj.get("is_collider")]
        if not colliders:
            print("[ECS Import] Found no colliders at the scene importation")
        for obj in colliders:   
            obj.display_type = 'WIRE'
            obj.hide_render = True

        self.report({'INFO'}, f"Level imported successfully from {self.filepath}")
        return {'FINISHED'}

# ------------------------
# Panels
# ------------------------

# Panel for toggling colliders
class VIEW3D_PT_toggle_colliders(bpy.types.Panel):
    bl_label = "Toggle Colliders"
    bl_idname = "VIEW3D_PT_toggle_colliders"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "ECS"

    def draw(self, context):
        layout = self.layout

        layout.label(text="Toggle Colliders")
        layout.operator("object.toggle_colliders", icon='MOD_WIREFRAME')

class ECSPanelMixin:
    def draw(self, context):
        layout = self.layout
        obj = context.object

        if obj is None:
            layout.label(text="Select an object to use ECS")
            return

        # extra boolean added per object
        layout.prop(obj, "is_ecs_entity")


        props = context.scene.ecs_props
        layout.prop(props, "component_to_add")
        layout.operator("object.add_component")
        layout.separator()

        for comp in getattr(obj, "ecs_components", []):
            box = layout.box()
            row = box.row()
            row.label(text=comp.name)
            op = row.operator("object.remove_component", text="X")
            op.component = comp.name

            attr_name = comp.name.lower()
            comp_data = getattr(obj, attr_name, None)
            if comp_data and hasattr(comp_data, "draw"):
                comp_data.draw(box)

class OBJECT_PT_ecs(ECSPanelMixin, bpy.types.Panel):
    bl_label = "ECS Components"
    bl_idname = "OBJECT_PT_ecs"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"


class VIEW3D_PT_ecs(ECSPanelMixin, bpy.types.Panel):
    bl_label = "ECS Components"
    bl_idname = "VIEW3D_PT_ecs"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "ECS"

# Panel for exporting
class VIEW3D_PT_export(bpy.types.Panel):
    bl_label = "Export"
    bl_idname = "VIEW3D_PT_export"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "ECS"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Exporting the scene
        box = layout.box()
        box.label(text="Export / Import")
        box.operator("export.level_glb", icon='EXPORT')
        box.operator("import.level_glb", icon='IMPORT')

# ------------------------
# Register / Unregister
# ------------------------

classes = [ComponentTag, ECSProperties, OBJECT_OT_add_component, OBJECT_OT_remove_component, OBJECT_OT_toggle_colliders, OBJECT_OT_fit_collider_to_mesh, EXPORT_OT_level_glb, IMPORT_OT_level_glb, OBJECT_PT_ecs, VIEW3D_PT_toggle_colliders, VIEW3D_PT_ecs, VIEW3D_PT_export]

def register():
    discover_components()
    register_components()
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Object.ecs_components = bpy.props.CollectionProperty(type=ComponentTag)
    bpy.types.Scene.ecs_props = bpy.props.PointerProperty(type=ECSProperties)

    bpy.types.Object.is_ecs_entity = bpy.props.BoolProperty(
        name="Is ECS Entity",
        description="Tag this object as an ECS entity (if it doesn't have any components)",
        default=False
    )

    print("[ECS DEBUG] ECS Addon Registered")

def unregister():
    unregister_components()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Object.ecs_components
    del bpy.types.Scene.ecs_props
    del bpy.types.Object.is_ecs_entity
    print("[ECS DEBUG] ECS Addon Unregistered")

if __name__ == "__main__":
    register()