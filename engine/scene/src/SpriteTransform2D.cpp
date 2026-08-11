#include <trace2d/scene/SpriteTransform2D.hpp>

#include <cmath>
#include <limits>
#include <numbers>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] SpritePoseStatus Success() noexcept
{
    return SpritePoseStatus{};
}

[[nodiscard]] SpritePoseStatus Failure(
    const SpritePoseError error,
    const SpritePoseField field) noexcept
{
    return SpritePoseStatus{error, field};
}

[[nodiscard]] bool IsFinite(const Vector2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool TryConvertFiniteFloat(const double value, float& outValue) noexcept
{
    constexpr double MaximumFloat = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value > MaximumFloat || value < -MaximumFloat)
    {
        return false;
    }
    outValue = static_cast<float>(value);
    return std::isfinite(outValue);
}

[[nodiscard]] bool TryLerp(const float previous, const float current, const float alpha, float& outValue) noexcept
{
    if (alpha == 0.0F)
    {
        outValue = previous;
        return true;
    }
    if (alpha == 1.0F)
    {
        outValue = current;
        return true;
    }

    const double result = static_cast<double>(previous) +
        (static_cast<double>(current) - static_cast<double>(previous)) *
            static_cast<double>(alpha);
    return TryConvertFiniteFloat(result, outValue);
}

[[nodiscard]] double WrapSignedRadians(const double radians) noexcept
{
    constexpr double Pi = std::numbers::pi_v<double>;
    constexpr double Tau = Pi * 2.0;

    double wrapped = std::fmod(radians, Tau);
    if (wrapped > Pi)
    {
        wrapped -= Tau;
    }
    else if (wrapped < -Pi)
    {
        wrapped += Tau;
    }
    return wrapped;
}

[[nodiscard]] bool TryInterpolateRotation(
    const float previous,
    const float current,
    const float alpha,
    float& outValue) noexcept
{
    if (alpha == 0.0F)
    {
        outValue = previous;
        return true;
    }
    if (alpha == 1.0F)
    {
        outValue = current;
        return true;
    }

    const double previousDouble = static_cast<double>(previous);
    const double delta = WrapSignedRadians(static_cast<double>(current) - previousDouble);
    return TryConvertFiniteFloat(
        previousDouble + delta * static_cast<double>(alpha),
        outValue);
}
} // namespace

std::string_view ToString(const SpritePoseError value) noexcept
{
    switch (value)
    {
    case SpritePoseError::None: return "none";
    case SpritePoseError::NonFiniteTransform: return "non_finite_transform";
    case SpritePoseError::NonFiniteAlpha: return "non_finite_alpha";
    case SpritePoseError::AlphaOutOfRange: return "alpha_out_of_range";
    case SpritePoseError::InterpolationOverflow: return "interpolation_overflow";
    }
    return "unknown";
}

std::string_view ToString(const SpritePoseField value) noexcept
{
    switch (value)
    {
    case SpritePoseField::None: return "none";
    case SpritePoseField::Position: return "position";
    case SpritePoseField::RotationRadians: return "rotation_radians";
    case SpritePoseField::Scale: return "scale";
    case SpritePoseField::Alpha: return "alpha";
    }
    return "unknown";
}

SpritePoseStatus ValidateSpritePose(const SpritePose2D& pose) noexcept
{
    if (!IsFinite(pose.transform.position))
    {
        return Failure(SpritePoseError::NonFiniteTransform, SpritePoseField::Position);
    }
    if (!std::isfinite(pose.transform.rotationRadians))
    {
        return Failure(SpritePoseError::NonFiniteTransform, SpritePoseField::RotationRadians);
    }
    if (!IsFinite(pose.transform.scale))
    {
        return Failure(SpritePoseError::NonFiniteTransform, SpritePoseField::Scale);
    }
    return Success();
}

SpritePoseStatus SnapSpritePoseHistory(
    SpritePoseHistory2D& history,
    const SpritePose2D& authoritativePose) noexcept
{
    const SpritePoseStatus validation = ValidateSpritePose(authoritativePose);
    if (!validation.Succeeded())
    {
        return validation;
    }

    history.previousFixed = authoritativePose;
    history.currentFixed = authoritativePose;
    return Success();
}

SpritePoseStatus CommitSpriteFixedPose(
    SpritePoseHistory2D& history,
    const SpritePose2D& authoritativePose) noexcept
{
    const SpritePoseStatus validation = ValidateSpritePose(authoritativePose);
    if (!validation.Succeeded())
    {
        return validation;
    }

    history.previousFixed = history.currentFixed;
    history.currentFixed = authoritativePose;
    return Success();
}

SpritePoseStatus ResolveSpriteAuthoritativeCurrent(
    const SpritePoseHistory2D& history,
    SpritePose2D& outPose) noexcept
{
    const SpritePoseStatus validation = ValidateSpritePose(history.currentFixed);
    if (!validation.Succeeded())
    {
        outPose = SpritePose2D{};
        return validation;
    }

    outPose = history.currentFixed;
    return Success();
}

SpritePoseStatus InterpolateSpritePose(
    const SpritePoseHistory2D& history,
    const float alpha,
    SpritePose2D& outPose) noexcept
{
    outPose = SpritePose2D{};

    const SpritePoseStatus previousValidation = ValidateSpritePose(history.previousFixed);
    if (!previousValidation.Succeeded())
    {
        return previousValidation;
    }
    const SpritePoseStatus currentValidation = ValidateSpritePose(history.currentFixed);
    if (!currentValidation.Succeeded())
    {
        return currentValidation;
    }
    if (!std::isfinite(alpha))
    {
        return Failure(SpritePoseError::NonFiniteAlpha, SpritePoseField::Alpha);
    }
    if (alpha < 0.0F || alpha > 1.0F)
    {
        return Failure(SpritePoseError::AlphaOutOfRange, SpritePoseField::Alpha);
    }

    if (!TryLerp(
            history.previousFixed.transform.position.x,
            history.currentFixed.transform.position.x,
            alpha,
            outPose.transform.position.x) ||
        !TryLerp(
            history.previousFixed.transform.position.y,
            history.currentFixed.transform.position.y,
            alpha,
            outPose.transform.position.y))
    {
        return Failure(SpritePoseError::InterpolationOverflow, SpritePoseField::Position);
    }

    if (!TryInterpolateRotation(
            history.previousFixed.transform.rotationRadians,
            history.currentFixed.transform.rotationRadians,
            alpha,
            outPose.transform.rotationRadians))
    {
        return Failure(SpritePoseError::InterpolationOverflow, SpritePoseField::RotationRadians);
    }

    if (!TryLerp(
            history.previousFixed.transform.scale.x,
            history.currentFixed.transform.scale.x,
            alpha,
            outPose.transform.scale.x) ||
        !TryLerp(
            history.previousFixed.transform.scale.y,
            history.currentFixed.transform.scale.y,
            alpha,
            outPose.transform.scale.y))
    {
        return Failure(SpritePoseError::InterpolationOverflow, SpritePoseField::Scale);
    }

    outPose.flipX = history.currentFixed.flipX;
    outPose.flipY = history.currentFixed.flipY;
    return Success();
}
} // namespace trace2d::scene
