
#pragma once

#include "vst3utils/enum_array.h"
#include "vst3utils/norm_plain_conversion.h"
#include "vst3utils/parameter_description.h"
#include "vst3utils/smooth_value.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <cmath>
#include <optional>

// #define O303_EXTENDED_PARAMETERS

namespace rosic {
class AcidPattern;
}

//------------------------------------------------------------------------
namespace o303 {

using Steinberg::Vst::ParamID;
using Steinberg::Vst::UnitID;

//------------------------------------------------------------------------
enum class Unit : UnitID
{
	pattern = 'patt'
};

//------------------------------------------------------------------------
constexpr UnitID asUnitID (Unit u) { return static_cast<UnitID> (u); }

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

	SeqMode,
	SeqChordFollow,
	SeqPlayingStep,
	SeqActivePattern,

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

static const constexpr std::array SeqChordFollowStrings = {u"Off", u"Scale", u"Chord"};
static constexpr auto MaxSeqPatternSteps = 16u;

using vst3utils::param::range;
using vst3utils::param::linear_functions;
using vst3utils::param::exponent_functions;
using vst3utils::param::range_description;
using vst3utils::param::list_description;
using vst3utils::param::convert_functions;
using vst3utils::param::steps_functions;

static const constexpr convert_functions decayParamValueFunc = exponent_functions<200, 2000> ();
static const constexpr convert_functions decayAltParamValueFunc = exponent_functions<30, 3000> ();

//------------------------------------------------------------------------
static constexpr std::array<vst3utils::param::description, Parameters::count ()>
	parameterDescriptions = {
		{
			{range_description (u"waveform", 0.85, linear_functions<0, 1> (), 0)},
			{range_description (u"tuning", 400, linear_functions<400, 480> (), 0, u"Hz")},
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

			{list_description (u"Sequencer", 0, vst3utils::param::strings_on_off)},
			{list_description (u"Chord Follow", 0, SeqChordFollowStrings)},
			{steps_description (u"Playing Step", 1,
								steps_functions<MaxSeqPatternSteps - 1u, 1> ())},
			{steps_description (u"Active Pattern", 1, steps_functions<15, 1> ())},

#ifdef O303_EXTENDED_PARAMETERS
			{range_description (u"amp sustain", -60., linear_functions<-60, 0> (), 0)},
			{range_description (u"shaper drive", 36.9, linear_functions<0, 60> (), 0)},
			{range_description (u"shaper offset", 4.37, linear_functions<-10, 10> (), 0)},
			{range_description (u"pre-filter hpf", 44.5, exponent_functions<10, 500> (), 0)},
			{range_description (u"feedback hpf", 150., exponent_functions<10, 500> (), 0)},
			{range_description (u"post-filter hpf", 24., exponent_functions<10, 500> (), 0)},
			{range_description (u"square phase shift", 180, linear_functions<0, 360> (), 0)},
#endif // O303_EXTENDED_PARAMETERS
		 }
};

static const constexpr auto msgIDPattern = "Pattern";
static const constexpr auto attrIDPatternIndex = "PatternIndex";

struct PatternData
{
	struct Note
	{
		int16_t key;
		int8_t octave;
		int8_t accent;
		int8_t slide;
		int8_t gate;
	};
	double stepLength;
	int32_t numSteps;
	Note note[16];
};

static constexpr auto NumSeqKeys = 12u;
static constexpr auto NumSeqOctaves = 4u;

static const constexpr std::array SeqKeyStrings = {u"C", u"C#", u"D", u"D#", u"E", u"F", u"F#",
												   u"G", u"G#", u"A", u"A#", u"B", u"C`"};

//------------------------------------------------------------------------
enum class SeqPatternParameterID
{
	NumSteps = 1000,
	StepLength,

	Key0,
	Key1,
	Key2,
	Key3,
	Key4,
	Key5,
	Key6,
	Key7,
	Key8,
	Key9,
	Key10,
	Key11,
	Key12,
	Key13,
	Key14,
	Key15,

	Octave0,
	Octave1,
	Octave2,
	Octave3,
	Octave4,
	Octave5,
	Octave6,
	Octave7,
	Octave8,
	Octave9,
	Octave10,
	Octave11,
	Octave12,
	Octave13,
	Octave14,
	Octave15,

