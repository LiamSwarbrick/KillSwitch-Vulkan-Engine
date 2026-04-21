bl_info = {
    "name": "Adventure Engine Level Editor",
    "author": "Hamish Adventurers",
    "version": (1, 0, 0),
    "blender": (5, 0, 0),
    "category": "Object",
}

import bpy
from bpy_extras.io_utils import ExportHelper, ImportHelper

import inspect


# ------------------------
# Components
# ------------------------

class BaseComponent(bpy.types.PropertyGroup):
    bl_label = "Base Component"

    def draw(self, layout):
        for prop in self.bl_rna.properties.keys():
            if prop != "rna_type":
                layout.prop(self, prop)

class PlayerInput(BaseComponent):
    bl_label = "PlayerInput"

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

class ColliderComponent(BaseComponent):
    bl_label = "Collider"
    collider_type: bpy.props.EnumProperty(
        name="Type",
        items=[
            ("COL_TYPE_SPHERE","Sphere",""),
            ("COL_TYPE_BOX","Box",""),
            ("COL_TYPE_CAPSULE","Capsule","")],
        default="COL_TYPE_SPHERE"
    )
    radius: bpy.props.FloatProperty(default=1.0)
    half_widths: bpy.props.FloatVectorProperty(size=3, default=(0.5,0.5,0.5), subtype='XYZ')
    height: bpy.props.FloatProperty(default=2.0)

    def draw(self, layout):
        layout.prop(self, "collider_type")
        if self.collider_type == "SPHERE":
            layout.prop(self, "radius")
        elif self.collider_type == "BOX":
            layout.prop(self, "half_widths")
        elif self.collider_type == "CAPSULE":
            layout.prop(self, "radius")
            layout.prop(self, "height")

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
                obj.ecs_components.remove(i)
                found = True
                self.report({'INFO'}, f"Removed {self.component} from {obj.name}")
                print(f"[ECS DEBUG] Removed {self.component} from {obj.name}")
                break
        if not found:
            self.report({'WARNING'}, f"{self.component} not found on {obj.name}")
            print(f"[ECS DEBUG] Tried to remove {self.component} from {obj.name}, not found")
        return {'FINISHED'}

# -----------------------------------------
# --- EXPORT TO GLTF ----------------------
# -----------------------------------------

# serializer from BaseComponent / ECSProperty to gltf extra
def serialize_ecs(obj):
    data = {}

    for comp in getattr(obj, "ecs_components", []):
        name = comp.name
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

            comp_dict[prop.identifier] = value

        data[name] = comp_dict

    return data


def bake_ecs_to_custom_properties():
    for obj in bpy.data.objects:
        ecs_data = serialize_ecs(obj)

        if ecs_data:
            obj["_ecs"] = ecs_data


def strip_ecs_runtime_properties():
    ECS_RUNTIME_KEYS = [
        "ecs_components",
        *[name.lower() for name in COMPONENT_CLASSES.keys()]
    ]

    for obj in bpy.data.objects:
        for key in ECS_RUNTIME_KEYS:
            if key in obj:
                try:
                    del obj[key]
                except Exception:
                    pass


def apply_ecs_to_object(obj, ecs_data):
    # Clear existing components
    obj.ecs_components.clear()

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
                # 🔧 Handle vector conversion (VERY important)
                prop_meta = comp_data.bl_rna.properties.get(key)

                if prop_meta and prop_meta.type == 'FLOAT' and prop_meta.is_array:
                    value = tuple(value)

                setattr(comp_data, key, value)

            except Exception as e:
                print(f"[ECS IMPORT] Failed to set {comp_name}.{key}: {e}")


def import_ecs_from_scene():
    for obj in bpy.data.objects:
        ecs_data = obj.get("_ecs")

        if not ecs_data:
            continue

        if not isinstance(ecs_data, dict):
            print(f"[ECS IMPORT] Invalid ECS data on {obj.name}")
            continue

        apply_ecs_to_object(obj, ecs_data)

        print(f"[ECS IMPORT] Applied ECS to {obj.name}")


# the exporter and importer operator
class EXPORT_OT_level_glb(bpy.types.Operator, ExportHelper):
    bl_idname = "export.level_glb"
    bl_label = "Build Level (.glb)"
    filename_ext = ".glb"

    def execute(self, context):        
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


        strip_ecs_runtime_properties()

        bake_ecs_to_custom_properties()
        

        bpy.ops.export_scene.gltf(
            filepath=self.filepath,
            
            # Exports separate .gltf, .bin, .png files
            export_format='GLTF_SEPARATE',
            export_image_format='AUTO', 
            export_texture_dir='textures', 
            
            # Change export config
            use_visible=False,
            export_extras=True,
            
            # Apply all modifiers before export
            export_apply=True
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

        self.report({'INFO'}, f"Level imported successfully from {self.filepath}")
        return {'FINISHED'}

# ------------------------
# Panels
# ------------------------

class ECSPanelMixin:
    def draw(self, context):
        layout = self.layout
        obj = context.object

        if obj is None:
            layout.label(text="Select an object to use ECS")
            return

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

classes = [ComponentTag, ECSProperties, OBJECT_OT_add_component, OBJECT_OT_remove_component, EXPORT_OT_level_glb, IMPORT_OT_level_glb, OBJECT_PT_ecs, VIEW3D_PT_ecs, VIEW3D_PT_export]

def register():
    discover_components()
    register_components()
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Object.ecs_components = bpy.props.CollectionProperty(type=ComponentTag)
    bpy.types.Scene.ecs_props = bpy.props.PointerProperty(type=ECSProperties)
    print("[ECS DEBUG] ECS Addon Registered")

def unregister():
    unregister_components()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Object.ecs_components
    del bpy.types.Scene.ecs_props
    print("[ECS DEBUG] ECS Addon Unregistered")

if __name__ == "__main__":
    register()