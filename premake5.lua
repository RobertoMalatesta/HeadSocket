dofile("etc/premake5/Utils.lua")

solution "HeadSocket"
  configurations { "Debug", "Release" }
  platforms { "32-bit", "64-bit" }
  location(path.join(".build", _ACTION))
  
filter { "platforms:32-bit" }
  architecture "x86"

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
  
  if _ACTION == "vs2013" then
    defines { "_MSC_VER=1700" }
  elseif _ACTION == "vs2015" then
    defines { "_MSC_VER=1900" }
  end
  
filter { }
  includedirs { "." }
  targetdir "bin/%{cfg.buildcfg}"
  
group "tests"
  include "tests"

-- Dummy HeadSocket project
group ""
  project "HeadSocket"
  kind "StaticLib"
  language("C++")
  files { "*.h" }
  removeplatforms "*" -- Make sure we do not build this
