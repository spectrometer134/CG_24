#pragma once
#include <string>

#include "GCore/Components.h"
#include "GCore/GOP.h"
#include "pxr/usd/usdGeom/xform.h"

USTC_CG_NAMESPACE_OPEN_SCOPE
struct USTC_CG_API CurveComponent : public GeometryComponent {
    explicit CurveComponent(Geometry* attached_operand);

    std::string to_string() const override;

    pxr::VtArray<pxr::GfVec3f> vertices;
    pxr::VtArray<float> width;
    pxr::VtArray<pxr::GfVec3f> displayColor;

    GeometryComponentHandle copy(Geometry* operand) const override;
};

USTC_CG_NAMESPACE_CLOSE_SCOPE
