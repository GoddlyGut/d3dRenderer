#pragma once

#include <iostream>
#include <chrono>

class Time
{
public:

	float m_previousTime = 0.0f;
	std::chrono::high_resolution_clock::time_point lastTime;
	std::chrono::high_resolution_clock::time_point currentTime;
	double deltaTime;
};


