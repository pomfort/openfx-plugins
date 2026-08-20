// Copyright (c) 2026 Pomfort GmbH
// SPDX-License-Identifier: BSD-3-Clause

#include "MetalGainExample.h"

#include <stdio.h>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"
#include "ofxsLog.h"

#define kPluginName "Metal Gain Example"
#define kPluginGrouping "Pomfort"
#define kPluginDescription "RGBA gain with a spatial vignette, rendered through Metal textures. Renders the gain alone when the host asks for a spatial-free render."
#define kPluginIdentifier "com.pomfort.MetalGainExample"
#define kPluginSupportWebpage "https://github.com/pomfort/openfx-plugins"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

#define kSupportsTiles false
#define kSupportsMultiResolution false
#define kSupportsMultipleClipPARs false

////////////////////////////////////////////////////////////////////////////////

class GainProcessor : public OFX::ImageProcessor
{
public:
    explicit GainProcessor(OFX::ImageEffect& p_Instance);

    virtual void processImagesMetalTextures(bool hasNoSpatialAwareness);

    void setSrcImg(OFX::Image* p_SrcImg);
    void setScales(float p_ScaleR, float p_ScaleG, float p_ScaleB, float p_ScaleA);

private:
    OFX::Image* _srcImg;
    float _scales[4];
};

GainProcessor::GainProcessor(OFX::ImageEffect& p_Instance)
    : OFX::ImageProcessor(p_Instance)
{
}

#ifdef __APPLE__
extern void OpenWebpage(const char* p_URL);

extern void RunMetalKernel(void* p_CmdQ,
                           const int p_Width,
                           const int p_Height,
                           float* p_Gain,
                           const float* p_Input,
                           float* p_Output,
                           const bool hasNoSpatialAwareness);
#endif

void GainProcessor::processImagesMetalTextures(bool hasNoSpatialAwareness)
{
#ifdef __APPLE__
    const OfxRectI& bounds = _srcImg->getBounds();
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;

    float* input = static_cast<float*>(_srcImg->getPixelData());
    float* output = static_cast<float*>(_dstImg->getPixelData());

    RunMetalKernel(_pMetalCmdQ,
                   width,
                   height,
                   _scales,
                   input,
                   output,
                   hasNoSpatialAwareness);
#endif
}

void GainProcessor::setSrcImg(OFX::Image* p_SrcImg)
{
    _srcImg = p_SrcImg;
}

void GainProcessor::setScales(float p_ScaleR, float p_ScaleG, float p_ScaleB, float p_ScaleA)
{
    _scales[0] = p_ScaleR;
    _scales[1] = p_ScaleG;
    _scales[2] = p_ScaleB;
    _scales[3] = p_ScaleA;
}

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin instance: parameters, identity check, render */
class MetalGainExampleEffect : public OFX::ImageEffect
{
public:
    explicit MetalGainExampleEffect(OfxImageEffectHandle p_Handle);

    /* Override the render */
    virtual void render(const OFX::RenderArguments& p_Args);

    /* Override changedParam */
    virtual void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName);

    /* Override changed clip */
    virtual void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName);

    /* Set the enabledness of the component scale params depending on the type of input image and the state of the scaleComponents param */
    void setEnabledness();

    /* Set up and run a processor */
    void setupAndProcess(GainProcessor &p_GainProcessor, const OFX::RenderArguments& p_Args);

private:
    // Does not own the following pointers
    OFX::Clip* m_DstClip;
    OFX::Clip* m_SrcClip;

    OFX::DoubleParam* m_Scale;
    OFX::DoubleParam* m_ScaleR;
    OFX::DoubleParam* m_ScaleG;
    OFX::DoubleParam* m_ScaleB;
    OFX::DoubleParam* m_ScaleA;
    OFX::BooleanParam* m_ComponentScalesEnabled;
};

MetalGainExampleEffect::MetalGainExampleEffect(OfxImageEffectHandle p_Handle)
    : ImageEffect(p_Handle)
{
    m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
    m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

    m_Scale = fetchDoubleParam("scale");
    m_ScaleR = fetchDoubleParam("scaleR");
    m_ScaleG = fetchDoubleParam("scaleG");
    m_ScaleB = fetchDoubleParam("scaleB");
    m_ScaleA = fetchDoubleParam("scaleA");
    m_ComponentScalesEnabled = fetchBooleanParam("scaleComponents");

    // Set the enabledness of our RGBA sliders
    setEnabledness();
}

