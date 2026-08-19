#include "PresentationPlayground.hpp"

#include <trace2d/render/Material2D.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d;

[[nodiscard]] bool Near(const float left, const float right, const float epsilon = 0.0005F) noexcept
{
    return std::abs(left - right) <= epsilon;
}

[[nodiscard]] render::MaterialParameterBlock2D PrepareHeadlessMaterial(
    render::MaterialParameterLayout2D& outLayout)
{
    const assets::ResourceHandleUntyped shaderIdentity{
        327U,
        1U,
        assets::ResourceTypeDomain::Shader2D,
    };
    const std::array<render::MaterialParameterDeclaration2D, 2U> declarations{{
        {"flashColor", render::MaterialParameterType2D::Color},
        {"flashAmount", render::MaterialParameterType2D::Float},
    }};
    if (!render::PrepareMaterialParameterLayout2D(
            shaderIdentity,
            declarations,
            outLayout).Succeeded())
    {
        throw std::runtime_error{"P1 headless material layout preparation failed."};
    }

    const std::array<render::MaterialParameterValue2D, 2U> defaults{{
        render::MaterialColor2D(1.0F, 0.35F, 0.10F, 1.0F),
        render::MaterialFloat2D(0.0F),
    }};
    render::MaterialParameterBlock2D block{};
    if (!render::PrepareMaterialParameterBlock2D(
            outLayout,
            defaults,
            block).Succeeded())
    {
        throw std::runtime_error{"P1 headless material block preparation failed."};
    }
    return block;
}

void Require(const bool condition, const char* const message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}
} // namespace

int main()
{
    try
    {
        render::MaterialParameterLayout2D layout{};
        const render::MaterialParameterBlock2D material = PrepareHeadlessMaterial(layout);
        examples::PresentationPlayground proof{layout, material};

        Require(proof.StartShowcase(), "P1 could not start the presentation showcase.");
        Require(proof.Step(80ms), "P1 manual presentation step at 80 ms failed.");

        const scene::Transform2D* const buttonAtFlashPeak = proof.ButtonTransform();
        const scene::Transform2D* const panelAtFlashPeak = proof.PanelTransform();
        Require(buttonAtFlashPeak != nullptr, "P1 lost the button target.");
        Require(panelAtFlashPeak != nullptr, "P1 lost the panel target.");
        Require(Near(proof.FlashAmount(), 1.0F), "hit_flash did not reach its deterministic peak.");
        Require(
            buttonAtFlashPeak->scale.x > 1.0F && buttonAtFlashPeak->scale.x < 1.19F,
            "button_punch did not advance under the supplied presentation delta.");
        Require(
            panelAtFlashPeak->position.x > -260.0F && panelAtFlashPeak->position.x < 160.0F,
            "panel_slide did not advance under the supplied presentation delta.");

        Require(proof.Step(180ms), "P1 manual presentation step to terminal state failed.");
        Require(proof.AllCompleted(), "P1 showcase sequences did not complete at 260 ms.");

        const scene::Transform2D* const buttonFinal = proof.ButtonTransform();
        const scene::Transform2D* const panelFinal = proof.PanelTransform();
        Require(buttonFinal != nullptr && panelFinal != nullptr, "P1 lost a terminal target.");
        Require(Near(proof.FlashAmount(), 0.0F), "hit_flash did not return to zero.");
        Require(
            Near(buttonFinal->scale.x, 1.0F) && Near(buttonFinal->scale.y, 1.0F),
            "button_punch did not return to its authored scale.");
        Require(
            Near(panelFinal->position.x, 160.0F) && Near(panelFinal->position.y, -100.0F),
            "panel_slide did not reach its authored destination.");

        const examples::PresentationProofMetrics metrics = proof.Metrics();
        Require(metrics.bindings.createdCount >= 5U, "P1 did not route recipe writes through TweenBinding2D.");
        Require(metrics.bindings.retainedBindingCapacity >= 8U, "P1 retained TweenBinding capacity was not prepared.");
        Require(metrics.sequences.retainedSequenceCapacity >= 4U, "P1 retained Sequence capacity was not prepared.");
        Require(metrics.materialTargets.retainedTargetCapacity >= 1U, "P1 retained Material2D target capacity was not prepared.");
        Require(metrics.materialTargets.appliedWriteCount > 0U, "P1 Material2D target did not receive tweened writes.");

        Require(proof.RestartShowcase(), "P1 deterministic showcase restart failed.");
        Require(proof.Step(260ms), "P1 single-delta replay step failed.");
        Require(proof.AllCompleted(), "P1 single-delta replay did not reach the same terminal state.");
        const scene::Transform2D* const replayButton = proof.ButtonTransform();
        const scene::Transform2D* const replayPanel = proof.PanelTransform();
        Require(replayButton != nullptr && replayPanel != nullptr, "P1 lost a replay target.");
        Require(
            Near(proof.FlashAmount(), 0.0F) &&
                Near(replayButton->scale.x, 1.0F) &&
                Near(replayPanel->position.x, 160.0F),
            "P1 manual-step replay diverged from the expected terminal state.");

        std::cout
            << "P1 presentation proof PASS: hit_flash + button_punch + panel_slide + hit_impact composition; "
            << "bindings=" << metrics.bindings.createdCount
            << ", material_writes=" << metrics.materialTargets.appliedWriteCount
            << ", retained_sequences=" << metrics.sequences.retainedSequenceCapacity
            << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "P1 presentation proof FAIL: " << error.what() << '\n';
        return 1;
    }
}
