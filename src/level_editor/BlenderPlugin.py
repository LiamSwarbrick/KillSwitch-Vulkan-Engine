bl_info = {
    "name": "Adventure Engine Level Editor",
    "blender": (5, 0, 0),
    "category": "Object",
}

import bpy, math, json, os
from bpy_extras.io_utils import ImportHelper, ExportHelper

# Global cache of components
COMPONENTS_LIST = {}

# Reads components from JSON, stores and generates UI list
def load_json(self, context):
    global COMPONENTS_LIST
    items = []
    
    # Get the path to the current .blend file
    blend_dir = bpy.path.abspath("//")
    if not blend_dir:
        return [('NONE', 'SAVE .BLEND', 'SAVE YOUR BLEND FILE IN SAME DIR AS JSON')]
    
    # Load json into global components list
    json_path = os.path.join(blend_dir, "..", "components.json")
    if os.path.exists(json_path):
        try:
            with open(json_path, 'r') as f:
                COMPONENTS_LIST = json.load(f)
                
            # Generate dropdown text from the data
            for key in COMPONENTS_LIST.keys():
                items.append((key, key.replace("_", " ").title(), f"Properties: {list(COMPONENTS_LIST[key].keys())}"))
            return items
        
        except Exception as e:
            return [('ERROR', 'JSON INVALID', str(e))]
    else:
        return [('JSON NOT FOUND', '.JSON INCORRECTLY PLACED', 'MAKE SURE ITS IN SAME DIR AS .BLEND')]


# --- DROPDOWNS ---
def dropdown_types():
    # Different types of colliders/triggers
    bpy.types.Scene.active_collision_type = bpy.props.EnumProperty(
        name="Collision Type",
        description="Select which type of collision to apply",
        items=[
            ('BOX', "Box", "Box obviously"),
            ('SPHERE', "Sphere", "Ball"),
            ('CAPSULE', "Capsule", "Human-akin geometry"),
        ],
        default='BOX'
    )
    
    # JSON components in more readable format
    bpy.types.Scene.active_component_type = bpy.props.EnumProperty(
        name="Component",
        items=load_json
    )
    
    # Enabling/disabling properties dropdown
    bpy.types.Object.show_custom_properties = bpy.props.BoolProperty(
        name="Show Custom Properties",
        default=True
    )
    

# --- CREATING ROOM BASE ------------------------
class MESH_OT_create_room_template(bpy.types.Operator):
    bl_idname = "object.create_room_template"
    bl_label = "Generate Basic Room"
    bl_options = {'REGISTER', 'UNDO'}

    def create_wall(self, name, loc, scale, parent):
        # Create a wall mesh
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
        wall = bpy.context.active_object
        wall.name = name
        wall.scale = scale
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        wall.parent = parent
        
        # Create a wall collider
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
        wall_col = bpy.context.active_object
        wall_col.name = "COL_" + name
        wall_col.scale = scale
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        wall_col.parent = wall
        
        # Apply custom collider properties
        wall_col["game_is_collider"] = True
        wall_col["game_collider_type"] = "BOX"
        wall_col.display_type = 'WIRE'
        wall_col.color = (0.15, 1.0, 0.95, 1.0) # Cyan

    def execute(self, context):
        # Create a root object
        bpy.ops.object.empty_add(type='SPHERE', location=(0,0,0))
        root = bpy.context.active_object
        root.name = "ROOM_Prefab"
        root.show_in_front = True
        
        # Create the floor (10x10x0.2)
        self.create_wall("Floor", (0, 0, -0.1), (10, 10, 0.2), root)

        # Create the walls (10x3x0.2)
        self.create_wall("Wall_N", (0, 5, 1.5), (10, 0.2, 3), root)
        self.create_wall("Wall_S", (0, -5, 1.5), (10, 0.2, 3), root)
        self.create_wall("Wall_E", (5, 0, 1.5), (0.2, 10, 3), root)
        self.create_wall("Wall_W", (-5, 0, 1.5), (0.2, 10, 3), root)

        self.report({'INFO'}, "10x10 Room Template Generated")
        return {'FINISHED'}


# --- COLLIDER TYPE SETTING ----------------
class MESH_OT_assign_collider(bpy.types.Operator):
    bl_idname = "object.assign_collider"
    bl_label = "Assign Collider"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        
        if obj is None:
            self.report({'ERROR'}, "Select an object")
            return {'CANCELLED'}
        
        # If it was a trigger previously, remove old data
        if obj.name.startswith("TRIG_"):
            obj.name = obj.name[5:]
        if "game_is_trigger" in obj:
            del obj["game_is_trigger"]
        if "game_trigger_type" in obj:
            del obj["game_trigger_type"]
        
        collider_type = context.scene.active_collision_type

        # Add custom properties (extras in gltf)
        obj["game_is_collider"] = True
        obj["game_collider_type"] = collider_type

        # Add prefix to node name
        if not obj.name.startswith("COL_"):
            obj.name = "COL_" + obj.name

        # Display as wireframe
        obj.display_type = 'WIRE'
        obj.color = (0.15, 1.0, 0.95, 1.0)
        
        self.report({'INFO'}, f"Converted {obj.name} to {collider_type} collider")
        return {'FINISHED'}
    
    
