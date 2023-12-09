#pragma once
#include "stdafx.h"
using namespace DirectX;

class Camera
{
public:
	Camera();
	XMFLOAT3 eyePosition;
	float distance;
	float m_camYaw, m_camPitch;
	float m_aspectRatio;

	XMMATRIX viewMatrix();
	XMMATRIX projectionMatrix();
};