void MetalGainExampleEffect::render(const OFX::RenderArguments& p_Args)
{
    if ((m_DstClip->getPixelDepth() == OFX::eBitDepthFloat) && (m_DstClip->getPixelComponents() == OFX::ePixelComponentRGBA))
    {
        GainProcessor imageScaler(*this);
        setupAndProcess(imageScaler, p_Args);
    }
    else
    {
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

void MetalGainExampleEffect::changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName)
{
    if (p_ParamName == "scaleComponents")
    {
        setEnabledness();
    }
    else if (p_ParamName == "pluginSupportWebpage")
    {
        OpenWebpage(kPluginSupportWebpage);
    }
}

void MetalGainExampleEffect::changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName)
{
    if (p_ClipName == kOfxImageEffectSimpleSourceClipName)
    {
        setEnabledness();
    }
}

void MetalGainExampleEffect::setEnabledness()
{
    // the component enabledness depends on the clip being RGBA and the param being true
    const bool enable = (m_ComponentScalesEnabled->getValue() && (m_SrcClip->getPixelComponents() == OFX::ePixelComponentRGBA));

    m_ScaleR->setEnabled(enable);
    m_ScaleG->setEnabled(enable);
    m_ScaleB->setEnabled(enable);
    m_ScaleA->setEnabled(enable);
}

void MetalGainExampleEffect::setupAndProcess(GainProcessor& p_GainProcessor, const OFX::RenderArguments& p_Args)
{
    // Get the dst image
    std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = dst->getPixelComponents();

    // Get the src image
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));
    OFX::BitDepthEnum srcBitDepth = src->getPixelDepth();
    OFX::PixelComponentEnum srcComponents = src->getPixelComponents();

    // Check to see if the bit depth and number of components are the same
    if ((srcBitDepth != dstBitDepth) || (srcComponents != dstComponents))
    {
        OFX::throwSuiteStatusException(kOfxStatErrValue);
    }

    double rScale = 1.0, gScale = 1.0, bScale = 1.0, aScale = 1.0;

    if (m_ComponentScalesEnabled->getValueAtTime(p_Args.time))
    {
        rScale = m_ScaleR->getValueAtTime(p_Args.time);
        gScale = m_ScaleG->getValueAtTime(p_Args.time);
        bScale = m_ScaleB->getValueAtTime(p_Args.time);
        aScale = m_ScaleA->getValueAtTime(p_Args.time);
    }

    const double scale = m_Scale->getValueAtTime(p_Args.time);
    rScale *= scale;
    gScale *= scale;
    bScale *= scale;

    // Set the images
    p_GainProcessor.setDstImg(dst.get());
    p_GainProcessor.setSrcImg(src.get());

    // Hand the GPU render arguments (command queue, enabled-API flags,
    // no-spatial-awareness) to the processor
    p_GainProcessor.setGPURenderArgs(p_Args);

    // Set the render window
    p_GainProcessor.setRenderWindow(p_Args.renderWindow);

    // Set the scales
    p_GainProcessor.setScales(rScale, gScale, bScale, aScale);

    // Call the base class process member, this will call the derived templated process code
    p_GainProcessor.process();
}

////////////////////////////////////////////////////////////////////////////////

using namespace OFX;

MetalGainExampleFactory::MetalGainExampleFactory()
    : OFX::PluginFactoryHelper<MetalGainExampleFactory>(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor)
{
}

void MetalGainExampleFactory::describe(OFX::ImageEffectDescriptor& p_Desc)
{
    // Basic labels
    p_Desc.setLabels(kPluginName, kPluginName, kPluginName);
    p_Desc.setPluginGrouping(kPluginGrouping);
    p_Desc.setPluginDescription(kPluginDescription);

    // Add the supported contexts, only filter at the moment
    p_Desc.addSupportedContext(eContextFilter);

    // Add supported pixel depths
    p_Desc.addSupportedBitDepth(eBitDepthFloat);

    // Set a few flags
    p_Desc.setSingleInstance(false);
    p_Desc.setHostFrameThreading(false);
    p_Desc.setSupportsMultiResolution(kSupportsMultiResolution);
    p_Desc.setSupportsTiles(kSupportsTiles);
    p_Desc.setTemporalClipAccess(false);
    p_Desc.setRenderTwiceAlways(false);
    p_Desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);

    // Setup OpenCL render capability flags
    p_Desc.setSupportsOpenCLBuffersRender(false);
    p_Desc.setSupportsOpenCLImagesRender(false);

    // Setup CUDA render capability flags on non-Apple system
