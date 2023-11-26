#pragma once
#include <iostream>
#include "stdafx.h"
#include <DirectXMath.h>
#define _USE_MATH_DEFINES
#include <math.h>

using namespace DirectX;

class Node
{
public:
	Node();
	std::string name;
	XMFLOAT3 position;
	XMFLOAT3 rotation;
	XMFLOAT3 scale;
	XMMATRIX modelMatrix();
	
};

