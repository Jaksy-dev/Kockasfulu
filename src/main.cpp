#include <iostream>
#include <string>
#include <iosfwd>
#include <limits.h>
#include <random>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <memory>

#include "chess.hpp"

using namespace chess;

constexpr auto DRAW_SCORE = 0;
constexpr auto INF = INT_MAX;
constexpr auto DEPTH = 8; // half-moves

constexpr auto STARTER_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct BestMove
{
    Move move;
    int eval;
};

enum class EntryFlag
{
    EXACT,
    LOWER_BOUND,
    UPPER_BOUND
};

struct TTEntry
{
    BestMove bestmove;
    int depth;
    EntryFlag flag;
    // is_valid???
};

static Board current_board = Board(STARTER_FEN);

static std::unordered_map<uint64_t, TTEntry> transposition_table;

static std::mt19937 rng(std::random_device{}());

std::vector<std::string> split_by_space(const std::string &input)
{
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string word;
    while (iss >> word)
    {
        tokens.push_back(word);
    }
    return tokens;
}

int evaluate(const Board &board)
{
    int score = 0;
    score += board.pieces(PieceType::PAWN, Color::WHITE).count();
    score += 3 * (board.pieces(PieceType::BISHOP, Color::WHITE) | board.pieces(PieceType::KNIGHT, Color::WHITE)).count();
    score += 5 * board.pieces(PieceType::ROOK, Color::WHITE).count();
    score += 9 * board.pieces(PieceType::QUEEN, Color::WHITE).count();

    score -= board.pieces(PieceType::PAWN, Color::BLACK).count();
    score -= 3 * (board.pieces(PieceType::BISHOP, Color::BLACK) | board.pieces(PieceType::KNIGHT, Color::BLACK)).count();
    score -= 5 * board.pieces(PieceType::ROOK, Color::BLACK).count();
    score -= 9 * board.pieces(PieceType::QUEEN, Color::BLACK).count();

    return score * 100;
}

BestMove negamax(Board &board, int depth, int alpha, int beta)
{
    const int alphaOrig = alpha;

    // Transposition table lookup
    const auto hash = board.hash();
    auto tt_it = transposition_table.find(hash);
    if (tt_it != transposition_table.end() && tt_it->second.depth >= depth)
    {
        const auto &entry = tt_it->second;
        if (entry.flag == EntryFlag::EXACT)
        {
            return entry.bestmove;
        }
        else if (entry.flag == EntryFlag::LOWER_BOUND && entry.bestmove.eval >= beta)
        {
            return entry.bestmove;
        }
        else if (entry.flag == EntryFlag::UPPER_BOUND && entry.bestmove.eval <= alpha)
        {
            return entry.bestmove;
        }
    }

    // Terminal conditions
    if (board.isHalfMoveDraw() || board.isRepetition())
    {
        return {.move = Move::NO_MOVE, .eval = DRAW_SCORE};
    }

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty())
    {
        return {.move = Move::NO_MOVE, .eval = board.inCheck() ? -INF : DRAW_SCORE};
    }

    // Base case - return evaluation
    if (depth == 0)
    {
        return {.move = Move::NO_MOVE, .eval = evaluate(board)};
    }

    std::shuffle(moves.begin(), moves.end(), rng);

    int value = INT_MIN;
    Move bestMove = moves.front();

    for (const auto &move : moves)
    {
        board.makeMove(move);
        BestMove childResult = negamax(board, depth - 1, -beta, -alpha);
        int childValue = -childResult.eval; // Negate the child's evaluation
        board.unmakeMove(move);

        if (childValue > value)
        {
            value = childValue;
            bestMove = move;
        }

        alpha = std::max(alpha, value);
        if (alpha >= beta)
        {
            break; // Beta cutoff
        }
    }

    // Store in transposition table
    TTEntry entry;
    entry.bestmove = {.move = bestMove, .eval = value};
    entry.depth = depth;
    if (value <= alphaOrig)
    {
        entry.flag = EntryFlag::UPPER_BOUND;
    }
    else if (value >= beta)
    {
        entry.flag = EntryFlag::LOWER_BOUND;
    }
    else
    {
        entry.flag = EntryFlag::EXACT;
    }
    transposition_table[hash] = entry;

    return {.move = bestMove, .eval = value};
}

void bestMove()
{
    const auto starttime = std::chrono::high_resolution_clock::now();
    const auto bestmove = negamax(current_board, DEPTH, INT_MIN, INT_MAX);
    std::cout << "info score cp " << bestmove.eval << " time " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - starttime).count() << "\n";
    std::cout << "bestmove " << uci::moveToUci(bestmove.move) << "\n";
}

void parseCommand(const std::string &input)
{
    auto commands = split_by_space(input);
    auto main_command = commands.front();
    if (main_command == "uci")
    {
        std::cout << "id name kockasfulu\n";
        std::cout << "uciok\n";
    }
    if (main_command == "isready")
    {
        std::cout << "readyok\n";
    }
    if (main_command == "ucinewgame")
    {
        current_board = Board(STARTER_FEN);
    }
    if (main_command == "position")
    {
        if (commands[1] == "fen")
        {
            current_board = Board(commands[2] + " " + commands[3] + " " + commands[4] + " " + commands[5] + " " + commands[6]);
            if (commands.size() > 7)
            {
                for (auto it = commands.begin() + 9; it != commands.end(); ++it)
                {
                    auto move = uci::uciToMove(current_board, *it);
                    current_board.makeMove(move);
                }
            }
        }
        else if (commands[1] == "startpos")
        {
            current_board = Board(STARTER_FEN);
            if (commands.size() > 2)
            {
                for (auto it = commands.begin() + 3; it != commands.end(); ++it)
                {
                    auto move = uci::uciToMove(current_board, *it);
                    current_board.makeMove(move);
                }
            }
        }
    }
    if (main_command == "moves")
    {
        for (auto it = commands.begin(); it != commands.end(); ++it)
        {
            current_board.makeMove(uci::uciToMove(current_board, *it));
        }
    }

    if (main_command == "go")
    {
        bestMove();
    }
    if (main_command == "stop")
    {
        bestMove();
    }
    if (main_command == "quit")
    {
        exit(0);
    }
}

int main()
{
    while (true)
    {
        std::string command;
        std::getline(std::cin, command);
        if (!command.empty())
        {
            parseCommand(command);
        }
    }
}
