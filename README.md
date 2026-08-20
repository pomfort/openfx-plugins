<img src="img/fn-icon.png" alt="Pomfort" width="77" align="right" >

# OpenFX for Livegrade

![Standard](img/openfx-version-badge.svg)
![Rendering](img/metal-badge.svg)

OpenFX is the established, vendor-neutral plugin standard for image effects under the Academy Software Foundation. Livegrade Frontier hosts OpenFX image effect plugins as nodes in its look pipeline and applies them to **live camera signals in real time**, on film sets and in live productions, for one or several video streams simultaneously. Third-party effects such as film emulation, halation, diffusion or texture become part of the live grade, next to Pomfort's own grading and effect nodes.

Compatible with:
* Livegrade Frontier

The current Livegrade Frontier download and the full user documentation, including how OpenFX plugins are applied in the user interface, are available in the [Pomfort Account](https://account.pomfort.com).

## Introduction

This is the technical documentation for **plugin developers**: what Livegrade's OpenFX host offers, and what a plugin has to declare to work in it.

> [!NOTE]
> The OpenFX effect node is part of **Livegrade Frontier**. The regular Livegrade edition does not host OpenFX plugins yet.

* Livegrade implements a host for the OpenFX **Image Effect** plugin API (`kOfxImageEffectPluginApi`), supporting version 1.5 or newer of the standard.
* It is a **GPU-only, Metal-only** host: images are handed over as Metal textures, never as CPU buffers, and rendering happens on the Metal command queue the host provides. A plugin therefore has to declare [Metal textures and coordinates](#metal-textures-and-coordinates-required) to be usable at all, and it has to support `kOfxImageEffectContextFilter` with RGBA source/output clips ([Filter context and RGBA](#filter-context-and-rgba-required)).
* Optionally it declares [Rendering without spatial awareness](#rendering-without-spatial-awareness-optional), which lets its effect be included when a look is baked into a LUT.

Everything else about the host is announced through the OpenFX API and can be read at runtime: contexts, components, pixel depths, the suites it provides, the actions it issues and their arguments.

### The live use case

OpenFX plugins are usually written for applications that render a timeline: frames are produced ahead of time, cached, and re-rendered when something changes. Livegrade instead applies looks to **live video signals**: the picture coming out of the camera while it is being shot on a film set, or while a live production is on air. A plugin in the node chain renders every incoming frame at signal frame rate, for the viewer and for the SDI outputs. There is no pre-render and no cache. A plugin that does not keep up causes dropped frames in the live signal rather than a longer export.

This has two consequences for plugin design:

* **One plugin instance per video stream.** Livegrade can process several streams at the same time, each with its own node chain and its own parameter values. The same plugin is instantiated several times and renders concurrently on different images, so instances must not share mutable state and per-instance GPU resources should be sized accordingly ([Render thread safety](#render-thread-safety)).
* **Parameters change while rendering runs.** Parameter edits reach the plugin as `instanceChanged` between renders, and the result is expected on the live picture immediately. Expensive setup work triggered by such an edit becomes visible as an interruption in the signal.

Besides the live path, the same plugin also renders stills. A plugin that declares `kOfxImageEffectPropNoSpatialAwareness` additionally renders LUTs, which is how a look reaches LUT boxes, cameras and other LUT processing devices ([Rendering without spatial awareness](#rendering-without-spatial-awareness-optional)).

### Host capabilities

* The **Filter** context (one source clip, one output clip), RGBA
* Metal texture rendering on the command queue passed with the render action. Textures use the OpenFX bottom-left origin ([Metal textures and coordinates](#metal-textures-and-coordinates-required))
* Region of interest / region of definition negotiation
* Parameters, parameter groups, and the `instanceChanged` cascade
* Several instances of the same plugin, rendering different live streams in parallel

Not supported:

* Contexts other than Filter (no generators, transitions, paint, retimers)
* CPU, OpenGL, OpenCL and CUDA rendering
* Components other than RGBA, multiple clip depths or pixel aspect ratios
* Parameter animation and keyframes, custom interacts and overlays, parametric parameters, parameter pages
* Tiled rendering and temporal clip access: a plugin only ever sees the frame it is asked to render

### Installation and discovery

Plugins are ordinary OpenFX bundles (`<name>.ofx.bundle`), discovered in `~/Library/OFX/Plugins` and `/Library/OFX/Plugins`. The user directory is scanned first and shadows a plugin of the same identifier in the system directory. A bundle outside these locations can be loaded explicitly from the node's plugin menu (**Load…**); **Rescan** picks up plugins installed while Livegrade is running. The bundle's binary is loaded dynamically and has to contain native Apple silicon (`arm64`) code. Only the first plugin of a bundle is used.

A look stores which plugin was used and the parameter values, not the plugin itself. Where the plugin is missing, the node passes the image through unchanged and keeps the stored values, so the look survives the round trip and works again once the plugin is installed.

## Example plugin and recommendations for writing plugins

[`examples/MetalGain`](examples/MetalGain) contains a complete, buildable plugin covering Metal textures, the Filter context and the spatial-free variant. It is an example implementation of the requirements described below, and the fastest way to see a working plugin in Livegrade.

Its [README](examples/MetalGain/README) also documents how a plugin should be written for a host that renders a live signal:

* [Recommendations for writing custom plugins](examples/MetalGain/README.md#recommendations-for-writing-custom-plugins): where image processing belongs, why CPU pixel paths are unsuitable, and how descriptive code and kernel code are separated
* [The Metal texture extension](examples/MetalGain/README.md#the-metal-texture-extension): the extension header and the support library additions a plugin needs for Metal texture handover
* [Build and installation](examples/MetalGain/README.md#build-and-installation): Makefile and Xcode project setup, and how to verify the coordinate convention and the spatial-free render in Livegrade

## Requirements for plugins

### Metal textures and coordinates (required)

Livegrade hands over images as Metal textures. The plugin must declare this capability on its **plugin descriptor**, during the `kOfxActionDescribe` action:

```c
gPropertySuite->propSetString(effectProps,
                              kOfxImageEffectPropMetalRenderSupported, 0, "true");
gPropertySuite->propSetString(effectProps,
                              kOfxImageEffectPropMetalTextureSupported, 0, "true");
```

`kOfxImageEffectPropMetalTextureSupported` is an OpenFX extension (proposed by Video Village) that turns the Metal render path from `MTLBuffer` handover into `MTLTexture` handover:

```c
#define kOfxImageEffectPropMetalTextureSupported "OfxImageEffectPropMetalTextureSupported"
#define kOfxImageEffectPropMetalTextureEnabled   "OfxImageEffectPropMetalTextureEnabled"
```

A plugin that does not declare it **cannot be used**: the node reports that the plugin does not support Metal texture handover and fails to load without images being processed. On its own descriptor the host declares both Metal properties as `"true"` and the OpenGL, OpenCL and CUDA ones as `"false"`, which allows a plugin to verify the render path before declaring its own capabilities.

For a plugin that also runs in other hosts, Metal texture handover is normally an **additional** render path rather than a replacement. Hosts that do not know the extension ignore the property and keep handing over images the way they already do (as `MTLBuffer`, or through the CPU, CUDA or OpenCL paths) and the plugin selects its path per render from the `…Enabled` properties in the render in-args. Declaring `kOfxImageEffectPropMetalTextureSupported` therefore adds Livegrade support without affecting where the plugin already works.

In the render actions the host sets `kOfxImageEffectPropMetalTextureEnabled` to `1` and `kOfxImageEffectPropMetalEnabled` to `0`. `kOfxImagePropData` of both the source and the output image is then an `id<MTLTexture>` (bottom-left origin, see below), and `kOfxImageEffectPropMetalCommandQueue` in the render in-args is the `id<MTLCommandQueue>` to encode onto. The plugin owns neither texture: it must not release them, and it must not assume the same textures are used for the next render.

While Metal places a texture's origin at the top left, OpenFX places an image's origin at the bottom left, with y increasing upwards. The textures Livegrade passes follow the **OpenFX convention**: their origin is the bottom left, consistent with `kOfxImagePropBounds`, `kOfxImagePropRegionOfDefinition`, the regions of interest and the render window. A plugin therefore works in OpenFX coordinates throughout and never has to flip anything.

> [!NOTE]
> `kOfxImageEffectPropPixelDepth` describes the OpenFX image, not the texture. The texture may use a different format, currently half-float RGBA. **Read the pixel format from the `MTLTexture` itself** rather than relying on the declared depth. `kOfxImagePropRowBytes` has no meaning for a texture and is reported as `0`.

### Filter context and RGBA (required)

The host describes the plugin in the **Filter** context only, and offers exactly two clips:

* `"Source"`: the image coming from the previous node
* `"Output"`: the image the plugin writes

Both clips have to declare support for `kOfxImageComponentRGBA` in `kOfxImageEffectPropSupportedComponents` during `kOfxImageEffectActionDescribeInContext`. A plugin that defines a different set of clips, or clips that do not support RGBA, cannot be instantiated.

### Rendering without spatial awareness (optional)

Livegrade can bake a look into a 3D LUT for LUT boxes in a live signal chain, for cameras and monitoring paths, and for on-set deliverables. A spatial effect (a blur, a glow, grain that depends on pixel position) cannot be expressed as a LUT, so by default a node hosting an OpenFX plugin is **excluded from LUT creation**: it renders in the viewer and on SDI, but is left out when a LUT is generated.

A plugin that can render *without spatial awareness*, either inherently or by disabling its spatial parts on request, can opt in by declaring this on its plugin descriptor (`kOfxActionDescribe`):

```c
gPropertySuite->propSetString(effectProps,
                              kOfxImageEffectPropNoSpatialAwareness, 0, "true");
```

The property was added in OpenFX 1.5.1 and is part of the standard. When a plugin declares it, Livegrade includes the node in LUT creation and signals the spatial-free render by setting the same property to `"true"` in the in-args of `kOfxImageEffectActionBeginSequenceRender`, `kOfxImageEffectActionEndSequenceRender` and `kOfxImageEffectActionRender`. The plugin is then expected to disable every effect that depends on the position of a pixel, so that the same input color always produces the same output color. The host trusts that declaration and does not verify the behavior.

Spatial-free rendering uses **separate instances**, created with the flag set for the whole sequence, so a node renders live through one instance and bakes through another.

Livegrade also offers the spatial-free variant as a **preview**: for a plugin that declares the property, the node's settings show a *Full Plugin Mode* / *Non-Spatial Effects Only* selector, and the second mode renders the viewer through a spatial-free instance as well, showing exactly what LUT-based outputs receive. The setting is a verification aid. It is not stored with the look, and a recalled look is always in *Full Plugin Mode*. During development it allows a direct comparison of the spatial-free variant against the full render.

For plugins that are purely color operations, such as film emulation, print emulation or tone mapping, declaring this property is recommended. It is what makes a look containing the plugin usable in a LUT-based workflow.

### Render thread safety

The host honors `kOfxImageEffectPluginRenderThreadSafety`:

| Declared value | How the host schedules renders |
|---|---|
| `kOfxImageEffectRenderUnsafe` | All instances of the plugin are serialized on one queue |
| `kOfxImageEffectRenderInstanceSafe` | One queue per instance; different instances may render in parallel |
| `kOfxImageEffectRenderFullySafe` | Treated like instance-safe |

`kOfxImageEffectRenderInstanceSafe` is the expected and best-tested case.

The host may hold **several instances of the same plugin at once**: one per node, per live video stream and per grading slot ([The live use case](#the-live-use-case)), instances kept alive briefly so they can be reused, and separate instances for spatial-free rendering ([Rendering without spatial awareness](#rendering-without-spatial-awareness-optional)). Instances must not share mutable state, and per-instance caches should be sized with that in mind.

### Parameters

Livegrade builds the node's user interface from the plugin's parameter definitions. The scalar types (numbers, booleans, choices including string choices, text and push buttons) each get a native control, and labels, hints, ranges, display ranges, digits, enabled and secret flags are honored. Groups (`kOfxParamPropParent`) become collapsible sections and are the way to structure a large parameter set; parameter pages are ignored. Multi-dimensional types (positions, sizes, colors) and custom or parametric parameters have no user interface: they keep their default value and are not stored in the look.

Parameter values are static per look: the host advertises no animation support and never issues keyframe calls. `instanceChanged` is fully supported, including push buttons, and any metadata or values the plugin changes in response are read back into the user interface immediately.

For parameter types or layout hints not covered here, see [Contact](#contact).

### Time

`kOfxPropTime` in the render action carries a frame number derived from the timecode of the frame being processed: it advances while a live signal runs or material plays, and stays constant while a frame is held. A plugin with time-varying behavior, such as animated grain, therefore animates during playback and holds still on a paused frame.

Renders without a frame context, such as stills, thumbnails and LUT creation, use time `0`, as do all non-render actions.

## Color management

The OpenFX node is currently **not** color-managed: the host declares `kOfxImageEffectColourManagementNone` and passes the image on exactly as the previous node produced it, without applying Livegrade's context colorspace. The input encoding therefore depends on where the user places the node in the chain. A plugin that expects a specific encoding should expose a source transfer function or colorspace parameter, as most film emulation plugins do. The user can then match it to the node's position.

## Contact

For questions about the host, or for a plugin that does not behave as expected in Livegrade, write to [contact@pomfort.com](mailto:contact@pomfort.com).
