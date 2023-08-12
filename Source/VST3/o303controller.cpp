#include "o303pids.h"
#include "vst3utils/parameter.h"
#include "vst3utils/message.h"
#include "../DSPCode/rosic_AcidPattern.h"
#include "public.sdk/source/vst/vsteditcontroller.cpp"
#include "public.sdk/source/vst/vsthelpers.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
#include "vstgui/lib/algorithm.h"
#include "vstgui/lib/cclipboard.h"
#include "vstgui/lib/cdropsource.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/ioptionmenulistener.h"
#include "vstgui/lib/iviewlistener.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"
using namespace VSTGUI;
#endif

#include <unordered_map>
#include <string_view>

//------------------------------------------------------------------------
namespace o303 {

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
struct EditorDelegate
#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
: VST3EditorDelegate,
  OptionMenuListenerAdapter,
  ViewListenerAdapter
{
	void setZoom (double zoom) { uiZoom = zoom; }

	double getZoom () const { return uiZoom; }

	void didOpen (VST3Editor* editor) override
	{
		editor->setAllowedZoomFactors (zoomFactors);
		editor->setZoomFactor (uiZoom);
	}

	void onZoomChanged (VST3Editor* editor, double newZoom) override { uiZoom = newZoom; }

	CView* verifyView (CView* view, const UIAttributes& attributes,
					   const IUIDescription* description, VST3Editor* editor) override
	{
		if (auto customViewName = attributes.getAttributeValue (IUIDescription::kCustomViewName))
		{
			if (*customViewName == "SetupMenu")
			{
				if (auto menu = dynamic_cast<COptionMenu*> (view))
				{
					menu->registerViewListener (this);
					menu->registerOptionMenuListener (this);
					setupZoomMenu (menu, editor);
				}
			}
		}
		return view;
	}

	void viewWillDelete (CView* view) override
	{
		if (auto menu = dynamic_cast<COptionMenu*> (view))
			menu->unregisterOptionMenuListener (this);
		view->unregisterViewListener (this);
	}

	void setupZoomMenu (COptionMenu* menu, VST3Editor* editor)
	{
		menu->setStyle (menu->getStyle () | COptionMenu::kMultipleCheckStyle);
		auto titleItem = new CMenuItem ("UI Zoom");
		titleItem->setEnabled (false);
		menu->addEntry (titleItem);
		for (auto index = 0u; index < zoomFactors.size (); ++index)
		{
			auto factor = zoomFactors[index];
			UTF8String factorString = std::to_string (static_cast<int32_t> (factor * 100.));
			factorString += " %";
			auto item = new CCommandMenuItem ({factorString, static_cast<int32_t> (index)});
			item->setActions ([editor, factor] (auto item) { editor->setZoomFactor (factor); });
			menu->addEntry (item);
		}
	}

	void onOptionMenuPrePopup (COptionMenu* menu) override
	{
		if (auto index = indexOf (zoomFactors.begin (), zoomFactors.end (), uiZoom))
		{
			for (auto item : *menu->getItems ())
			{
				if (item->getTag () >= 0)
				{
					item->setChecked (item->getTag () == *index);
				}
			}
		}
	}

private:
	static const std::vector<double> zoomFactors;
	double uiZoom {1.};
};
#else
{
	void setZoom (double zoom) {}
	double getZoom () const { return 1.; }
};
#endif
#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
const std::vector<double> EditorDelegate::zoomFactors = {0.75, 1.0, 1.25, 1.50, 1.75, 2.0};
#endif

//------------------------------------------------------------------------
struct Controller : U::Extends<EditControllerEx1, U::Directly<IMidiMapping>>
{
	std::unordered_map<CtrlNumber, ParameterID> midiCtrlerMap;
	std::unordered_map<ParamID, std::unique_ptr<vst3utils::parameter>> uiParams;
	std::unique_ptr<EditorDelegate> editorDelegate {std::make_unique<EditorDelegate> ()};

	using Parameter = vst3utils::parameter;

	static const constexpr UnitID uidPattern = 'patt';

	template<typename T>
	Parameter* getParameter (T pid, size_t offset = 0) const
	{
		return static_cast<Parameter*> (
			parameters.getParameter (static_cast<uint32> (asIndex (pid) + offset)));
	}

