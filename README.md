# FO4Upscaling

F4SE plugins for Fallout 4 providing frame generation and spatial upscaling.

Fork of [doodlum/fo4test](https://github.com/doodlum/fo4test) migrated to [DearModdingFO4 CommonLibF4](https://github.com/Dear-Modding-FO4/commonlibf4) for single-DLL multi-runtime support (OG/NG/AE).

## Plugins

| Plugin | DLL | Description |
|--------|-----|-------------|
| **AAAFrameGeneration** | `AAAFrameGeneration.dll` | Frame generation with FSR3 (AMD) and DLSS-G (NVIDIA RTX 40+) via D3D11/D3D12 interop |
| **Upscaling** | `Upscaling.dll` | Spatial upscaling with AMD FSR3 and NVIDIA DLSS via Streamline |

## Frame Generation

The FrameGeneration plugin supports two backends, configurable via `FrameGeneration.ini`:

| Backend | Setting | GPU Requirement | Description |
|---------|---------|-----------------|-------------|
| **FSR3** | `iFrameGenType=0` | Any GPU | AMD FidelityFX frame interpolation |
| **DLSS-G** | `iFrameGenType=1` | NVIDIA RTX 40+ | NVIDIA DLSS Frame Generation via Streamline SDK |

DLSS-G integration uses the NVIDIA Streamline SDK with:
- D3D11-to-D3D12 interop (game renders D3D11, frame gen runs D3D12)
- Reflex low-latency markers for frame pacing
- Automatic swap chain management via Streamline's manual hooking API

## Requirements

- [Visual Studio 2022](https://visualstudio.microsoft.com/) (Desktop C++ workload)
- [CMake 3.21+](https://cmake.org/)
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` environment variable set
- [Git](https://git-scm.com/)

## User Requirements

- [Address Library for F4SE](https://www.nexusmods.com/fallout4/mods/47327)
- [Fallout 4 Script Extender (F4SE)](https://f4se.silverlock.org/)

## Build

```bash
git clone --recursive https://github.com/northaxosky/FO4Upscaling.git
cd FO4Upscaling
cmake -S . --preset=default
cmake --build build --config Release
```

Output:
- `build/plugins/FrameGeneration/Release/AAAFrameGeneration.dll`
- `build/plugins/Upscaling/Release/Upscaling.dll`

## Scripts

Before using scripts, copy `scripts/.env.example` to `scripts/.env` and configure paths for your setup:

```bash
cp scripts/.env.example scripts/.env
# Edit scripts/.env with your MO2 mod folder, game paths, etc.
```

### Deploy to MO2 mod folder

```bash
./scripts/deploy.sh build    # Build + deploy DLLs and shaders
./scripts/deploy.sh deploy   # Deploy only (skip build)
```

### Automated test

```bash
./scripts/test.sh            # Build, deploy, launch game, monitor logs, screenshot, cleanup
```

Results are saved to `test-results/<timestamp>/` with logs, screenshots, and a pass/fail summary.

## Project Structure

```
plugins/
  FrameGeneration/       # AAAFrameGeneration target (FSR3 + DLSS-G frame gen)
  Upscaling/             # Upscaling target (FSR3 + DLSS spatial upscaling)
include/                 # Shared headers (PCH, ENB SDK, Detours)
extern/
  CommonLibF4/           # DearModdingFO4 (address-independent, all runtimes)
  FidelityFX-SDK/        # AMD FSR3 SDK
  Streamline/            # NVIDIA Streamline SDK v2.10.3 (DLSS, DLSS-G, Reflex)
package/
  F4SE/Plugins/          # Runtime files (HLSL shaders, INI configs, Streamline DLLs)
cmake/
  Common.cmake           # Shared build config (configure_xse_plugin function)
scripts/
  .env.example           # Template for local path config
  deploy.sh              # Build + deploy to MO2 test mod
  test.sh                # Automated game test pipeline
  compare-fg.sh          # A/B/C comparison: No FG vs FSR3 vs DLSS-G
```

## License

MIT
