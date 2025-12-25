set_project("a1")
set_languages("c++23")

set_allowedplats("linux", "macosx", "windows")
set_allowedmodes("debug", "release")

add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })

if is_mode("debug") then
	set_symbols("debug")
	set_optimize("none")
	if not is_plat("windows") then
		add_cxxflags("-fsanitize=address,undefined", { force = true })
		add_ldflags("-fsanitize=address,undefined", { force = true })
	end
end

if is_mode("release") then
	set_symbols("hidden")
	set_optimize("fastest")
	set_strip("all")
end

add_requires(
	"spdlog",
	{ system = false, version = "1.15.3", configs = { header_only = false, std_format = true, noexcept = true } }
)

add_requires("cpp-linenoise", { system = false })

add_cxxflags("-fno-rtti", "-fno-exceptions")

target("gpamgr_core", function()
	set_kind("object")
	add_includedirs("include/")
	add_files("src/*.cc")
	add_packages("spdlog", { public = true })
	add_packages("cpp-linenoise", { public = true })
end)

target("gpamgr", function()
	set_default(true)
	set_kind("binary")
	add_files("exec/main.cc")
	add_includedirs("include/")
	add_deps("gpamgr_core")
	add_packages("spdlog", { public = true })
end)

target("ut", function()
	set_default(false)
	set_kind("binary")
	add_files("test/*.cc", "exec/ut.cc")
	add_includedirs("include/")
	add_deps("gpamgr_core")
	add_packages("spdlog", { public = true })
end)
