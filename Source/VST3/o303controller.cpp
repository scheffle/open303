#include "o303pids.h"
#include "vst3utils/parameter.h"
#include "public.sdk/source/vst/vsteditcontroller.cpp"
#include "public.sdk/source/vst/vsthelpers.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#ifdef SMTG_ENABLE_VSTGUI_SUPPORT
#include "vstgui/lib/algorithm.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/ioptionmenulistener.h"
#include "vstgui/lib/iviewlistener.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"
using namespace VSTGUI;
#endif

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
	std::unique_ptr<EditorDelegate> editorDelegate {std::make_unique<EditorDelegate> ()};

	using Parameter = vst3utils::parameter;

	Parameter* getParameter (ParameterID pid) const
	{
		return static_cast<Parameter*> (parameters.getParameterByIndex (asIndex (pid)));
	}

	tresult PLUGIN_API initialize (FUnknown* context) override
	{
		auto result = EditControllerEx1::initialize (context);
		if (result != kResultTrue)
			return result;

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
			auto listener = [this] (Parameter&, ParamValue value) {
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
					if (auto handler = getComponentHandler ())
						handler->restartComponent (kParamValuesChanged);
				}
			};
			param->add_listener (listener);
			listener (*param, 0.);
		}

		return kResultTrue;
	}

	tresult PLUGIN_API terminate () override { return EditControllerEx1::terminate (); }

	tresult PLUGIN_API setComponentState (IBStream* state) override
	{
		if (auto params = loadParameterState (state))
		{
			for (auto i = 0u; i < Parameters::count (); ++i)
			{
				if (auto param = parameters.getParameter (i))
					param->setNormalized (params->at (i).get ());
			}
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
		switch (midiControllerNumber)
		{
			case ControllerNumbers::kCtrlVolume:
			{
				id = asIndex (ParameterID::Volume);
				return kResultTrue;
			}
			case ControllerNumbers::kCtrlFilterCutoff:
			{
				id = asIndex (ParameterID::Cutoff);
				return kResultTrue;
			}
			case ControllerNumbers::kCtrlFilterResonance:
			{
				id = asIndex (ParameterID::Resonance);
				return kResultTrue;
			}
			case ControllerNumbers::kCtrlGPC6:
			{
				id = asIndex (ParameterID::Envmod);
				return kResultTrue;
			}
			case ControllerNumbers::kPitchBend:
			{
				id = asIndex (ParameterID::PitchBend);
				return kResultTrue;
			}
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
	Parameters result;
	for (auto& p : result)
	{
		double value;
		if (!s.readDouble (value))
			return {};
		p.set (value);
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
		s.writeDouble (p.get ());
	}
	return true;
}

//------------------------------------------------------------------------
} // o303
