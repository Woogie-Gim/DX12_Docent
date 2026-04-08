#pragma once
#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 Pos;		// 위치 (X, Y, Z)
	DirectX::XMFLOAT3 Normal;	// 법선 벡터 (면이 바라보는 방향)
	DirectX::XMFLOAT2 TexC;		// 텍스처 좌표 (U, V)
};