#pragma once

#include "stdafx.h"
using namespace DirectX;

struct Vertex {
	XMFLOAT3 position;
	XMFLOAT3 color;
	XMFLOAT2 texCoord;
};

struct FaceIndices {
	unsigned int a, b, c;
};

struct MVPMatrix
{
	XMMATRIX model;
	XMMATRIX view;
	XMMATRIX projection;
	XMMATRIX normalMatrix;
};

struct FragmentUniform {
	UINT lightCount;
	XMFLOAT3 cameraPosition;
};


struct SubMesh {
	UINT indexCount;
	UINT startIndexLocation;
	INT baseVertexLocation;
};