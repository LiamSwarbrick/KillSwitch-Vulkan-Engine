bl_info = {
    "name": "Adventure Engine Level Editor",
    "blender": (5, 0, 0),
    "category": "Object",
}


import bpy
from bpy_extras.io_utils import ExportHelper


# --- COLLIDER TYPES DROPDOWN ---
def collider_types():
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


# --- TOGGLE PHYSICS VIEW ------------
class VIEW_OT_toggle_physics_visual(bpy.types.Operator):
    bl_idname = "view.toggle_physics_visual"
    bl_label = "Toggle Physics View"

    def execute(self, context):
        
        # Add and update new custom property to scene to track view type
        if "physics_view_active" not in context.scene:
            context.scene["physics_view_active"] = False
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


# --- EXPORT TO GLTF ----------------------
class EXPORT_OT_level_glb(bpy.types.Operator, ExportHelper):
    bl_idname = "export.level_glb"
    bl_label = "Build Level (.glb)"
    filename_ext = ".glb"

    def execute(self, context):
        # All custom properties that want exporting should have this prefix! -----
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
            export_apply=True
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
        
        # Assigning colliders from the dropdown
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
        
        # Exporting the scene
        box = layout.box()
        box.label(text="Export:")
        box.operator("export.level_glb", icon='EXPORT')



# --- REGISTRATION ---
def register():
    collider_types()
    bpy.utils.register_class(MESH_OT_assign_collider)
    bpy.utils.register_class(MESH_OT_assign_trigger)
    bpy.utils.register_class(VIEW_OT_toggle_physics_visual)
    bpy.utils.register_class(LEVEL_PT_editor_panel)
    bpy.utils.register_class(EXPORT_OT_level_glb)

if __name__ == "__main__":
    register()