# Copyright Epic Games, Inc. All Rights Reserved.
"""
SetupVoxelFramework.py

Unreal Engine Python Bootstrap Script for VoxelFramework.

Purpose:
  Creates a complete, project-owned suite of starter voxel configuration assets
  under /Game/VoxelFramework/, assigns sensible defaults, wires the reference graph,
  configures project default world settings, and runs configuration validation.

Idempotency Model:
  - If an asset does not exist: CREATES it with starter defaults.
  - If an asset exists and overwrite=False: inspects properties and REPAIRS any
    missing references or uninitialized fields in-place (UPDATED), or leaves valid
    assets untouched (SKIPPED).
  - If an asset exists and overwrite=True: UPDATES all properties in-place without
    deleting UObjects or breaking external references.

Usage in Unreal Editor:
  1. Window -> Developer Tools -> Output Log (or Python Console)
  2. Run:
     import VoxelFramework.SetupVoxelFramework as setup
     setup.run()
     
     # Or to reset all properties to defaults in-place:
     setup.run(overwrite=True)
"""

import unreal

# Logging Helpers
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
        Idempotently loads an existing asset or creates a new one.
        Never calls delete_asset to ensure non-destructive execution.
        """
        full_asset_path = f"{package_path}/{asset_name}"
        exists = self.editor_asset_lib.does_asset_exist(full_asset_path)
        
        if exists:
            asset = self.editor_asset_lib.load_asset(full_asset_path)
            if asset:
                return asset, False
            else:
                log_error(f"Failed to load existing asset at '{full_asset_path}'")
                self.stats["errors"].append(asset_name)
                return None, False

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_class)
        
        asset = self.asset_tools.create_asset(asset_name, package_path, asset_class, factory)
        if asset:
            return asset, True
        else:
            log_error(f"Failed to create asset '{asset_name}' in '{package_path}'")
            self.stats["errors"].append(asset_name)
            return None, False

    def setup_blocks(self):
        """Creates or updates starter block definitions: Stone, Dirt, Grass."""
        blocks = {}
        block_configs = [
            ("DA_Block_Stone", 1, "Stone", 0, unreal.LinearColor(0.6, 0.6, 0.6, 1.0)),
            ("DA_Block_Dirt",  2, "Dirt",  1, unreal.LinearColor(0.5, 0.35, 0.2, 1.0)),
            ("DA_Block_Grass", 3, "Grass", 2, unreal.LinearColor(0.3, 0.75, 0.3, 1.0)),
        ]
        
        for asset_name, block_id, display_name, mat_idx, tint in block_configs:
            block, is_new = self.create_or_get_asset(asset_name, self.blocks_path, unreal.VoxelBlockDefinition)
            if not block:
                continue

            key = display_name.lower()
            if is_new:
                block.set_editor_property("block_id", block_id)
                block.set_editor_property("display_name", unreal.Text(display_name))
                block.set_editor_property("is_solid", True)
                block.set_editor_property("generates_collision", True)
                block.set_editor_property("material_layer_index", mat_idx)
                block.set_editor_property("vertex_tint", tint)
                self.editor_asset_lib.save_loaded_asset(block)
                self.stats["created"].append(asset_name)
            elif self.overwrite:
                block.set_editor_property("block_id", block_id)
                block.set_editor_property("display_name", unreal.Text(display_name))
                block.set_editor_property("is_solid", True)
                block.set_editor_property("generates_collision", True)
                block.set_editor_property("material_layer_index", mat_idx)
                block.set_editor_property("vertex_tint", tint)
                self.editor_asset_lib.save_loaded_asset(block)
                self.stats["updated"].append(f"{asset_name} (overwritten)")
            else:
                # Inspect for repair
                current_id = block.get_editor_property("block_id")
                if current_id == 0:
                    block.set_editor_property("block_id", block_id)
                    block.set_editor_property("display_name", unreal.Text(display_name))
                    block.set_editor_property("is_solid", True)
                    block.set_editor_property("generates_collision", True)
                    self.editor_asset_lib.save_loaded_asset(block)
                    self.stats["updated"].append(f"{asset_name} (repaired uninitialized BlockId)")
                else:
                    self.stats["skipped"].append(asset_name)

            blocks[key] = block

        return blocks

    def _build_plains_layers(self, blocks):
        """Helper to construct the 3 standard terrain layers."""
        layer_grass = unreal.VoxelTerrainLayer()
        layer_grass.set_editor_property("block", blocks.get("grass"))
        layer_grass.set_editor_property("thickness_voxels", 1)

        layer_dirt = unreal.VoxelTerrainLayer()
        layer_dirt.set_editor_property("block", blocks.get("dirt"))
        layer_dirt.set_editor_property("thickness_voxels", 3)

        layer_stone = unreal.VoxelTerrainLayer()
        layer_stone.set_editor_property("block", blocks.get("stone"))
        layer_stone.set_editor_property("thickness_voxels", 1)

        return [layer_grass, layer_dirt, layer_stone]

    def setup_biomes(self, blocks):
        """Creates or updates the starter Plains biome with layered terrain."""
        biomes = []
        plains, is_new = self.create_or_get_asset("DA_Biome_Plains", self.biomes_path, unreal.VoxelBiomeDefinition)
        
        if plains:
            if is_new:
                plains.set_editor_property("display_name", unreal.Text("Plains"))
                plains.set_editor_property("temperature_range", unreal.Vector2D(0.0, 1.0))
                plains.set_editor_property("humidity_range", unreal.Vector2D(0.0, 1.0))
                plains.set_editor_property("ambient_tint", unreal.LinearColor.WHITE)
                plains.set_editor_property("vegetation_density", 0.05)
                plains.set_editor_property("terrain_layers", self._build_plains_layers(blocks))
                self.editor_asset_lib.save_loaded_asset(plains)
                self.stats["created"].append("DA_Biome_Plains")
            elif self.overwrite:
                plains.set_editor_property("display_name", unreal.Text("Plains"))
                plains.set_editor_property("temperature_range", unreal.Vector2D(0.0, 1.0))
                plains.set_editor_property("humidity_range", unreal.Vector2D(0.0, 1.0))
                plains.set_editor_property("ambient_tint", unreal.LinearColor.WHITE)
                plains.set_editor_property("vegetation_density", 0.05)
                plains.set_editor_property("terrain_layers", self._build_plains_layers(blocks))
                self.editor_asset_lib.save_loaded_asset(plains)
                self.stats["updated"].append("DA_Biome_Plains (overwritten)")
            else:
                # Inspect for repair (e.g. empty layers or null blocks in layers)
                existing_layers = plains.get_editor_property("terrain_layers")
                needs_repair = len(existing_layers) == 0 or any(l.get_editor_property("block") is None for l in existing_layers)
                if needs_repair:
                    plains.set_editor_property("terrain_layers", self._build_plains_layers(blocks))
                    self.editor_asset_lib.save_loaded_asset(plains)
                    self.stats["updated"].append("DA_Biome_Plains (repaired missing terrain layers)")
                else:
                    self.stats["skipped"].append("DA_Biome_Plains")

            biomes.append(plains)
        return biomes

    def setup_generation_definition(self, blocks):
        """Creates or updates the procedural generation definition."""
        gen_def, is_new = self.create_or_get_asset("DA_Generation_Default", self.config_path, unreal.VoxelGenerationDefinition)
        
        if gen_def:
            if is_new or self.overwrite:
                climate = gen_def.get_editor_property("climate")
                climate.set_editor_property("frequency", 0.001)
                climate.set_editor_property("temperature_seed_offset", 1000)
                climate.set_editor_property("humidity_seed_offset", 2000)
                gen_def.set_editor_property("climate", climate)

                terrain = gen_def.get_editor_property("terrain")
                terrain.set_editor_property("base_height", 64)
                terrain.set_editor_property("height_amplitude", 32.0)
                terrain.set_editor_property("base_frequency", 0.01)
                terrain.set_editor_property("noise_octaves", 4)
                terrain.set_editor_property("lacunarity", 2.0)
                terrain.set_editor_property("persistence", 0.5)
                terrain.set_editor_property("fallback_stone_block", blocks.get("stone"))
                terrain.set_editor_property("fallback_dirt_block", blocks.get("dirt"))
                terrain.set_editor_property("fallback_grass_block", blocks.get("grass"))
                terrain.set_editor_property("fallback_dirt_depth", 4)
                gen_def.set_editor_property("terrain", terrain)

                caves = gen_def.get_editor_property("caves")
                caves.set_editor_property("enabled", True)
                caves.set_editor_property("carve_threshold", 0.58)
                caves.set_editor_property("density_frequency", 0.045)
                caves.set_editor_property("noise_octaves", 3)
                caves.set_editor_property("surface_protection_depth", 3)
                caves.set_editor_property("cave_seed_offset", 5000)
                gen_def.set_editor_property("caves", caves)

                self.editor_asset_lib.save_loaded_asset(gen_def)
                if is_new:
                    self.stats["created"].append("DA_Generation_Default")
                else:
                    self.stats["updated"].append("DA_Generation_Default (overwritten)")
            else:
                # Inspect for repair (missing fallback block references)
                terrain = gen_def.get_editor_property("terrain")
                repaired = False
                if terrain.get_editor_property("fallback_stone_block") is None and blocks.get("stone"):
                    terrain.set_editor_property("fallback_stone_block", blocks.get("stone"))
                    repaired = True
                if terrain.get_editor_property("fallback_dirt_block") is None and blocks.get("dirt"):
                    terrain.set_editor_property("fallback_dirt_block", blocks.get("dirt"))
                    repaired = True
                if terrain.get_editor_property("fallback_grass_block") is None and blocks.get("grass"):
                    terrain.set_editor_property("fallback_grass_block", blocks.get("grass"))
                    repaired = True
                if repaired:
                    gen_def.set_editor_property("terrain", terrain)
                    self.editor_asset_lib.save_loaded_asset(gen_def)
                    self.stats["updated"].append("DA_Generation_Default (repaired missing fallback blocks)")
                else:
                    self.stats["skipped"].append("DA_Generation_Default")

        return gen_def

    def setup_streaming_preset(self):
        """Creates or updates the streaming preset with distance bands and budgets."""
        streaming, is_new = self.create_or_get_asset("DA_Streaming_Default", self.config_path, unreal.VoxelStreamingPreset)
        
        if streaming:
            if is_new or self.overwrite:
                streaming.set_editor_property("simulation_distance", 4)
                streaming.set_editor_property("render_distance", 8)
                streaming.set_editor_property("generation_distance", 10)
                streaming.set_editor_property("persistence_distance", 12)
                streaming.set_editor_property("streaming_budget_ms", 1.5)
                streaming.set_editor_property("max_mesh_finalizations_per_tick", 4)
                streaming.set_editor_property("max_collision_finalizations_per_tick", 4)
                self.editor_asset_lib.save_loaded_asset(streaming)
                if is_new:
                    self.stats["created"].append("DA_Streaming_Default")
                else:
                    self.stats["updated"].append("DA_Streaming_Default (overwritten)")
            else:
                render_dist = streaming.get_editor_property("render_distance")
                if render_dist <= 0:
                    streaming.set_editor_property("simulation_distance", 4)
                    streaming.set_editor_property("render_distance", 8)
                    streaming.set_editor_property("generation_distance", 10)
                    streaming.set_editor_property("persistence_distance", 12)
                    self.editor_asset_lib.save_loaded_asset(streaming)
                    self.stats["updated"].append("DA_Streaming_Default (repaired uninitialized distances)")
                else:
                    self.stats["skipped"].append("DA_Streaming_Default")

        return streaming

    def setup_physics_preset(self):
        """Creates or updates the Chaos physical collision preset."""
        physics, is_new = self.create_or_get_asset("DA_Physics_Default", self.config_path, unreal.VoxelPhysicsPreset)
        
        if physics:
            if is_new or self.overwrite:
                physics.set_editor_property("collision_mode", unreal.VoxelCollisionMode.COMPLEX)
                physics.set_editor_property("async_cooking", True)
                physics.set_editor_property("collision_profile_name", "BlockAll")
                self.editor_asset_lib.save_loaded_asset(physics)
                if is_new:
                    self.stats["created"].append("DA_Physics_Default")
                else:
                    self.stats["updated"].append("DA_Physics_Default (overwritten)")
            else:
                self.stats["skipped"].append("DA_Physics_Default")

        return physics

    def setup_world_definition(self, gen_def, biomes, streaming_preset, physics_preset):
        """Creates or updates the composite World Definition connecting all presets."""
        world_def, is_new = self.create_or_get_asset("DA_VoxelWorld_Default", self.config_path, unreal.VoxelWorldDefinition)
        
        if world_def:
            if is_new or self.overwrite:
                world_def.set_editor_property("world_name", unreal.Text("Default Voxel World"))
                world_def.set_editor_property("world_seed", 1234)
                world_def.set_editor_property("voxel_world_size", 100.0)
                world_def.set_editor_property("generation_definition", gen_def)
                world_def.set_editor_property("biomes", biomes)
                world_def.set_editor_property("streaming_preset", streaming_preset)
                world_def.set_editor_property("physics_preset", physics_preset)
                self.editor_asset_lib.save_loaded_asset(world_def)
                if is_new:
                    self.stats["created"].append("DA_VoxelWorld_Default")
                else:
                    self.stats["updated"].append("DA_VoxelWorld_Default (overwritten)")
            else:
                # Inspect for repair (missing references)
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
                    self.stats["updated"].append("DA_VoxelWorld_Default (wired missing references)")
                else:
                    self.stats["skipped"].append("DA_VoxelWorld_Default")

        return world_def

    def configure_project_defaults(self, world_def):
        """Configures UVoxelWorldSettings::DefaultWorldDefinition in Project Settings and flushes to config ini."""
        try:
            settings = None
            if hasattr(unreal, "VoxelWorldSettings"):
                settings = unreal.get_default_object(unreal.VoxelWorldSettings)
            if settings:
                settings.set_editor_property("default_world_definition", world_def)
                settings.modify()
                if hasattr(settings, "try_update_default_config_file"):
                    settings.try_update_default_config_file()
                elif hasattr(settings, "save_config"):
                    settings.save_config()
                log_info("Configured and persisted Project Settings -> Plugins -> Voxel World -> DefaultWorldDefinition")
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
        log_info(f"  Created: {len(self.stats['created'])} ({', '.join(self.stats['created']) if self.stats['created'] else 'None'})")
        log_info(f"  Updated: {len(self.stats['updated'])} ({', '.join(self.stats['updated']) if self.stats['updated'] else 'None'})")
        log_info(f"  Skipped: {len(self.stats['skipped'])} ({', '.join(self.stats['skipped']) if self.stats['skipped'] else 'None'})")
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


if __name__ == "__main__":
    run()
