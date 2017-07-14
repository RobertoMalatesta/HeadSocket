dofile("etc/premake5/Utils.lua")

solution "HeadSocket"
  configurations { "Debug", "Release" }
  platforms { "64-bit" }
  location(path.join(".build", _ACTION))

filter { "platforms:64-bit" }
  architecture "x86_64"
  
filter { "configurations:Debug" }
  defines { "_DEBUG", }
  flags { "Symbols" }
  targetsuffix "_debug"
  
filter { "configurations:Release" }
  defines { "_NDEBUG", "NDEBUG" }
  optimize "On"

filter { "system:windows" }
  defines { "_CRT_SECURE_NO_WARNINGS", "_WIN32_WINNT=0x0601", "WINVER=0x0601" }
  
  if _ACTION == "vs2015" then
    defines { "_MSC_VER=1900" }
  elseif _ACTION == "vs2017" then
    defines { "_MSC_VER=1910" }
  end
  
filter { }
  includedirs { "." }
  targetdir "bin/%{cfg.buildcfg}"
  flags { "StaticRuntime" }
  
group "tests"
  include "tests"

-- Dummy HeadSocket project
group ""
  project "HeadSocket"
  kind "StaticLib"
  language("C++")
  defines { "HEADSOCKET_IMPLEMENTATION" }
  files { "headsocket/*.h", "README.md" }
  removeplatforms "*" -- Make sure we do not build this
