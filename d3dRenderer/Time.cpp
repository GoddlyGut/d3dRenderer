#include "Time.h"

Time::Time() {
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&previousTime);
}

void Time::LoopCall()
{
	QueryPerformanceCounter(&currentTime);
	deltaTime = double(currentTime.QuadPart - previousTime.QuadPart) / frequency.QuadPart;
	previousTime = currentTime;
}

