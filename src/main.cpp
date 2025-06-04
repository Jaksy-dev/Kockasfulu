#include <iostream>
#include <string>
#include <iosfwd>
#include <limits.h>
#include <random>
#include <algorithm>
#include <random>
#include <chrono>

#include "chess.hpp"

using namespace chess;

constexpr auto DRAW_SCORE = 0;
constexpr auto WHITE_WIN = INT_MAX;
constexpr auto BLACK_WIN = INT_MIN;

constexpr auto WHITE_PAWN_VALUE = 1;
constexpr auto WHITE_KNIGHT_VALUE = 3;
constexpr auto WHITE_BISHOP_VALUE = 3;

constexpr auto STARTER_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct BestMove
{
    Move move;
    int eval;
};

static Board current_board = Board(STARTER_FEN);
static std::string current_pgn{};
static std::default_random_engine rng(std::random_device{}());

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
    // Returns the piece material difference in centipawns.

    int score = 0;
    score += board.pieces(PieceType::PAWN, Color::WHITE).count();
    score += 3 * board.pieces(PieceType::BISHOP, Color::WHITE).count();
    score += 3 * board.pieces(PieceType::KNIGHT, Color::WHITE).count();
    score += 5 * board.pieces(PieceType::ROOK, Color::WHITE).count();
    score += 9 * board.pieces(PieceType::QUEEN, Color::WHITE).count();

    score -= board.pieces(PieceType::PAWN, Color::BLACK).count();
    score -= 3 * board.pieces(PieceType::BISHOP, Color::BLACK).count();
    score -= 3 * board.pieces(PieceType::KNIGHT, Color::BLACK).count();
    score -= 5 * board.pieces(PieceType::ROOK, Color::BLACK).count();
    score -= 9 * board.pieces(PieceType::QUEEN, Color::BLACK).count();

    return score * 100;
}

BestMove alphabeta(Board board, int depth, int alpha, int beta, Color current_player)
{
    Movelist moves;
    movegen::legalmoves(moves, board);

    std::shuffle(moves.begin(), moves.end(), rng);

    auto side_to_move = board.sideToMove();

    if (board.isHalfMoveDraw())
    {
        // 50 move rule
        // I'm really not sure who is supposed to be the winner in the case of this ::checkmate. TODO investigate.
        return {.move = Move::NO_MOVE,
                .eval = board.getHalfMoveDrawType()
                                    .first == GameResultReason::CHECKMATE
                            ? (side_to_move == Color::BLACK ? WHITE_WIN : BLACK_WIN)
                            : DRAW_SCORE} /*50 move rule*/;
    }

    if (board.isRepetition())
    {
        /*threefold*/
        return {
            .move = Move::NO_MOVE,
            .eval = DRAW_SCORE};
    }
    // no moves means game over
    if (moves.empty())
    {
        return {.move = Move::NO_MOVE, .eval = board.inCheck() ? (side_to_move == Color::BLACK ? WHITE_WIN : BLACK_WIN) /*checkmate*/ : DRAW_SCORE /*stalemate*/};
    }

    if (depth == 0)
    {
        if (current_player == Color::WHITE)
        {
            BestMove bestmove{.move = moves.front(), .eval = INT_MIN};

            for (auto &&move : moves)
            {
                Board newboard = board;
                newboard.makeMove(move);
                int eval = evaluate(newboard);
                if (eval > bestmove.eval)
                {
                    bestmove.move = move;
                    bestmove.eval = eval;
                }
            }
            return bestmove;
        }
        else
        {
            BestMove bestmove{.move = moves.front(), .eval = INT_MAX};

            for (auto &&move : moves)
            {
                Board newboard = board;
                newboard.makeMove(move);
                int eval = evaluate(newboard);
                if (eval < bestmove.eval)
                {
                    bestmove.move = move;
                    bestmove.eval = eval;
                }
            }
            return bestmove;
        }
    }

    if (current_player == Color::WHITE)
    {
        BestMove bestmove{.move = moves.front(), .eval = INT_MIN};

        for (auto &&move : moves)
        {
            Board newboard = board;
            newboard.makeMove(move);
            BestMove candidatemove = alphabeta(newboard, depth - 1, alpha, beta, Color::BLACK);
            if (candidatemove.eval > bestmove.eval)
            {
                bestmove.move = move;
                bestmove.eval = candidatemove.eval;
            }
            if (candidatemove.eval >= beta){
                break;
            }
            alpha = std::max(alpha, candidatemove.eval);
        }
        return bestmove;
    }
    else
    {
        BestMove bestmove{.move = moves.front(), .eval = INT_MAX};

        for (auto &&move : moves)
        {
            Board newboard = board;
            newboard.makeMove(move);
            BestMove candidatemove = alphabeta(newboard, depth - 1, alpha, beta, Color::WHITE);
            if (candidatemove.eval < bestmove.eval)
            {
                bestmove.move = move;
                bestmove.eval = candidatemove.eval;
            }
            if (candidatemove.eval <= alpha)
            {
                break;
            }
            beta = std::min(beta, candidatemove.eval);
        }
        return bestmove;
    }
}

void bestMove()
{
    auto bestmove = alphabeta(current_board, 4, INT_MIN, INT_MAX, current_board.sideToMove());
    std::cout << "info score cp " << bestmove.eval << "\n";
    std::cout << "bestmove " << uci::moveToUci(bestmove.move) << "\n";
}

void parseCommand(const std::string &input)
{
    // assumes the GUI does not send random thrash input
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
            // fenstring is 6 segments long
            current_board = Board(commands[2] + " " + commands[3] + " " + commands[4] + " " + commands[5] + " " + commands[6]);
            if (commands.size() > 7)
            // moves are inputted after fenstring
            {
                for (auto it = commands.begin() + 9; it != commands.end(); ++it)
                {
                    auto move = uci::uciToMove(current_board, *it);
                    current_pgn.append(uci::moveToSan(current_board, move));
                    current_pgn.append(" ");
                    current_board.makeMove(move);
                }
            }
        }
        else if (commands[1] == "startpos")
        {
            current_board = Board(STARTER_FEN);
            current_pgn = "";
            if (commands.size() > 2)
            // moves are inputted after "startpos"
            {
                for (auto it = commands.begin() + 3; it != commands.end(); ++it)
                {
                    auto move = uci::uciToMove(current_board, *it);
                    current_pgn.append(uci::moveToSan(current_board, move)).append(" ");
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
        std::cout << current_pgn << "\n";
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
