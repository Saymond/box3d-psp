# box3d-psp

A port of [Box3D](https://github.com/erincatto/box3d) — Erin Catto's 3D physics engine — to the Sony PlayStation Portable.

- **Physics** runs on the Allegrex **VFPU** (vector FPU) via a new `B3_SIMD_VFPU` path in Box3D's SIMD shim.
- **Graphics** run on the PSP **GU** (graphics unit) with hardware transform and a CPU-computed per-vertex lighting pass.
- **4 interactive scenes**: stacks, brick wall, pyramid, drivable car on bumpy terrain.

## Repository layout

```
box3d-psp/
├── src/
│   ├── psp_main.c           # entry point + 4 demo scenes
│   ├── psp_host.c           # GU init, pad, camera, vertex batching, sky, lighting
│   ├── psp_host.h
│   ├── vfpu_math.c          # VFPU mat4 / quat / sin / cos / sqrt
│   ├── vfpu_math.h
│   └── psp_box3d_config.h   # Box3D capacity overrides
├── box3d-patches/
│   ├── simd_vfpu.h          # VFPU SIMD shim (drop into box3d/src/)
│   ├── 0001-core-h-add-psp-vfpu-detection.patch
│   └── 0002-simd-h-include-vfpu-shim.patch
├── CMakeLists.txt           # psp-cmake build script
├── LICENSE
└── README.md
```

## Requirements

- **PSPDEV toolchain** (psp-gcc 15.2.0+, pspsdk): [github.com/pspdev/pspdev/releases](https://github.com/pspdev/pspdev/releases)
- **Box3D source** (clone upstream next to this repo):
  ```bash
  git clone https://github.com/erincatto/box3d.git
  ```
- **PPSSPP** (optional, for testing): [ppsspp.org](https://www.ppsspp.org/)

## Building

```bash
# 1. Clone Box3D next to box3d-psp
git clone https://github.com/erincatto/box3d.git

# 2. Apply patches
cd box3d
git apply ../box3d-psp/box3d-patches/0001-core-h-add-psp-vfpu-detection.patch
git apply ../box3d-psp/box3d-patches/0002-simd-h-include-vfpu-shim.patch
cp ../box3d-psp/box3d-patches/simd_vfpu.h src/simd_vfpu.h
cd ..

# 3. Configure environment
export PSPDEV=/home/z/pspdev       # or wherever you installed the toolchain
export PATH="$PATH:$PSPDEV/bin"

# 4. Build
cd box3d-psp
psp-cmake -B build -S . -DCMAKE_C_COMPILER=$(which psp-gcc)
make -C build -j2

# 5. EBOOT.PBP appears in build/
```

## Running

### Real PSP
Copy `build/EBOOT.PBP` to `PSP/GAME/BOX3D_PSP/EBOOT.PBP` on the memory stick. Requires custom firmware (CFW).

### PPSSPP
```bash
ppsspp build/EBOOT.PBP
```

## Scenes

| # | Scene | Description |
|---|-------|-------------|
| 0 | **STACKS** | Ground plane + stacks of orange boxes, blue spheres, and a small box pyramid |
| 1 | **WALL** | Brick wall (8×6, running bond) — knock it over with projectiles |
| 2 | **PYRAMID** | Large 5-layer box pyramid (55 boxes) |
| 3 | **CAR** | Drivable yellow car on bumpy displacement-noise terrain with obstacle boxes |

## Controls

| Button | Action |
|--------|--------|
| START | Switch to next scene |
| SELECT | Launch a sphere projectile from the camera |
| Analog nub | Orbit camera look (yaw/pitch) |
| D-pad | Pan target (CAR scene: accelerate/brake) |
| L / R | Dolly (CAR scene: steer left/right) |
| X | Drop a box from above (not in CAR scene) |
| O | Reset current scene |
| START + SELECT | Quit |

## VFPU integration

Box3D's SIMD abstraction (`src/simd.h`) defines ~25 operations on a 4-float vector type `b3V32`. The SSE2 path uses `_mm_add_ps` etc.; this port adds a `B3_SIMD_VFPU` path that implements the same API with VFPU inline assembly:

| Box3D op | VFPU instruction |
|----------|------------------|
| `b3AddV` | `vadd.t` |
| `b3SubV` | `vsub.t` |
| `b3MulV` | `vmul.t` |
| `b3DivV` | `vdiv.t` |
| `b3MinV` | `vmin.t` |
| `b3MaxV` | `vmax.t` |
| `b3CrossV` | `vcrs.t` |
| `b3AbsV` | `vabs.t` |
| `b3NegV` | `vneg.t` |
| load / store | `lv.q` / `sv.q` |

These ops are used in the contact solver, broad-phase AABB tests, and mesh collision detection.

## GU rendering

- **Display**: 480×272, `GU_PSM_5551` color + depth, double-buffered in VRAM (stride 512)
- **Vertex format**: `GU_COLOR_8888 | GU_VERTEX_32BITF` — 16 bytes per vertex, 16-byte aligned
- **Transform**: `GU_TRANSFORM_3D` via `sceGumPerspective` + `sceGumLookAt`
- **Lighting**: CPU-computed per-vertex (`material × (ambient + sun × NdotL)`), packed into `GU_COLOR_8888`. Backend-agnostic — avoids GE lighting pipeline differences across PPSSPP backends.
- **Sky**: fullscreen gradient quad in screen space (`GU_TRANSFORM_2D`)
- **Geometry**: hull faces fan-triangulated, UV-sphere tessellation (6 rings × 10 segments), capsule = 2 endcap spheres

## Credits

- [Box3D](https://github.com/erincatto/box3d) by Erin Catto — MIT License
- [PSPDEV toolchain](https://github.com/pspdev) — BSD License
- [PPSSPP](https://github.com/hrydgard/ppsspp) — GPL 2.0+

## License

MIT — see [LICENSE](LICENSE)
