"struct PSInput\n"
"{\n"
"    float4 Pos   : SV_POSITION;\n"
"    float4 Color : COLOR;\n"
"};\n"
"\n"
"void main(in PSInput PSIn,\n"
"          out float4 Color  : SV_Target0,\n"
"          out float4 MeshId : SV_Target1)\n"
"{\n"
"    Color  = PSIn.Color;\n"
"    MeshId = float4(0.0, 0.0, 0.0, 0.0);\n"
"}\n"