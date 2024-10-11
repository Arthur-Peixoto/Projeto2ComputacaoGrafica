/**********************************************************************************
// Vertex (Arquivo de Sombreamento)
//
// Cria��o:     22 Jul 2020
// Atualiza��o: 08 Set 2023
// Compilador:  Direct3D Shader Compiler (FXC)
//
// Descri��o:   Um vertex shader que faz a transforma��o de v�rtices
//              a partir de uma matriz combinada WorldViewProj fornecida
//
**********************************************************************************/

cbuffer Object
{
    float4x4 WorldViewProj;
    float4 color;
};

struct VertexIn
{
    float3 PosL  : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout;

    // transforma para espa�o homog�neo de recorte
    vout.PosH = mul(float4(vin.PosL, 1.0f), WorldViewProj);

    // apenas passa a cor do v�rtice para o pixel shader
    vout.Color = vin.Color;

    return vout;
}