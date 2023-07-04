#pragma once

#include "normplain.h"
#include <variant>
#include <array>

//------------------------------------------------------------------------
namespace VST3 {

//------------------------------------------------------------------------
struct Range
{
	double min {0.};
	double max {1.};
	uint32_t precision {1};
	const char16_t* unit {nullptr};
};

//------------------------------------------------------------------------
struct StepCount
{
	uint32_t value {0};
	uint32_t startValue {0};
	const char16_t* unit {nullptr};
};

//------------------------------------------------------------------------
struct ParamDesc
{
	using Norm2ProcNativeFunc = double (*) (double normValue);

	const char16_t* name {nullptr};
	const double defaultNormalized {0.};
	const std::variant<StepCount, Range> rangeOrStepCount {Range {}};
	const char16_t* const* stringList {nullptr};
	const Norm2ProcNativeFunc toNative = [] (double v) { return v; };
};

//------------------------------------------------------------------------
static const constexpr std::array<const char16_t*, 2> OnOffStrings = {u"off", u"on"};

//------------------------------------------------------------------------
struct SmoothParameter
{
	double process ()
	{
		smoothed_value = alpha * value + (1. - alpha) * smoothed_value;
		return smoothed_value;
	}
	void setValue (double v) { value = v; }
	double getValue () const { return value; }

	double operator* () { return smoothed_value; }
	operator double () { return smoothed_value; }

	SmoothParameter& operator= (double v)
	{
		setValue (v);
		return *this;
	}

	void setAlpha (double v) { alpha = v; }

private:
	double alpha {0.1};
	double value {0.};
	double smoothed_value {0.};
};

//------------------------------------------------------------------------
template <typename T, typename EnumT, size_t size = static_cast<size_t> (EnumT::Count)>
struct EnumArray : std::array<T, size>
{
	using Base = std::array<T, size>;

	T& operator[] (EnumT index) { return Base::operator[] (static_cast<size_t> (index)); }
	const T& operator[] (EnumT index) const
	{
		return Base::operator[] (static_cast<size_t> (index));
	}
	const T& operator[] (size_t index) const { return Base::operator[] (index); }
	T& operator[] (size_t index) { return Base::operator[] (index); }

	void set (size_t index, const T& value) { Base::operator[] (index) = value; }
};

//------------------------------------------------------------------------
} // VST3
