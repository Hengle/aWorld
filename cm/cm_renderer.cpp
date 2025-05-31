#include "cg/cg_local.hpp"
#include "cm_typedefs.hpp"
#include "cm_brush.hpp"
#include "cm_entity.hpp"
#include "cg/cg_offsets.hpp"
#include "cm_renderer.hpp"
#include "com/com_vector.hpp"
#include "net/nvar_table.hpp"
#include "r/backend/rb_endscene.hpp"
#include "r/r_utils.hpp"
#include "r/r_drawactive.hpp"
#include "utils/hook.hpp"
#include <iostream>

void RB_DrawDebug([[maybe_unused]] GfxViewParms* viewParms)
{

	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void, GfxViewParms*>(HOOK_PREFIX(__func__))->call(viewParms);
#else
		return;
#endif

#if(DEBUG_SUPPORT)
	hooktable::find<void, GfxViewParms*>(HOOK_PREFIX(__func__))->call(viewParms);
#endif
	

	CM_ShowCollision(viewParms);

}


/***********************************************************************
 >
***********************************************************************/

void CM_MakeInteriorRenderable(const std::vector<fvec3>& points, const float* _color)
{
	GfxColor color;
	std::size_t idx{};

	auto n_points = points.size();

	R_ConvertColorToBytes(_color, &color);

	for (idx = 0; idx < n_points; ++idx) {
		RB_SetPolyVertice(points[idx].As<float*>(), &color, tess->vertexCount + idx, idx, 0);
	}

	for (idx = 2; idx < n_points; ++idx) {
		tess->indices[tess->indexCount + 0] = static_cast<short>(tess->vertexCount);
		tess->indices[tess->indexCount + 1] = static_cast<short>(idx + tess->vertexCount);
		tess->indices[tess->indexCount + 2] = static_cast<short>(idx + tess->vertexCount - 1);
		tess->indexCount += 3;
	}

	tess->vertexCount += n_points;
}

int CM_MakeOutlinesRenderable(const std::vector<fvec3>& points, const float* color, bool depthTest, int nverts)
{
	auto n_points = points.size();
	auto vert_index_prev = n_points - 1u;

	for (auto i : std::views::iota(0u, n_points)) {

		nverts = RB_AddDebugLine(CGDebugData::g_debugPolyVerts, depthTest,
			points[vert_index_prev].As<float*>(), points[i].As<float*>(), color, nverts);
		vert_index_prev = i;
	}

	return nverts;
}
void CM_DrawCollisionPoly(const std::vector<fvec3>& points, const float* colorFloat, bool depthtest)
{
	RB_DrawPolyInteriors(points, colorFloat, true, depthtest);
}

