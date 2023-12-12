#include "Camera.h"

Camera::Camera() : m_camYaw(0.0f), m_camPitch(0.0f), distance(10), eyePosition({ 0.0f, 0.0f, -10.0f }) {
}

XMMATRIX Camera::viewMatrix() {



	eyePosition.x = distance * sinf(m_camYaw) * cosf(m_camPitch);
	eyePosition.y = distance * sinf(m_camPitch);
	eyePosition.z = distance * cosf(m_camYaw) * cosf(m_camPitch);


	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(&eyePosition),  // eyePosition
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),   // focalPoint
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)    // upDirection
	);



	return viewMatrix;
}

void Camera::Update(UINT width, UINT height) {
	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}



void Camera::UpdateCameraPosition(int deltaX, int deltaY) {
	m_camYaw += deltaX * 0.01f;
	m_camPitch += deltaY * 0.01f;

	const float maxPitch = DirectX::XM_PIDIV2 - 0.01f; // Slightly less than 90 degrees
	m_camPitch = max(-maxPitch, min(m_camPitch, maxPitch));
}

void Camera::UpdateZoomDelta(float zoomDelta) {
	float minDistance = 5.0f;
	float maxDistance = 200.0f;
	distance += zoomDelta;
	distance = max(minDistance, min(distance, maxDistance));
	distance = distance;
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