#pragma once

#include "paramdesc.h"
#include "public.sdk/source/vst/vstparameters.h"
#include <functional>

//------------------------------------------------------------------------
namespace VST3 {

//------------------------------------------------------------------------
class Parameter : public Steinberg::Vst::Parameter
{
public:
	using Flags = Steinberg::Vst::ParameterInfo::ParameterFlags;
	using ParamID = Steinberg::Vst::ParamID;
	using ParamValue = Steinberg::Vst::ParamValue;
	using String128 = Steinberg::Vst::String128;
	using TChar = Steinberg::Vst::TChar;

	Parameter (ParamID pid, const ParamDesc& desc, int32_t flags = Flags::kCanAutomate);

	ParamValue getPlain () const { return toPlain (getNormalized ()); }

	//-- overrides
	bool setNormalized (ParamValue v) override;
	void toString (ParamValue valueNormalized, String128 string) const override;
	bool fromString (const TChar* string, ParamValue& valueNormalized) const override;
	ParamValue toPlain (ParamValue valueNormalized) const override;
	ParamValue toNormalized (ParamValue plainValue) const override;

	//--- listener
	using ValueChangedFunc = std::function<void (Parameter&,ParamValue)>;
	using Token = uint64_t;
	static constexpr const Token InvalidToken = 0u;

	Token addListener (const ValueChangedFunc& func);
	void removeListener (Token token);

	//-- custom conversion
	using ToPlainFunc = std::function<ParamValue (const Parameter& param, ParamValue norm)>;
	using ToNormalizedFunc = std::function<ParamValue (const Parameter& param, ParamValue plain)>;
	using ToStringFunc = std::function<void (const Parameter& param, ParamValue norm, String128 outString)>;
	using FromStringFunc = std::function<bool (const Parameter& param, const TChar* string, ParamValue& outNorm)>;

	void setCustomToPlainFunc (const ToPlainFunc& func);
	void setCustomToNormalizedFunc (const ToNormalizedFunc& func);
	void setCustomToStringFunc (const ToStringFunc& func);
	void setCustomFromStringFunc (const FromStringFunc& func);

	const ParamDesc& description () const { return desc; }
private:
	const ParamDesc& desc;
	std::vector<std::pair<ValueChangedFunc, Token>> listeners;
	Token tokenCounter {0};

	ToPlainFunc toPlainFunc {};
	ToNormalizedFunc toNormalizedFunc {};
	ToStringFunc toStringFunc {};
	FromStringFunc fromStringFunc {};
};

//------------------------------------------------------------------------
} // VST3
