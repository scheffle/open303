
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include "o303cids.h"

#include "public.sdk/source/main/pluginfactory_constexpr.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
namespace o303 {

FUnknown* createProcessor (void*);
FUnknown* createController (void*);

//------------------------------------------------------------------------
} // o303

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF ("AS", "http://", "email:", 2)

//------------------------------------------------------------------------
DEF_CLASS  (o303::ProcessorUID,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"Open303",
			Vst::kDistributable,
			Vst::PlugType::kInstrument,
			"1.0.0",
			kVstVersionString,
			o303::createProcessor, nullptr)

DEF_CLASS  (o303::ControllerUID,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"Open303",
			Vst::kDistributable,
			"",
			"1.0.0",
			kVstVersionString,
			o303::createController, nullptr)

//------------------------------------------------------------------------
END_FACTORY

