# FO4Upscaling

F4SE plugins for Fallout 4 providing frame generation and spatial upscaling (FSR3 + DLSS).

Fork of [doodlum/fo4test](https://github.com/doodlum/fo4test) migrated to [DearModdingFO4 CommonLibF4](https://github.com/Dear-Modding-FO4/commonlibf4) for single-DLL multi-runtime support (OG/NG/AE).

## Plugins

| Plugin | DLL | Description |
|--------|-----|-------------|
| **AAAFrameGeneration** | `AAAFrameGeneration.dll` | AMD FidelityFX frame generation via D3D11/D3D12 interop |
| **Upscaling** | `Upscaling.dll` | Spatial upscaling with AMD FSR3 and NVIDIA DLSS via Streamline |

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
  FrameGeneration/       # AAAFrameGeneration target
  Upscaling/             # Upscaling target
include/                 # Shared headers (PCH, ENB SDK, Detours)
extern/
  CommonLibF4/           # DearModdingFO4 (address-independent, all runtimes)
  FidelityFX-SDK/        # AMD FSR3 SDK
  Streamline/            # NVIDIA Streamline SDK (DLSS)
cmake/
  Common.cmake           # Shared build config (configure_xse_plugin function)
scripts/
  deploy.sh              # Build + deploy to MO2 test mod
  test.sh                # Automated game test pipeline
```

## License

MIT
