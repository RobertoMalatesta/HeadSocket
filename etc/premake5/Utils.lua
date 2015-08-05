-----------------------------------------------------------------------------------------------------------------------
function readFile(fileName)

  local f = io.open(fileName, "rb")
  local content = f:read("*all")
  f:close()
  
  return content
  
end

-----------------------------------------------------------------------------------------------------------------------
function writeFile(fileName, content)

  local f = io.open(fileName, "wb")
  f:write(content)
  f:close()
  
end

-----------------------------------------------------------------------------------------------------------------------
function generateStringSource(fileName)

  print("Generating string source: " .. fileName)

  local content = "R\"H34D50CK3T(" .. readFile(fileName) .. ")H34D50CK3T\";"
  writeFile(fileName .. ".inl", content)  

end

-----------------------------------------------------------------------------------------------------------------------
function printTable(t)

	for k, v in pairs(t) do
		local vt = type(v)
		if vt == "table" then
			print(k .. ": [table]")
		elseif vt == "boolean" then
			if v == true then
				print(k .. ": true")
			else
				print(k .. ": false")
			end
		else
			print(k .. ": " .. v)
		end
	end

end

-- Generate named project using defined params
-- Supported parameters and default values:
--
--     type ("lib") - console application ("console")
--                  - windowed application ("windowed")
--                  - console/windowed in debug/release ("app")
--                  - library or DLL, depends on selected configuration ("lib")
--                  - static library ("static")
--                  - dynamic library ("dynamic")
--
--     language ("C++") - project language
--
--     include_current (false) - add current directory to includes
--
--     name ("") - overriding project name
--
-----------------------------------------------------------------------------------------------------------------------
function generateProject(params)

	local name = group().current.filename
	local projectGroup = group().group
	
	print("Generating project: " .. projectGroup .. "/" .. name)
	
	function getParam(key, default)
		local result = params[key]
		if result == nil then	return default end
		return result
	end
	
	local _type = getParam("type", "lib")
	local _language = getParam("language", "C++")
	local _include_current = getParam("include_current", false)
	local _name = getParam("name", "")

	language(_language)
	
	if _type == "lib" then
		filter { "configurations:Static*" }
			kind "StaticLib"
		filter { "configurations:DLL*" }
			kind "SharedLib"
		filter { }
	elseif _type == "console" then
		kind "ConsoleApp"
	elseif _type == "windowed" then
		kind "WindowedApp"
	elseif _type == "static" then
		kind "StaticLib"
	elseif _type == "dynamic" then
		kind "SharedLib"
	elseif _type == "app" then
		filter { "configurations:Debug" }
			kind "ConsoleApp"
		filter { "configurations:Release" }
			kind "WindowedApp"
		filter { }
	end
	
	files { "**.c", "**.cpp", "**.h", "**.hpp", "**.inl" }
	
	if _include_current == true then
		includedirs { "." }
	end
	
	if _name ~= "" then
		targetname(_name)
	end
	
end
