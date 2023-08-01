#include "../DSPCode/rosic_Open303.h"
#include "o303cids.h"
#include "o303pids.h"

#include "vst3utils/event_iterator.h"
#include "vst3utils/message.h"
#include "vst3utils/parameter_changes_iterator.h"
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
enum class ChordFollow
{
	Off,
	Scale,
	Chord,
};

//------------------------------------------------------------------------
struct Processor : AudioEffect
{
	using RTTransfer = RTTransferT<Parameters>;
	Parameters parameter;
	RTTransfer paramTransfer;
	rosic::Open303 open303Core;
	ParameterUpdater peakUpdater {asIndex (ParameterID::AudioPeak)};
	ParameterUpdater seqStepUpdater {asIndex (ParameterID::SeqPlayingStep)};
	const vst3utils::param::convert_func* decayValueFunc = &decayParamValueFunc.to_plain;
	ChordFollow chordFollowMode {ChordFollow::Off};

	Processor ()
	{
		setControllerClass (ControllerUID);
		processContextRequirements.needTempo ();

		parameter[asIndex (ParameterID::DecayMode)].set_alpha (1.);
		parameter[asIndex (ParameterID::Filter_Type)].set_alpha (1.);
		parameter[asIndex (ParameterID::SeqActivePattern)].set_alpha (1.);

		for (auto index = 0u; index < parameter.size (); ++index)
		{
#ifdef O303_EXTENDED_PARAMETERS
			if (index >= asIndex (ParameterID::Amp_Sustain))
				parameter[index].set_alpha (1.);
#endif
			parameter[index].set (parameterDescriptions[index].default_normalized);

			updateParameter (index, parameter[index]);
		}
		auto pid = asIndex (SeqPatternParameterID::NumSteps);
		for (const auto& desc : seqParameterDescriptions)
			setSeqParameter (pid++, desc.default_normalized);
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
	tresult PLUGIN_API setActive (TBool state) override
	{
		if (!state)
			open303Core.allNotesOff ();
		return AudioEffect::setActive (state);
	}

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
			seqStepUpdater.init (newSetup.sampleRate);
			open303Core.sequencer.setSampleRate (newSetup.sampleRate);
		}
		return result;
	}

	tresult PLUGIN_API setState (Steinberg::IBStream* state) override
	{
		if (auto params = loadParameterState (state))
		{
			for (auto index = 0; index < open303Core.sequencer.getNumPatterns (); ++index)
				loadAcidPattern (*open303Core.sequencer.getPattern (index), state);
			paramTransfer.transferObject_ui (std::make_unique<Parameters> (std::move (*params)));
			return kResultTrue;
		}
		return kInternalError;
	}

	tresult PLUGIN_API getState (Steinberg::IBStream* state) override
	{
		if (saveParameterState (parameter, state))
		{
			for (auto index = 0; index < open303Core.sequencer.getNumPatterns (); ++index)
				saveAcidPattern (*open303Core.sequencer.getPattern (index), state);
			return kResultTrue;
		}
		return kInternalError;
	}

	tresult PLUGIN_API notify (IMessage* message) override
	{
		vst3utils::message msg (message);
		if (msg.get_id () == msgIDPattern)
		{
			if (auto v = msg.get_attributes ().get<int> (attrIDPatternIndex))
			{
				sendPatternToController (*v);
			}
			return kResultTrue;
		}
		return kResultFalse;
	}

	void sendPatternToController (int patternIndex)
	{
		if (!peerConnection)
			return;

		const auto& pattern = open303Core.sequencer.getPattern (patternIndex);
		assert (pattern != nullptr);

		vst3utils::message msg (owned (allocateMessage ()));
		if (!msg.is_valid ())
			return;
		msg.set_id (msgIDPattern.data ());
		auto attributes = msg.get_attributes ();
		if (!attributes.is_valid ())
			return;
		PatternData data;
		data.stepLength = pattern->getStepLength ();
		data.numSteps = pattern->getNumSteps ();
		for (auto step = 0; step < 16; ++step)
		{
			data.note[step].key = pattern->getKey (step);
			data.note[step].octave = pattern->getOctave (step);
			data.note[step].accent = pattern->getAccent (step);
			data.note[step].slide = pattern->getSlide (step);
			data.note[step].gate = pattern->getGate (step);
		}
		attributes.set (msgIDPattern.data (), data);
		peerConnection->notify (msg);
	}

	void handleParameterChanges (IParameterChanges* inputParameterChanges)
	{
		if (!inputParameterChanges)
			return;
		for (auto paramQueue : inputParameterChanges)
		{
			for (auto point : paramQueue)
			{
				if (point.pid < parameter.size ())
					parameter[point.pid].set (point.value);
				else
				{
					setSeqParameter (point.pid, point.value);
				}
			}
		}
	}

	int getActivePattern () const { return open303Core.sequencer.getActivePattern (); }

	void setSeqParameter (int32 pid, ParamValue value)
	{
		if (pid >= asIndex (SeqPatternParameterID::Key0) &&
			pid <= asIndex (SeqPatternParameterID::Key15))
		{
			auto step = pid -= asIndex (SeqPatternParameterID::Key0);
			open303Core.sequencer.setKey (getActivePattern (), step,
										  vst3utils::normalized_to_steps (NumSeqKeys, 0, value));
		}
		else if (pid >= asIndex (SeqPatternParameterID::Octave0) &&
				 pid <= asIndex (SeqPatternParameterID::Octave15))
		{
			auto step = pid -= asIndex (SeqPatternParameterID::Octave0);
			open303Core.sequencer.setOctave (
				getActivePattern (), step,
				vst3utils::normalized_to_steps (NumSeqOctaves, -2, value));
		}
		else if (pid >= asIndex (SeqPatternParameterID::Accent0) &&
				 pid <= asIndex (SeqPatternParameterID::Accent15))
		{
			auto step = pid -= asIndex (SeqPatternParameterID::Accent0);
			open303Core.sequencer.setAccent (getActivePattern (), step,
											 vst3utils::normalized_to_steps (1, 0, value));
		}
		else if (pid >= asIndex (SeqPatternParameterID::Slide0) &&
				 pid <= asIndex (SeqPatternParameterID::Slide15))
		{
			auto step = pid -= asIndex (SeqPatternParameterID::Slide0);
			open303Core.sequencer.setSlide (getActivePattern (), step,
											vst3utils::normalized_to_steps (1, 0, value));
		}
		else if (pid >= asIndex (SeqPatternParameterID::Gate0) &&
				 pid <= asIndex (SeqPatternParameterID::Gate15))
		{
			auto step = pid -= asIndex (SeqPatternParameterID::Gate0);
			open303Core.sequencer.setGate (getActivePattern (), step,
										   vst3utils::normalized_to_steps (1, 0, value));
		}
		else if (pid == asIndex (SeqPatternParameterID::StepLength))
		{
			open303Core.sequencer.setStepLength (value);
		}
		else if (pid == asIndex (SeqPatternParameterID::NumSteps))
		{
			open303Core.sequencer.getPattern (getActivePattern ())
				->setNumSteps (vst3utils::normalized_to_steps (MaxSeqPatternSteps - 1, 1, value));
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
			case ParameterID::Decay:
				open303Core.setDecay ((*decayValueFunc) (value));
				break;
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
			case ParameterID::AudioPeak:
				break;
			case ParameterID::DecayMode:
				decayValueFunc =
					value < 0.5 ? &decayParamValueFunc.to_plain : &decayAltParamValueFunc.to_plain;
				updateParameter (asIndex (ParameterID::Decay),
								 parameter[asIndex (ParameterID::Decay)]);
				break;
			case ParameterID::SeqMode:
				open303Core.sequencer.setMode (pd[index].convert.to_plain (value));
				break;
			case ParameterID::SeqChordFollow:
				chordFollowMode = static_cast<ChordFollow> (pd[index].convert.to_plain (value));
				for (auto bit = 0u; bit < 12u; ++bit)
					open303Core.sequencer.setKeyPermissible (bit, true);
				break;
			case ParameterID::SeqActivePattern:
				open303Core.sequencer.setActivePattern (pd[index].convert.to_plain (value) - 1);
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
			if (it->type == Event::kScaleEvent)
				handleScaleEvent (*it);
			else if (it->type == Event::kChordEvent)
				handleChordEvent (*it);
			++it;
		}
	}

	void handleChordEvent (const Event& event)
	{
		if (chordFollowMode != ChordFollow::Chord)
			return;
		auto root = event.chord.root % 12;
		for (auto bit = 0u; bit < 12u; ++bit)
		{
			bool permissible = event.chord.mask & (1 << bit);
			auto key = (root + bit) % 12;
			open303Core.sequencer.setKeyPermissible (key, permissible);
		}
	}

	void handleScaleEvent (const Event& event)
	{
		if (chordFollowMode != ChordFollow::Scale)
			return;

		for (auto bit = 0u; bit < 12u; ++bit)
		{
			bool permissible = event.scale.mask & (1 << bit);
			open303Core.sequencer.setKeyPermissible (bit, permissible);
		}
	}

	void handleEvent (const Event& event)
	{
		if (event.type == Event::kNoteOnEvent)
			open303Core.noteOn (event.noteOn.pitch, event.noteOn.velocity * 127.);
		else if (event.type == Event::kNoteOffEvent)
			open303Core.noteOn (event.noteOff.pitch, 0);
	}

	template<SymbolicSampleSizes SampleSize>
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
		seqStepUpdater.process (
			vst3utils::steps_to_normalized (MaxSeqPatternSteps - 1, 0,
											open303Core.sequencer.getCurrentPlayingStep ()),
			data);
	}

	tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) override
	{
		paramTransfer.accessTransferObject_rt ([this] (auto& param) {
			for (auto index = 0u; index < param.size () && index < parameter.size (); ++index)
			{
				parameter[index].set (param[index].get ());
			}
		});
		if (data.inputParameterChanges)
			handleParameterChanges (data.inputParameterChanges);

		if (data.numSamples <= 0)
			return kResultTrue;

		if (data.processContext && data.processContext->state & ProcessContext::kTempoValid)
			open303Core.sequencer.setTempo (data.processContext->tempo);

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
