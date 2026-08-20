// Copyright (c) 2026 Pomfort GmbH
// SPDX-License-Identifier: BSD-3-Clause

#import <Metal/Metal.h>

#include <unordered_map>
#include <mutex>

const char* kernelSource =  \
"#include <metal_stdlib>\n" \
"using namespace metal;\n" \
"\n" \
"float sdf_pentagram(float2 p, float r)\n" \
"{\n" \
"    constexpr auto k1x = 0.809016994f;\n" \
"    constexpr auto k2x = 0.309016994f;\n" \
"    constexpr auto k1y = 0.587785252f;\n" \
"    constexpr auto k2y = 0.951056516f;\n" \
"    constexpr auto k1z = 0.726542528f;\n" \
"\n" \
"    const auto v1 = float2( k1x,-k1y);\n" \
"    const auto v2 = float2(-k1x,-k1y);\n" \
"    const auto v3 = float2( k2x,-k2y);\n" \
"\n" \
"    p.x = abs(p.x);\n" \
"    p -= 2.0f * max(dot(v1, p), 0.0f) * v1;\n" \
"    p -= 2.0f * max(dot(v2, p), 0.0f) * v2;\n" \
"    p.x = abs(p.x);\n" \
"    p.y -= r;\n" \
"\n" \
"    return length(p - v3 * clamp(dot(p, v3), 0.0f, k1z * r)) * sign(p.y * v3.x - p.x * v3.y);\n" \
"}\n" \
"\n" \
"kernel void example_kernel(constant const float& gain_r [[ buffer(0) ]],\n" \
"                           constant const float& gain_g [[ buffer(1) ]],\n" \
"                           constant const float& gain_b [[ buffer(2) ]],\n" \
"                           constant const float& gain_a [[ buffer(3) ]],\n" \
"                           constant const bool& has_no_spatial_awareness [[ buffer(4) ]],\n" \
"                           texture2d<float, access::read> input_texture [[ texture(0) ]],\n" \
"                           texture2d<float, access::write> output_texture [[ texture(1) ]],\n" \
"                           const uint2 index [[ thread_position_in_grid ]])\n" \
"{\n" \
"    const auto input_color = input_texture.read(index);\n" \
"\n" \
"    const auto output_color = float4(input_color.r * gain_r,\n" \
"                                     input_color.g * gain_g,\n" \
"                                     input_color.b * gain_b,\n" \
"                                     input_color.a * gain_a);\n" \
"\n" \
"    if (has_no_spatial_awareness) {\n" \
"        output_texture.write(output_color, index);\n" \
"    } else {\n" \
"        const auto width = input_texture.get_width();\n" \
"        const auto height = input_texture.get_height();\n" \
"\n" \
"        const auto scale = clamp(sdf_pentagram(float2(index) - 0.5f * float2(width, height),\n" \
"                                               0.25f * min(width, height)),\n" \
"                                 0.0f,\n" \
"                                 1.0f);\n" \
"\n" \
"        output_texture.write((2.0f - scale) * output_color, index);\n" \
"    }\n" \
"}\n";

std::mutex s_PipelineQueueMutex;
typedef std::unordered_map<id<MTLCommandQueue>, id<MTLComputePipelineState>> PipelineQueueMap;
PipelineQueueMap s_PipelineQueueMap;

void RunMetalKernel(void* p_CmdQ,
                    const int p_Width,
                    const int p_Height,
                    float* p_Gain,
                    const float* p_Input,
                    float* p_Output,
                    const bool hasNoSpatialAwareness)
{
    const char* kernelName = "example_kernel";

    id<MTLCommandQueue>            queue = static_cast<id<MTLCommandQueue> >(p_CmdQ);
    id<MTLDevice>                  device = queue.device;
    id<MTLComputePipelineState>    pipelineState;    // Metal pipeline

    std::unique_lock<std::mutex> lock(s_PipelineQueueMutex);

    const auto it = s_PipelineQueueMap.find(queue);
    if (it == s_PipelineQueueMap.end())
    {
        id<MTLLibrary>                 metalLibrary;     // Metal library
        id<MTLFunction>                kernelFunction;   // Compute kernel
        NSError* err;

        MTLCompileOptions* options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeFast;
        if (!(metalLibrary    = [device newLibraryWithSource:@(kernelSource) options:options error:&err]))
        {
            fprintf(stderr, "Failed to load metal library, %s\n", err.localizedDescription.UTF8String);
            return;
        }
        [options release];
        if (!(kernelFunction  = [metalLibrary newFunctionWithName:[NSString stringWithUTF8String:kernelName]/* constantValues : constantValues */]))
        {
            fprintf(stderr, "Failed to retrieve kernel\n");
            [metalLibrary release];
            return;
        }
        if (!(pipelineState   = [device newComputePipelineStateWithFunction:kernelFunction error:&err]))
        {
            fprintf(stderr, "Unable to compile, %s\n", err.localizedDescription.UTF8String);
            [metalLibrary release];
            [kernelFunction release];
            return;
        }

        s_PipelineQueueMap[queue] = pipelineState;

        //Release resources
        [metalLibrary release];
        [kernelFunction release];
    }
    else
    {
        pipelineState = it->second;
    }

    id<MTLTexture> srcTexture = reinterpret_cast<id<MTLTexture>>(p_Input);
    id<MTLTexture> dstTexture = reinterpret_cast<id<MTLTexture>>(p_Output);

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    commandBuffer.label = [NSString stringWithFormat:@"example_kernel"];

    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
    [computeEncoder setComputePipelineState:pipelineState];

    const NSUInteger exeWidth = pipelineState.threadExecutionWidth;
    MTLSize threadGroupCount = MTLSizeMake(exeWidth, 1, 1);
    MTLSize threadGroups     = MTLSizeMake((p_Width + exeWidth - 1)/exeWidth, p_Height, 1);

    [computeEncoder setTexture:srcTexture atIndex:0];
    [computeEncoder setTexture:dstTexture atIndex:1];
    [computeEncoder setBytes:&p_Gain[0] length:sizeof(float) atIndex:0];
    [computeEncoder setBytes:&p_Gain[1] length:sizeof(float) atIndex:1];
    [computeEncoder setBytes:&p_Gain[2] length:sizeof(float) atIndex:2];
    [computeEncoder setBytes:&p_Gain[3] length:sizeof(float) atIndex:3];

    // Prefer switching between separate spatially-aware and non-spatially-aware
    // functions for real-world applications to avoid needless branching within kernels.
    [computeEncoder setBytes:&hasNoSpatialAwareness length:sizeof(bool) atIndex:4];

    [computeEncoder dispatchThreadgroups:threadGroups threadsPerThreadgroup: threadGroupCount];

    [computeEncoder endEncoding];
    [commandBuffer commit];
}
