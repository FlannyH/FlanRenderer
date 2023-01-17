struct PixelInput
{
    float4 position : SV_Position;
    float3 colour : COLOR;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 texcoord0 : TEXCOORD0;
    float3 texcoord1 : TEXCOORD1;
};

struct PixelOutput
{
    float4 attachment0 : SV_Target0;
};

//Texture2D texture_albedo : register(t0);
//SamplerState texture_sampler : register(s0);

PixelOutput main(PixelInput pixel_input)
{
    float3 in_colour = pixel_input.colour;
    PixelOutput output;
    output.attachment0 = float4(in_colour, 1.0f) * clamp(dot(pixel_input.normal, float3(1.0f, 1.0f, 0.0f)), 0.1f, 1.0f);
    //output.attachment0 *= texture_albedo.Sample(texture_sampler, pixel_input.texcoord0);
    return output;
}