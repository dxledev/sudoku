/**
 * @file puzzle.cpp
 * @brief Sudoku constraint checks, solving helpers, and randomized generation.
 *
 * Candidate digits are represented by bits. The bounded solution counter is
 * used to prove uniqueness, while generation retries randomized clue removal
 * until the requested clue band and tier-specific logical constraints hold.
 */
#include "puzzle.h"

#include <algorithm>
#include <bit>
#include <numeric>
#include <stdexcept>

namespace sudoku {
namespace {

constexpr unsigned allDigits = 0x3fe;

int boxOf(int index) {
    return (index / 27) * 3 + (index % 9) / 3;
}

class Solver {
public:
    explicit Solver(const Grid &source) : grid(source) {
        for (int index = 0; index < 81; ++index) {
            if (grid[index])
                mark(index, grid[index]);
        }
    }

    int count(int limit, std::mt19937 *random = nullptr) {
        int index = -1;
        unsigned available = 0;
        int smallest = 10;
        for (int cell = 0; cell < 81; ++cell) {
            if (grid[cell])
                continue;
            const unsigned options = allDigits & ~(rows[cell / 9] | columns[cell % 9] | boxes[boxOf(cell)]);
            const int size = std::popcount(options);
            if (!size)
                return 0;
            if (size < smallest) {
                index = cell;
                available = options;
                smallest = size;
                if (size == 1)
                    break;
            }
        }
        if (index < 0) {
            solution = grid;
            return 1;
        }
        std::array<int, 9> digits{};
        int size = 0;
        for (int digit = 1; digit <= 9; ++digit) {
            if (available & (1u << digit))
                digits[size++] = digit;
        }
        if (random)
            std::shuffle(digits.begin(), digits.begin() + size, *random);
        int found = 0;
        for (int choice = 0; choice < size && found < limit; ++choice) {
            const int digit = digits[choice];
            grid[index] = digit;
            mark(index, digit);
            found += count(limit - found, random);
            unmark(index, digit);
            grid[index] = 0;
        }
        return found;
    }

    Grid solution{};

private:
    Grid grid;
    std::array<unsigned, 9> rows{}, columns{}, boxes{};

    void mark(int index, int digit) {
        const unsigned bit = 1u << digit;
        rows[index / 9] |= bit;
        columns[index % 9] |= bit;
        boxes[boxOf(index)] |= bit;
    }

    void unmark(int index, int digit) {
        const unsigned bit = ~(1u << digit);
        rows[index / 9] &= bit;
        columns[index % 9] &= bit;
        boxes[boxOf(index)] &= bit;
    }
};

bool fillHiddenSingle(Grid &grid) {
    for (int kind = 0; kind < 3; ++kind) {
        for (int unit = 0; unit < 9; ++unit) {
            for (int digit = 1; digit <= 9; ++digit) {
                int found = -1;
                int count = 0;
                for (int offset = 0; offset < 9; ++offset) {
                    const int index = kind == 0 ? unit * 9 + offset
                        : kind == 1 ? offset * 9 + unit
                        : (unit / 3 * 3 + offset / 3) * 9 + unit % 3 * 3 + offset % 3;
                    if (!grid[index] && (candidates(grid, index) & (1u << digit))) {
                        found = index;
                        ++count;
                    }
                }
                if (count == 1) {
                    grid[found] = digit;
                    return true;
                }
            }
        }
    }
    return false;
}

}

const DifficultyInfo &difficultyInfo(Difficulty difficulty) {
    return difficulties.at(static_cast<std::size_t>(difficulty));
}

Difficulty parseDifficulty(std::string_view name) {
    for (const auto &info : difficulties) {
        if (info.name == name)
            return info.value;
    }
    throw std::invalid_argument("Difficulty must be easy, medium, hard, or expert");
}

bool arePeers(int first, int second) {
    return first != second && (first / 9 == second / 9 || first % 9 == second % 9 || boxOf(first) == boxOf(second));
}

std::uint16_t candidates(const Grid &grid, int index) {
    if (grid[index])
        return 0;
    unsigned mask = allDigits;
    for (int other = 0; other < 81; ++other) {
        if (grid[other] && arePeers(index, other))
            mask &= ~(1u << grid[other]);
    }
    return static_cast<std::uint16_t>(mask);
}

bool isValid(const Grid &grid, bool requireComplete) {
    for (int index = 0; index < 81; ++index) {
        if (grid[index] < (requireComplete ? 1 : 0) || grid[index] > 9)
            return false;
        if (!grid[index])
            continue;
        for (int other = 0; other < index; ++other) {
            if (grid[index] == grid[other] && arePeers(index, other))
                return false;
        }
    }
    return true;
}

int countSolutions(const Grid &grid, int limit) {
    if (limit < 1 || !isValid(grid))
        return 0;
    return Solver(grid).count(limit);
}

bool solveSingles(Grid &grid) {
    if (!isValid(grid))
        return false;
    while (true) {
        bool changed = false;
        for (int index = 0; index < 81; ++index) {
            if (grid[index])
                continue;
            const unsigned mask = candidates(grid, index);
            if (!mask)
                return false;
            if (std::popcount(mask) == 1) {
                grid[index] = std::countr_zero(mask);
                changed = true;
            }
        }
        if (!changed)
            changed = fillHiddenSingle(grid);
        if (!changed)
            return isValid(grid, true);
    }
}

Puzzle generatePuzzle(Difficulty difficulty, std::mt19937 &random, const std::function<bool()> &cancelled) {
    const auto &info = difficultyInfo(difficulty);
    for (int attempt = 0; attempt < 16; ++attempt) {
        Solver solver(Grid{});
        solver.count(1, &random);
        Puzzle puzzle{solver.solution, solver.solution, difficulty};
        std::array<int, 81> order{};
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), random);
        int clues = 81;
        for (int index : order) {
            if (cancelled && cancelled())
                throw std::runtime_error("Generation cancelled");
            const int previous = puzzle.clues[index];
            puzzle.clues[index] = 0;
            bool accepted = countSolutions(puzzle.clues) == 1;
            if (accepted && difficulty == Difficulty::Easy) {
                auto trial = puzzle.clues;
                accepted = solveSingles(trial);
            }
            if (accepted)
                --clues;
            else
                puzzle.clues[index] = previous;
            if (clues == info.targetClues)
                break;
        }
        if (clues <= info.maximumClues) {
            if (difficulty == Difficulty::Expert) {
                auto trial = puzzle.clues;
                if (solveSingles(trial))
                    continue;
            }
            return puzzle;
        }
    }
    throw std::runtime_error("Could not create a puzzle at this difficulty. Please try again.");
}

}
