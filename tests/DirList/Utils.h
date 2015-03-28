#pragma once

#include <functional>
#include <string>
#include <stdint.h>

typedef std::function<void(const std::string &fileName, uint64_t size, bool isDirectory)> EnumerationCallback;

static size_t enumerateDirectoryInternal(const std::string &path, bool recursive, EnumerationCallback callback, const std::string &relPath)
{
  size_t result = 0;
  WIN32_FIND_DATA ffd;
  std::string fixedPath = path;

  if (fixedPath.empty())
    return 0;

  if (fixedPath[fixedPath.length() - 1] == '\\')
    fixedPath = fixedPath.substr(0, fixedPath.length() - 1);

  HANDLE h = FindFirstFile((fixedPath + "\\*").data(), &ffd);

  if (h == INVALID_HANDLE_VALUE)
    return 0;

  do
  {
    LARGE_INTEGER fileSize;
    fileSize.LowPart = ffd.nFileSizeLow;
    fileSize.HighPart = ffd.nFileSizeHigh;

    std::string fileName = ffd.cFileName;

    if (fileName == ".." || fileName == ".")
      continue;

    std::string fullFileName = fileName;
    if (!relPath.empty())
      fullFileName = relPath + std::string("\\") + fileName;

    bool isDirectory = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    callback(fullFileName.c_str(), fileSize.QuadPart, isDirectory);

    if (isDirectory && !(fileName == "." || fileName == "..") && recursive)
      result += enumerateDirectoryInternal((fixedPath + std::string("\\") + fileName).c_str(), true, callback, fullFileName.c_str());
    else
      ++result;

  } while (FindNextFile(h, &ffd) != 0);

  FindClose(h);

  return result;
}

static size_t enumerateDirectory(const std::string &path, bool recursive, EnumerationCallback callback)
{
  return enumerateDirectoryInternal(path, recursive, callback, "");
}

static bool fileOrDirectoryExists(const std::string &fileName)
{
  DWORD attribs = GetFileAttributes(fileName.c_str());
  return attribs != INVALID_FILE_ATTRIBUTES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string replace(const std::string &str, const std::string& oldStr, const std::string& newStr)
{
  std::string result = str;

  size_t pos = 0;
  while ((pos = result.find(oldStr, pos)) != std::string::npos)
  {
    result.replace(pos, oldStr.length(), newStr);
    pos += newStr.length();
  }

  return result;
}

static std::string escape(const std::string &str)
{
  std::string result = replace(str, "\\", "\\\\");
  result = replace(result, "\"", "\\\"");
  return result;
}

static std::string deUTFize(const std::string &str)
{
  std::string result = str;
  for (size_t i = 0; i < result.length(); ++i)
    if (result[i] > 127 || result[i] < 0)
      result[i] = '_';

  return result;
}
