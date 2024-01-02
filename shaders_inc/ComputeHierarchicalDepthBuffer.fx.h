"#include \"SSR_Common.fxh\"\n"
"\n"
"#ifdef SSR_OPTION_INVERTED_DEPTH\n"
"    #define MipConvFunc max\n"
"#else\n"
"    #define MipConvFunc min\n"
"#endif // SSR_OPTION_INVERTED_DEPTH\n"
"\n"
"Texture2D<float> g_TextureLastMip;\n"
"\n"
"float SampleDepth(uint2 Location, uint2 Offset, uint2 Dimension)\n"
"{\n"
"    uint2 Position = Location + Offset;\n"
"    if (Position.x >= Dimension.x || Position.y >= Dimension.y)\n"
"        return g_TextureLastMip.Load(int3(Location, 0));\n"
"    return g_TextureLastMip.Load(int3(Position, 0));\n"
"}\n"
"\n"
"float ComputeHierarchicalDepthBufferPS(in float4 Position : SV_Position) : SV_Depth\n"
"{\n"
"    uint2 LastMipDimension;\n"
"    g_TextureLastMip.GetDimensions(LastMipDimension.x, LastMipDimension.y);\n"
"\n"
"    const uint2 RemappedPosition = uint2(2 * uint2(Position.xy));\n"
"\n"
"    float4 SampledPixels;\n"
"    SampledPixels.x = SampleDepth(RemappedPosition, uint2(0, 0), LastMipDimension);\n"
"    SampledPixels.y = SampleDepth(RemappedPosition, uint2(0, 1), LastMipDimension);\n"
"    SampledPixels.z = SampleDepth(RemappedPosition, uint2(1, 0), LastMipDimension);\n"
"    SampledPixels.w = SampleDepth(RemappedPosition, uint2(1, 1), LastMipDimension);\n"
"\n"
"    float MinDepth = MipConvFunc(MipConvFunc(SampledPixels.x, SampledPixels.y), MipConvFunc(SampledPixels.z, SampledPixels.w));\n"
"\n"
"    const bool IsWidthOdd = (LastMipDimension.x & 1) != 0;\n"
"    const bool IsHeightOdd = (LastMipDimension.y & 1) != 0;\n"
"\n"
"    if (IsWidthOdd)\n"
"    {\n"
"        SampledPixels.x = SampleDepth(RemappedPosition, uint2(2, 0), LastMipDimension);\n"
"        SampledPixels.y = SampleDepth(RemappedPosition, uint2(2, 1), LastMipDimension);\n"
"        MinDepth = MipConvFunc(MinDepth, MipConvFunc(SampledPixels.x, SampledPixels.y));\n"
"    }\n"
"\n"
"    if (IsHeightOdd)\n"
"    {\n"
"        SampledPixels.x = SampleDepth(RemappedPosition, uint2(0, 2), LastMipDimension);\n"
"        SampledPixels.y = SampleDepth(RemappedPosition, uint2(1, 2), LastMipDimension);\n"
"        MinDepth = MipConvFunc(MinDepth, MipConvFunc(SampledPixels.x, SampledPixels.y));\n"
"    }\n"
"\n"
"    if (IsWidthOdd && IsHeightOdd)\n"
"    {\n"
"        MinDepth = MipConvFunc(MinDepth, SampleDepth(RemappedPosition, uint2(2, 2), LastMipDimension));\n"
"    }\n"
"\n"
"    return MinDepth;\n"
"}\n"