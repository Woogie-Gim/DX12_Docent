// 레지스터 b0 : 개별 물체 정보
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float2 gUVOffset;
    float2 gUVScale;
};

// 레지스터 b1 : 화면 공용 정보 (PassConstants)
cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gCameraPos;
    float pad1;
    float3 gLightDir;
    float pad2;
    float3 gLightColor;
    float pad3;
};

Texture2D gDiffuseMap : register(t0);               // 실제 이미지 데이터
SamplerState gsamAnisotropicWrap : register(s0);    // 이미지를 어떻게 읽을지 결정하는 필터

struct VertexIn
{
    float3 PosL  : POSITION;    // 입력 위치 (X, Y, Z)
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;  // 출력 위치 (시스템 변수, 화면 좌표)
    float3 PosW : POSITION; // 월드 공간에서의 픽셀 위치
    float3 NormalW : NORMAL; // 월드 공간에서의 법선 벡터
    float2 TexC : TEXCOORD;
};

// 정점 셰이더 : 꼭짓점 좌표를 MVP 행렬과 곱해 화면 좌표로 전환
VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // 월드 변환 (물체를 3D 공간에 배치)
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    // 뷰 & 투영 변환 (카메라 렌즈를 통해 화면 좌표로 변환)
    vout.PosH = mul(posW, gViewProj);
    
    // 법선 벡터 회전
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    
    // 원본 UV 좌표에 스케일을 곱하고 오프셋을 더함
    vout.TexC = (vin.TexC * gUVScale) + gUVOffset;
    
    return vout;
}

// 픽셀 셰이더 : 꼭짓점 사이의 픽셀에 색을 칠함
float4 PS(VertexOut pin) : SV_Target
{
    // 텍스처에서 기본 색상 가져오기
    float4 texColor = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
    
    // 벡터 정규화 (길이를 1로 맞춤)
    float3 normal = normalize(pin.NormalW);
    float3 lightDir = normalize(-gLightDir); // 빛이 '날아가는' 방향의 반대(광원을 향하는 방향)
    float3 viewDir = normalize(gCameraPos - pin.PosW);
    
    // Ambient (환경광): 빛이 직접 닿지 않아도 아주 캄캄하지 않게 기본적으로 깔아주는 빛
    float3 ambient = texColor.rgb * 0.2f;
    
    // Diffuse (난반사광): 빛을 정면으로 받을수록 밝아짐 (내적 활용)
    float diffuseFactor = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diffuseFactor * texColor.rgb * gLightColor;
    
    // Specular (정반사광/하이라이트): 매끈한 표면에서 빛이 반사되어 눈으로 들어오는 빛
    float3 reflectDir = reflect(-lightDir, normal);
    float specFactor = pow(max(dot(viewDir, reflectDir), 0.0f), 32.0f); // 32는 광택의 정도
    float3 specular = specFactor * gLightColor * 0.5f; // 하이라이트 강도 조절
    
    // 최종 색상 = 환경광 + 난반사광 + 정반사광
    float3 finalColor = ambient + diffuse + specular;
    
    return float4(finalColor, texColor.a);
}