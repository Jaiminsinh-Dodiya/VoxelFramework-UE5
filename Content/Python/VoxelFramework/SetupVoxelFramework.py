# Copyright Epic Games, Inc. All Rights Reserved.
"""
SetupVoxelFramework.py

Unreal Engine Python Bootstrap Script for VoxelFramework.

Purpose:
  Creates a complete, project-owned suite of starter voxel configuration assets
  under /Game/VoxelFramework/, assigns sensible defaults, wires the reference graph,
  configures project default world settings, and runs configuration validation.

Usage in Unreal Editor:
  1. Window -> Developer Tools -> Output Log (or Python Console)
  2. Run:
     import VoxelFramework.SetupVoxelFramework as setup
     setup.run()
     
     # Or to overwrite existing configuration:
     setup.run(overwrite=True)
"""

import unreal

# Logging Helper
def log_info(msg):
    unreal.log(f"[VoxelFramework] {msg}")

def log_warning(msg):
    unreal.log_warning(f"[VoxelFramework] {msg}")

def log_error(msg):
    unreal.log_error(f"[VoxelFramework] {msg}")


class VoxelBootstrap:
    def __init__(self, base_path="/Game/VoxelFramework", overwrite=False):
        self.base_path = base_path.rstrip("/")
        self.config_path = f"{self.base_path}/Config"
        self.biomes_path = f"{self.base_path}/Biomes"
        self.blocks_path = f"{self.base_path}/Blocks"
        self.overwrite = overwrite
        
        self.asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        self.editor_asset_lib = unreal.EditorAssetLibrary
        
        self.stats = {
            "created": [],
            "updated": [],
            "skipped": [],
            "errors": []
        }

    def create_or_get_asset(self, asset_name, package_path, asset_class):
        """
        Idempotently creates a new DataAsset or loads an existing one.
        Distinguishes CREATE vs SKIP/UPDATE based on overwrite policy.
        """
        full_asset_path = f"{package_path}/{asset_name}"
        exists = self.editor_asset_lib.does_asset_exist(full_asset_path)
        
        if exists and not self.overwrite:
            asset = self.editor_asset_lib.load_asset(full_asset_path)
            if asset:
                self.stats["skipped"].append(asset_name)
                return asset, False
            else:
                log_error(f"Failed to load existing asset at {full_asset_path}")
                self.stats["errors"].append(asset_name)
                return None, False

        # If overwriting existing asset, delete it first to ensure clean state
        if exists and self.overwrite:
            self.editor_asset_lib.delete_asset(full_asset_path)

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_class)
        
        asset = self.asset_tools.create_asset(asset_name, package_path, asset_class, factory)
        if asset:
            self.stats["created"].append(asset_name)
            return asset, True
        else:
            log_error(f"Failed to create asset '{asset_name}' in '{package_path}'")
            self.stats["errors"].append(asset_name)
            return None, False

    def setup_blocks(self):
        """Creates standard starter block definitions: Stone, Dirt, Grass."""
        blocks = {}
        
        # 1. Stone (Block ID 1)
        stone, is_new = self.create_or_get_asset("DA_Block_Stone", self.blocks_path, unreal.VoxelBlockDefinition)
        if stone and (is_new or self.overwrite):
            stone.set_editor_property("block_id", 1)
            stone.set_editor_property("display_name", unreal.Text("Stone"))
            stone.set_editor_property("is_solid", True)
            stone.set_editor_property("generates_collision", True)
            stone.set_editor_property("material_layer_index", 0)
            stone.set_editor_property("vertex_tint", unreal.LinearColor(0.6, 0.6, 0.6, 1.0))
            self.editor_asset_lib.save_loaded_asset(stone)
        blocks["stone"] = stone

        # 2. Dirt (Block ID 2)
        dirt, is_new = self.create_or_get_asset("DA_Block_Dirt", self.blocks_path, unreal.VoxelBlockDefinition)
        if dirt and (is_new or self.overwrite):
            dirt.set_editor_property("block_id", 2)
            dirt.set_editor_property("display_name", unreal.Text("Dirt"))
            dirt.set_editor_property("is_solid", True)
            dirt.set_editor_property("generates_collision", True)
            dirt.set_editor_property("material_layer_index", 1)
            dirt.set_editor_property("vertex_tint", unreal.LinearColor(0.5, 0.35, 0.2, 1.0))
            self.editor_asset_lib.save_loaded_asset(dirt)
        blocks["dirt"] = dirt

        # 3. Grass (Block ID 3)
        grass, is_new = self.create_or_get_asset("DA_Block_Grass", self.blocks_path, unreal.VoxelBlockDefinition)
        if grass and (is_new or self.overwrite):
            grass.set_editor_property("block_id", 3)
            grass.set_editor_property("display_name", unreal.Text("Grass"))
            grass.set_editor_property("is_solid", True)
            grass.set_editor_property("generates_collision", True)
            grass.set_editor_property("material_layer_index", 2)
            grass.set_editor_property("vertex_tint", unreal.LinearColor(0.3, 0.75, 0.3, 1.0))
            self.editor_asset_lib.save_loaded_asset(grass)
        blocks["grass"] = grass

        return blocks

    def setup_biomes(self, blocks):
        """Creates a starter Plains biome with Grass/Dirt/Stone layering."""
        biomes = []
        plains, is_new = self.create_or_get_asset("DA_Biome_Plains", self.biomes_path, unreal.VoxelBiomeDefinition)
        
        if plains and (is_new or self.overwrite):
            plains.set_editor_property("display_name", unreal.Text("Plains"))
            plains.set_editor_property("temperature_range", unreal.Vector2D(0.0, 1.0))
            plains.set_editor_property("humidity_range", unreal.Vector2D(0.0, 1.0))
            plains.set_editor_property("ambient_tint", unreal.LinearColor.WHITE)
            plains.set_editor_property("vegetation_density", 0.05)

            # Layer 1: Grass (Top layer, depth 1)
            layer_grass = unreal.VoxelTerrainLayer()
            layer_grass.set_editor_property("block", blocks["grass"])
            layer_grass.set_editor_property("thickness_voxels", 1)

            # Layer 2: Dirt (Sub-surface layer, depth 3)
            layer_dirt = unreal.VoxelTerrainLayer()
            layer_dirt.set_editor_property("block", blocks["dirt"])
            layer_dirt.set_editor_property("thickness_voxels", 3)

            # Layer 3: Stone (Bedrock / deep stone layer, depth 1)
            layer_stone = unreal.VoxelTerrainLayer()
            layer_stone.set_editor_property("block", blocks["stone"])
            layer_stone.set_editor_property("thickness_voxels", 1)

            plains.set_editor_property("terrain_layers", [layer_grass, layer_dirt, layer_stone])
            self.editor_asset_lib.save_loaded_asset(plains)
            
        biomes.append(plains)
        return biomes

    def setup_generation_definition(self, blocks):
        """Creates the default procedural generation definition with climate, terrain, and caves."""
        gen_def, is_new = self.create_or_get_asset("DA_Generation_Default", self.config_path, unreal.VoxelGenerationDefinition)
        
        if gen_def and (is_new or self.overwrite):
            # Climate
            climate = gen_def.get_editor_property("climate")
            climate.set_editor_property("frequency", 0.001)
            climate.set_editor_property("temperature_seed_offset", 1000)
            climate.set_editor_property("humidity_seed_offset", 2000)
            gen_def.set_editor_property("climate", climate)

            # Terrain
            terrain = gen_def.get_editor_property("terrain")
            terrain.set_editor_property("base_height", 64)
            terrain.set_editor_property("height_amplitude", 32.0)
            terrain.set_editor_property("base_frequency", 0.01)
            terrain.set_editor_property("noise_octaves", 4)
            terrain.set_editor_property("lacunarity", 2.0)
            terrain.set_editor_property("persistence", 0.5)
            terrain.set_editor_property("fallback_stone_block", blocks["stone"])
            terrain.set_editor_property("fallback_dirt_block", blocks["dirt"])
            terrain.set_editor_property("fallback_grass_block", blocks["grass"])
            terrain.set_editor_property("fallback_dirt_depth", 4)
            gen_def.set_editor_property("terrain", terrain)

            # Caves
            caves = gen_def.get_editor_property("caves")
            caves.set_editor_property("enabled", True)
            caves.set_editor_property("carve_threshold", 0.58)
            caves.set_editor_property("density_frequency", 0.045)
            caves.set_editor_property("noise_octaves", 3)
            caves.set_editor_property("surface_protection_depth", 3)
            caves.set_editor_property("cave_seed_offset", 5000)
            gen_def.set_editor_property("caves", caves)

            self.editor_asset_lib.save_loaded_asset(gen_def)

        return gen_def

    def setup_streaming_preset(self):
        """Creates the default distance bands and frame budget streaming preset."""
        streaming, is_new = self.create_or_get_asset("DA_Streaming_Default", self.config_path, unreal.VoxelStreamingPreset)
        
        if streaming and (is_new or self.overwrite):
            streaming.set_editor_property("simulation_distance", 4)
            streaming.set_editor_property("render_distance", 8)
            streaming.set_editor_property("generation_distance", 10)
            streaming.set_editor_property("persistence_distance", 12)
            streaming.set_editor_property("streaming_budget_ms", 1.5)
            streaming.set_editor_property("max_mesh_finalizations_per_tick", 4)
            streaming.set_editor_property("max_collision_finalizations_per_tick", 4)
            self.editor_asset_lib.save_loaded_asset(streaming)

        return streaming

    def setup_physics_preset(self):
        """Creates the default Chaos physical collision preset."""
        physics, is_new = self.create_or_get_asset("DA_Physics_Default", self.config_path, unreal.VoxelPhysicsPreset)
        
        if physics and (is_new or self.overwrite):
            physics.set_editor_property("collision_mode", unreal.VoxelCollisionMode.COMPLEX)
            physics.set_editor_property("async_cooking", True)
            physics.set_editor_property("collision_profile_name", "BlockAll")
            self.editor_asset_lib.save_loaded_asset(physics)

        return physics

    def setup_world_definition(self, gen_def, biomes, streaming_preset, physics_preset):
        """Creates the composite World Definition connecting all presets and definitions."""
        world_def, is_new = self.create_or_get_asset("DA_VoxelWorld_Default", self.config_path, unreal.VoxelWorldDefinition)
        
        if world_def and (is_new or self.overwrite):
            world_def.set_editor_property("world_name", unreal.Text("Default Voxel World"))
            world_def.set_editor_property("world_seed", 1234)
            world_def.set_editor_property("voxel_world_size", 100.0)
            world_def.set_editor_property("generation_definition", gen_def)
            world_def.set_editor_property("biomes", biomes)
            world_def.set_editor_property("streaming_preset", streaming_preset)
            world_def.set_editor_property("physics_preset", physics_preset)
            self.editor_asset_lib.save_loaded_asset(world_def)
        elif world_def and not is_new:
            # Repair any missing references if asset already existed
            modified = False
            if world_def.get_editor_property("generation_definition") is None and gen_def:
                world_def.set_editor_property("generation_definition", gen_def)
                modified = True
            if len(world_def.get_editor_property("biomes")) == 0 and biomes:
                world_def.set_editor_property("biomes", biomes)
                modified = True
            if world_def.get_editor_property("streaming_preset") is None and streaming_preset:
                world_def.set_editor_property("streaming_preset", streaming_preset)
                modified = True
            if world_def.get_editor_property("physics_preset") is None and physics_preset:
                world_def.set_editor_property("physics_preset", physics_preset)
                modified = True
            if modified:
                self.editor_asset_lib.save_loaded_asset(world_def)
                self.stats["updated"].append("DA_VoxelWorld_Default (wired missing refs)")

        return world_def

    def configure_project_defaults(self, world_def):
        """Configures UVoxelWorldSettings::DefaultWorldDefinition in Project Settings."""
        try:
            settings = None
            if hasattr(unreal, "VoxelWorldSettings"):
                settings = unreal.get_default_object(unreal.VoxelWorldSettings)
            if settings:
                settings.set_editor_property("default_world_definition", world_def)
                settings.modify()
                log_info("Configured Project Settings -> Plugins -> Voxel World -> DefaultWorldDefinition")
                return True
        except Exception as e:
            log_warning(f"Could not automatically set Project Settings default (optional): {e}")
        return False

    def validate_configuration(self, world_def):
        """Audits the generated World Definition using UVoxelConfigValidator."""
        if not world_def:
            return False

        messages = unreal.VoxelConfigValidator.validate_world_definition(world_def, 32, 8)
        has_errors = False
        
        for msg in messages:
            if msg.severity == unreal.VoxelValidationSeverity.ERROR:
                log_error(f"Validation Error: {msg.message} (Suggestion: {msg.suggestion})")
                has_errors = True
            elif msg.severity == unreal.VoxelValidationSeverity.WARNING:
                log_warning(f"Validation Warning: {msg.message} (Suggestion: {msg.suggestion})")
            else:
                log_info(f"Validation Info: {msg.message}")

        return not has_errors

    def execute(self):
        """Executes the complete bootstrap pipeline with structured reporting."""
        log_info("==================================================")
        log_info("VoxelFramework — Unreal Python Bootstrap Started")
        log_info(f"Target Directory: {self.base_path}")
        log_info(f"Overwrite Mode:   {self.overwrite}")
        log_info("==================================================")

        # 1. Blocks
        blocks = self.setup_blocks()
        
        # 2. Biomes
        biomes = self.setup_biomes(blocks)
        
        # 3. Generation Definition
        gen_def = self.setup_generation_definition(blocks)
        
        # 4. Streaming Preset
        streaming = self.setup_streaming_preset()
        
        # 5. Physics Preset
        physics = self.setup_physics_preset()
        
        # 6. World Definition
        world_def = self.setup_world_definition(gen_def, biomes, streaming, physics)

        # 7. Configure Project Settings
        self.configure_project_defaults(world_def)

        # 8. Validate
        is_valid = self.validate_configuration(world_def)

        # Report Summary
        log_info("--------------------------------------------------")
        log_info("Bootstrap Summary:")
        log_info(f"  Created Assets: {len(self.stats['created'])} ({', '.join(self.stats['created']) if self.stats['created'] else 'None'})")
        log_info(f"  Skipped (Existing): {len(self.stats['skipped'])} ({', '.join(self.stats['skipped']) if self.stats['skipped'] else 'None'})")
        if self.stats["updated"]:
            log_info(f"  Updated: {len(self.stats['updated'])} ({', '.join(self.stats['updated'])})")
        if self.stats["errors"]:
            log_error(f"  Errors:  {len(self.stats['errors'])} ({', '.join(self.stats['errors'])})")

        validation_status = "PASS" if is_valid else "FAIL"
        log_info(f"Configuration Validation: {validation_status}")
        log_info("==================================================")
        
        if is_valid and not self.stats["errors"]:
            log_info("Bootstrap completed successfully. Ready for PIE!")
            return True
        else:
            log_error("Bootstrap completed with warnings or errors. Check log above.")
            return False


def run(overwrite=False, base_path="/Game/VoxelFramework"):
    """Main entry point to execute the VoxelFramework bootstrap."""
    bootstrap = VoxelBootstrap(base_path=base_path, overwrite=overwrite)
    return bootstrap.execute()


# Run automatically when executed as standalone script
run()
