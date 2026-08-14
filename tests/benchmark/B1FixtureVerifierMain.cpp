#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/particles/ParticleEffect.hpp>
#include <trace2d/particles/ParticleProgram.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
[[nodiscard]] bool ReadFile(const std::filesystem::path& path, std::string& out)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        return false;
    }
    out = stream.str();
    return true;
}

[[nodiscard]] bool VerifySprite(const std::filesystem::path& path)
{
    std::string text{};
    if (!ReadFile(path, text))
    {
        return false;
    }

    const auto parsed = trace2d::assets::ParseSpriteAssetToml(text, "benchmark/b1/hero.sprite.toml", path.string());
    if (!parsed.Succeeded() || parsed.asset->regions.size() != 1U)
    {
        return false;
    }

    const auto& asset = *parsed.asset;
    const auto& region = asset.regions.front();
    return asset.sampling == trace2d::assets::SpriteSampling::Nearest &&
        region.sourceSize == trace2d::assets::SpritePixelSize{16U, 16U} &&
        region.trimOffset == trace2d::assets::SpritePixelOffset{2U, 1U} &&
        region.trimSize == trace2d::assets::SpritePixelSize{12U, 14U} &&
        region.pivot == trace2d::assets::SpriteRationalPivot{8, 8, 1};
}

[[nodiscard]] bool VerifyParticle(const std::filesystem::path& path)
{
    std::string text{};
    if (!ReadFile(path, text))
    {
        return false;
    }

    const auto parsed = trace2d::particles::ParseParticleEffectToml(
        text,
        "benchmark/b1/hit_spark.trace2d.particle.toml",
        {},
        path.string());
    if (!parsed.Succeeded())
    {
        return false;
    }

    const trace2d::particles::ParticleProgram program = trace2d::particles::CompileParticleProgram(*parsed.asset);
    return program.definition.maxParticles <= 64U &&
        program.definition.periodicCount <= 8U &&
        program.definition.lifetimeFrames.maxValue <= 6U &&
        !program.lifecycle.playOnLoad;
}
}

int main(const int argc, char** const argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: trace2d_b1_fixture_verify <sprite|particle> <fixture>\n";
        return 2;
    }

    const std::string_view kind{argv[1]};
    const std::filesystem::path fixture{argv[2]};
    bool accepted = false;
    if (kind == "sprite")
    {
        accepted = VerifySprite(fixture);
    }
    else if (kind == "particle")
    {
        accepted = VerifyParticle(fixture);
    }
    else
    {
        std::cerr << "unknown verifier kind: " << kind << '\n';
        return 2;
    }

    std::cout << (accepted ? "accepted" : "rejected") << '\n';
    return accepted ? 0 : 1;
}
