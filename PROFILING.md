# Profiling

Per-frame CPU profiler with CSV export and a matplotlib comparison plot.

## What gets captured

Each frame the engine records the CPU time of:
- `Renderer_DrawCallSort`, `Renderer_RecordCmds` &mdash; Vulkan parallelizable work
- `Renderer_QueueSubmit`, `Renderer_Present`, `Renderer_FenceWait` &mdash; serial / GPU sync
- `Physics`, `Physics_Raycast`
- `Animation`, `Animation_System`
- `AI_Threading` (EnemyAI), `Movement`, `PlayerInput`, `Combat`, `Health`, `Despawn`
- `Render_Submit`, `Renderer_DrawFrame`, `Scene::Update`

The output is `profile/wave<N>.csv` per run.

## Headless capture (recommended)

Launch the game with CLI flags. It forces `GameState::Playing`, starts at the
requested wave, captures for N seconds, then exits.

### Windows

```powershell
# 1) Build (use the MSBuild or make sure you run release once in Visual Studio)
MSBuild AdventureEngine.sln /t:Build /p:Configuration=release /p:Platform=x64

# 2) One-shot sweep: runs wave 1, 5, 10 and writes the comparison PNGs
python tools/run_profile_sweep.py --waves 1 5 10 --seconds 10 --config release

# Or a single capture
bin\release-game.exe --profile-wave 5 --profile-seconds 10 --profile-out profile
```

### Linux

```bash
# 1) Build (premake5 generates a Makefile)
./premake5 gmake2
make config=release_x86_64 -j

# 2) One-shot sweep
python3 tools/run_profile_sweep.py --waves 1 5 10 --seconds 10 \
    --exe bin/release-game

# Or a single capture
./bin/release-game --profile-wave 5 --profile-seconds 10 --profile-out profile
```

Outputs (in `profile/`):
- `wave1.csv`, `wave5.csv`, `wave10.csv`
- `cpu_breakdown.png` &mdash; mean ms/frame per section, grouped bars per wave
- `cpu_timeline.png`  &mdash; per-frame timeline for the three critical sections
