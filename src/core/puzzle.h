#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <random>
#include <string_view>

namespace sudoku {

using Grid = std::array<int, 81>;
using Notes = std::array<std::uint16_t, 81>;
enum class Difficulty { Easy, Medium, Hard, Expert };

struct DifficultyInfo {
    Difficulty value;
    std::string_view name;
    int targetClues;
    int maximumClues;
};

inline constexpr std::array difficulties{
    DifficultyInfo{Difficulty::Easy, "easy", 44, 46},
    DifficultyInfo{Difficulty::Medium, "medium", 35, 38},
    DifficultyInfo{Difficulty::Hard, "hard", 29, 32},
    DifficultyInfo{Difficulty::Expert, "expert", 24, 26},
};

struct Puzzle {
    Grid clues{};
    Grid solution{};
    Difficulty difficulty = Difficulty::Easy;
};

const DifficultyInfo &difficultyInfo(Difficulty difficulty);
Difficulty parseDifficulty(std::string_view name);
bool arePeers(int first, int second);
std::uint16_t candidates(const Grid &grid, int index);
bool isValid(const Grid &grid, bool requireComplete = false);
int countSolutions(const Grid &grid, int limit = 2);
bool solveSingles(Grid &grid);
Puzzle generatePuzzle(Difficulty difficulty, std::mt19937 &random,
                      const std::function<bool()> &cancelled = {});

}
