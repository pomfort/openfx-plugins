#pragma once
#ifndef __OFXMETALTEXTURE_H__
#define __OFXMETALTEXTURE_H__

/** @file ofxMetalTexture.h

    This file contains the Metal texture extension for OpenFX Image Effect
    Plug-ins. For details see \ref ofxMetalTexture.

    The two properties are proposed for inclusion in the OpenFX standard
    (AcademySoftwareFoundation/openfx, standard change proposal #NNN), where
    they live in the Metal group of ofxGPURender.h. The definitions and the
    documentation below match that proposal. Each #define is guarded so this
    header can be included next to the proposal's headers; it will be removed
    once the properties ship in an OpenFX release.
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ofxMetalTexture Metal Texture Extension
 *
 * Images can be passed to a Metal plug-in in one of two ways: as Metal
 * buffers (id<MTLBuffer>), negotiated with kOfxImageEffectPropMetalRenderSupported,
 * or as Metal textures (id<MTLTexture>), negotiated with
 * kOfxImageEffectPropMetalTextureSupported. The two capabilities are
 * independent; a host or plug-in may support either, both or neither. For
 * each action the host chooses one of the paths both sides support and
 * signals its choice with kOfxImageEffectPropMetalEnabled (buffers) or
 * kOfxImageEffectPropMetalTextureEnabled (textures).
 * @{
 */

#ifndef kOfxImageEffectPropMetalTextureSupported
/** @brief Indicates whether a host or plug-in can support Metal texture render

    - Type - string X 1
    - Property Set - plug-in descriptor (read/write), host descriptor (read only)
    - Default - "false" for a plug-in
    - Valid Values - This must be one of
      - "false"  - the host or plug-in does not support Metal texture render
      - "true"   - the host or plug-in can support Metal texture render
      - "needed" - the plug-in can only render with Metal textures and has
                   no other render path (plug-in descriptor only)

    This property adds a second way of passing images to a Metal plug-in: as
    Metal textures (id<MTLTexture>) instead of Metal buffers (id<MTLBuffer>).
    It is independent of ::kOfxImageEffectPropMetalRenderSupported, which
    covers Metal buffers only. A plug-in may declare buffers, textures, both
    or neither, and so may a host:

      - buffers only: the host MAY set ::kOfxImageEffectPropMetalEnabled.
      - textures only: the host MAY set ::kOfxImageEffectPropMetalTextureEnabled.
      - both: the host chooses one of the two per action.
      - neither: images are passed as CPU memory.

    For a given action the host MUST set at most one of the two enabled
    properties, and only for a path that both host and plug-in declared as
    supported. A host that does not know this property ignores it and keeps
    passing images as before, as CPU memory or Metal buffers, so declaring
    texture support does not change how a plug-in works with existing hosts.
    A plug-in that supports only textures appears to such a host as a plug-in
    without Metal support.

    The plug-in sets this property on its descriptor in ::kOfxActionDescribe.
    The host sets it on the host descriptor.
 */
#define kOfxImageEffectPropMetalTextureSupported "OfxImageEffectPropMetalTextureSupported"
#endif

#ifndef kOfxImageEffectPropMetalTextureEnabled
/** @brief Indicates that a plug-in SHOULD use Metal texture render in
the current action

   If a plug-in and host have both set
   ::kOfxImageEffectPropMetalTextureSupported="true" (or the plug-in
   "needed") then the host MAY set this property to indicate that it is
   passing images as Metal textures. ::kOfxImageEffectPropMetalRenderSupported
   does not need to be set for this.

   - Type - int X 1
   - Property Set - inArgs property set of the following actions...
      - ::kOfxImageEffectActionRender
      - ::kOfxImageEffectActionBeginSequenceRender
      - ::kOfxImageEffectActionEndSequenceRender
   - Valid Values
      - 0 indicates that ::kOfxImagePropData of each image of each clip is
          to be interpreted according to ::kOfxImageEffectPropMetalEnabled:
          a Metal id<MTLBuffer> when that property is 1, otherwise a CPU
          memory pointer.
      - 1 indicates that ::kOfxImagePropData of each image of each clip
          is a Metal id<MTLTexture>. ::kOfxImageEffectPropMetalEnabled MUST
          be 0 in this case; a host MUST NOT set both properties to 1.

   When this property is 1:

   - ::kOfxImageEffectPropMetalCommandQueue holds the id<MTLCommandQueue>
     the plug-in SHOULD encode its work onto, with the same rules as for
     Metal buffers.
   - The host owns the textures. The plug-in MUST NOT release them and
     MUST NOT assume that the same textures are passed in the next
     render. The host MUST keep them alive until the work the plug-in
     enqueued on the command queue has completed.
   - The textures follow the OpenFX coordinate convention: their origin is
     the bottom left with y increasing upwards, consistent with
     ::kOfxImagePropBounds, ::kOfxImagePropRegionOfDefinition, the regions
     of interest and the render window. A plug-in works in OpenFX
     coordinates throughout and does not need to flip.
   - ::kOfxImageEffectPropPixelDepth and ::kOfxImageEffectPropComponents
     describe the OpenFX image; the texture MAY use a different
     MTLPixelFormat. The plug-in SHOULD read the pixel format, dimensions
     and usage from the MTLTexture itself.
   - ::kOfxImagePropRowBytes has no meaning for a texture and is 0.
   - Source textures MUST be readable from shaders; the output texture
     MUST be writable from shaders.
 */
#define kOfxImageEffectPropMetalTextureEnabled "OfxImageEffectPropMetalTextureEnabled"
#endif

/** @}*/ // end ofxMetalTexture doc group

#ifdef __cplusplus
}
#endif

#endif
