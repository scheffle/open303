
#pragma once

#include "vst3utils/enum_array.h"
#include "vst3utils/norm_plain_conversion.h"
#include "vst3utils/parameter_description.h"
#include "vst3utils/smooth_value.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <cmath>
#include <optional>

//------------------------------------------------------------------------
namespace o303 {

using Steinberg::Vst::ParamID;

// #define O303_EXTENDED_PARAMETERS

//------------------------------------------------------------------------
enum class ParameterID
{
	Waveform,
	Tuning,
	Cutoff,
	Resonance,
	Envmod,
	Decay,
	Accent,
	Volume,
	Filter_Type,
	AudioPeak,
	PitchBend,
	DecayMode,

#ifdef O303_EXTENDED_PARAMETERS
	Amp_Sustain,
	Tanh_Shaper_Drive,
	Tanh_Shaper_Offset,
	Pre_Filter_Hpf,
	Feedback_Hpf,
	Post_Filter_Hpf,
	Square_Phase_Shift,
#endif

	enum_end,
};
using Parameters = vst3utils::enum_array<vst3utils::smooth_value<double>, ParameterID>;

//------------------------------------------------------------------------
inline constexpr ParamID asIndex (ParameterID p) { return static_cast<ParamID> (p); }

//------------------------------------------------------------------------
static const constexpr std::array FilterTypeStrings = {
	u"Flat",  u"LP 6",	   u"LP 12",   u"LP 18",   u"LP 24",   u"HP 6",	   u"HP 12",  u"HP 18",
	u"HP 24", u"BP 12/12", u"BP 6/18", u"BP 18/6", u"BP 6/12", u"BP 12/6", u"BP 6/6", u"TB 303",
};

static const constexpr std::array DecayModeStrings = {u"Original", u"Extended"};

using vst3utils::param::range;
using vst3utils::param::linear_functions;
using vst3utils::param::exponent_functions;
using vst3utils::param::range_description;
using vst3utils::param::list_description;
using vst3utils::param::convert_functions;

static const constexpr convert_functions decayParamValueFunc = exponent_functions<200, 2000> ();
static const constexpr convert_functions decayAltParamValueFunc = exponent_functions<30, 3000> ();

//------------------------------------------------------------------------
static constexpr std::array<vst3utils::param::description, Parameters::count ()>
	parameterDescriptions = {
		{
			{range_description (u"waveform", 0.85, linear_functions<0, 1> (), 0)},
			{range_description (u"tuning", 440, linear_functions<400, 480> (), 0, u"Hz")},
			{range_description (u"cutoff", 2394, exponent_functions<314, 2394> (), 0, u"Hz")},
			{range_description (u"resonance", 50, linear_functions<0, 100> (), 0)},
			{range_description (u"envmod", 25, linear_functions<0, 100> (), 0, u"%")},
			{range_description (u"decay", 300, decayParamValueFunc, 0, u"ms")},
			{range_description (u"accent", 50, linear_functions<0, 100> (), 0, u"%")},
			{range_description (u"volume", -12, linear_functions<-60, 0> (), 1, u"dB")},
			{list_description (u"filter type", 15, FilterTypeStrings)},
			{range_description (u"audio peak", 0., linear_functions<0, 1> ())},
			{range_description (u"pitchbend", 0, linear_functions<-12, 12> (), 2)},
			{list_description (u"decay mode", 0, DecayModeStrings)},

#ifdef O303_EXTENDED_PARAMETERS
			{range_description (u"amp sustain", -60., linear_functions<-60, 0> (), 0)},
			{range_description (u"shaper drive", 36.9, linear_functions<0, 60> (), 0)},
			{range_description (u"shaper offset", 4.37, linear_functions<-10, 10> (), 0)},
			{range_description (u"pre-filter hpf", 44.5, exponent_functions<10, 500> (), 0)},
			{range_description (u"feedback hpf", 150., exponent_functions<10, 500> (), 0)},
			{range_description (u"post-filter hpf", 24., exponent_functions<10, 500> (), 0)},
			{range_description (u"square phase shift", 180, linear_functions<0, 360> (), 0)},
#endif
		 }
};

std::optional<Parameters> loadParameterState (Steinberg::IBStream* stream);
bool saveParameterState (const Parameters& parameter, Steinberg::IBStream* stream);

//------------------------------------------------------------------------
} // o303
