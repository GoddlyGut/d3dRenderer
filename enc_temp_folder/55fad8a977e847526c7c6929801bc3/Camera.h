#pragma once
#include "stdafx.h"
using namespace DirectX;

class Camera
{
public:
	Camera();


	XMMATRIX viewMatrix();
	XMMATRIX projectionMatrix();
	void Update(UINT width, UINT height);
	void UpdateCameraPosition(int deltaX, int deltaY);
	float m_aspectRatio;
	void UpdateZoomDelta(float zoomDelta);

private:
	XMFLOAT3 eyePosition;
	float distance;
	float m_camYaw, m_camPitch;
	
};

