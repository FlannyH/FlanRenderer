cbuffer camera_transform : register(b0)
{
    matrix view;
    matrix projection;
};

cbuffer camera_transform : register(b1)
{
    matrix model;
};


struct VertexInput
{
    float3 position : POSITION;
    float3 colour : COLOR;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 texcoord0 : TEXCOORD0;
    float3 texcoord1 : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 colour : COLOR;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 texcoord0 : TEXCOORD0;
    float3 texcoord1 : TEXCOORD1;
};

VertexOutput main(VertexInput vertex_input)
{
    VertexOutput output;
    output.position = float4(vertex_input.position, 0.5f);
    output.colour = vertex_input.colour;
    output.normal = vertex_input.normal;
    output.tangent = vertex_input.tangent;
    output.texcoord0 = vertex_input.texcoord0;
    output.texcoord1 = vertex_input.texcoord1;
    return output;
}