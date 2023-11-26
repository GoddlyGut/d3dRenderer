#include "Node.h"

Node::Node() {
	name = "untitled";
	position = XMFLOAT3(0, 0, 0);
	rotation = XMFLOAT3(0, 0, 0);
	scale = XMFLOAT3(1, 1, 1);
}

XMMATRIX Node::modelMatrix() {
	XMMATRIX modelMatrix = XMMatrixIdentity();
	XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
	modelMatrix = modelMatrix * translationMatrix;		  //pitch, yaw, roll
	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rotation.x * M_PI / 180.f, rotation.y * M_PI / 180.f, rotation.z * M_PI / 180.f);
	modelMatrix = modelMatrix * rotationMatrix;
	XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	modelMatrix = modelMatrix * scaleMatrix;
	return modelMatrix;
}