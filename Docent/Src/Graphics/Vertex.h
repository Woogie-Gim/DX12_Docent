#pragma once
#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 Pos;		// 위치 (X, Y, Z)
	DirectX::XMFLOAT2 TexC;		// 텍스처 좌표 (U, V)
};