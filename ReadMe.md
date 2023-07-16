# VST3 port of Open303

## How to build

### You need:

    * cmake
    * VST SDK (minimum version 3.7.8)
    * [vst3utils](https://github.com/scheffle/vst3utils)
    * compiler with c++17 support

### Build:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE -Dvst3sdk_PATH=Path/To/VST3SDK -Dvst3utils_PATH=Path/To/vst3utils ../
cmake --build .
```

On macOS you should use the Xcode cmake generator : `-GXcode`

## Original Readme.txt:

Open303 is a free and open source emulation of the famous Roland TB-303 bass synthesizer for the VST plugin interface (VST is a trademark of Steinberg Media Technologies GmbH). 

In order to compile it from the source code, you need the VST-SDK v2.4 from Steinberg and drop it into the folder 'Libraries', such that the directory vstsdk2.4 (from the SDK) exists as direct subfolder of 'Libraries'. 

Compilation with Microsoft Visual Studio 2008:
Load the solution-file Open303.sln (in the folder 'Build/VisualStudio2008') with Microsoft Visual Studio 2008 and try to build the plugin. If it works, you will find the results of the compilation (the final .dll and some intermediate files) in the subfolder 'Debug' or 'Release' of 'Build/VisualStudio2008', depending on whether you selected a debug- or release-build. 

Compilation with CodeBlocks:
Load the CodeBlocks project file Open303.cbp (in the folder 'Build/CodeBlocks') - and build away. The results will be found in the subfolder bin/Debug or bin/Release. On my setup, i get 15 compiler warnings which are all rooted in source files of the VST-SDK (not the Open303 code itself) - so i guess we may safely ignore them.


good luck, Robin Schmidt