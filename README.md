# Skyrim Community Shaders

SKSE core plugin for community-driven advanced graphics modifications.

[Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/86492)

## Requirements

- [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
  - Any version of PowerShell will work, including the version installed on Windows by default
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
  - Desktop development with C++
- [CMake](https://cmake.org/)
  - Edit the `PATH` environment variable and add the cmake.exe install path as a new value
  - Instructions for finding and editing the `PATH` environment variable can be found [here](https://www.java.com/en/download/help/path.html)
- [Git](https://git-scm.com/downloads)
  - Edit the `PATH` environment variable and add the Git.exe install path as a new value
- [Vcpkg](https://github.com/microsoft/vcpkg)
  - Install vcpkg by running `git clone https://github.com/microsoft/vcpkg` and `.\vcpkg\bootstrap-vcpkg.bat` in PowerShell
  - After install, add a new environment variable named `VCPKG_ROOT` with the value as the path to the folder containing vcpkg

## User Requirements

- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
  - Needed for SSE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
  - Needed for VR

## Register Visual Studio as a Generator

- Open `x64 Native Tools Command Prompt`
- Run `cmake`
- Close the cmd window

## Building
Open PowerShell and run the following commands:

```
git clone https://github.com/doodlum/skyrim-community-shaders.git
cd skyrim-community-shaders
# pull commonlib /extern to override the path settings
git submodule update --init --recursive
#configure cmake
cmake -S . --preset=ALL
#build dll
cmake --build build --config Release
```

## License

### Default

[GPL-3.0-or-later](COPYING) WITH [Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source)](EXCEPTIONS.md).  
Specifically, the Modded Code is Skyrim (and its variants) and Modding Libraries include [SKSE](https://skse.silverlock.org/) and Commonlib (and variants).

### Shaders

See LICENSE within each directory; if none, it's [Default](#default)

- [Features Shaders](features)
- [Package Shaders](package/Shaders/)
