#pragma once

#include <windows.h>

class Time
{
public:
	LARGE_INTEGER frequency;        // Counts per second
	LARGE_INTEGER currentTime;      // Current time
	LARGE_INTEGER previousTime;     // Previous time
	double deltaTime;
	Time();
	void LoopCall();
};

