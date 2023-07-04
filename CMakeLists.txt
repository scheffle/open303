cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14 CACHE STRING "")

set(vst3sdk_SOURCE_DIR CACHE PATH ../../vst3sdk)
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(open303 VERSION 1.0.0)

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

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
		Source/VST3/normplain.h
		Source/VST3/o303cids.h
		Source/VST3/o303controller.cpp
		Source/VST3/o303factory.cpp
		Source/VST3/o303pids.h
		Source/VST3/o303processor.cpp
		Source/VST3/paramdesc.h
		Source/VST3/parameter.cpp
		Source/VST3/parameter.h
		Source/VST3/version.h
)

target_compile_features(Open303
    PUBLIC
        cxx_std_17
)

target_link_libraries(Open303
    PRIVATE
        sdk
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
            "Resource/editor.uidesc"
            "Resource/PlainKnob.png"
            "Resource/LED.png"
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