GfxPointVertex verts[2075];
void CM_DrawCollisionEdges(const std::vector<fvec3>& points, const float* colorFloat, bool depthtest)
{
	const auto numPoints = points.size();
	auto vert_count = 0;
	auto vert_index_prev = numPoints - 1u;


	for (auto i : std::views::iota(0u, numPoints)) {
		vert_count = RB_AddDebugLine(verts, depthtest, points[i].As<vec_t*>(), points[vert_index_prev].As<vec_t*>(), colorFloat, vert_count);
		vert_index_prev = i;
	}

	RB_DrawLines3D(vert_count / 2, 1, verts, depthtest);

}
using IntChild = ImNVar<int>;
using FloatChild = ImNVar<float>;
using BoolChild = ImNVar<bool>;
void CM_ShowCollision([[maybe_unused]] GfxViewParms* viewParms)
{

	if (CClipMap::Size() == NULL && CGentities::Size() == NULL)
		return;

	auto showCollision = NVar_FindMalleableVar<bool>("Show Collision");

	if (!showCollision->Get())
		return;

	cplane_s frustum_planes[6];
	CreateFrustumPlanes(frustum_planes);

	const auto brush = showCollision->GetChild("Brush Filter");

	cm_renderinfo render_info =
	{
		.frustum_planes = frustum_planes,
		.num_planes = 5,
		.draw_dist = showCollision->GetChildAs<FloatChild>("Draw Distance")->Get(),
		.depth_test = showCollision->GetChildAs<BoolChild>("Depth Test")->Get(),
		.poly_type = static_cast<EPolyType>(showCollision->GetChildAs<IntChild>("Poly type")->Get()),
		.only_colliding = showCollision->GetChildAs<BoolChild>("Ignore Noncolliding")->Get(),
		.only_bounces = brush->GetChildAs<BoolChild>("Only Bounces")->Get(),
		.only_elevators = brush->GetChildAs<BoolChild>("Only Elevators")->Get(),
		.alpha = showCollision->GetChildAs<FloatChild>("Transparency")->Get()
	};

	CGDebugData::tessVerts = 0;
	CGDebugData::tessIndices = 0;

	if(render_info.poly_type != pt_edges) {
		std::unique_lock<std::mutex> lock(CClipMap::GetLock());
		RB_BeginSurfaceInternal(true, render_info.depth_test);

		CClipMap::ForEach([&render_info](const GeometryPtr_t& poly) {

			if (RB_CheckTessOverflow(poly->num_verts, 3 * (poly->num_verts - 2)))
				RB_TessOverflow(true, render_info.depth_test);

			if (poly->RB_MakeInteriorsRenderable(render_info)) {
				CGDebugData::tessVerts += poly->num_verts;
				CGDebugData::tessIndices += 3 * (poly->num_verts - 2);
			}
			
		});

		std::unique_lock<std::mutex> gentLock(CGentities::GetLock());

		CGentities::ForEach([&render_info](const GentityPtr_t& gent) {

			auto numVerts = gent->GetNumVerts();
			if (RB_CheckTessOverflow(numVerts, 3 * (numVerts - 2)))
				RB_TessOverflow(true, render_info.depth_test);

			if (gent->RB_MakeInteriorsRenderable(render_info)) {
				CGDebugData::tessVerts += numVerts;
				CGDebugData::tessIndices += 3 * (numVerts - 2);
			}
		});

		RB_EndTessSurface();
	}

	if (render_info.poly_type != pt_polys) {

		int vert_count = 0;
		std::unique_lock<std::mutex> lock(CClipMap::GetLock());

		CClipMap::ForEach([&](const GeometryPtr_t& poly) {
			if (poly->RB_MakeOutlinesRenderable(render_info, vert_count)) {
				CGDebugData::tessVerts += poly->num_verts;
				CGDebugData::tessIndices += 3 * (poly->num_verts - 2);
			}
		});

		std::unique_lock<std::mutex> gentLock(CGentities::GetLock());

		const auto showConnections = NVar_FindMalleableVar<bool>("Show Collision")
			->GetChildAs<ImNVar<std::string>>("Entity Filter")
			->GetChildAs<ImNVar<bool>>("Draw Connections")->Get();

		CGentities::ForEach([&](const GentityPtr_t& gent) {
			auto numVerts = gent->GetNumVerts();

			if (gent->RB_MakeOutlinesRenderable(render_info, vert_count)) {
				CGDebugData::tessVerts += numVerts;
				CGDebugData::tessIndices += 3 * (numVerts - 2);
			}

			if (showConnections) {
				gent->RB_RenderConnections(render_info, vert_count);
			}
		});

		if (vert_count)
			RB_DrawLines3D(vert_count / 2, 1, CGDebugData::g_debugPolyVerts, render_info.depth_test);
	}
}

void CM_DrawBrushBounds(const cbrush_t* brush, bool depthTest)
{
	RB_DrawBoxEdges(brush->mins, brush->maxs, depthTest, vec4_t{1,0,0,1});
}

bool CM_IsRenderingAsPolygons()
{
	return NVar_FindMalleableVar<bool>("Show Collision")->GetChildAs<BoolChild>("As Polygons")->Get();
}