	tresult PLUGIN_API initialize (FUnknown* context) override
	{
		auto result = EditControllerEx1::initialize (context);
		if (result != kResultTrue)
			return result;

		addUnit (new Unit (u"Pattern", uidPattern));

		for (auto pid = 0u; pid < Parameters::count (); ++pid)
		{
			auto param = new Parameter (pid, parameterDescriptions[pid]);
			parameters.addParameter (param);
		}

		if (auto param = getParameter (ParameterID::AudioPeak))
		{
			param->getInfo ().flags = ParameterInfo::kIsReadOnly;
		}

		if (auto param = getParameter (ParameterID::DecayMode))
		{
			auto listener = [this] (Parameter& p, ParamValue value) {
				if (auto param = getParameter (ParameterID::Decay))
				{
					if (value < 0.5)
					{
						param->set_custom_to_normalized_func ([] (const auto&, auto v) {
							return decayParamValueFunc.to_normalized (v);
						});
						param->set_custom_to_plain_func (
							[] (const auto&, auto v) { return decayParamValueFunc.to_plain (v); });
					}
					else
					{
						param->set_custom_to_normalized_func ([] (const auto&, auto v) {
							return decayAltParamValueFunc.to_normalized (v);
						});
						param->set_custom_to_plain_func ([] (const auto&, auto v) {
							return decayAltParamValueFunc.to_plain (v);
						});
					}
					param->changed ();
				}
			};
			param->add_listener (listener);
			listener (*param, 0.);
		}
		if (auto param = getParameter (ParameterID::SeqActivePattern))
		{
			param->add_listener ([this] (auto& param, auto v) {
				vst3utils::message msg (Steinberg::owned (allocateMessage ()));
				msg.set_id (msgIDPattern);
				msg.get_attributes ().set<int> (attrIDPatternIndex,
												param.toPlain (v) - param.toPlain (0.));
				peerConnection->notify (msg);
			});
		}

		midiCtrlerMap = {
			{ControllerNumbers::kCtrlVolume,			 ParameterID::Volume	},
			{ControllerNumbers::kCtrlFilterCutoff,	   ParameterID::Cutoff	  },
			{ControllerNumbers::kCtrlFilterResonance, ParameterID::Resonance},
			{ControllerNumbers::kCtrlGPC6,			   ParameterID::Envmod	  },
			{ControllerNumbers::kPitchBend,			ParameterID::PitchBend},
		};

		if (auto param = getParameter (ParameterID::SeqPlayingStep))
			param->getInfo ().flags = ParameterInfo::kIsReadOnly;

		auto pid = asIndex (SeqPatternParameterID::NumSteps);
		for (const auto& desc : seqParameterDescriptions)
		{
			auto param = new Parameter (pid, desc);
			param->getInfo ().unitId = uidPattern;
			parameters.addParameter (param);
			++pid;
		}

		enum UIParamID
		{
			Copy = 10000,
			Paste,
			Clear,
			ShiftLeft,
			ShiftRight,
			PatternUp,
			PatternDown,
		};

		static constexpr vst3utils::param::description copyParamDesc =
			vst3utils::param::steps_description (u"Copy", 0, steps_functions<1> ());
		static constexpr vst3utils::param::description pasteParamDesc =
			vst3utils::param::steps_description (u"Paste", 0, steps_functions<1> ());
		static constexpr vst3utils::param::description clearParamDesc =
			vst3utils::param::steps_description (u"Clear", 0, steps_functions<1> ());
		static constexpr vst3utils::param::description shiftLeftParamDesc =
			vst3utils::param::steps_description (u"ShiftLeft", 0, steps_functions<1> ());
		static constexpr vst3utils::param::description shiftRightParamDesc =
			vst3utils::param::steps_description (u"ShiftRight", 0, steps_functions<1> ());
		static constexpr vst3utils::param::description patternUpParamDesc =
			vst3utils::param::steps_description (u"PatternUp", 0, steps_functions<1> ());
		static constexpr vst3utils::param::description patternDownParamDesc =
			vst3utils::param::steps_description (u"PatternDown", 0, steps_functions<1> ());
		auto copyParam = std::make_unique<vst3utils::parameter> (UIParamID::Copy, copyParamDesc);
		auto pasteParam = std::make_unique<vst3utils::parameter> (UIParamID::Paste, pasteParamDesc);
		auto clearParam = std::make_unique<vst3utils::parameter> (UIParamID::Clear, clearParamDesc);
		auto shiftLeftParam =
			std::make_unique<vst3utils::parameter> (UIParamID::ShiftLeft, shiftLeftParamDesc);
		auto shiftRightParam =
			std::make_unique<vst3utils::parameter> (UIParamID::ShiftRight, shiftRightParamDesc);
		auto patternUpParam =
			std::make_unique<vst3utils::parameter> (UIParamID::PatternUp, patternUpParamDesc);
		auto patternDownParam =
			std::make_unique<vst3utils::parameter> (UIParamID::PatternDown, patternDownParamDesc);

		copyParam->add_listener ([this] (auto&, auto value) {
			if (value > 0.5)
				performCopy ();
		});
		pasteParam->add_listener ([this] (auto&, auto value) {
			if (value > 0.5)
				performPaste ();
		});
		clearParam->add_listener ([this] (auto&, auto value) {
			if (value > 0.5)
				performClear ();
		});
		shiftLeftParam->add_listener ([this] (auto&, auto value) {
			if (value > 0.5)
				performShiftLeft ();
		});
		shiftRightParam->add_listener ([this] (auto&, auto value) {
			if (value > 0.5)
				performShiftRight ();
		});
		patternUpParam->add_listener ([this] (auto& obj, auto value) {
			if (value > 0.5)
			{
				performPatternChange (1);
				obj.setNormalized (0.);
			}
		});
		patternDownParam->add_listener ([this] (auto& obj, auto value) {
			if (value > 0.5)
			{
				performPatternChange (-1);
				obj.setNormalized (0.);
			}
		});

		uiParams.emplace (UIParamID::Copy, std::move (copyParam));
		uiParams.emplace (UIParamID::Paste, std::move (pasteParam));
		uiParams.emplace (UIParamID::Clear, std::move (clearParam));
		uiParams.emplace (UIParamID::ShiftLeft, std::move (shiftLeftParam));
		uiParams.emplace (UIParamID::ShiftRight, std::move (shiftRightParam));
		uiParams.emplace (UIParamID::PatternUp, std::move (patternUpParam));
		uiParams.emplace (UIParamID::PatternDown, std::move (patternDownParam));

		return kResultTrue;
	}

