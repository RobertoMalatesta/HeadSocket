project("XmPlayer")

generateStringSource("XmPlayer.html")

generateProject(
{
  type = "console",
	language = "C++",
})

files { "XmPlayer.html" }
debugdir "."
