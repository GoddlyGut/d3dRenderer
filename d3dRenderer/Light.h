#pragma once

#include "DXSample.h"

using namespace DirectX;

enum LightType {
	unused = 0,
	Sunlight = 1,
	Spotlight = 2,
	Pointlight = 3,
	Ambientlight = 4
};

struct Light {
	DirectX::XMFLOAT3 position;  // 12 bytes
	//float pad1;                  // 4 bytes of padding
	float intensity;             // 4 bytes
	DirectX::XMFLOAT3 color;     // 12 bytes
	//float pad2;                  // 4 bytes of padding
	float coneAngle;
	DirectX::XMFLOAT3 specularColor; // 12 bytes
	//float pad3;                  // 4 bytes of padding
	float coneAttenuation;
	DirectX::XMFLOAT3 attenuation;  // 12 bytes
	//float pad4;
	int type;
	DirectX::XMFLOAT3 coneDirection;// 12 bytes
	//float pad5;

};


