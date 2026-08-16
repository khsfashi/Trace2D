#include "../B2QualificationCandidateCommon.hpp"

namespace trace2d::benchmark::b2
{
std::unique_ptr<application::Game> CreateCandidate(scene::ComponentRegistry& registry)
{
    return qualification::CreateQualificationCandidate(registry, 6U);
}
} // namespace trace2d::benchmark::b2
