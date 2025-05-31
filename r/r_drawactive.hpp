#pragma once

#include "cg/cg_local.hpp"

void CG_DrawActive();

struct IDirect3DDevice9;

long __stdcall R_EndScene(IDirect3DDevice9* device);

struct CGDebugData
{
	static volatile int tessVerts;
	static volatile int tessIndices;
	static GfxPointVertex g_debugPolyVerts[2725];

};