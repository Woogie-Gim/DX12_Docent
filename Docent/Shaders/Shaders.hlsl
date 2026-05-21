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
    
    float3 gSpotLightPos; // 스포트라이트 위치 (X, Y, Z)
    float gSpotLightRange; // 빛이 도달하는 최대 거리 (기본값 추천: 15.0f)
    
    float3 gSpotLightDir; // 빛이 향하는 방향 벡터
    float gSpotLightSpotPower; // 원뿔 경계면의 부드러운 감쇄 강도 (기본값 추천: 64.0f)
    
    float3 gSpotLightColor; // 조명 색상 (R, G, B)
    float pad2;
};

Texture2D gDiffuseMap : register(t0);               // 실제 이미지 데이터
Texture2D gNormalMap : register(t1);                // 노멀 맵 텍스처
Texture2D gMetallicRoughnessMap : register(t2);     // 거칠기 맵 텍스처
Texture2D gEmissiveMap : register(t3);              // 발광 맵 텍스처
SamplerState gsamAnisotropicWrap : register(s0);    // 이미지를 어떻게 읽을지 결정하는 필터

struct VertexIn
{
    float3 PosL  : POSITION;    // 입력 위치 (X, Y, Z)
    float3 NormalL : NORMAL;    // 입력 법선
    float2 TexC : TEXCOORD;     // 입력 UV
    float3 TangentL : TANGENT;  // 입력 접선  
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;     // 출력 위치 (시스템 변수, 화면 좌표)
    float3 PosW : POSITION;         // 월드 공간에서의 픽셀 위치
    float3 NormalW : NORMAL;        // 월드 공간에서의 법선 벡터
    float2 TexC : TEXCOORD;
    float3 TangentW : TANGENT;      // 월드 공간 접선
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
    
    // 법선 벡터 및 접선 벡터 회전
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);
    
    // 원본 UV 좌표에 스케일을 곱하고 오프셋을 더함
    vout.TexC = (vin.TexC * gUVScale) + gUVOffset;
    
    return vout;
}

// 노멀 매핑 함수 - 노멀 맵 정보를 월드 공간 법선으로 변환
float3 NormalSampleToWorldSpace(float3 normalSample, float3 unitNormalW, float3 unitTangentW)
{
    // 노멀 맵의 색상(0~1)을 벡터(-1~1)로 변환
    float3 normalT = 2.0f * normalSample - 1.0f;

    // 그람-슈미트 직교화를 이용해 완벽한 직교 기저(TBN 행렬) 구축
    float3 N = unitNormalW;
    float3 T = normalize(unitTangentW - dot(unitTangentW, N) * N);
    float3 B = cross(N, T); // 부법선 벡터 계산

    // TBN 행렬 생성 (접선 공간 > 월드 공간 변환 행렬)
    float3x3 TBN = float3x3(T, B, N);

    // 노멀 맵 벡터를 월드 공간으로 변환
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

// 픽셀 셰이더 : 꼭짓점 사이의 픽셀에 색을 칠함
float4 PS(VertexOut pin) : SV_Target
{
    // 텍스처에서 기본 색상 가져오기
    float4 texColor = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
    
    // 거칠기 텍스처 샘플링 (G채널 = Roughness)
    float4 mrSample = gMetallicRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC);
    float roughness = mrSample.g;
    
    // 빛과 상관없이 물체 스스로 내는 빛 색상 추출  
    // 단일 채널(흑백) 로드 대비 임시 변수
    float4 emiSample = gEmissiveMap.Sample(gsamAnisotropicWrap, pin.TexC);
    
    // R채널의 값을 G, B에도 똑같이 복사하여 무채색(하얀색) 발광으로 변환
    float3 emissive = float3(emiSample.r, emiSample.r, emiSample.r);
    
    // 노멀 맵에서 정보를 읽어와 표면의 미세 굴곡을 반영한 법선 벡터 계산
    float3 normalMapSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    
    if (normalMapSample.b > 0.8f && normalMapSample.r > 0.3f && normalMapSample.g > 0.3f)
    {
        roughness = 1.0f; // 거칠기를 최대치(1.0)로 강제 고정하여 빛 반사를 완전히 흩어버림
        texColor.rgb *= 0.8f; // 벽면이 너무 밝게 뜨지 않도록 기본 색상을 살짝 한 톤 다운
    }
    
    // Ambient (환경광): 조명이 닿지 않는 외곽 지역의 최소 시야 보장
    float3 ambient = texColor.rgb * 0.3f;
    
    // 벡터 정규화 (길이를 1로 맞춤) 및 노멀 매핑 적용
    float3 normal = normalize(pin.NormalW);
    float3 tangent = normalize(pin.TangentW);
    
    normal = NormalSampleToWorldSpace(normalMapSample, normal, tangent);
    
    // 고정된 태양빛 대신 광원의 위치에서 현재 픽셀 위치를 향하는 빛의 방향 벡터 계산
    float3 lightVec = gSpotLightPos - pin.PosW;
    float d = length(lightVec); // 광원과 표면 사이의 실제 거리
    
    // 빛의 도달 범위를 벗어나면 완전히 어둡게 처리
    if (d > gSpotLightRange)
        return float4(ambient + emissive, texColor.a);
    
    lightVec /= d; // lightVec 정규화
    float3 viewDir = normalize(gCameraPos - pin.PosW);
    
    // 거리 감쇄 (Attenuation) 계산: 거리가 멀어질수록 빛이 부드럽게 감소
    float att = max(0.0f, 1.0f - (d / gSpotLightRange));
    
    // 원뿔 각도 (Spotlight Cone) 계산
    // 조명이 바라보는 방향(gSpotLightDir)과 픽셀로 향하는 방향(-lightVec)의 내적
    float spotFactor = max(dot(-lightVec, normalize(gSpotLightDir)), 0.0f);
    // 지정된 SpotPower 제곱승을 통해 원뿔 형태의 선명한 빛 테두리 형성
    float spotCone = pow(spotFactor, gSpotLightSpotPower);
    
    // 최종 조명 결합 계수 (거리 감쇄 * 원뿔 필터)
    float lightIntensity = att * spotCone;
    
    // Diffuse (난반사광) 연산
    float diffuseFactor = max(dot(normal, lightVec), 0.0f);
    float3 diffuse = diffuseFactor * texColor.rgb * gSpotLightColor * lightIntensity;
    
    // Specular (정반사광) 연산
    float3 reflectDir = reflect(-lightVec, normal);
    float specPower = max(1.0f, (1.0f - roughness) * 128.0f);
    float specFactor = pow(max(dot(viewDir, reflectDir), 0.0f), specPower);
    float3 specular = specFactor * gSpotLightColor * (1.0f - roughness) * 0.5f * lightIntensity;
    
    // 최종 색상 합산 (환경광 + 난반사광 + 정반사광 + 자체 발광)
    float3 finalColor = ambient + diffuse + specular + emissive;
    
    return float4(finalColor, texColor.a);
}