	PatternData makePatternData () const
	{
		PatternData data {};
		data.stepLength = getParameter (SeqPatternParameterID::StepLength)->getNormalized ();
		data.numSteps = getParameter (SeqPatternParameterID::NumSteps)->getPlain ();
		for (auto i = 0; i < 16; ++i)
		{
			data.note[i].key = getParameter (SeqPatternParameterID::Key0, i)->getPlain ();
			data.note[i].octave = getParameter (SeqPatternParameterID::Octave0, i)->getPlain ();
			data.note[i].accent = getParameter (SeqPatternParameterID::Accent0, i)->getPlain ();
			data.note[i].slide = getParameter (SeqPatternParameterID::Slide0, i)->getPlain ();
			data.note[i].gate = getParameter (SeqPatternParameterID::Gate0, i)->getPlain ();
		}
		return data;
	}

	void performEditOfCurrentValue (const Parameter& param)
	{
		beginEdit (param.getInfo ().id);
		performEdit (param.getInfo ().id, param.getNormalized ());
		endEdit (param.getInfo ().id);
	}

	void applyPatternData (const PatternData& data, bool performEdit = true)
	{
		startGroupEdit ();
		if (auto p = getParameter (SeqPatternParameterID::StepLength))
		{
			p->setNormalized (data.stepLength);
			if (performEdit)
				performEditOfCurrentValue (*p);
		}
		if (auto p = getParameter (SeqPatternParameterID::NumSteps))
		{
			p->setPlain (data.numSteps);
			if (performEdit)
				performEditOfCurrentValue (*p);
		}
		for (auto i = 0; i < 16; ++i)
		{
			if (auto p = getParameter (SeqPatternParameterID::Key0, i))
			{
				p->setPlain (data.note[i].key);
				if (performEdit)
					performEditOfCurrentValue (*p);
			}
			if (auto p = getParameter (SeqPatternParameterID::Octave0, i))
			{
				p->setPlain (data.note[i].octave);
				if (performEdit)
					performEditOfCurrentValue (*p);
			}
			if (auto p = getParameter (SeqPatternParameterID::Accent0, i))
			{
				p->setPlain (data.note[i].accent);
				if (performEdit)
					performEditOfCurrentValue (*p);
			}
			if (auto p = getParameter (SeqPatternParameterID::Slide0, i))
			{
				p->setPlain (data.note[i].slide);
				if (performEdit)
					performEditOfCurrentValue (*p);
			}
			if (auto p = getParameter (SeqPatternParameterID::Gate0, i))
			{
				p->setPlain (data.note[i].gate);
				if (performEdit)
					performEditOfCurrentValue (*p);
			}
		}

		finishGroupEdit ();
	}

