#include "o303pids.h"
#include "parameter.h"
#include "public.sdk/source/vst/vsteditcontroller.cpp"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
#include "vstgui/plugin-bindings/vst3editor.h"
#endif

//------------------------------------------------------------------------
namespace o303 {

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
struct Controller : EditControllerEx1, IMidiMapping
{
	//---Interface---------
	OBJ_METHODS (EditControllerEx1, EditControllerEx1)
	DEFINE_INTERFACES
		DEF_INTERFACE (IMidiMapping)
	END_DEFINE_INTERFACES (EditControllerEx1)
	REFCOUNT_METHODS (EditControllerEx1)

	using Parameter = VST3::Parameter;

	tresult PLUGIN_API initialize (FUnknown* context) override
	{
		auto result = EditControllerEx1::initialize (context);
		if (result != kResultTrue)
			return result;

		for (auto pid = 0; pid < asIndex (ParameterID::Count); ++pid)
		{
			auto param = new Parameter (pid, parameterDescriptions[pid]);
			parameters.addParameter (param);
		}

		if (auto param = static_cast<Parameter*> (
		        parameters.getParameterByIndex (asIndex (ParameterID::Cutoff))))
		{
			param->setCustomToPlainFunc ([] (const auto&, auto v) {
				return parameterDescriptions[asIndex (ParameterID::Cutoff)].toNative (v);
			});
		}
		if (auto param = static_cast<Parameter*> (
		        parameters.getParameterByIndex (asIndex (ParameterID::Decay))))
		{
			param->setCustomToPlainFunc ([] (const auto&, auto v) {
				return parameterDescriptions[asIndex (ParameterID::Decay)].toNative (v);
			});
		}

		if (auto param = static_cast<Parameter*> (
		        parameters.getParameterByIndex (asIndex (ParameterID::AudioPeak))))
		{
			param->getInfo ().flags = ParameterInfo::kIsReadOnly;
		}

		return kResultTrue;
	}

	tresult PLUGIN_API terminate () override { return EditControllerEx1::terminate (); }

	tresult PLUGIN_API setComponentState (IBStream* state) override
	{
		if (auto params = loadParameterState (state))
		{
			for (auto i = 0; i < asIndex (ParameterID::Count); ++i)
			{
				if (auto param = parameters.getParameter (i))
					param->setNormalized (params->at (i).getValue ());
			}
			return kResultTrue;
		}
		return kInternalError;
	}

	tresult PLUGIN_API getMidiControllerAssignment (int32 busIndex, int16 channel,
	                                                CtrlNumber midiControllerNumber,
	                                                ParamID& id) override
	{
		if (busIndex != 0 || channel != 0)
			return kInvalidArgument;
		switch (midiControllerNumber)
		{
			case ControllerNumbers::kCtrlVolume: id = asIndex (ParameterID::Volume); break;
			case ControllerNumbers::kCtrlFilterCutoff: id = asIndex (ParameterID::Cutoff); break;
			case ControllerNumbers::kCtrlFilterResonance:
				id = asIndex (ParameterID::Resonance);
				break;
			case ControllerNumbers::kCtrlGPC6: id = asIndex (ParameterID::Envmod); break;
			case ControllerNumbers::kPitchBend: id = asIndex (ParameterID::PitchBend); break;
		}
		return kResultFalse;
	}

#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
	IPlugView* createView (FIDString name) override
	{
		return new VSTGUI::VST3Editor (this, "Open303", "editor.uidesc");
	}
#endif
};

//------------------------------------------------------------------------
FUnknown* createController (void*)
{
	auto instance = new Controller ();
	return instance->unknownCast ();
}

//------------------------------------------------------------------------
static constexpr int32 stateID = 'o303';
static constexpr int32 stateVersion = 1;

//------------------------------------------------------------------------
std::optional<Parameters> loadParameterState (Steinberg::IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	int32 id;
	if (!s.readInt32 (id) || id != stateID)
		return {};
	int32 version;
	if (!s.readInt32 (version) || version > stateVersion) // future build?
		return {};
	Parameters result;
	for (auto& p : result)
	{
		double value;
		if (!s.readDouble (value))
			return {};
		p.setValue (value);
	}
	return {result};
}

//------------------------------------------------------------------------
bool saveParameterState (const Parameters& parameter, Steinberg::IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	s.writeInt32 (stateID);
	s.writeInt32 (stateVersion);
	for (const auto& p : parameter)
	{
		s.writeDouble (p.getValue ());
	}
	return true;
}

//------------------------------------------------------------------------
} // o303
