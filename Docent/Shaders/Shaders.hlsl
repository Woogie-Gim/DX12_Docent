// 상수 버퍼 : CPU에서 보내준 카메라의 MVP 행렬을 받음
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;     // World * View * Projection 행렬
};
Texture2D gDiffuseMap : register(t0);               // 실제 이미지 데이터
SamplerState gsamAnisotropicWrap : register(s0);    // 이미지를 어떻게 읽을지 결정하는 필터

struct VertexIn
{
    float3 PosL  : POSITION;    // 입력 위치 (X, Y, Z)
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;  // 출력 위치 (시스템 변수, 화면 좌표)
    float2 TexC : TEXCOORD;
};

// 정점 셰이더 : 꼭짓점 좌표를 MVP 행렬과 곱해 화면 좌표로 전환
VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // 3D 위치를 4차원으로 변환하고 MVP 행렬 곱함
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    
    vout.TexC = vin.TexC; // 좌표 그대로 전달    
    
    return vout;
}

// 픽셀 셰이더 : 꼭짓점 사이의 픽셀에 색을 칠함
float4 PS(VertexOut pin) : SV_Target
{
    // 텍스처에서 해당 좌표의 색상을 추출
    return gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
}