#ifndef __APPLE__
    p_Desc.setSupportsCudaRender(false);
    p_Desc.setSupportsCudaStream(false);
#endif

    // Setup Metal render capability flags only on Apple system
#ifdef __APPLE__
    p_Desc.setSupportsMetalRender(false);
    p_Desc.setSupportsMetalTexture(true);
    p_Desc.setSupportsNoSpatialAwareness(true);
#endif
}

static DoubleParamDescriptor* defineScaleParam(OFX::ImageEffectDescriptor& p_Desc, const std::string& p_Name, const std::string& p_Label,
                                               const std::string& p_Hint, GroupParamDescriptor* p_Parent)
{
    DoubleParamDescriptor* param = p_Desc.defineDoubleParam(p_Name);
    param->setLabels(p_Label, p_Label, p_Label);
    param->setScriptName(p_Name);
    param->setHint(p_Hint);
    param->setDefault(1);
    param->setRange(0, 3);
    param->setIncrement(0.1);
    param->setDisplayRange(0, 3);
    param->setDoubleType(eDoubleTypeScale);

    if (p_Parent)
    {
        param->setParent(*p_Parent);
    }

    return param;
}

void MetalGainExampleFactory::describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum /*p_Context*/)
{
    // Source clip only in the filter context
    // Create the mandated source clip
    ClipDescriptor* srcClip = p_Desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);

    // Create the mandated output clip
    ClipDescriptor* dstClip = p_Desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);

    // Make some pages and to things in
    PageParamDescriptor* page = p_Desc.definePageParam("Controls");

    // Group param to group the scales
    GroupParamDescriptor* componentScalesGroup = p_Desc.defineGroupParam("componentScales");
    componentScalesGroup->setHint("Scales on the individual component");
    componentScalesGroup->setLabels("Components", "Components", "Components");

    // Make overall scale params
    DoubleParamDescriptor* param = defineScaleParam(p_Desc, "scale", "scale", "Scales all component in the image", 0);
    page->addChild(*param);

    // Add a boolean to enable the component scale
    BooleanParamDescriptor* boolParam = p_Desc.defineBooleanParam("scaleComponents");
    boolParam->setDefault(true);
    boolParam->setHint("Enables scales on individual components");
    boolParam->setLabels("Scale Components", "Scale Components", "Scale Components");
    boolParam->setParent(*componentScalesGroup);
    page->addChild(*boolParam);

    // Make the four component scale params
    param = defineScaleParam(p_Desc, "scaleR", "red", "Scales the red component of the image", componentScalesGroup);
    page->addChild(*param);

    param = defineScaleParam(p_Desc, "scaleG", "green", "Scales the green component of the image", componentScalesGroup);
    page->addChild(*param);

    param = defineScaleParam(p_Desc, "scaleB", "blue", "Scales the blue component of the image", componentScalesGroup);
    page->addChild(*param);

    param = defineScaleParam(p_Desc, "scaleA", "alpha", "Scales the alpha component of the image", componentScalesGroup);
    page->addChild(*param);

    // The plugin declares its support webpage as kPluginSupportWebpage and exposes it as a
    // push button. The host presents the button; pressing it opens the declared address.
    PushButtonParamDescriptor* supportParam = p_Desc.definePushButtonParam("pluginSupportWebpage");
    supportParam->setLabels("Support", "Support", "Support");
    supportParam->setHint("Opens the support webpage of this plugin.");
    page->addChild(*supportParam);
}

ImageEffect* MetalGainExampleFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum /*p_Context*/)
{
    return new MetalGainExampleEffect(p_Handle);
}

void OFX::Plugin::getPluginIDs(PluginFactoryArray& p_FactoryArray)
{
    static MetalGainExampleFactory exampleFactory;
    p_FactoryArray.push_back(&exampleFactory);
}
