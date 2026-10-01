#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <random>
#include <string_view>

namespace sudoku {

/** @brief Row-major 9 by 9 grid; each cell is empty (0) or contains a digit (1-9). */
using Grid = std::array<int, 81>;
/** @brief Per-cell pencil notes encoded as bits 1 through 9. */
using Notes = std::array<std::uint16_t, 81>;
/** @brief Supported clue-density and logical-constraint tiers. */
enum class Difficulty {
    Easy, ///< 44-46 clues and solvable using singles.
    Medium, ///< 35-38 clues.
    Hard, ///< 29-32 clues.
    Expert ///< 24-26 clues and not solvable using singles alone.
};

/** @brief Public generation targets associated with a difficulty tier. */
struct DifficultyInfo {
    Difficulty value; ///< Difficulty represented by this entry.
    std::string_view name; ///< Stable lowercase CLI and persistence name.
    int targetClues; ///< Preferred number of starting clues.
    int maximumClues; ///< Highest allowed number of starting clues.
};

/** @brief Difficulty metadata ordered from easiest to hardest. */
inline constexpr std::array difficulties{
    DifficultyInfo{Difficulty::Easy, "easy", 44, 46},
    DifficultyInfo{Difficulty::Medium, "medium", 35, 38},
    DifficultyInfo{Difficulty::Hard, "hard", 29, 32},
    DifficultyInfo{Difficulty::Expert, "expert", 24, 26},
};

/** @brief A generated or restored puzzle and its known solution. */
struct Puzzle {
    Grid clues{}; ///< Given cells, with zeroes for editable cells.
    Grid solution{}; ///< Complete valid solution corresponding to @ref clues.
    Difficulty difficulty = Difficulty::Easy; ///< Tier used to generate the puzzle.
};

/** @brief Return metadata for a difficulty enum value. */
const DifficultyInfo &difficultyInfo(Difficulty difficulty);
/** @brief Convert a lowercase difficulty name to its enum, throwing if unknown. */
Difficulty parseDifficulty(std::string_view name);
/** @brief Return whether two valid cell indices share a row, column, or 3 by 3 box. */
bool arePeers(int first, int second);
/** @brief Return the bit mask of legal digits for a cell in the supplied grid. */
std::uint16_t candidates(const Grid &grid, int index);
/** @brief Check Sudoku row, column, and box constraints; optionally require all cells. */
bool isValid(const Grid &grid, bool requireComplete = false);
/** @brief Count solutions up to @p limit, allowing callers to stop early. */
int countSolutions(const Grid &grid, int limit = 2);
/** @brief Repeatedly solve naked and hidden singles in place. */
bool solveSingles(Grid &grid);
/**
 * @brief Generate a randomized puzzle meeting the selected tier's constraints.
 * @param difficulty Target clue-density and logical tier.
 * @param random Caller-owned random engine, allowing deterministic generation.
 * @param cancelled Optional cooperative cancellation check used during generation.
 * @return Puzzle with a unique solution; throws if generation is cancelled or fails.
 */
Puzzle generatePuzzle(Difficulty difficulty, std::mt19937 &random,
                      const std::function<bool()> &cancelled = {});

}
