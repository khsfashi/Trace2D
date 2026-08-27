#include "NightfallProduct.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
using CharacterId = NightfallProduct::CharacterId;
using StageId = NightfallProduct::StageId;
using AchievementId = NightfallProduct::AchievementId;
using CharacterDefinition = NightfallProduct::CharacterDefinition;
using StageDefinition = NightfallProduct::StageDefinition;
using AchievementDefinition = NightfallProduct::AchievementDefinition;

constexpr std::array<CharacterDefinition, NightfallProduct::CharacterCount()> Characters{{
    {
        "별의 전사",
        "균형형 / 자동 사격",
        "기본에 충실한 전사. 이동, 화력, 체력이 모두 안정적입니다.",
        1.0F,
        1.0F,
        1.0F,
        1.0F,
    },
    {
        "잿불 기사",
        "고화력 / 중장갑",
        "느리지만 더 강하고 튼튼합니다. 첫 스테이지 클리어로 해금됩니다.",
        0.90F,
        1.35F,
        1.03F,
        1.20F,
    },
    {
        "달빛 사냥꾼",
        "고속 / 연사 특화",
        "빠르게 움직이고 더 자주 사격합니다. 누적 150킬로 해금됩니다.",
        1.12F,
        0.92F,
        0.78F,
        0.86F,
    },
}};

constexpr std::array<StageDefinition, NightfallProduct::StageCount()> Stages{{
    {
        "월광 폐허",
        "생존 시간 3분",
        "첫 번째 밤. 적의 밀도가 서서히 증가합니다.",
        180.0F,
        1.0F,
        1.0F,
        1.0F,
    },
    {
        "잿불 지하묘지",
        "생존 시간 3분 30초",
        "더 질기고 빠른 적이 몰려옵니다. 월광 폐허 클리어로 해금됩니다.",
        210.0F,
        1.22F,
        1.10F,
        1.16F,
    },
    {
        "성운 심연",
        "생존 시간 4분",
        "최종 스테이지. 강한 적과 높은 밀도를 버티면 완주입니다.",
        240.0F,
        1.48F,
        1.20F,
        1.32F,
    },
}};

constexpr std::array<AchievementDefinition, NightfallProduct::AchievementCount()> Achievements{{
    {"첫 사냥", "적을 처음으로 처치하세요."},
    {"밤의 생존자", "아무 스테이지나 처음 클리어하세요."},
    {"백인참", "누적 100킬을 달성하세요."},
    {"성장하는 별", "한 번의 런에서 레벨 8에 도달하세요."},
    {"심연 정복", "성운 심연을 클리어하세요."},
    {"건재한 귀환", "체력 75% 이상을 남기고 클리어하세요."},
}};

[[nodiscard]] constexpr std::uint32_t Bit(const std::size_t index) noexcept
{
    return 1U << static_cast<std::uint32_t>(index);
}

[[nodiscard]] constexpr std::uint32_t CharacterBit(const CharacterId id) noexcept
{
    return Bit(static_cast<std::size_t>(id));
}

[[nodiscard]] constexpr std::uint32_t StageBit(const StageId id) noexcept
{
    return Bit(static_cast<std::size_t>(id));
}

[[nodiscard]] constexpr std::uint32_t AchievementBit(const AchievementId id) noexcept
{
    return Bit(static_cast<std::size_t>(id));
}

[[nodiscard]] bool IsUp(const trace2d::input::InputControl control) noexcept
{
    return control == trace2d::input::InputControl::ArrowUp || control == trace2d::input::InputControl::KeyW;
}

[[nodiscard]] bool IsDown(const trace2d::input::InputControl control) noexcept
{
    return control == trace2d::input::InputControl::ArrowDown || control == trace2d::input::InputControl::KeyS;
}

[[nodiscard]] bool IsLeft(const trace2d::input::InputControl control) noexcept
{
    return control == trace2d::input::InputControl::ArrowLeft || control == trace2d::input::InputControl::KeyA;
}

[[nodiscard]] bool IsRight(const trace2d::input::InputControl control) noexcept
{
    return control == trace2d::input::InputControl::ArrowRight || control == trace2d::input::InputControl::KeyD;
}

[[nodiscard]] bool IsConfirm(const trace2d::input::InputControl control) noexcept
{
    return control == trace2d::input::InputControl::Enter || control == trace2d::input::InputControl::Space;
}

