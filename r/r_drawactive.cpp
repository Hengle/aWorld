#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cl/cl_utils.hpp"
#include "net/nvar_table.hpp"
#include "r/gui/r_main_gui.hpp"
#include "r/r_drawtools.hpp"
#include "r/r_utils.hpp"
#include "r_drawactive.hpp"

#include "utils/hook.hpp"
#include "utils/typedefs.hpp"

#include "cm/cm_typedefs.hpp"
#include "cm/cm_entity.hpp"

volatile int CGDebugData::tessVerts{};
volatile int CGDebugData::tessIndices{};
GfxPointVertex CGDebugData::g_debugPolyVerts[2725];

static void CG_DrawDebug(float& y)
{

	char buff[256];
	sprintf_s(buff,
		"verts:   %i\n"
		"indices: %i\n",
		CGDebugData::tessVerts, CGDebugData::tessIndices);

	R_DrawTextWithEffects(buff, "fonts/bigdevFont", 0, y, 0.15f, 0.2f, 0, vec4_t{ 1,0.753f,0.796f,0.7f }, 3, vec4_t{ 1,0,0,0 });

	y += 40.f;
}

void CG_DrawActive()
{
	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#else
		return;
#endif
	const auto showCollision = NVar_FindMalleableVar<bool>("Show Collision");

	if (showCollision->Get() && showCollision->GetChildAs<ImNVar<std::string>>("Entity Filter")->GetChild("Spawnstrings")->As<ImNVar<bool>>()->Get()) {
		const auto drawDist = showCollision->GetChildAs<ImNVar<float>>("Draw Distance")->Get();
		CGentities::ForEach([&showCollision, &drawDist](const GentityPtr_t& ptr) {
			if(ptr)
				ptr->CG_Render2D(drawDist); 
		});
	}

	float y = 40.f;
	if(NVar_FindMalleableVar<bool>("Developer")->Get())
		CG_DrawDebug(y);

#if(DEBUG_SUPPORT)
	return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#endif

}