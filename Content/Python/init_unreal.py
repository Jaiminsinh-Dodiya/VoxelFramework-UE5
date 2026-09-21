# Copyright Epic Games, Inc. All Rights Reserved.
"""
init_unreal.py

Editor startup script for VoxelFramework plugin.
Notifies the developer that VoxelFramework is loaded and provides the Python bootstrap command.
"""

import unreal

unreal.log("[VoxelFramework] Plugin loaded. To bootstrap starter assets, run: import VoxelFramework.SetupVoxelFramework as setup; setup.run()")