	Accent0,
	Accent1,
	Accent2,
	Accent3,
	Accent4,
	Accent5,
	Accent6,
	Accent7,
	Accent8,
	Accent9,
	Accent10,
	Accent11,
	Accent12,
	Accent13,
	Accent14,
	Accent15,

	Slide0,
	Slide1,
	Slide2,
	Slide3,
	Slide4,
	Slide5,
	Slide6,
	Slide7,
	Slide8,
	Slide9,
	Slide10,
	Slide11,
	Slide12,
	Slide13,
	Slide14,
	Slide15,

	Gate0,
	Gate1,
	Gate2,
	Gate3,
	Gate4,
	Gate5,
	Gate6,
	Gate7,
	Gate8,
	Gate9,
	Gate10,
	Gate11,
	Gate12,
	Gate13,
	Gate14,
	Gate15,

	enum_end,
};

//------------------------------------------------------------------------
inline constexpr ParamID asIndex (SeqPatternParameterID p) { return static_cast<ParamID> (p); }

//------------------------------------------------------------------------
static constexpr vst3utils::enum_array<vst3utils::param::description, SeqPatternParameterID,
									   static_cast<size_t> (SeqPatternParameterID::NumSteps)>
	seqParameterDescriptions = {
		{
			steps_description (u"Num Steps", 16, steps_functions<MaxSeqPatternSteps - 1u, 1> ()),
			range_description (u"Step Length", 25, linear_functions<0, 100> (), 0, u"%"),

			list_description (u"Key 1", 0, SeqKeyStrings),
			list_description (u"Key 2", 0, SeqKeyStrings),
			list_description (u"Key 3", 0, SeqKeyStrings),
			list_description (u"Key 4", 0, SeqKeyStrings),
			list_description (u"Key 5", 0, SeqKeyStrings),
			list_description (u"Key 6", 0, SeqKeyStrings),
			list_description (u"Key 7", 0, SeqKeyStrings),
			list_description (u"Key 8", 0, SeqKeyStrings),
			list_description (u"Key 9", 0, SeqKeyStrings),
			list_description (u"Key 10", 0, SeqKeyStrings),
			list_description (u"Key 11", 0, SeqKeyStrings),
			list_description (u"Key 12", 0, SeqKeyStrings),
			list_description (u"Key 13", 0, SeqKeyStrings),
			list_description (u"Key 14", 0, SeqKeyStrings),
			list_description (u"Key 15", 0, SeqKeyStrings),
			list_description (u"Key 16", 0, SeqKeyStrings),

			steps_description (u"Octave 1", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 2", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 3", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 4", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 5", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 6", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 7", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 8", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 9", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 10", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 11", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 12", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 13", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 14", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 15", 0, steps_functions<4, -2> ()),
			steps_description (u"Octave 16", 0, steps_functions<4, -2> ()),

			list_description (u"Accent 1", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 2", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 3", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 4", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 5", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 6", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 7", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 8", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 9", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 10", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 11", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 12", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 13", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 14", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 15", 0, vst3utils::param::strings_on_off),
			list_description (u"Accent 16", 0, vst3utils::param::strings_on_off),

			list_description (u"Slide 1", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 2", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 3", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 4", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 5", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 6", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 7", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 8", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 9", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 10", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 11", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 12", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 13", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 14", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 15", 0, vst3utils::param::strings_on_off),
			list_description (u"Slide 16", 0, vst3utils::param::strings_on_off),

			list_description (u"Gate 1", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 2", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 3", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 4", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 5", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 6", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 7", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 8", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 9", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 10", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 11", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 12", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 13", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 14", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 15", 0, vst3utils::param::strings_on_off),
			list_description (u"Gate 16", 0, vst3utils::param::strings_on_off),
		 }
};

std::optional<Parameters> loadParameterState (Steinberg::IBStream* stream);
bool saveParameterState (const Parameters& parameter, Steinberg::IBStream* stream);

bool loadAcidPattern (rosic::AcidPattern& pattern, Steinberg::IBStream* stream);
bool saveAcidPattern (const rosic::AcidPattern& pattern, Steinberg::IBStream* stream);

//------------------------------------------------------------------------
} // o303
