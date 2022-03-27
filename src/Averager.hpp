#pragma once

#include <cstdint>

template <typename T, uint8_t samplesCount, typename TSumType = T>
struct Averager {
	Averager(T initial = 0) {
		for (uint8_t i = 0; i < samplesCount; i++) {
			last[i] = initial;
		}
		sum = initial * samplesCount;
	}
	
	T last[samplesCount];
	TSumType sum;
	uint8_t index = 0;

	void pushSample(T sample) {
		index = (index + 1) % samplesCount;
		sum -= last[index];
		last[index] = sample;
		sum += sample;
	}

	T getAverage() {
		return (sum / samplesCount) + ((sum % samplesCount) > (samplesCount / 2) ? 1 : 0);
	}
};
