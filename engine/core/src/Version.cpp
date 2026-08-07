#include <trace2d/core/Version.hpp>

#ifndef TRACE2D_VERSION_STRING
#define TRACE2D_VERSION_STRING "0.0.0"
#endif

namespace trace2d::core
{
std::string_view Version() noexcept
{
    return TRACE2D_VERSION_STRING;
}
}
