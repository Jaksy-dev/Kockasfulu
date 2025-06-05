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
constexpr auto DEPTH = 6; // half-moves

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
};

static Board current_board = Board(STARTER_FEN);

std::unique_ptr<std::unordered_map<uint64_t, TTEntry>> transposition_table;

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
    if (board.isHalfMoveDraw()){
        return board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE ? -INF : DRAW_SCORE;
    }

    if (board.isRepetition()){
        return DRAW_SCORE;
    }

    Movelist moves;
    movegen::legalmoves(moves, board);

    // no moves means game over
    if (moves.empty()){
        return board.inCheck() ? -INF : DRAW_SCORE;
    }

    int score = 0;

    score += board.pieces(PieceType::PAWN, Color::WHITE).count() * 100;
    score += board.pieces(PieceType::KNIGHT, Color::WHITE).count() * 300;
    score += board.pieces(PieceType::BISHOP, Color::WHITE).count() * 300;
    score += board.pieces(PieceType::ROOK, Color::WHITE).count() * 500;
    score += board.pieces(PieceType::QUEEN, Color::WHITE).count() * 900;

    score -= board.pieces(PieceType::PAWN, Color::BLACK).count() * 100;
    score -= board.pieces(PieceType::KNIGHT, Color::BLACK).count() * 300;
    score -= board.pieces(PieceType::BISHOP, Color::BLACK).count() * 300;
    score -= board.pieces(PieceType::ROOK, Color::BLACK).count() * 500;
    score -= board.pieces(PieceType::QUEEN, Color::BLACK).count() * 900;

    return score;
}

int negamax(Board &board, int depth, int alpha, int beta)
{
    if (depth == 0)
    {
        return board.sideToMove() == Color::WHITE ? evaluate(board) : -evaluate(board);
    }

    int max = -INF;
    Movelist moves;
    movegen::legalmoves(moves, board);

    for (const auto &move : moves)
    {
        board.makeMove(move);
        int score = -negamax(board, depth - 1, -beta, -alpha);
        board.unmakeMove(move);

        if (score > max)
        {
            max = score;
        }
        if (score > alpha)
        {
            alpha = score;
        }
        if (alpha >= beta)
        {
            break; // Beta cutoff
        }
    }
    return max;
}

BestMove findBestMove(Board &board, int depth)
{
    Movelist moves;
    movegen::legalmoves(moves, board);
    Move bestMove = moves.front();

    std::shuffle(moves.begin(), moves.end(), rng);

    int bestValue = -INF;
    int alpha = -INF+1;
    int beta = INF;

    for (const auto &move : moves)
    {
        board.makeMove(move);
        int moveValue = -negamax(board, depth - 1, -beta, -alpha);
        board.unmakeMove(move);
        if (moveValue > bestValue)
        {
            bestValue = moveValue;
            bestMove = move;
        }
        if (moveValue > alpha)
        {
            alpha = moveValue;
        }
    }

    return {.move = bestMove, .eval = bestValue};
}

void bestMove()
{
    const auto starttime = std::chrono::high_resolution_clock::now();
    const auto bestmove = findBestMove(current_board, DEPTH);
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
    transposition_table = std::make_unique<std::unordered_map<uint64_t, TTEntry>>();
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
