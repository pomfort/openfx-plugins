#pragma once
#ifndef __OFXMETALTEXTURE_H__
#define __OFXMETALTEXTURE_H__

/** @file ofxMetalTexture.h

    This file contains optional Metal Texture extensions for OpenFX
    Image Effect Plug-ins. For details see \ref ofxMetalTexture.
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ofxMetalTexture Metal Texture Extensions
 * @{
 */

/** @brief Indicates whether a host or plug-in can support Metal texture render

    - Type - string X 1
    - Property Set - plug-in descriptor (read/write), host descriptor (read only)
    - Default - "false" for a plug-in
    - Valid Values - This must be one of
      - "false"  - the host or plug-in does not support Metal texture render
      - "true"   - the host or plug-in can support Metal texture render
 */
#define kOfxImageEffectPropMetalTextureSupported "OfxImageEffectPropMetalTextureSupported"

/** @brief Indicates that a plug-in SHOULD use Metal texture render in
the current action

   If a plug-in and host have both set
   ::kOfxImageEffectPropMetalRenderSupported="true" and
   ::kOfxImageEffectPropMetalTextureSupported="true" then the host MAY
   set this property to indicate that it is passing images as Metal
   textures.

   - Type - int X 1
   - Property Set - inArgs property set of the following actions...
      - ::kOfxImageEffectActionRender
      - ::kOfxImageEffectActionBeginSequenceRender
      - ::kOfxImageEffectActionEndSequenceRender
   - Valid Values
      - 0 indicates that ::kOfxImagePropData should be interpreted using
          ::kOfxImageEffectPropMetalEnabled: as a Metal id<MTLBuffer> when
          that property is 1, otherwise as a CPU memory pointer.
      - 1 indicates that the ::kOfxImagePropData of each image of each clip
          is a Metal id<MTLTexture>, and ::kOfxImageEffectPropMetalEnabled
          SHOULD be 0.
 */
#define kOfxImageEffectPropMetalTextureEnabled "OfxImageEffectPropMetalTextureEnabled"

/** @}*/ // end ofxMetalTexture doc group

#ifdef __cplusplus
}
#endif

#endif
