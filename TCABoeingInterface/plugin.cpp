
#include <XPLMInstance.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMMenus.h>
#include <XPLMUtilities.h>
#include <XPLMPlugin.h>
#include <XPLMDataAccess.h>
#include <XPLMProcessing.h>
#include <XPLMPlanes.h>
#include <XPLMNavigation.h>
#include "plugin.h"
#include <exception>
#include <filesystem>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <chrono>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>
#include <map>
#include <sys/stat.h>
using namespace std;
using namespace std::chrono;

#if LIN
	#include <GL/gl.h>
#elif __GNUC__
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

#define MSG_ADD_DATAREF 0x01000000           //  Add dataref to DRE message

#if IBM
#include <windows.h>
BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif

const string versionNumber = "0.1";

#if !XPLM300
	#error This plugin requires version 300 of the SDK
#endif

#pragma region Deklarace

XPLMDataRef dr_McpAltitude = NULL;
XPLMDataRef dr_McpHeading = NULL;
XPLMDataRef dr_McpSpeed = NULL;

XPLMDataRef dr_acfDesc = NULL;

XPLMCommandRef cmd_McpAltHld = NULL;
XPLMCommandRef cmd_McpHdgSel = NULL;
XPLMCommandRef cmd_McpLvlChg = NULL;

XPLMCommandRef cmd_Tca_Decrease = NULL;
XPLMCommandRef cmd_Tca_Increase = NULL;
XPLMCommandRef cmd_Tca_SelPress = NULL;
XPLMCommandRef cmd_Tca_IAS = NULL;
XPLMCommandRef cmd_Tca_HDG = NULL;
XPLMCommandRef cmc_Tca_ALT = NULL;

uint64_t PrevTime = 0;

#pragma endregion

#pragma region Pomocne funkce

void LogDebugString(string msg, bool debug = true) {
	if (debug) XPLMDebugString(("Boeing TCA Interface - " + msg).c_str());
}

uint64_t MsSinceEpoch() 
{
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/*
	altref = "laminar/B738/autopilot/mcp_alt_dial"
	hdgref = "laminar/B738/autopilot/mcp_hdg_dial"
	spdref = "laminar/B738/autopilot/mcp_speed_dial_kts_mach"
	altcom = "laminar/B738/autopilot/alt_hld_press"
	hdgcom = "laminar/B738/autopilot/hdg_sel_press"
	spdcom = "laminar/B738/autopilot/lvl_chg_press"
*/

void GetZiboDrefs() 
{
	dr_McpAltitude = XPLMFindDataRef("laminar/B738/autopilot/mcp_alt_dial");
	dr_McpHeading = XPLMFindDataRef("laminar/B738/autopilot/mcp_hdg_dial");
	dr_McpSpeed = XPLMFindDataRef("laminar/B738/autopilot/mcp_speed_dial_kts_mach");

	cmd_McpAltHld = XPLMFindCommand("laminar/B738/autopilot/alt_hld_press");
	cmd_McpHdgSel = XPLMFindCommand("laminar/B738/autopilot/hdg_sel_press");
	cmd_McpLvlChg = XPLMFindCommand("laminar/B738/autopilot/lvl_chg_press");
}

/*
	altref = "sim/cockpit/autopilot/altitude"
	hdgref = "sim/cockpit/autopilot/heading"
	spdref = "sim/cockpit/autopilot/airspeed"
	altcom = "sim/autopilot/altitude_hold"
	hdgcom = "sim/autopilot/heading"
	spdcom = "sim/autopilot/level_change"
*/

void GetDefaultDrefs()
{
	dr_McpAltitude = XPLMFindDataRef("sim/cockpit/autopilot/altitude");
	dr_McpHeading = XPLMFindDataRef("sim/cockpit/autopilot/heading");
	dr_McpSpeed = XPLMFindDataRef("sim/cockpit/autopilot/airspeed");

	cmd_McpAltHld = XPLMFindCommand("sim/autopilot/altitude_hold");
	cmd_McpHdgSel = XPLMFindCommand("sim/autopilot/heading");
	cmd_McpLvlChg = XPLMFindCommand("sim/autopilot/level_change");
}

#pragma endregion

/*
 *  The XPluginStart function is called by X-Plane right after the plugin's DLL is loaded.
 */
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
	strcpy(outName, ("Boeing TCA Interface ver. " + versionNumber).c_str());
	strcpy(outSig, "jbr.boeing.tca");
	strcpy(outDesc, "Interface plugin for Thrustmaster Boeing TCA throttle quadrant");

	return 1;
}

/*
 *  The XPluginStop function is called by X-Plane right before the DLL is unloaded.
 *  The plugin will be disabled (if it was enabled) before this routine is called.
 */
PLUGIN_API void XPluginStop(void) { }

/*
 *  The XPluginEnable function is called by X-Plane right before the plugin is enabled.
 *  Until the plugin is enabled, it will not receive any other callbacks and its UI will be hidden and/or disabled.
 */
PLUGIN_API int XPluginEnable(void) { return 1; }

/*
 *  The XPluginDisable function is called by X-Plane right before the plugin is disabled. When the plugin is disabled,
 *  it will not receive any other callbacks and its UI will be hidden and/or disabled.
 */
PLUGIN_API void XPluginDisable(void) { }

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, long msg, void*)
{
	switch (msg)
	{
		case XPLM_MSG_PLANE_LOADED:
			LogDebugString("PLANE_LOADED\n");

			char acfDescription[512];
			memset(acfDescription, 0, sizeof(acfDescription));

			if (!dr_acfDesc) dr_acfDesc = XPLMFindDataRef("sim/aircraft/view/acf_descrip");
			XPLMGetDatab(dr_acfDesc, acfDescription, 0, sizeof(acfDescription));

			if (acfDescription == "Boeing 737 - 800X") GetZiboDrefs();
				else GetDefaultDrefs();



			break;
	}
}


/*

int frontStairCmdHandler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon)
{
	if (inPhase == xplm_CommandEnd && (ZiboLoaded || DefaultLoaded))
		processMenuItem(foreStairs, 0, overrideFront);
	return 0;
}

    cmd_FrontStairs = XPLMCreateCommand("jbr/737ghe/toggles/front_stair", "Toggle front stairs");
	XPLMRegisterCommandHandler(cmd_FrontStairs, frontStairCmdHandler, 1, (void*)0);
*/