"struct PSInput\n"
"{\n"
"    float4 Pos   : SV_POSITION;\n"
"    float4 Color : COLOR;\n"
"};\n"
"\n"
"void main(in PSInput PSIn,\n"
"          out float4 Color     : SV_Target0,\n"
"          out float4 MeshId    : SV_Target1,\n"
"          out float4 MotionVec : SV_Target2,\n"
"          out float4 Normal    : SV_Target3,\n"
"          out float4 BaseColor : SV_Target4,\n"
"          out float4 Material  : SV_Target5,\n"
"          out float4 IBL       : SV_Target6)\n"
"{\n"
"    Color  = PSIn.Color;\n"
"\n"
"    MeshId    = float4(0.0, 0.0, 0.0, 0.0);\n"
"    MotionVec = float4(0.0, 0.0, 0.0, 0.0);\n"
"    Normal    = float4(0.0, 0.0, 0.0, 0.0);\n"
"    BaseColor = float4(0.0, 0.0, 0.0, 0.0);\n"
"    Material  = float4(0.0, 0.0, 0.0, 0.0);\n"
"    IBL       = float4(0.0, 0.0, 0.0, 0.0);\n"
"}\n"