	void performCopy ()
	{
#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
		auto data = makePatternData ();
		auto clipboardData = CDropSource::create (&data, sizeof (data), CDropSource::Type::kBinary);
		CClipboard::set (clipboardData);
#endif
	}

	void performPaste ()
	{
#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
		if (auto data = VSTGUI::CClipboard::get ())
		{
			for (auto it = begin (data); it != end (data); ++it)
			{
				if ((*it).type == IDataPackage::Type::kBinary &&
					(*it).dataSize == sizeof (PatternData))
				{
					auto patternData = reinterpret_cast<const PatternData*> ((*it).data);
					applyPatternData (*patternData);
					break;
				}
			}
		}
#endif
	}

	void performClear ()
	{
		PatternData data {};
		applyPatternData (data);
	}

	void performShiftLeft ()
	{
		auto data = makePatternData ();
		for (auto i = 1; i < 16; ++i)
		{
			std::swap (data.note[i - 1], data.note[i]);
		}
		applyPatternData (data);
	}

	void performShiftRight ()
	{
		auto data = makePatternData ();
		for (auto i = 14; i >= 0; --i)
		{
			std::swap (data.note[i], data.note[i + 1]);
		}
		applyPatternData (data);
	}

	void performPatternChange (int32 amount)
	{
		if (auto param = getParameter (ParameterID::SeqActivePattern))
		{
			auto pat = param->getPlain () + amount;
			if (pat > 16)
				pat = 0;
			else if (pat < 1)
				pat = 16;
			beginEdit (asIndex (ParameterID::SeqActivePattern));
			param->setPlain (pat);
			performEdit (asIndex (ParameterID::SeqActivePattern), param->getNormalized ());
			endEdit (asIndex (ParameterID::SeqActivePattern));
		}
	}

	tresult PLUGIN_API terminate () override { return EditControllerEx1::terminate (); }

	Steinberg::Vst::Parameter* getParameterObject (ParamID tag) override
	{
		if (auto param = EditControllerEx1::getParameterObject (tag))
			return param;
		auto it = uiParams.find (tag);
		if (it != uiParams.end ())
			return it->second.get ();
		return nullptr;
	}

	tresult PLUGIN_API notify (IMessage* message) override
	{
		vst3utils::message msg (message);
		if (msg.get_id () == msgIDPattern)
		{
			auto attributes = msg.get_attributes ();
			if (!attributes.is_valid ())
				return kInternalError;
			if (auto v = attributes.get<PatternData, 1> (msgIDPattern))
			{
				applyPatternData (*v->data, false);
			}
			return kResultTrue;
		}
		return kResultFalse;
	}

	tresult PLUGIN_API setComponentState (IBStream* state) override
	{
		if (auto params = loadParameterState (state))
		{
			for (auto i = 0u; i < Parameters::count (); ++i)
			{
				if (auto param = parameters.getParameter (i))
					param->setNormalized (params->at (i).get ());
			}
			if (auto param = getParameter (ParameterID::SeqActivePattern))
				param->changed ();
			return kResultTrue;
		}
		return kInternalError;
	}

