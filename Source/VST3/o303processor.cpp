#include "../DSPCode/rosic_Open303.h"
#include "o303cids.h"
#include "o303pids.h"

#include "public.sdk/source/vst/utility/audiobuffers.h"
#include "public.sdk/source/vst/utility/processdataslicer.h"
#include "public.sdk/source/vst/utility/sampleaccurate.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

//------------------------------------------------------------------------
namespace o303 {

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
struct EventIterator
{
	EventIterator () : eventList (nullptr), index (-1) {}
	EventIterator (IEventList& eventList) : eventList (&eventList), index (-1) {}
	EventIterator (IEventList& eventList, int32 index) : eventList (&eventList), index (index)
	{
		updateEvent ();
	}

	bool operator== (const EventIterator& it) const
	{
		return index == it.index && it.eventList == eventList;
	}

	bool operator!= (const EventIterator& it) const
	{
		return index != it.index || it.eventList != eventList;
	}

	EventIterator& operator++ () noexcept
	{
		if (index >= 0)
			++index;
		updateEvent ();
		return *this;
	}

	EventIterator operator+= (size_t adv) noexcept
	{
		auto prev = *this;
		index += static_cast<int32> (adv);
		updateEvent ();
		return prev;
	}

	Event& operator* () { return e; }
	Event* operator-> () { return &e; }

private:
	void updateEvent ()
	{
		if (!eventList || eventList->getEvent (index, e) != kResultTrue)
			index = -1;
	}

	Event e;
	int32 index;
	IEventList* eventList;
};

//------------------------------------------------------------------------
EventIterator begin (IEventList* eventList)
{
	return eventList ? EventIterator (*eventList, 0) : EventIterator ();
}

//------------------------------------------------------------------------
EventIterator end (IEventList* eventList)
{
	return eventList ? EventIterator (*eventList, -1) : EventIterator ();
}

//------------------------------------------------------------------------
struct Processor : AudioEffect
{
	struct ParameterUpdater
	{
		ParameterUpdater () noexcept = default;
		ParameterUpdater (ParamID parameterID) : parameterID (parameterID) {}

		void setParameterID (ParamID pID) noexcept { parameterID = pID; }

		void init (Vst::SampleRate sampleRate, double hertz = 60.) noexcept
		{
			updateInterval = static_cast<Vst::TSamples> (sampleRate / hertz);
			updateCountdown = 0;
		}

		inline void process (Vst::ParamValue currentValue, Vst::ProcessData& data) noexcept
		{
			assert (updateInterval > 0 && "update interval not set");
			if (reached (data.numSamples))
			{
				checkAndSendParameterUpdate (currentValue, data);
			}
		}

		template <typename Proc>
		inline void process (Vst::ParamValue currentValue, Vst::ProcessData& data,
		                     Proc func) noexcept
		{
			assert (updateInterval > 0 && "update interval not set");
			if (reached (data.numSamples))
			{
				checkAndSendParameterUpdate (func (lastValue, currentValue, updateInterval), data);
			}
		}

	private:
		inline bool reached (int32 samples) noexcept
		{
			updateCountdown -= samples;
			if (updateCountdown <= 0)
			{
				updateCountdown += updateInterval;
				// if update interval is smaller than the processing block, make sure we don't
				// underflow
				if (updateCountdown <= 0)
					updateCountdown = updateInterval;
				return true;
			}
			return false;
		}

		inline void checkAndSendParameterUpdate (Vst::ParamValue newValue,
		                                         Vst::ProcessData& data) noexcept
		{
			if (lastValue == newValue || data.outputParameterChanges == nullptr)
				return;
			int32 index;
			if (auto queue = data.outputParameterChanges->addParameterData (parameterID, index))
				queue->addPoint (0, newValue, index);
			lastValue = newValue;
		}

		Vst::ParamID parameterID {0};
		Vst::ParamValue lastValue {0};
		Vst::TSamples updateCountdown {0};
		Vst::TSamples updateInterval {0};
	};

	Parameters parameter;
	rosic::Open303 open303Core;
	ParameterUpdater peakUpdater {asIndex (ParameterID::AudioPeak)};

	Processor ()
	{
		setControllerClass (kOpen303ControllerUID);
		for (auto index = 0u; index < parameter.size (); ++index)
		{
			parameter[index].setValue (parameterDescriptions[index].defaultNormalized);
			updateParameter(index, parameter[index]);
		}
	}

	tresult PLUGIN_API initialize (FUnknown* context) override
	{
		tresult result = AudioEffect::initialize (context);
		if (result != kResultOk)
			return result;
		addAudioInput (u"Stereo In", Vst::SpeakerArr::kStereo);
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
			parameter = *params;
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
					parameter[paramQueue->getParameterId ()].setValue (value);
			}
		}
	}

	void updateParameter (size_t index, double value)
	{
		value = parameterDescriptions[index].toNative (value);
		switch (static_cast<ParameterID> (index))
		{
			case ParameterID::Waveform: open303Core.setWaveform (value); break;
			case ParameterID::Tuning: open303Core.setTuning (value); break;
			case ParameterID::Cutoff: open303Core.setCutoff (value); break;
			case ParameterID::Resonance: open303Core.setResonance (value); break;
			case ParameterID::Envmod: open303Core.setEnvMod (value); break;
			case ParameterID::Decay: open303Core.setDecay (value); break;
			case ParameterID::Accent: open303Core.setAccent (value); break;
			case ParameterID::Volume: open303Core.setVolume (value); break;
			case ParameterID::Filter_Type: open303Core.filter.setMode (value); break;
			case ParameterID::PitchBend: open303Core.setPitchBend (value); break;
			case ParameterID::AudioPeak: break;
#ifdef O303_EXTENDED_PARAMETERS
			case ParameterID::Amp_Sustain: open303Core.setAmpSustain (value); break;
			case ParameterID::Tanh_Shaper_Drive: open303Core.setTanhShaperDrive (value); break;
			case ParameterID::Tanh_Shaper_Offset: open303Core.setTanhShaperOffset (value); break;
			case ParameterID::Pre_Filter_Hpf: open303Core.setPreFilterHighpass (value); break;
			case ParameterID::Feedback_Hpf: open303Core.setFeedbackHighpass (value); break;
			case ParameterID::Post_Filter_Hpf: open303Core.setPostFilterHighpass (value); break;
			case ParameterID::Square_Phase_Shift: open303Core.setSquarePhaseShift (value); break;
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

		auto eventIterator = begin (data.inputEvents);
		auto eventEndIterator = end (data.inputEvents);
		advanceToNextNoteEvent (eventIterator, eventEndIterator);
		auto sampleCounter = 0;
		auto peak = static_cast<SampleType> (0.);

		ProcessDataSlicer slicer (8);
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
				if (eventIterator->sampleOffset <= 0)
				{
					handleEvent (*eventIterator);
					++eventIterator;
					advanceToNextNoteEvent (eventIterator, eventEndIterator);
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
				peak += *left;
			}
			sampleCounter += data.numSamples;
		});
		if (peak == static_cast<SampleType> (0.))
		{
			data.outputs[0].silenceFlags = 0x3;
		}

		peakUpdater.process (peak, data);
	}

	tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) override
	{
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
