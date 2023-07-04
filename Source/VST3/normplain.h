
#pragma once

#include <algorithm>
#include <cstdint>

//------------------------------------------------------------------------
namespace VST3 {

//------------------------------------------------------------------------
template <typename T>
inline constexpr T normalizedToPlain (T min, T max, T normalizedValue) noexcept
{
	return normalizedValue * (max - min) + min;
}

//------------------------------------------------------------------------
template <typename ReturnType = int32_t, typename ValueType = double>
inline constexpr ReturnType normalizedToSteps (int32_t numSteps, int32_t startValue,
                                               ValueType normalizedValue) noexcept
{
	return static_cast<ReturnType> (
	    std::min (numSteps,
	              static_cast<int32_t> (normalizedValue * static_cast<ValueType> (numSteps + 1))) +
	    startValue);
}

//------------------------------------------------------------------------
template <typename ReturnType = double, typename ValueType>
inline constexpr ReturnType plainToNormalized (ReturnType min, ReturnType max,
                                               ValueType plainValue) noexcept
{
	return (plainValue - min) / (max - min);
}

//------------------------------------------------------------------------
template <typename ReturnType = double, typename ValueType = double>
inline constexpr ReturnType stepsToNormalized (int32_t numSteps, int32_t startValue,
                                               ValueType plainValue) noexcept
{
	return static_cast<ReturnType> (plainValue - startValue) / static_cast<ReturnType> (numSteps);
}

//------------------------------------------------------------------------
} // VST3
