#include <trace2d/core/FileTransaction.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
class TemporaryDirectory final
{
public:
    TemporaryDirectory()
    {
        const auto serial = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("trace2d-core-file-transaction-" + std::to_string(serial));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored{};
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_{};
};

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(output);
    output << text;
    ASSERT_TRUE(output);
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    EXPECT_TRUE(input);
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::size_t FileCount(const std::filesystem::path& directory)
{
    std::size_t count = 0U;
    for (const auto& entry : std::filesystem::directory_iterator{directory})
    {
        if (entry.is_regular_file()) ++count;
    }
    return count;
}
} // namespace

TEST(FileTransactionTests, ValidationFailurePreservesExistingTargetAndRemovesTemporaryFile)
{
    TemporaryDirectory directory{};
    const std::filesystem::path target = directory.Path() / "slot.save";
    WriteText(target, "old-authoritative-state");

    bool validatorCalled = false;
    std::string error{};
    const bool committed = trace2d::core::CommitTextFileAtomically(
        target,
        "new-candidate-state",
        "test save",
        error,
        [&](const std::filesystem::path& temporary, std::string& validationError)
        {
            validatorCalled = true;
            EXPECT_EQ(ReadText(temporary), "new-candidate-state");
            validationError = "intentional validation failure";
            return false;
        });

    EXPECT_FALSE(committed);
    EXPECT_TRUE(validatorCalled);
    EXPECT_EQ(error, "intentional validation failure");
    EXPECT_EQ(ReadText(target), "old-authoritative-state");
    EXPECT_EQ(FileCount(directory.Path()), 1U);
}

TEST(FileTransactionTests, SuccessfulValidationReplacesTargetAndLeavesNoTemporaryFile)
{
    TemporaryDirectory directory{};
    const std::filesystem::path target = directory.Path() / "slot.save";
    WriteText(target, "old-authoritative-state");

    std::string error{};
    const bool committed = trace2d::core::CommitTextFileAtomically(
        target,
        "new-authoritative-state",
        "test save",
        error,
        [](const std::filesystem::path& temporary, std::string& validationError)
        {
            static_cast<void>(validationError);
            return ReadText(temporary) == "new-authoritative-state";
        });

    EXPECT_TRUE(committed) << error;
    EXPECT_EQ(ReadText(target), "new-authoritative-state");
    EXPECT_EQ(FileCount(directory.Path()), 1U);
}
