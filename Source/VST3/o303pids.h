
#pragma once

#include "paramdesc.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <cmath>
#include <optional>

//------------------------------------------------------------------------
namespace o303 {

using Steinberg::Vst::ParamID;

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

#ifdef O303_EXTENDED_PARAMETERS
	Amp_Sustain,
	Tanh_Shaper_Drive,
	Tanh_Shaper_Offset,
	Pre_Filter_Hpf,
	Feedback_Hpf,
	Post_Filter_Hpf,
	Square_Phase_Shift,
#endif

	Count,
};
using Parameters = VST3::EnumArray<VST3::SmoothParameter, ParameterID>;

//------------------------------------------------------------------------
inline constexpr ParamID asIndex (ParameterID p)
{
	return static_cast<ParamID> (p);
}

//------------------------------------------------------------------------
static const constexpr std::array<const char16_t*, 16> FilterTypeStrings = {
    u"Flat",  u"LP 6",     u"LP 12",   u"LP 18",   u"LP 24",   u"HP 6",    u"HP 12",  u"HP 18",
    u"HP 24", u"BP 12/12", u"BP 6/18", u"BP 18/6", u"BP 6/12", u"BP 12/6", u"BP 6/6", u"TB 303",
};

//------------------------------------------------------------------------
template <typename T>
inline constexpr T normalizedToExp (T min, T max, T normalizedValue) noexcept
{
	return min * std::exp (normalizedValue * std::log (max / min));
}

//------------------------------------------------------------------------
static constexpr std::array<VST3::ParamDesc, asIndex (ParameterID::Count)>
    parameterDescriptions = {{
        {u"waveform", 0.85, {VST3::Range {0., 1., 2}}, nullptr},
        {u"tuning", 0.5, {VST3::Range {400., 480., 0, u"Hz"}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (400., 480., v); } },
        {u"cutoff", 1., {VST3::Range {0., 1., 0, u"Hz"}}, nullptr, [] (auto v) { return normalizedToExp (314., 2394., v); } },
        {u"resonance", 0.5, {VST3::Range {0., 100., 0, u"%"}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (0., 100., v); } },
        {u"envmod", 0.25, {VST3::Range {0., 100., 0, u"%"}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (0., 100., v); } },
        {u"decay", 0.1, {VST3::Range {200., 2000., 0, u"ms"}}, nullptr, [] (auto v) { return normalizedToExp (200., 2000., v); } },
        {u"accent", 0.5, {VST3::Range {0., 100., 0, u"%"}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (0., 100., v); } },
        {u"volume", 0.8, {VST3::Range {-60., 0., 1, u"dB"}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (-60., 0., v); } },
        {u"filter type", 1., {VST3::StepCount {FilterTypeStrings.size () - 1}}, FilterTypeStrings.data (), [] (auto v) -> double { return VST3::normalizedToSteps (FilterTypeStrings.size () - 1, 0, v); } },
        {u"audio peak", 0., {VST3::Range {0., 1., 2}}, nullptr},
        {u"pitchbend", 0.5, {VST3::Range {0., 1., 2}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (-12., 12., v); }},

#ifdef O303_EXTENDED_PARAMETERS
        {u"amp sustain", 0., {VST3::Range {-60., 0., 1}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (-60., 0., v); } },
        {u"shaper drive", 0., {VST3::Range {0., 60., 0}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (0., 600., v); } },
        {u"shaper offset", 0., {VST3::Range {-10., 10., 0}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (-10., 10., v); } },
        {u"pre-filter hpf", 0., {VST3::Range {10., 500., 0}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (10., 500., v); } },
        {u"feedback hpf", 0., {VST3::Range {10., 500., 0}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (10., 500., v); } },
        {u"post-filter hpf", 0., {VST3::Range {10., 500., 0}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (10., 500., v); } },
        {u"square phase shift", 0., {VST3::Range {0., 360, 0}}, nullptr, [] (auto v) { return VST3::normalizedToPlain (0., 360., v); } },
#endif
    }};
    
std::optional<Parameters> loadParameterState (Steinberg::IBStream* stream);
bool saveParameterState (const Parameters& parameter, Steinberg::IBStream* stream);

//------------------------------------------------------------------------
} // o303
