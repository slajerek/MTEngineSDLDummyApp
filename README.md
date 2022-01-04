# MTEngineSDLDummyApp 

This is example application that utlises the MTEngineSDL. The application has a simple ImGui view that can be made fullscreen and a main menu bar with workspaces.
You can also not use ImGui and just use SDL2 by a simple wrapper `DummyInit.cpp`, you need to uncomment the SDL Renderer code.

# How to compile

## macOS

Application assumes that the MTEngineSDL is in a folder near the MTEngineSDLDummyApp.
Xcode project is in `./platform/MacOS`

## Windows

Visual Studio project is in `./platform/Windows`.

## Linux

```
mkdir build
cd build
cmake ./../
make
```
