#include "../B2QualificationCandidateCommon.hpp"

namespace trace2d::benchmark::b2
{
std::unique_ptr<application::Game> CreateCandidate(scene::ComponentRegistry& registry)
{
    // Meaningful seeded defect: the task requires a six-fixed-step cooldown. Five steps makes the
    // verifier-owned frame-14 attack land one fixed step early and must be rejected.
    return qualification::CreateQualificationCandidate(registry, 5U);
}
} // namespace trace2d::benchmark::b2
