// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    rosco.cpp

    Specific (per target) constants for the rosco emulator.

****************************************************************************/

#include "emu.h"
#include "main.h"

#define APPNAME                 "ROSCO"
#define APPNAME_LOWER           "rosco"
#define CONFIGNAME              "rosco"
#define COPYRIGHT               "Copyright Nicola Salmoria\nand the MAME team\nhttps://mamedev.org"
#define COPYRIGHT_INFO          "Copyright Nicola Salmoria and the MAME team"

const char * emulator_info::get_appname() { return APPNAME; }
const char * emulator_info::get_appname_lower() { return APPNAME_LOWER; }
const char * emulator_info::get_configname() { return CONFIGNAME; }
const char * emulator_info::get_copyright() { return COPYRIGHT; }
const char * emulator_info::get_copyright_info() { return COPYRIGHT_INFO; }
