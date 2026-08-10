#include <trace2d/particles/ParticleEffect.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
TEST(ParticleEffectInvalidInputTests, ZeroPeriodicIntervalWithBurstsReturnsSchemaError)
{
    const std::string text = R"toml(format_version = 1

[effect]
id = "invalid_interval"
backend = "cpu"
max_particles = 16
duration_frames = 4
loop = false
play_on_load = true
simulation_space = "world"

[emission]
start_frame = 0
count = 1
every_frames = 0

[spawn]
shape = "point"
offset = [0.0, 0.0]
box_half_extents = [0.0, 0.0]
circle_radius = 0.0

[lifetime]
frames = [1, 1]

[motion]
speed = [0.0, 0.0]
angle_radians = [0.0, 0.0]
acceleration = [0.0, 0.0]

[scale]
initial = [1.0, 1.0]
end_multiplier = 1.0

[rotation]
initial_radians = [0.0, 0.0]
angular_velocity_radians_per_frame = [0.0, 0.0]

[color]
initial_min = [1.0, 1.0, 1.0, 1.0]
initial_max = [1.0, 1.0, 1.0, 1.0]
end = [1.0, 1.0, 1.0, 1.0]

[presentation]
blend = "alpha"
sprites = ["textures/particles/point.png"]

[[bursts]]
frame = 0
count = 1
)toml";

    const auto result = trace2d::particles::ParseParticleEffectToml(
        text,
        "effects/invalid_interval.trace2d.particle.toml");

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.asset, nullptr);

    bool foundIntervalDiagnostic = false;
    for (const auto& diagnostic : result.diagnostics)
    {
        if (diagnostic.code == trace2d::particles::ParticleEffectErrorCode::SchemaError &&
            diagnostic.path == "emission.every_frames")
        {
            foundIntervalDiagnostic = true;
            break;
        }
    }
    EXPECT_TRUE(foundIntervalDiagnostic);
}
} // namespace
