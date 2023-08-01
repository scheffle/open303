cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14 CACHE STRING "")

project(open303 VERSION 2.0.0)

set(vst3sdk_PATH "../vst3sdk" CACHE PATH "Path to the VST SDK")
set(vst3utils_PATH "../vst3utils" CACHE PATH "Path to vst3utils")

if(NOT vst3sdk_PATH)
    message(FATAL_ERROR "You need to provide the path to the VST3 SDK via the variable vst3sdk_PATH !")
endif()
if(NOT vst3utils_PATH)
    message(FATAL_ERROR "You need to provide the path to vst3utils via the variable vst3utils_PATH !")
endif()

set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES 0)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES 0)
set(SMTG_CXX_STANDARD 17)

add_subdirectory("${vst3sdk_PATH}" ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

add_subdirectory("${vst3utils_PATH}" ${PROJECT_BINARY_DIR}/vst3utils)

add_library(libopen303
     Source/DSPCode/GlobalDefinitions.h
     Source/DSPCode/GlobalFunctions.cpp
     Source/DSPCode/GlobalFunctions.h
     Source/DSPCode/fft4g.c
     Source/DSPCode/rosic_AcidPattern.cpp
     Source/DSPCode/rosic_AcidPattern.h
     Source/DSPCode/rosic_AcidSequencer.cpp
     Source/DSPCode/rosic_AcidSequencer.h
     Source/DSPCode/rosic_AnalogEnvelope.cpp
     Source/DSPCode/rosic_AnalogEnvelope.h
     Source/DSPCode/rosic_BiquadFilter.cpp
     Source/DSPCode/rosic_BiquadFilter.h
     Source/DSPCode/rosic_BlendOscillator.cpp
     Source/DSPCode/rosic_BlendOscillator.h
     Source/DSPCode/rosic_Complex.cpp
     Source/DSPCode/rosic_Complex.h
     Source/DSPCode/rosic_DecayEnvelope.cpp
     Source/DSPCode/rosic_DecayEnvelope.h
     Source/DSPCode/rosic_EllipticQuarterBandFilter.cpp
     Source/DSPCode/rosic_EllipticQuarterBandFilter.h
     Source/DSPCode/rosic_FourierTransformerRadix2.cpp
     Source/DSPCode/rosic_FourierTransformerRadix2.h
     Source/DSPCode/rosic_FunctionTemplates.cpp
     Source/DSPCode/rosic_FunctionTemplates.h
     Source/DSPCode/rosic_LeakyIntegrator.cpp
     Source/DSPCode/rosic_LeakyIntegrator.h
     Source/DSPCode/rosic_MidiNoteEvent.cpp
     Source/DSPCode/rosic_MidiNoteEvent.h
     Source/DSPCode/rosic_MipMappedWaveTable.cpp
     Source/DSPCode/rosic_MipMappedWaveTable.h
     Source/DSPCode/rosic_NumberManipulations.cpp
     Source/DSPCode/rosic_NumberManipulations.h
     Source/DSPCode/rosic_OnePoleFilter.cpp
     Source/DSPCode/rosic_OnePoleFilter.h
     Source/DSPCode/rosic_Open303.cpp
     Source/DSPCode/rosic_Open303.h
     Source/DSPCode/rosic_RealFunctions.cpp
     Source/DSPCode/rosic_RealFunctions.h
     Source/DSPCode/rosic_TeeBeeFilter.cpp
     Source/DSPCode/rosic_TeeBeeFilter.h    
)

target_compile_features(libopen303
    PUBLIC
        cxx_std_17
)

smtg_add_vst3plugin(Open303
	SOURCES_LIST
		Source/VST3/o303cids.h
		Source/VST3/o303controller.cpp
		Source/VST3/o303factory.cpp
		Source/VST3/o303pids.h
		Source/VST3/o303processor.cpp
		Source/VST3/version.h
)

target_compile_features(Open303
    PUBLIC
        cxx_std_17
)

target_link_libraries(Open303
    PRIVATE
        sdk
        vst3utils
        libopen303
)

if(SMTG_ENABLE_VSTGUI_SUPPORT)
	target_compile_definitions(Open303
		PUBLIC
			SMTG_ENABLE_VSTGUI_SUPPORT
	)
    target_sources(Open303
        PRIVATE
            Resource/editor.uidesc
    )
    target_link_libraries(Open303
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(Open303
        RESOURCES
            "Resource/Background.png"
            "Resource/editor.uidesc"
            "Resource/PlainKnob.png"
            "Resource/PlainKnob_2.0x.png"
            "Resource/LED.png"
            "Resource/LED_2.0x.png"
    )
	smtg_target_add_plugin_snapshots(Open303
		RESOURCES
			"Resource/C81FEB9C94F14346A9A7A84D91E4E5FE_snapshot.png"
			"Resource/C81FEB9C94F14346A9A7A84D91E4E5FE_snapshot_2.0x.png"
	)
endif()

if(SMTG_MAC)
	smtg_target_setup_universal_binary(libopen303)
	smtg_target_setup_universal_binary(Open303)

    smtg_target_set_bundle(Open303
        BUNDLE_IDENTIFIER de.arne-scheffler.vst3.O303
        INFOPLIST "${CMAKE_CURRENT_LIST_DIR}/Resource/Info.plist" PREPROCESS
    )
    smtg_target_set_debug_executable(Open303
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(Open303 PRIVATE 
        Resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Open303)

        smtg_target_set_debug_executable(Open303
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)

