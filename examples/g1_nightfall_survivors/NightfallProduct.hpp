#pragma once

#include "NightfallSurvivorsGame.hpp"

#include <trace2d/input/Input.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

class NightfallProduct final
{
public:
    enum class Screen : std::uint8_t
    {
        MainMenu = 0,
        Profile,
        Achievements,
        Settings,
        CharacterSelect,
        StageSelect,
        Playing,
        Pause,
        Result,
    };

    enum class CharacterId : std::uint8_t
    {
        StarWarrior = 0,
        EmberKnight,
        MoonRanger,
        Count,
    };

    enum class StageId : std::uint8_t
    {
        MoonlitRuins = 0,
        EmberCrypt,
        AstralAbyss,
        Count,
    };

    enum class AchievementId : std::uint8_t
    {
        FirstHunt = 0,
        FirstClear,
        HundredKills,
        RisingStar,
        AstralClear,
        HealthyClear,
        Count,
    };

    enum class InputDisposition : std::uint8_t
    {
        Consumed = 0,
        PassToGame,
        Quit,
    };

    struct CharacterDefinition final
    {
        std::string_view nameKo{};
        std::string_view subtitleKo{};
        std::string_view descriptionKo{};
        float moveSpeedMultiplier{1.0F};
        float damageMultiplier{1.0F};
        float fireIntervalMultiplier{1.0F};
        float maximumHealthMultiplier{1.0F};
    };

    struct StageDefinition final
    {
        std::string_view nameKo{};
        std::string_view subtitleKo{};
        std::string_view descriptionKo{};
        float durationSeconds{180.0F};
        float enemyHealthMultiplier{1.0F};
        float enemySpeedMultiplier{1.0F};
        float spawnRateMultiplier{1.0F};
    };

    struct AchievementDefinition final
    {
        std::string_view nameKo{};
        std::string_view descriptionKo{};
    };

    struct Profile final
    {
        std::uint32_t totalRuns{0U};
        std::uint32_t totalKills{0U};
        std::uint32_t totalClears{0U};
        std::uint32_t bestLevel{0U};
        std::uint32_t stars{0U};
        float bestSurvivalSeconds{0.0F};
        std::uint32_t unlockedCharactersMask{1U};
        std::uint32_t unlockedStagesMask{1U};
        std::uint32_t achievementsMask{0U};
        std::uint32_t sfxVolumeStep{2U};
        bool cameraShakeEnabled{true};
    };

    struct RunSummary final
    {
        bool valid{false};
        bool cleared{false};
        std::uint32_t kills{0U};
        std::uint32_t level{0U};
        std::uint32_t health{0U};
        std::uint32_t maximumHealth{0U};
        float elapsedSeconds{0.0F};
        std::uint32_t starsEarned{0U};
        std::uint32_t newlyUnlockedCharactersMask{0U};
        std::uint32_t newlyUnlockedStagesMask{0U};
        std::uint32_t newlyUnlockedAchievementsMask{0U};
    };

    explicit NightfallProduct(std::filesystem::path savePath);

    void Load();
    void Save() const;
    void ApplySettings(NightfallSurvivorsGame& game) const noexcept;

    [[nodiscard]] InputDisposition HandleInput(
        trace2d::input::InputControl control,
        NightfallSurvivorsGame& game);
    void ObserveRun(NightfallSurvivorsGame& game);

    [[nodiscard]] Screen CurrentScreen() const noexcept;
    [[nodiscard]] std::size_t Selection() const noexcept;
    [[nodiscard]] CharacterId SelectedCharacter() const noexcept;
    [[nodiscard]] StageId SelectedStage() const noexcept;
    [[nodiscard]] const Profile& PlayerProfile() const noexcept;
    [[nodiscard]] const RunSummary& LastRunSummary() const noexcept;

    [[nodiscard]] bool IsCharacterUnlocked(CharacterId character) const noexcept;
    [[nodiscard]] bool IsStageUnlocked(StageId stage) const noexcept;
    [[nodiscard]] bool IsAchievementUnlocked(AchievementId achievement) const noexcept;
    [[nodiscard]] float SfxVolume() const noexcept;
    [[nodiscard]] bool CameraShakeEnabled() const noexcept;

    [[nodiscard]] static const CharacterDefinition& Character(CharacterId id) noexcept;
    [[nodiscard]] static const StageDefinition& Stage(StageId id) noexcept;
    [[nodiscard]] static const AchievementDefinition& Achievement(AchievementId id) noexcept;
    [[nodiscard]] static constexpr std::size_t CharacterCount() noexcept
    {
        return static_cast<std::size_t>(CharacterId::Count);
    }
    [[nodiscard]] static constexpr std::size_t StageCount() noexcept
    {
        return static_cast<std::size_t>(StageId::Count);
    }
    [[nodiscard]] static constexpr std::size_t AchievementCount() noexcept
    {
        return static_cast<std::size_t>(AchievementId::Count);
    }

private:
    void MoveSelection(int delta) noexcept;
    void GoBack(NightfallSurvivorsGame& game) noexcept;
    void StartSelectedRun(NightfallSurvivorsGame& game);
    void CompleteRun(NightfallSurvivorsGame& game);
    void EvaluateAchievements(const NightfallSurvivorsGame& game, bool cleared) noexcept;
    void UnlockProgression(bool cleared) noexcept;
    [[nodiscard]] std::size_t ItemCountForCurrentScreen() const noexcept;
    [[nodiscard]] NightfallSurvivorsGame::RunConfig BuildRunConfig() const noexcept;

    std::filesystem::path savePath_{};
    Profile profile_{};
    RunSummary lastRun_{};
    Screen screen_{Screen::MainMenu};
    Screen screenBeforePause_{Screen::Playing};
    std::size_t selection_{0U};
    CharacterId selectedCharacter_{CharacterId::StarWarrior};
    StageId selectedStage_{StageId::MoonlitRuins};
};