# --- TRIGGER TYPE SETTING ----------------
class MESH_OT_assign_trigger(bpy.types.Operator):
    bl_idname = "object.assign_trigger"
    bl_label = "Assign Trigger"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        
        if obj is None:
            self.report({'ERROR'}, "Select an object")
            return {'CANCELLED'}
        
        # If it was a collider previously, remove old data
        if obj.name.startswith("COL_"):
            obj.name = obj.name[4:]
        if "game_is_collider" in obj:
            del obj["game_is_collider"]
        if "game_collider_type" in obj:
            del obj["game_collider_type"]
        
        trigger_type = context.scene.active_collision_type

        # Add custom property (extras in gltf)
        obj["game_is_trigger"] = True
        obj["game_trigger_type"] = trigger_type

        # Add prefix to node name
        if not obj.name.startswith("TRIG_"):
            obj.name = "TRIG_" + obj.name

        # Display as wireframe
        obj.display_type = 'WIRE'
        obj.color = (1.0, 0.6, 0.0, 1.0)
        
        self.report({'INFO'}, f"Converted {obj.name} to {trigger_type} trigger")
        return {'FINISHED'}
    

# --- ADD OBJECT COMPONENT ----------------------
class MESH_OT_add_component(bpy.types.Operator):
    bl_idname = "object.add_component"
    bl_label = "Add Component"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        global COMPONENTS_LIST
        obj = context.active_object
        
        if obj is None:
            self.report({'ERROR'}, "Select an object")
            return {'CANCELLED'}
        
        # Get currently selected component properties from global
        component = context.scene.active_component_type
        properties = COMPONENTS_LIST.get(component)

        # Create new custom properties for current object
        if properties:
            for key, value in properties.items():
                obj[key] = value
            self.report({'INFO'}, f"Applied {component}")
        else:
            self.report({'ERROR'}, "Component data missing from cache")

        return {'FINISHED'}
    
    
# --- REMOVE OBJECT COMPONENT -------------------
class MESH_OT_remove_component(bpy.types.Operator):
    bl_idname = "object.remove_component"
    bl_label = "Remove Component"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        global COMPONENTS_LIST
        obj = context.active_object
        
        if obj is None:
            self.report({'ERROR'}, "Select an object")
            return {'CANCELLED'}
        
        # Get currently selected component properties from global
        component = context.scene.active_component_type
        properties = COMPONENTS_LIST.get(component, {})
        
        # Remove chosen custom properties 
        for key in properties.keys():
            if key in obj:
                del obj[key]
            
        self.report({'INFO'}, f"Removed {component} properties from {obj.name}")
        return {'FINISHED'}


# --- TOGGLE PHYSICS VIEW ------------
class VIEW_OT_toggle_physics_visual(bpy.types.Operator):
    bl_idname = "view.toggle_physics_visual"
    bl_label = "Toggle Physics View"

    def execute(self, context):
        
        # Add and update new custom property to scene to track view type
        if "physics_view_active" not in context.scene:
            context.scene["physics_view_active"] = True
        is_active = not context.scene["physics_view_active"]
        context.scene["physics_view_active"] = is_active
        
        # Update viewport shading
        for area in context.screen.areas:
            if area.type == 'VIEW_3D':
                for space in area.spaces:
                    if space.type == 'VIEW_3D':
                        if is_active:
                            space.shading.color_type = 'OBJECT' 
                            space.shading.wireframe_color_type = 'OBJECT'
                        else:
                            space.shading.color_type = 'MATERIAL'

        # Update object visibility
        for obj in bpy.data.objects:
            if obj.get("game_is_collider") and obj.name.startswith("COL_"):
                obj.hide_viewport = not is_active
            elif obj.get("game_is_trigger") and obj.name.startswith("TRIG_"):
                obj.hide_viewport = not is_active
        
        # Showing which mode is activated
        if is_active:
            current_mode = "PHYSICS"
        else:
            current_mode = "ART"
        self.report({'INFO'}, f"View Mode: {current_mode}")
                
        return {'FINISHED'}


