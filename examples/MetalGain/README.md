# Metal Gain Example

Minimal OpenFX image effect plugin for Livegrade Frontier. It applies an RGBA gain and a star-shaped vignette, rendered on the GPU through Metal textures, and declares the properties a plugin requires to run in Livegrade.

The host behavior referenced in this document (Metal texture handover, the Filter context, the spatial-free render variant, and the coordinate convention) is specified in the repository root, [github.com/pomfort/openfx-plugins](https://github.com/pomfort/openfx-plugins). This document covers the example itself only.

## Scope

The plugin combines two operations in a single Metal kernel:

* Gain: a per-pixel operation applied to the R, G, B and A channels
* Vignette: a position-dependent operation

The plugin declares `kOfxImageEffectPropNoSpatialAwareness`. When the host requests a render without spatial awareness, the kernel omits the vignette and applies the gain only. The orientation of the vignette verifies the coordinate convention of the images the host provides, as described under [Verification](#verification).

## Build and installation

### Requirements

* macOS with the Xcode command line tools (`xcode-select --install`)
* Apple silicon, matching the architecture Livegrade runs as. [Build](#build) describes universal builds.

### Build

```sh
make
```

All generated files are written to `build/`; the source directories remain unchanged.

```
build/
├── obj/                                # object files and header dependencies
└── MetalGainExample.ofx.bundle/
    └── Contents/
        ├── Info.plist
        └── MacOS/MetalGainExample.ofx
```

Rebuilds are incremental and header-aware. Editing `src/MetalGainExample.h` or an OpenFX header recompiles the dependent objects only.

The default architecture is `arm64`. Override `ARCHS` for a universal binary:

```sh
make ARCHS="arm64 x86_64"
```

`make clean` removes `build/` and all generated files.

### Xcode project

The repository contains no Xcode project. [XcodeGen](https://github.com/yonaskolb/XcodeGen) generates one from `project.yml`:

```sh
brew install xcodegen
xcodegen generate
open MetalGainExample.xcodeproj
```

The generated project builds the same bundle as the Makefile and is not committed. Regenerate it after adding or removing source files, and keep `project.yml` and the `Makefile` in sync.

Products are written to `build/xcode/<configuration>/` rather than to Xcode's derived data directory, and `make clean` removes them together with the Makefile's output. The build does not install the bundle. To install a build from Xcode, copy it to a plugin directory:

```sh
cp -R build/xcode/Debug/MetalGainExample.ofx.bundle ~/Library/OFX/Plugins/
```

An `.ofx` bundle cannot be launched directly. For debugging, set the scheme's run executable to the host application and attach to it.

### Installation

`make install` copies the bundle to the directory specified by `OFX_PLUGIN_PATH`, which defaults to the user plugin directory `~/Library/OFX/Plugins`. It is writable without elevated privileges:

```sh
make install
```

For a system-wide installation, set `OFX_PLUGIN_PATH` to the system plugin directory. It is not writable by regular users, so the command needs `sudo`:

```sh
sudo make install OFX_PLUGIN_PATH=/Library/OFX/Plugins
```

Livegrade scans the user directory first. A plugin installed there shadows a plugin with the same identifier in the system directory.

### Verification

Add an **OpenFX** node in Livegrade and select the plugin from the **Plugin** menu. Use **Rescan** if Livegrade was running during installation.

Expected result:

* Four sliders scale the R, G, B and A channels of the image.
* A star-shaped vignette covers the image, with its center spike pointing up.

The orientation of the vignette confirms the coordinate convention. The kernel indexes the texture with y increasing upwards, matching the OpenFX bottom-left origin of the images the host provides. A vignette pointing down indicates that images arrive vertically flipped.

Setting the node to **Non-Spatial Effects Only** in Livegrade removes the vignette and leaves the gain. This is the render variant the host uses when a look is baked into a 3D LUT.

## Project structure

| Path | Contents |
|---|---|
| `src/MetalGainExample.cpp` | Plugin description, parameters and render action |
| `src/MetalGainExample.h` | Plugin factory declaration |
| `src/MetalKernel.mm` | Metal compute kernel and pipeline setup |
| `src/SupportWebpage.mm` | Opens a webpage in the default browser |
| `Info.plist` | Bundle metadata, copied into the built bundle |
| `third_party/openfx/include` | OpenFX headers, version 1.5.1, unmodified |
| `third_party/openfx/Support` | OpenFX C++ support library, compiled into the plugin, extended as described under [The Metal texture extension](#the-metal-texture-extension) |
| `third_party/extensions/include` | `ofxMetalTexture.h`, not part of OpenFX, see [The Metal texture extension](#the-metal-texture-extension) |
| `Makefile` | Build rules. `OFX_PLUGIN_PATH` and `ARCHS` are overridable |
| `project.yml` | XcodeGen specification for generating an Xcode project |
| `build/` | Generated files, created by `make`, not committed |

## Recommendations for writing custom plugins

These recommendations apply to writing or adjusting OpenFX plugins for use in Pomfort products. Pomfort applications process live video signals. The cost of a frame is incurred in the signal path rather than in an export queue, and the render path must remain predictable.

* **Image processing in a Metal kernel.** `src/MetalKernel.mm` contains the complete pattern: kernel source, pipeline setup, and the dispatch onto the command queue the host provides.
* **Precompiled Metal libraries.** This example compiles its kernel source at load time. A plugin can also ship a prebuilt `.metallib` in its bundle and call into it.
* **No CPU-based image processing.** Per-pixel loops and fallback paths that read a texture back into host memory are not suitable. Pixel operations on the CPU compete with the live signal path.
* **Descriptive tasks in C++.** `src/MetalGainExample.cpp` describes the plugin, its parameters and their user interface, and reads parameter values per render. The OpenFX support library additionally offers a CPU render path through `multiThreadProcessImages()`; this example leaves it unimplemented and overrides `processImagesMetalTextures()` instead. Code that touches pixels belongs in `src/MetalKernel.mm` in all cases.


## The Metal texture extension

`ofxMetalTexture.h` defines the two properties required for Metal texture handover:

```c
#define kOfxImageEffectPropMetalTextureSupported "OfxImageEffectPropMetalTextureSupported"
#define kOfxImageEffectPropMetalTextureEnabled   "OfxImageEffectPropMetalTextureEnabled"
```

These properties are not part of the OpenFX standard. They are an extension proposed by Video Village and adopted by hosts and plugins ahead of standardization. The header is located in `third_party/extensions`, outside `third_party/openfx`, which contains an unmodified copy of the OpenFX project.

The bundled OpenFX C++ support library is extended accordingly. It adds `setSupportsMetalTexture()`, the `isEnabledMetalTexture` render argument, and the `processImagesMetalTextures()` hook this example implements. None of these exist upstream.

The support library also adds `setSupportsNoSpatialAwareness()` and the `hasNoSpatialAwareness` render argument. The underlying property is part of OpenFX 1.5.1; only the support library plumbing is added here.

A plugin built against a different copy of OpenFX requires the same extension header and the same support library additions, or direct use of the OpenFX C API.

## License

BSD 3-Clause, see [LICENSE](LICENSE). The bundled OpenFX headers and support library carry their own copyright under the same license.
