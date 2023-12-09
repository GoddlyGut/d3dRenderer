#include "Camera.h"

Camera::Camera() {
	eyePosition = { 0.0f, 0.0f, -10.0f };
}

XMMATRIX Camera::viewMatrix() {
	const float maxPitch = DirectX::XM_PIDIV2 - 0.01f; // Slightly less than 90 degrees
	m_camPitch = max(-maxPitch, min(m_camPitch, maxPitch));

	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		XMVectorSet(eyePosition.x, eyePosition.y, eyePosition.z, 0.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	);

	eyePosition.x = distance * sinf(m_camYaw) * cosf(m_camPitch);
	eyePosition.y = distance * sinf(m_camPitch);
	eyePosition.z = distance * cosf(m_camYaw) * cosf(m_camPitch);


	viewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(&eyePosition),  // eyePosition
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),   // focalPoint
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)    // upDirection
	);

	return viewMatrix;
}

XMMATRIX Camera::projectionMatrix() {
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
		XM_PIDIV4,           // Field of View in radians
		m_aspectRatio,         // Aspect ratio (width/height)
		0.1f,          // Near clipping plane
		1000.0f            // Far clipping plane
	);

	return projectionMatrix;
}