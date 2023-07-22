#include "../DSPCode/rosic_Open303.h"
#include "o303cids.h"
#include "o303pids.h"

#include "vst3utils/event_iterator.h"
#include "vst3utils/parameter_updater.h"
#include "public.sdk/source/vst/utility/audiobuffers.h"
#include "public.sdk/source/vst/utility/processdataslicer.h"
#include "public.sdk/source/vst/utility/rttransfer.h"
#include "public.sdk/source/vst/utility/sampleaccurate.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

//------------------------------------------------------------------------
namespace o303 {

using namespace Steinberg;
using namespace Steinberg::Vst;
using EventIterator = vst3utils::event_iterator;
using ParameterUpdater = vst3utils::throttled_parameter_updater;
using vst3utils::begin;
using vst3utils::end;

//------------------------------------------------------------------------
struct Processor : AudioEffect
{
	using RTTransfer = RTTransferT<Parameters>;
	Parameters parameter;
	RTTransfer paramTransfer;
	rosic::Open303 open303Core;
	ParameterUpdater peakUpdater {asIndex (ParameterID::AudioPeak)};
	const vst3utils::param::convert_func* decayValueFunc = &decayParamValueFunc.to_plain;

	Processor ()
	{
		setControllerClass (ControllerUID);
		for (auto index = 0u; index < parameter.size (); ++index)
		{
#ifdef O303_EXTENDED_PARAMETERS
			if (index >= asIndex (ParameterID::Amp_Sustain))
				parameter[index].set_alpha (1.);
#endif
			parameter[index].set (parameterDescriptions[index].default_normalized);

			updateParameter (index, parameter[index]);
		}
	}

	tresult PLUGIN_API initialize (FUnknown* context) override
	{
		tresult result = AudioEffect::initialize (context);
		if (result != kResultOk)
			return result;
		addAudioOutput (u"Stereo Out", Vst::SpeakerArr::kStereo);
		addEventInput (u"Event Input", 1);
		return kResultOk;
	}
	tresult PLUGIN_API terminate () override { return AudioEffect::terminate (); }
	tresult PLUGIN_API setActive (TBool state) override { return AudioEffect::setActive (state); }

	tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
	                                       SpeakerArrangement* outputs, int32 numOuts) override
	{
		if (numIns != numOuts || numIns != 1)
			return kResultFalse;
		if (inputs[0] != outputs[0] || inputs[0] != Vst::SpeakerArr::kStereo)
			return kResultFalse;
		return kResultTrue;
	}

	tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) override
	{
		return kResultTrue;
	}

	tresult PLUGIN_API setupProcessing (Vst::ProcessSetup& newSetup) override
	{
		auto result = AudioEffect::setupProcessing (newSetup);
		if (result == kResultTrue)
		{
			peakUpdater.init (newSetup.sampleRate);
		}
		return result;
	}

	tresult PLUGIN_API setState (Steinberg::IBStream* state) override
	{
		if (auto params = loadParameterState (state))
		{
			paramTransfer.transferObject_ui (std::make_unique<Parameters> (std::move (*params)));
			return kResultTrue;
		}
		return kInternalError;
	}

	tresult PLUGIN_API getState (Steinberg::IBStream* state) override
	{
		if (saveParameterState (parameter, state))
			return kResultTrue;
		return kInternalError;
	}

	void handleParameterChanges (IParameterChanges* inputParameterChanges)
	{
		int32 numParamsChanged = inputParameterChanges->getParameterCount ();
		for (int32 index = 0; index < numParamsChanged; index++)
		{
			if (auto* paramQueue = inputParameterChanges->getParameterData (index))
			{
				Vst::ParamValue value;
				int32 sampleOffset;
				int32 numPoints = paramQueue->getPointCount ();
				if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
					parameter[paramQueue->getParameterId ()].set (value);
			}
		}
	}

	void updateParameter (size_t index, double value)
	{
		const auto& pd = parameterDescriptions;

		switch (static_cast<ParameterID> (index))
		{
			case ParameterID::Waveform:
				open303Core.setWaveform (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Tuning:
				open303Core.setTuning (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Cutoff:
				open303Core.setCutoff (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Resonance:
				open303Core.setResonance (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Envmod:
				open303Core.setEnvMod (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Decay: open303Core.setDecay ((*decayValueFunc) (value)); break;
			case ParameterID::Accent:
				open303Core.setAccent (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Volume:
				open303Core.setVolume (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Filter_Type:
				open303Core.filter.setMode (pd[index].convert.to_plain (value));
				break;
			case ParameterID::PitchBend:
				open303Core.setPitchBend (pd[index].convert.to_plain (value));
				break;
			case ParameterID::AudioPeak: break;
			case ParameterID::DecayMode:
				decayValueFunc = value < 0.5 ? &decayParamValueFunc.to_plain :
				                               &decayAltParamValueFunc.to_plain;
				updateParameter (asIndex (ParameterID::Decay),
				                 parameter[asIndex (ParameterID::Decay)]);
				break;
#ifdef O303_EXTENDED_PARAMETERS
			case ParameterID::Amp_Sustain:
				open303Core.setAmpSustain (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Tanh_Shaper_Drive:
				open303Core.setTanhShaperDrive (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Tanh_Shaper_Offset:
				open303Core.setTanhShaperOffset (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Pre_Filter_Hpf:
				open303Core.setPreFilterHighpass (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Feedback_Hpf:
				open303Core.setFeedbackHighpass (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Post_Filter_Hpf:
				open303Core.setPostFilterHighpass (pd[index].convert.to_plain (value));
				break;
			case ParameterID::Square_Phase_Shift:
				open303Core.setSquarePhaseShift (pd[index].convert.to_plain (value));
				break;
#endif
		}
	}

	void advanceToNextNoteEvent (EventIterator& it, const EventIterator& end)
	{
		while (it != end)
		{
			if (it->type == Event::kNoteOnEvent || it->type == Event::kNoteOffEvent)
				break;
			++it;
		}
	}

	void handleEvent (const Event& event)
	{
		if (event.type == Event::kNoteOnEvent)
			open303Core.noteOn (event.noteOn.pitch, event.noteOn.velocity * 127.);
		else if (event.type == Event::kNoteOffEvent)
			open303Core.noteOn (event.noteOff.pitch, 0);
	}

	template <SymbolicSampleSizes SampleSize>
	void processSliced (Steinberg::Vst::ProcessData& data)
	{
		using SampleType =
		    std::conditional_t<SampleSize == SymbolicSampleSizes::kSample32, float, double>;

		static constexpr auto SampleAccuracy = 4;

		auto eventIterator = begin (data.inputEvents);
		auto eventEndIterator = end (data.inputEvents);
		advanceToNextNoteEvent (eventIterator, eventEndIterator);
		auto sampleCounter = SampleAccuracy;
		auto peak = static_cast<SampleType> (0.);

		ProcessDataSlicer slicer (SampleAccuracy);
		slicer.process<SampleSize> (data, [&] (ProcessData& data) {
			for (auto index = 0u; index < parameter.size (); ++index)
			{
				auto& p = parameter[index];
				auto old = *p;
				if (p.process () != old)
				{
					updateParameter (index, *p);
				}
			}
			if (eventIterator != eventEndIterator)
			{
				eventIterator->sampleOffset -= data.numSamples;
				while (eventIterator->sampleOffset <= 0)
				{
					handleEvent (*eventIterator);
					++eventIterator;
					advanceToNextNoteEvent (eventIterator, eventEndIterator);
					if (eventIterator == eventEndIterator)
						break;
					eventIterator->sampleOffset -= sampleCounter;
				}
			}

			auto& outs = data.outputs[0];
			auto left = getChannelBuffers<SampleSize> (outs)[0];
			auto right = getChannelBuffers<SampleSize> (outs)[1];
			for (auto index = 0; index < data.numSamples; ++index, ++left, ++right)
			{
				*left = *right = static_cast<SampleType> (open303Core.getSample ());
				assert (!isnan (*left));
				assert (!isinf (*left));
				peak += std::abs (*left);
			}
			sampleCounter += data.numSamples;
		});

		if (peak == static_cast<SampleType> (0.))
		{
			data.outputs[0].silenceFlags = 0x3;
		}

		peakUpdater.process (
		    vst3utils::exp_to_normalized<ParamValue> (0.00001, 1., peak / data.numSamples), data);
	}

	tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) override
	{
		paramTransfer.accessTransferObject_rt ([this] (auto& param) {
			for (auto index = 0u; index < param.size () && index < parameter.size (); ++index)
			{
				parameter[index].set(param[index].get ());
			}
		});
		if (data.inputParameterChanges)
			handleParameterChanges (data.inputParameterChanges);

		if (data.numSamples <= 0)
			return kResultTrue;

		if (processSetup.symbolicSampleSize == SymbolicSampleSizes::kSample32)
			processSliced<SymbolicSampleSizes::kSample32> (data);
		else
			processSliced<SymbolicSampleSizes::kSample64> (data);

		return kResultTrue;
	}
};

//------------------------------------------------------------------------
FUnknown* createProcessor (void*)
{
	auto processor = new Processor;
	return processor->unknownCast ();
}

//------------------------------------------------------------------------
} // o303
