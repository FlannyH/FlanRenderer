cbuffer camera_transform : register(b0)
{
    row_major matrix view;
    row_major matrix projection;
};

cbuffer model_transform : register(b1)
{
    row_major matrix model;
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
    float4 position = float4(vertex_input.position, 1.0f);
    position = mul(position, model);
    position = mul(position, view);
    position = mul(position, projection);
    output.position = position;
    output.colour = vertex_input.colour;
    output.normal = vertex_input.normal;
    output.tangent = vertex_input.tangent;
    output.texcoord0 = vertex_input.texcoord0;
    output.texcoord1 = vertex_input.texcoord1;
    return output;
}