	tresult PLUGIN_API setState (Steinberg::IBStream* state) override
	{
		if (Vst::Helpers::isProjectState (state) == kResultTrue)
		{
			IBStreamer streamer (state, kLittleEndian);
			double zoom;
			if (streamer.readDouble (zoom))
				editorDelegate->setZoom (zoom);
		}
		return kResultTrue;
	}

	tresult PLUGIN_API getState (Steinberg::IBStream* state) override
	{
		if (Vst::Helpers::isProjectState (state) == kResultTrue)
		{
			IBStreamer streamer (state, kLittleEndian);
			streamer.writeDouble (editorDelegate->getZoom ());
		}
		return kResultTrue;
	}

	tresult PLUGIN_API getMidiControllerAssignment (int32 busIndex, int16 channel,
													CtrlNumber midiControllerNumber,
													ParamID& id) override
	{
		if (busIndex != 0 || channel != 0)
			return kInvalidArgument;
		auto it = midiCtrlerMap.find (midiControllerNumber);
		if (it != midiCtrlerMap.end ())
		{
			id = asIndex (it->second);
			return kResultTrue;
		}
		return kResultFalse;
	}

#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
	IPlugView* createView (FIDString name) override
	{
		if (std::string_view (Vst::ViewType::kEditor) == name)
		{
			auto editor = new VST3Editor (this, "Open303", "editor.uidesc");
			editor->setDelegate (editorDelegate.get ());
			return editor;
		}
		return nullptr;
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
	uint32 numParameters;
	if (!s.readInt32u (numParameters) || numParameters == 0)
		return {};
	Parameters result;
	for (auto& p : result)
	{
		double value;
		if (!s.readDouble (value))
			return {};
		p.set (value);
		if (--numParameters == 0)
			break;
	}
	return {result};
}

//------------------------------------------------------------------------
bool saveParameterState (const Parameters& parameter, Steinberg::IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	s.writeInt32 (stateID);
	s.writeInt32 (stateVersion);
	s.writeInt32u (static_cast<uint32_t> (parameter.size ()));
	for (const auto& p : parameter)
	{
		s.writeDouble (p.get ());
	}
	return true;
}

static constexpr int32 patStateID = 'patt';
static constexpr int32 patStateVersion = 1;

//------------------------------------------------------------------------
bool loadAcidPattern (rosic::AcidPattern& pattern, Steinberg::IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	int32 id;
	if (!s.readInt32 (id) || id != patStateID)
		return false;
	int32 version;
	if (!s.readInt32 (version) || version > patStateVersion) // future build?
		return false;

	double stepLength {};
	if (!s.readDouble (stepLength))
		return false;
	pattern.setStepLength (stepLength);
	int32 numSteps {};
	if (!s.readInt32 (numSteps) || numSteps != pattern.getMaxNumSteps ())
		return false;
	if (!s.readInt32 (numSteps))
		return false;
	pattern.setNumSteps (numSteps);
	for (auto step = 0; step < pattern.getMaxNumSteps (); ++step)
	{
		int32 iv {};
		if (!s.readInt32 (iv))
			return false;
		pattern.setKey (step, iv);
		if (!s.readInt32 (iv))
			return false;
		pattern.setOctave (step, iv);
		bool bv {};
		if (!s.readBool (bv))
			return false;
		pattern.setAccent (step, bv);
		if (!s.readBool (bv))
			return false;
		pattern.setSlide (step, bv);
		if (!s.readBool (bv))
			return false;
		pattern.setGate (step, bv);
	}

	return true;
}

//------------------------------------------------------------------------
bool saveAcidPattern (const rosic::AcidPattern& pattern, Steinberg::IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	s.writeInt32 (patStateID);
	s.writeInt32 (patStateVersion);
	s.writeDouble (pattern.getStepLength ());
	s.writeInt32 (pattern.getMaxNumSteps ());
	s.writeInt32 (pattern.getNumSteps ());
	for (auto step = 0; step < pattern.getMaxNumSteps (); ++step)
	{
		s.writeInt32 (pattern.getKey (step));
		s.writeInt32 (pattern.getOctave (step));
		s.writeBool (pattern.getAccent (step));
		s.writeBool (pattern.getSlide (step));
		s.writeBool (pattern.getGate (step));
	}
	return true;
}

//------------------------------------------------------------------------
} // o303
