#include "parameter.h"
#include "pluginterfaces/base/ustring.h"

using namespace Steinberg;

//------------------------------------------------------------------------
namespace VST3 {

//------------------------------------------------------------------------
Parameter::Parameter (ParamID pid, const ParamDesc& desc, int32_t flags) : desc (desc)
{
	info.id = pid;
	info.flags = flags;
	info.defaultNormalizedValue = desc.defaultNormalized;
	tstrcpy (info.title, reinterpret_cast<const tchar*> (desc.name));
	if (auto stepCount = std::get_if<StepCount> (&desc.rangeOrStepCount))
	{
		info.stepCount = stepCount->value;
		setPrecision (0);
		if (stepCount->unit)
			UString (info.units, 128).assign (stepCount->unit);
	}
	else if (auto range = std::get_if<Range> (&desc.rangeOrStepCount))
	{
		setPrecision (range->precision);
		if (range->unit)
			UString (info.units, 128).assign (range->unit);
	}
	if (desc.stringList)
		info.flags |= Flags::kIsList;
	setNormalized (info.defaultNormalizedValue);
}

//------------------------------------------------------------------------
void Parameter::setCustomToPlainFunc (const ToPlainFunc& func)
{
	toPlainFunc = func;
}

//------------------------------------------------------------------------
void Parameter::setCustomToNormalizedFunc (const ToNormalizedFunc& func)
{
	toNormalizedFunc = func;
}

//------------------------------------------------------------------------
void Parameter::setCustomToStringFunc (const ToStringFunc& func)
{
	toStringFunc = func;
}

//------------------------------------------------------------------------
void Parameter::setCustomFromStringFunc (const FromStringFunc& func)
{
	fromStringFunc = func;
}

//------------------------------------------------------------------------
auto Parameter::addListener (const ValueChangedFunc& func) -> Token
{
	auto token = ++tokenCounter;
	listeners.emplace_back (func, token);
	return token;
}

//------------------------------------------------------------------------
void Parameter::removeListener (Token t)
{
	auto it = std::find_if (listeners.begin (), listeners.end (),
	                        [&] (const auto& p) { return p.second == t; });
	if (it != listeners.end ())
		listeners.erase (it);
}

//------------------------------------------------------------------------
bool Parameter::setNormalized (ParamValue v)
{
	auto res = Steinberg::Vst::Parameter::setNormalized (v);
	if (res)
		std::for_each (listeners.begin (), listeners.end (),
		               [this] (const auto& p) { p.first (*this, getNormalized ()); });
	return res;
}

//------------------------------------------------------------------------
void Parameter::toString (ParamValue valueNormalized, String128 string) const
{
	if (toStringFunc)
	{
		toStringFunc (*this, valueNormalized, string);
	}
	else
	{
		auto plain = toPlain (valueNormalized);
		if (auto stepCount = std::get_if<StepCount> (&desc.rangeOrStepCount); desc.stringList)
			tstrcpy (string,
			         reinterpret_cast<const tchar*> (
			             desc.stringList[static_cast<size_t> (plain - stepCount->startValue)]));
		else
		{
			UString wrapper (string, str16BufferSize (String128));
			if (!wrapper.printFloat (plain, precision))
				string[0] = 0;
		}
	}
}

//------------------------------------------------------------------------
bool Parameter::fromString (const TChar* string, ParamValue& valueNormalized) const
{
	if (fromStringFunc)
		return fromStringFunc (*this, string, valueNormalized);
	if (desc.stringList)
	{
		for (auto index = 0; index < std::get<StepCount> (desc.rangeOrStepCount).value; ++index)
		{
			if (tstrcmp (string, reinterpret_cast<const tchar*> (desc.stringList[index])) == 0)
			{
				valueNormalized = toNormalized (index);
				return true;
			}
		}
		return false;
	}
	UString wrapper (const_cast<TChar*> (string), tstrlen (string));
	if (wrapper.scanFloat (valueNormalized))
	{
		valueNormalized = toNormalized (valueNormalized);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
auto Parameter::toPlain (ParamValue valueNormalized) const -> ParamValue
{
	if (toPlainFunc)
		return toPlainFunc (*this, valueNormalized);
	if (auto stepCount = std::get_if<StepCount> (&desc.rangeOrStepCount))
	{
		return normalizedToSteps (stepCount->value, stepCount->startValue, valueNormalized);
	}
	auto range = std::get<Range> (desc.rangeOrStepCount);
	return normalizedToPlain (range.min, range.max, valueNormalized);
}

//------------------------------------------------------------------------
auto Parameter::toNormalized (ParamValue plainValue) const -> ParamValue
{
	if (toNormalizedFunc)
		return toNormalizedFunc (*this, plainValue);
	if (auto stepCount = std::get_if<StepCount> (&desc.rangeOrStepCount))
	{
		return stepsToNormalized (stepCount->value, stepCount->startValue, plainValue);
	}
	auto range = std::get<Range> (desc.rangeOrStepCount);
	return plainToNormalized (range.min, range.max, plainValue);
}

//------------------------------------------------------------------------
} // VST3
