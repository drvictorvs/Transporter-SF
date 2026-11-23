-- Transporter_SF: SFSE plugin for the transporter in Transporter_SF.psc (split from CCFE-SF):
-- the OSF UI menu and hotkey, the grav-jump countdown box and the Teleport console commands.
-- Built on the libxse CommonLibSF plugin template (https://github.com/libxse/commonlibsf-template).

-- CommonLibSF is expected in lib/commonlibsf (see README: "Build the plugin"). Use OSF's fork
-- (github.com/ozooma10/commonlibsf) at the commit OSF Settings 1.0 builds with: its menu and
-- event support is checked against Starfield 1.16.244, which the countdown menu relies on.
includes("../commonlibsf")

set_project("Transporter-SF")
set_version("0.7.0")
set_license("GPL-3.0")  -- CommonLibSF's license, with its modding exception
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")  -- optimized, with a .pdb for crash loggers such as Trainwreck
add_rules("plugin.vsxmake.autoupdate")

target("Transporter_SF")
    add_rules("commonlibsf.plugin", {
        name = "Transporter_SF",
        author = "Victor",
        description = "Star Trek-style transporter: menu, countdown box and Teleport commands for Transporter_SF.psc"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