# --- IMPORT FROM GLTF ---
class IMPORT_OT_import_gltf(bpy.types.Operator, ImportHelper):
    bl_idname = "object.import_gltf"
    bl_label = "Import Level (.gltf)"
    filename_ext = ".gltf"
    filter_glob: bpy.props.StringProperty(
        default="*.gltf;*.glb",
        options={'HIDDEN'},
        maxlen=255,
    )

    def execute(self, context):
        
        bpy.ops.import_scene.gltf(filepath=self.filepath)
    
        for screen in bpy.data.screens:
            for area in screen.areas:
                if area.type == 'VIEW_3D':
                    for space in area.spaces:
                        if space.type == 'VIEW_3D':
                            space.shading.wireframe_color_type = 'OBJECT'
                            space.shading.color_type = 'OBJECT'


        # Sets the colliders and triggers to correct colours/display types
        for obj in bpy.data.objects:
            if obj.name.startswith("COL_"):
                obj.display_type = 'WIRE'
                obj.color = (0.15, 1.0, 0.95, 1.0)
            elif obj.name.startswith("TRIG_"):
                obj.display_type = 'WIRE'
                obj.color = (1.0, 0.6, 0.0, 1.0)
                
        self.report({'INFO'}, f"Level imported successfully to {self.filepath}")
        return {'FINISHED'}


# --- EXPORT TO GLTF ----------------------
class EXPORT_OT_level_gltf(bpy.types.Operator, ExportHelper):
    bl_idname = "export.level_gltf"
    bl_label = "Export Level (.gltf)"
    filename_ext = ".gltf"

    def execute(self, context):
        # All custom properties that want exporting should have this prefix!!!!!
        prefix = "game_"
        
        # Clean all BlenderKit metadata from nodes custom properties
        for entity in bpy.data.objects:
            for key in list(entity.keys()):
                if not key.startswith(prefix) or key == "_RNA_UI":
                    del entity[key]
            
        # Same for material custom properties
        for material in bpy.data.materials:
            for key in list (material.keys()):
                if not key.startswith(prefix) or key == "_RNA_UI":
                    del material[key]
                    
        # Same for scene custom properties
        for scene in bpy.data.scenes:
            for key in list (scene.keys()):
                if not key.startswith(prefix) or key == "_RNA_UI":
                    del scene[key]
        
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
            export_apply=True,
            
            export_animations=True,
            export_skins=True
        )
        
        self.report({'INFO'}, f"Level exported successfully to {self.filepath}")
        return {'FINISHED'}


# --- UI Buttons ---
class LEVEL_PT_editor_panel(bpy.types.Panel):
    bl_label = "Adventure Engine Tools"
    bl_idname = "LEVEL_PT_editor_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'ADV_Engine'

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        obj = context.active_object
        
        # Creating a basic room structure
        box = layout.box()
        box.label(text="Create Room", icon='CHECKMARK')
        box.operator("object.create_room_template", icon='PRESET')
        
        layout.separator()
        
        # Assigning custom properties for components
        box = layout.box()
        box.label(text="Add Components", icon='ADD')
        box.prop(scene, "active_component_type")
        box.operator("object.add_component", icon='PLUS')
        box.operator("object.remove_component", icon='TRASH')
        box.separator(factor=3.0)
        
        # Easy editing of current custom properties for selected object
        if obj:
            # Make the list collapsible
            row = box.row(align=True)
            row.prop(obj, "show_custom_properties", text="", icon='TRIA_DOWN')
            row.label(text=f"OBJECT PROPERTIES: {obj.name}", icon='PROPERTIES')
            
            # If visible, get all user-added custom properties
            if obj.show_custom_properties:
                custom_properties = []
                for key in obj.keys():
                    if key.startswith("game_"):
                        custom_properties.append(key)
                
                # List references to all found custom properties so they are editable
                if len(custom_properties) > 0:
                    for property in custom_properties:
                        box.prop(obj, f'["{property}"]', text=property)
                else:
                    box.label(text="No game components found.", icon='INFO')
        
        layout.separator()
        
        # Assigning colliders/triggers from the dropdown
        box = layout.box()
        box.label(text="Collision Setup", icon='MOD_PHYSICS')
        box.prop(scene, "active_collision_type")
        box.operator("object.assign_collider", icon='CHECKMARK')
        box.operator("object.assign_trigger", icon='CHECKMARK')
        
        layout.separator()
        
        # Toggling physics view button
        box = layout.box()
        box.label(text="Viewport Settings:")
        is_active = scene.get("physics_view_active", False)
        box.operator("view.toggle_physics_visual", icon='VIEWZOOM')
        
        layout.separator()
        
        # Importing and exporting a glTF file
        box = layout.box()
        box.label(text="Import/Export:")
        box.operator("object.import_gltf", icon='IMPORT')
        box.operator("export.level_gltf", icon='EXPORT')



# --- REGISTRATION ---
def register():
    dropdown_types()
    bpy.utils.register_class(IMPORT_OT_import_gltf)
    bpy.utils.register_class(MESH_OT_create_room_template)
    bpy.utils.register_class(MESH_OT_assign_collider)
    bpy.utils.register_class(MESH_OT_assign_trigger)
    bpy.utils.register_class(MESH_OT_add_component)
    bpy.utils.register_class(MESH_OT_remove_component)
    bpy.utils.register_class(VIEW_OT_toggle_physics_visual)
    bpy.utils.register_class(LEVEL_PT_editor_panel)
    bpy.utils.register_class(EXPORT_OT_level_gltf)

if __name__ == "__main__":
    register()