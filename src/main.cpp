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
constexpr auto WHITE_WIN = INT_MAX;
constexpr auto BLACK_WIN = INT_MIN;

constexpr auto STARTER_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct BestMove
{
    Move move;
    int eval;
};

struct TTEntry
{
    BestMove bestmove;
    int depth;
};

static Board current_board = Board(STARTER_FEN);

static std::unordered_map<uint64_t, TTEntry> transposition_table;

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

BestMove alphabeta(Board &board, int depth, int alpha, int beta, Color current_player)
{
    Movelist moves;
    movegen::legalmoves(moves, board);

    std::random_shuffle(moves.begin(), moves.end());

    auto side_to_move = board.sideToMove();

    if (board.isHalfMoveDraw())
    {
        return {.move = Move::NO_MOVE,
                .eval = board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE
                            ? (side_to_move == Color::BLACK ? WHITE_WIN : BLACK_WIN)
                            : DRAW_SCORE};
    }

    if (board.isRepetition())
    {
        return {.move = Move::NO_MOVE, .eval = DRAW_SCORE};
    }

    if (moves.empty())
    {
        return {.move = Move::NO_MOVE, .eval = board.inCheck() ? (side_to_move == Color::BLACK ? WHITE_WIN : BLACK_WIN) : DRAW_SCORE};
    }

    // Transposition table probe
    const auto hash = board.hash();
    auto tt_it = transposition_table.find(hash);
    if (tt_it != transposition_table.end() && tt_it->second.depth >= depth)
    {
        return tt_it->second.bestmove;
    }

    // Base case
    if (depth == 0)
    {
        BestMove bestmove;
        if (current_player == Color::WHITE)
        {
            bestmove = {.move = moves.front(), .eval = INT_MIN};
            for (const auto &move : moves)
            {
                board.makeMove(move);
                int eval = evaluate(board);
                if (eval > bestmove.eval)
                {
                    bestmove.move = move;
                    bestmove.eval = eval;
                }
                board.unmakeMove(move);
            }
        }
        else
        {
            bestmove = {.move = moves.front(), .eval = INT_MAX};
            for (const auto &move : moves)
            {
                board.makeMove(move);
                int eval = evaluate(board);
                if (eval < bestmove.eval)
                {
                    bestmove.move = move;
                    bestmove.eval = eval;
                }
                board.unmakeMove(move);
            }
        }
        // Store in TT
        TTEntry entry = {bestmove, depth};
        if (tt_it == transposition_table.end() || tt_it->second.depth <= depth)
        {
            transposition_table[hash] = entry;
        }
        return bestmove;
    }

    // Recursive case
    BestMove bestmove;
    if (current_player == Color::WHITE)
    {
        bestmove = {.move = moves.front(), .eval = INT_MIN};
        for (const auto &move : moves)
        {
            board.makeMove(move);
            BestMove candidatemove = alphabeta(board, depth - 1, alpha, beta, Color::BLACK);
            if (candidatemove.eval > bestmove.eval)
            {
                bestmove.move = move;
                bestmove.eval = candidatemove.eval;
            }
            board.unmakeMove(move);
            if (candidatemove.eval >= beta)
            {
                break;
            }
            alpha = std::max(alpha, candidatemove.eval);
        }
    }
    else
    {
        bestmove = {.move = moves.front(), .eval = INT_MAX};
        for (const auto &move : moves)
        {
            board.makeMove(move);
            BestMove candidatemove = alphabeta(board, depth - 1, alpha, beta, Color::WHITE);
            if (candidatemove.eval < bestmove.eval)
            {
                bestmove.move = move;
                bestmove.eval = candidatemove.eval;
            }
            board.unmakeMove(move);
            if (candidatemove.eval <= alpha)
            {
                break;
            }
            beta = std::min(beta, candidatemove.eval);
        }
    }

    // Store in TT
    TTEntry entry = {bestmove, depth};
    if (tt_it == transposition_table.end() || tt_it->second.depth <= depth)
    {
        transposition_table[hash] = entry;
    }

    return bestmove;
}

void bestMove()
{
    const auto starttime = std::chrono::high_resolution_clock::now();
    const auto bestmove = alphabeta(current_board, 6, INT_MIN, INT_MAX, current_board.sideToMove());
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
