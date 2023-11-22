#pragma once

#include <iostream>
#include <chrono>

#include <chrono>

class Time {
public:
	Time() {
		// Initialize lastTime to the current time
		lastTime = std::chrono::high_resolution_clock::now();
	}

	// Updates the timer by calculating the deltaTime
	void Update() {
		// Update the current time
		currentTime = std::chrono::high_resolution_clock::now();

		// Calculate the time difference (deltaTime) in seconds
		std::chrono::duration<double> elapsed = currentTime - lastTime;
		deltaTime = elapsed.count();

		// Update lastTime for the next call
		lastTime = currentTime;
	}

	// Get the deltaTime in seconds
	double GetDeltaTime() const {
		return deltaTime;
	}

	// Reset the timer
	void Reset() {
		lastTime = std::chrono::high_resolution_clock::now();
		deltaTime = 0.0;
	}

private:
	std::chrono::high_resolution_clock::time_point lastTime;
	std::chrono::high_resolution_clock::time_point currentTime;
	double deltaTime = 0.0;
};


