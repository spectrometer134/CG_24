#include "SCore/SequencedComponent.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

std::string SequencedComponent::to_string() const
{
    return "Sequenced Component";
}

GeometryComponentHandle SequencedComponent::copy(Geometry* operand) const
{
    return {};
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
