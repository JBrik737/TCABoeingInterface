
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
#include <cstring>
#include <cmath>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>
#include <map>
#include <sys/stat.h>
using namespace std;

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

/*
 	altref = "laminar/B738/autopilot/mcp_alt_dial"
	hdgref = "laminar/B738/autopilot/mcp_hdg_dial"
	spdref = "laminar/B738/autopilot/mcp_speed_dial_kts_mach"
	altcom = "laminar/B738/autopilot/alt_hld_press"
	hdgcom = "laminar/B738/autopilot/hdg_sel_press"
	spdcom = "laminar/B738/autopilot/lvl_chg_press"
*/

const string versionNumber = "0.1";

#if !XPLM300
	#error This plugin requires version 300 of the SDK
#endif

#pragma region Deklarace

XPLMDataRef mcpAltitudeDref = NULL;
XPLMDataRef mcpHeadingDref = NULL;
XPLMDataRef mcpSpeedDref = NULL;

XPLMDataRef acfDescDref = NULL;

XPLMCommandRef mcpAltHldCmd = NULL;
XPLMCommandRef mcpHdgSelCmd = NULL;
XPLMCommandRef mcpLvlChgCmd = NULL;

#pragma endregion

#pragma region Pomocne funkce

void LogDebugString(string msg, bool debug = true) {
	if (debug) XPLMDebugString(("Boeing TCA Interface - " + msg).c_str());
}

void GetZiboDrefs() 
{
	mcpAltitudeDref = XPLMFindDataRef("sim/flightmodel/position/local_x");
	mcpHeadingDref = XPLMFindDataRef("sim/flightmodel/position/local_x");
	mcpSpeedDref = XPLMFindDataRef("sim/flightmodel/position/local_x");

	mcpAltHldCmd = XPLMFindCommand("");
	mcpHdgSelCmd = XPLMFindCommand("");
	mcpLvlChgCmd = XPLMFindCommand("");
}

void GetDefaultDrefs()
{

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
PLUGIN_API void XPluginStop(void) 
{
}

/*
 *  The XPluginEnable function is called by X-Plane right before the plugin is enabled.
 *  Until the plugin is enabled, it will not receive any other callbacks and its UI will be hidden and/or disabled.
 */
PLUGIN_API int XPluginEnable(void) {
	return 1;
}

/*
 *  The XPluginDisable function is called by X-Plane right before the plugin is disabled. When the plugin is disabled,
 *  it will not receive any other callbacks and its UI will be hidden and/or disabled.
 */
PLUGIN_API void XPluginDisable(void) 
{
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, long msg, void*)
{
	switch (msg)
	{
		case XPLM_MSG_PLANE_LOADED:
			LogDebugString("PLANE_LOADED\n");

			char acfDescription[512];
			memset(acfDescription, 0, sizeof(acfDescription));

			if (!acfDescDref) acfDescDref = XPLMFindDataRef("sim/aircraft/view/acf_descrip");
			XPLMGetDatab(acfDescDref, acfDescription, 0, sizeof(acfDescription));

			if (acfDescription == "Boeing 737 - 800X") GetZiboDrefs();
				else GetDefaultDrefs();

			break;
	}
}