[[nodiscard]] bool TryParseUnsigned(const std::string_view value, std::uint32_t& out) noexcept
{
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool TryParseFloat(const std::string_view value, float& out) noexcept
{
    try
    {
        std::size_t consumed = 0U;
        const std::string owned{value};
        const float parsed = std::stof(owned, &consumed);
        if (consumed != owned.size() || !std::isfinite(parsed)) return false;
        out = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace

NightfallProduct::NightfallProduct(std::filesystem::path savePath)
    : savePath_{std::move(savePath)}
{
}

void NightfallProduct::Load()
{
    profile_ = {};
    profile_.unlockedCharactersMask = CharacterBit(CharacterId::StarWarrior);
    profile_.unlockedStagesMask = StageBit(StageId::MoonlitRuins);
    profile_.sfxVolumeStep = 2U;
    profile_.cameraShakeEnabled = true;

    std::ifstream input{savePath_};
    if (!input) return;

    std::string line{};
    while (std::getline(input, line))
    {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string_view key{line.data(), separator};
        const std::string_view value{line.data() + separator + 1U, line.size() - separator - 1U};

        std::uint32_t unsignedValue = 0U;
        float floatValue = 0.0F;
        if (key == "total_runs" && TryParseUnsigned(value, unsignedValue)) profile_.totalRuns = unsignedValue;
        else if (key == "total_kills" && TryParseUnsigned(value, unsignedValue)) profile_.totalKills = unsignedValue;
        else if (key == "total_clears" && TryParseUnsigned(value, unsignedValue)) profile_.totalClears = unsignedValue;
        else if (key == "best_level" && TryParseUnsigned(value, unsignedValue)) profile_.bestLevel = unsignedValue;
        else if (key == "stars" && TryParseUnsigned(value, unsignedValue)) profile_.stars = unsignedValue;
        else if (key == "best_survival_seconds" && TryParseFloat(value, floatValue)) profile_.bestSurvivalSeconds = std::max(0.0F, floatValue);
        else if (key == "unlocked_characters" && TryParseUnsigned(value, unsignedValue)) profile_.unlockedCharactersMask = unsignedValue;
        else if (key == "unlocked_stages" && TryParseUnsigned(value, unsignedValue)) profile_.unlockedStagesMask = unsignedValue;
        else if (key == "achievements" && TryParseUnsigned(value, unsignedValue)) profile_.achievementsMask = unsignedValue;
        else if (key == "sfx_volume_step" && TryParseUnsigned(value, unsignedValue)) profile_.sfxVolumeStep = std::min(unsignedValue, 2U);
        else if (key == "camera_shake" && TryParseUnsigned(value, unsignedValue)) profile_.cameraShakeEnabled = unsignedValue != 0U;
    }

    profile_.unlockedCharactersMask |= CharacterBit(CharacterId::StarWarrior);
    profile_.unlockedStagesMask |= StageBit(StageId::MoonlitRuins);
}

void NightfallProduct::Save() const
{
    std::error_code directoryError{};
    const std::filesystem::path parent = savePath_.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, directoryError);

    const std::filesystem::path temporary = savePath_.string() + ".tmp";
    {
        std::ofstream output{temporary, std::ios::trunc};
        if (!output) return;
        output << "version=1\n";
        output << "total_runs=" << profile_.totalRuns << '\n';
        output << "total_kills=" << profile_.totalKills << '\n';
        output << "total_clears=" << profile_.totalClears << '\n';
        output << "best_level=" << profile_.bestLevel << '\n';
        output << "stars=" << profile_.stars << '\n';
        output << "best_survival_seconds=" << profile_.bestSurvivalSeconds << '\n';
        output << "unlocked_characters=" << profile_.unlockedCharactersMask << '\n';
        output << "unlocked_stages=" << profile_.unlockedStagesMask << '\n';
        output << "achievements=" << profile_.achievementsMask << '\n';
        output << "sfx_volume_step=" << profile_.sfxVolumeStep << '\n';
        output << "camera_shake=" << (profile_.cameraShakeEnabled ? 1 : 0) << '\n';
        if (!output.good()) return;
    }

    std::error_code renameError{};
    std::filesystem::remove(savePath_, renameError);
    renameError.clear();
    std::filesystem::rename(temporary, savePath_, renameError);
    if (renameError)
    {
        std::error_code cleanupError{};
        std::filesystem::remove(temporary, cleanupError);
    }
}

void NightfallProduct::ApplySettings(NightfallSurvivorsGame& game) const noexcept
{
    game.SetSfxVolume(SfxVolume());
}

float NightfallProduct::SfxVolume() const noexcept
{
    constexpr std::array<float, 3U> Volume{0.0F, 0.5F, 1.0F};
    return Volume[std::min<std::size_t>(profile_.sfxVolumeStep, Volume.size() - 1U)];
}

bool NightfallProduct::CameraShakeEnabled() const noexcept
{
    return profile_.cameraShakeEnabled;
}

void NightfallProduct::MoveSelection(const int delta) noexcept
{
    const std::size_t count = ItemCountForCurrentScreen();
    if (count == 0U)
    {
        selection_ = 0U;
        return;
    }
    const int current = static_cast<int>(selection_ % count);
    const int size = static_cast<int>(count);
    const int wrapped = (current + delta % size + size) % size;
    selection_ = static_cast<std::size_t>(wrapped);
}

std::size_t NightfallProduct::ItemCountForCurrentScreen() const noexcept
{
    switch (screen_)
    {
    case Screen::MainMenu: return 5U;
    case Screen::Settings: return 3U;
    case Screen::CharacterSelect: return CharacterCount();
    case Screen::StageSelect: return StageCount();
    case Screen::Pause: return 2U;
    case Screen::Result: return 2U;
    case Screen::Profile:
    case Screen::Achievements:
    case Screen::Playing:
        return 0U;
    }
    return 0U;
}

void NightfallProduct::GoBack(NightfallSurvivorsGame& game) noexcept
{
    switch (screen_)
    {
    case Screen::Profile:
    case Screen::Achievements:
    case Screen::Settings:
    case Screen::CharacterSelect:
        screen_ = Screen::MainMenu;
        selection_ = 0U;
        break;
    case Screen::StageSelect:
        screen_ = Screen::CharacterSelect;
        selection_ = static_cast<std::size_t>(selectedCharacter_);
        break;
    case Screen::Pause:
        screen_ = screenBeforePause_;
        selection_ = 0U;
        break;
    case Screen::Result:
        game.ReturnToIdle();
        screen_ = Screen::MainMenu;
        selection_ = 0U;
        break;
    case Screen::MainMenu:
    case Screen::Playing:
        break;
    }
}

NightfallSurvivorsGame::RunConfig NightfallProduct::BuildRunConfig() const noexcept
{
    const CharacterDefinition& character = Character(selectedCharacter_);
    const StageDefinition& stage = Stage(selectedStage_);
    NightfallSurvivorsGame::RunConfig config{};
    config.characterIndex = static_cast<std::uint32_t>(selectedCharacter_);
    config.stageIndex = static_cast<std::uint32_t>(selectedStage_);
    config.durationSeconds = stage.durationSeconds;
    config.moveSpeedMultiplier = character.moveSpeedMultiplier;
    config.damageMultiplier = character.damageMultiplier;
    config.fireIntervalMultiplier = character.fireIntervalMultiplier;
    config.maximumHealthMultiplier = character.maximumHealthMultiplier;
    config.enemyHealthMultiplier = stage.enemyHealthMultiplier;
    config.enemySpeedMultiplier = stage.enemySpeedMultiplier;
    config.spawnRateMultiplier = stage.spawnRateMultiplier;
    return config;
}

void NightfallProduct::StartSelectedRun(NightfallSurvivorsGame& game)
{
    lastRun_ = {};
    game.BeginRun(BuildRunConfig());
    ApplySettings(game);
    screen_ = Screen::Playing;
    selection_ = 0U;
}

NightfallProduct::InputDisposition NightfallProduct::HandleInput(
    const trace2d::input::InputControl control,
    NightfallSurvivorsGame& game)
{
    if (screen_ == Screen::Playing)
    {
        if (control == trace2d::input::InputControl::Escape)
        {
            screenBeforePause_ = Screen::Playing;
            screen_ = Screen::Pause;
            selection_ = 0U;
            return InputDisposition::Consumed;
        }
        return InputDisposition::PassToGame;
    }

    if (screen_ == Screen::Profile || screen_ == Screen::Achievements)
    {
        if (control == trace2d::input::InputControl::Escape || IsConfirm(control))
        {
            GoBack(game);
        }
        return InputDisposition::Consumed;
    }

    if (control == trace2d::input::InputControl::Escape)
    {
        if (screen_ == Screen::MainMenu) return InputDisposition::Quit;
        GoBack(game);
        return InputDisposition::Consumed;
    }

    if (IsUp(control))
    {
        MoveSelection(-1);
        return InputDisposition::Consumed;
    }
    if (IsDown(control))
    {
        MoveSelection(1);
        return InputDisposition::Consumed;
    }

    if (screen_ == Screen::Settings && (IsLeft(control) || IsRight(control)))
    {
        if (selection_ == 0U)
        {
            const int delta = IsRight(control) ? 1 : -1;
            const int current = static_cast<int>(profile_.sfxVolumeStep);
            profile_.sfxVolumeStep = static_cast<std::uint32_t>(std::clamp(current + delta, 0, 2));
            ApplySettings(game);
            Save();
        }
        else if (selection_ == 1U)
        {
            profile_.cameraShakeEnabled = !profile_.cameraShakeEnabled;
            Save();
        }
        return InputDisposition::Consumed;
    }

    if (!IsConfirm(control)) return InputDisposition::Consumed;

    switch (screen_)
    {
    case Screen::MainMenu:
        switch (selection_)
        {
        case 0U:
            screen_ = Screen::CharacterSelect;
            selection_ = static_cast<std::size_t>(selectedCharacter_);
            break;
        case 1U:
            screen_ = Screen::Profile;
            selection_ = 0U;
            break;
        case 2U:
            screen_ = Screen::Achievements;
            selection_ = 0U;
            break;
        case 3U:
            screen_ = Screen::Settings;
            selection_ = 0U;
            break;
        case 4U:
            return InputDisposition::Quit;
        default:
            break;
        }
        break;

    case Screen::Settings:
        if (selection_ == 0U)
        {
            profile_.sfxVolumeStep = (profile_.sfxVolumeStep + 1U) % 3U;
            ApplySettings(game);
            Save();
        }
        else if (selection_ == 1U)
        {
            profile_.cameraShakeEnabled = !profile_.cameraShakeEnabled;
            Save();
        }
        else
        {
            screen_ = Screen::MainMenu;
            selection_ = 0U;
        }
        break;

    case Screen::CharacterSelect:
    {
        const CharacterId candidate = static_cast<CharacterId>(selection_);
        if (IsCharacterUnlocked(candidate))
        {
            selectedCharacter_ = candidate;
            screen_ = Screen::StageSelect;
            selection_ = static_cast<std::size_t>(selectedStage_);
        }
        break;
    }

    case Screen::StageSelect:
    {
        const StageId candidate = static_cast<StageId>(selection_);
        if (IsStageUnlocked(candidate))
        {
            selectedStage_ = candidate;
            StartSelectedRun(game);
        }
        break;
    }

    case Screen::Pause:
        if (selection_ == 0U)
        {
            screen_ = Screen::Playing;
            selection_ = 0U;
        }
        else
        {
            game.ReturnToIdle();
            screen_ = Screen::MainMenu;
            selection_ = 0U;
        }
        break;

    case Screen::Result:
        if (selection_ == 0U)
        {
            StartSelectedRun(game);
        }
        else
        {
            game.ReturnToIdle();
            screen_ = Screen::MainMenu;
            selection_ = 0U;
        }
        break;

    case Screen::Profile:
    case Screen::Achievements:
    case Screen::Playing:
        break;
    }
    return InputDisposition::Consumed;
}

void NightfallProduct::ObserveRun(NightfallSurvivorsGame& game)
{
    if (screen_ != Screen::Playing) return;
    if (game.CurrentState() != NightfallSurvivorsGame::State::Won &&
        game.CurrentState() != NightfallSurvivorsGame::State::Lost)
    {
        return;
    }
    CompleteRun(game);
}

void NightfallProduct::UnlockProgression(const bool cleared) noexcept
{
    if (cleared && selectedStage_ == StageId::MoonlitRuins)
    {
        profile_.unlockedCharactersMask |= CharacterBit(CharacterId::EmberKnight);
        profile_.unlockedStagesMask |= StageBit(StageId::EmberCrypt);
    }
    if (cleared && selectedStage_ == StageId::EmberCrypt)
    {
        profile_.unlockedStagesMask |= StageBit(StageId::AstralAbyss);
    }
    if (profile_.totalKills >= 150U)
    {
        profile_.unlockedCharactersMask |= CharacterBit(CharacterId::MoonRanger);
    }
}

void NightfallProduct::EvaluateAchievements(
    const NightfallSurvivorsGame& game,
    const bool cleared) noexcept
{
    if (profile_.totalKills >= 1U)
        profile_.achievementsMask |= AchievementBit(AchievementId::FirstHunt);
    if (profile_.totalClears >= 1U)
        profile_.achievementsMask |= AchievementBit(AchievementId::FirstClear);
    if (profile_.totalKills >= 100U)
        profile_.achievementsMask |= AchievementBit(AchievementId::HundredKills);
    if (game.Level() >= 8U)
        profile_.achievementsMask |= AchievementBit(AchievementId::RisingStar);
    if (cleared && selectedStage_ == StageId::AstralAbyss)
        profile_.achievementsMask |= AchievementBit(AchievementId::AstralClear);
    if (cleared && game.MaximumHealth() > 0U &&
        game.Health() * 4U >= game.MaximumHealth() * 3U)
    {
        profile_.achievementsMask |= AchievementBit(AchievementId::HealthyClear);
    }
}

void NightfallProduct::CompleteRun(NightfallSurvivorsGame& game)
{
    const bool cleared = game.CurrentState() == NightfallSurvivorsGame::State::Won;
    const std::uint32_t oldCharacterMask = profile_.unlockedCharactersMask;
    const std::uint32_t oldStageMask = profile_.unlockedStagesMask;
    const std::uint32_t oldAchievementMask = profile_.achievementsMask;

    ++profile_.totalRuns;
    profile_.totalKills += game.KillCount();
    if (cleared) ++profile_.totalClears;
    profile_.bestLevel = std::max(profile_.bestLevel, game.Level());
    profile_.bestSurvivalSeconds = std::max(profile_.bestSurvivalSeconds, game.ElapsedSeconds());

    const std::uint32_t starsEarned = game.KillCount() / 5U +
        (cleared ? 25U * (static_cast<std::uint32_t>(selectedStage_) + 1U) : 0U);
    profile_.stars += starsEarned;

    UnlockProgression(cleared);
    EvaluateAchievements(game, cleared);

    lastRun_ = RunSummary{
        .valid = true,
        .cleared = cleared,
        .kills = game.KillCount(),
        .level = game.Level(),
        .health = game.Health(),
        .maximumHealth = game.MaximumHealth(),
        .elapsedSeconds = game.ElapsedSeconds(),
        .starsEarned = starsEarned,
        .newlyUnlockedCharactersMask = profile_.unlockedCharactersMask & ~oldCharacterMask,
        .newlyUnlockedStagesMask = profile_.unlockedStagesMask & ~oldStageMask,
        .newlyUnlockedAchievementsMask = profile_.achievementsMask & ~oldAchievementMask,
    };

    Save();
    screen_ = Screen::Result;
    selection_ = 0U;
}

NightfallProduct::Screen NightfallProduct::CurrentScreen() const noexcept { return screen_; }
std::size_t NightfallProduct::Selection() const noexcept { return selection_; }
NightfallProduct::CharacterId NightfallProduct::SelectedCharacter() const noexcept { return selectedCharacter_; }
NightfallProduct::StageId NightfallProduct::SelectedStage() const noexcept { return selectedStage_; }
const NightfallProduct::Profile& NightfallProduct::PlayerProfile() const noexcept { return profile_; }
const NightfallProduct::RunSummary& NightfallProduct::LastRunSummary() const noexcept { return lastRun_; }

bool NightfallProduct::IsCharacterUnlocked(const CharacterId character) const noexcept
{
    return (profile_.unlockedCharactersMask & CharacterBit(character)) != 0U;
}

bool NightfallProduct::IsStageUnlocked(const StageId stage) const noexcept
{
    return (profile_.unlockedStagesMask & StageBit(stage)) != 0U;
}

bool NightfallProduct::IsAchievementUnlocked(const AchievementId achievement) const noexcept
{
    return (profile_.achievementsMask & AchievementBit(achievement)) != 0U;
}

const NightfallProduct::CharacterDefinition& NightfallProduct::Character(const CharacterId id) noexcept
{
    return Characters[std::min<std::size_t>(static_cast<std::size_t>(id), Characters.size() - 1U)];
}

const NightfallProduct::StageDefinition& NightfallProduct::Stage(const StageId id) noexcept
{
    return Stages[std::min<std::size_t>(static_cast<std::size_t>(id), Stages.size() - 1U)];
}

const NightfallProduct::AchievementDefinition& NightfallProduct::Achievement(const AchievementId id) noexcept
{
    return Achievements[std::min<std::size_t>(static_cast<std::size_t>(id), Achievements.size() - 1U)];
}
