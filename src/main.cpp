#include <iostream>
#include <string>
#include <iosfwd>
#include <limits.h>
#include <random>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <numeric>

#include "chess.hpp"

using namespace chess;

constexpr auto DRAW_SCORE = 0;
constexpr auto INF = INT_MAX;
constexpr auto DEPTH = 10; // half-moves

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
    int eval;
    int depth;
    EntryFlag flag;
};

static Board current_board = Board(STARTER_FEN);

std::unique_ptr<std::unordered_map<uint64_t, TTEntry>> transposition_table;

static std::mt19937 rng(std::random_device{}());

static std::vector<int64_t> movetimes;

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

int evaluate(Board &board, const Movelist &moves)
{
    //todo: make it evaluate from sidetomove perspective
    if (board.isHalfMoveDraw())
    {
        return board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE ? -INF : DRAW_SCORE;
    }

    if (board.isRepetition())
    {
        return DRAW_SCORE;
    }

    // no moves means game over
    if (moves.empty())
    {
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

    // doubled pawns (only immediately doubled)
    auto w_pawns = board.pieces(PieceType::PAWN, Color::WHITE);
    auto w_pawns_north = w_pawns << static_cast<uint8_t>(Direction::NORTH);
    auto w_doubled = w_pawns & w_pawns_north;

    auto b_pawns = board.pieces(PieceType::PAWN, Color::BLACK);
    auto b_pawns_north = b_pawns << static_cast<uint8_t>(Direction::SOUTH);
    auto b_doubled = b_pawns & b_pawns_north;

    score -= w_doubled.count() * 50;
    score += b_doubled.count() * 50;

    // blocked pawns

    // isolated pawns

    // number of legal moves
    board.makeNullMove(); // I guess this is flawed too...
    Movelist their_moves;
    movegen::legalmoves(their_moves, board);
    board.unmakeNullMove();

    auto moves_difference = moves.size() - their_moves.size();
    board.sideToMove() == Color::WHITE ? score += moves_difference * 10 : score -= moves_difference * 10;

    return score;
}

int negamax(Board &board, int depth, int alpha, int beta, std::chrono::time_point<std::chrono::high_resolution_clock> time_limit)
{
    int alphaOrig = alpha;

    // (* Transposition Table Lookup; node is the lookup key for ttEntry *)
    // ttEntry := transpositionTableLookup(node)
    // if ttEntry.is_valid and ttEntry.depth ≥ depth then
    //     if ttEntry.flag = EXACT then
    //         return ttEntry.value
    //     else if ttEntry.flag = LOWERBOUND and ttEntry.value ≥ beta then
    //         return ttEntry.value
    //     else if ttEntry.flag = UPPERBOUND and ttEntry.value ≤ alpha then
    //         return ttEntry.value
    const auto hash = board.hash();
    const auto it = transposition_table->find(hash);
    if (it != transposition_table->end() && it->second.depth >= depth)
    {
        auto &tt_entry = it->second;

        if (tt_entry.flag == EntryFlag::EXACT)
        {
            return tt_entry.eval;
        }
        else if (tt_entry.flag == EntryFlag::LOWER_BOUND && tt_entry.eval >= beta)
        {
            return tt_entry.eval;
        }
        else if (tt_entry.flag == EntryFlag::UPPER_BOUND && tt_entry.eval <= alpha)
        {
            return tt_entry.eval;
        }
    }

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (depth == 0)
    {
        return board.sideToMove() == Color::WHITE ? evaluate(board, moves) : -evaluate(board, moves);
    }


    int max = -INF;

    for (const auto &move : moves)
    {
        board.makeMove(move);
        int score = -negamax(board, depth - 1, -beta, -alpha, time_limit);
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

    // (* Transposition Table Store; node is the lookup key for ttEntry *)
    // ttEntry.value := value
    // if value ≤ alphaOrig then
    //     ttEntry.flag := UPPERBOUND
    // else if value ≥ β then
    //     ttEntry.flag := LOWERBOUND
    // else
    //     ttEntry.flag := EXACT
    // ttEntry.depth := depth
    // ttEntry.is_valid := true
    // transpositionTableStore(node, ttEntry)

    if ((it != transposition_table->end() && it->second.depth < depth) || it == transposition_table->end()){ //is this if needed? investigate
        TTEntry tt_entry;
        tt_entry.eval = max;
        if (tt_entry.eval <= alphaOrig)
        {
            tt_entry.flag = EntryFlag::UPPER_BOUND;
        }
        else if (tt_entry.eval >= beta)
        {
            tt_entry.flag = EntryFlag::LOWER_BOUND;
        }
        else
        {
            tt_entry.flag = EntryFlag::EXACT;
        }
        tt_entry.depth = depth;
    
        (*transposition_table)[hash] = tt_entry;
    }

    return max;
}

BestMove findBestMove(Board &board, int depth, int time, int inc)
{
    using namespace std::literals;
    auto get_time_limit = [](int time, int inc)
    { return std::chrono::milliseconds{time / 20 + inc / 2}; };

    Movelist moves;
    movegen::legalmoves(moves, board);
    
    Move bestMove = moves.front();

    // std::shuffle(moves.begin(), moves.end(), rng);

    const auto time_limit = std::chrono::high_resolution_clock::now() + get_time_limit(time, inc);
    auto iter = 1;
    int bestValue;

    do
    {

        bestValue = -INF;
        int alpha = -INF + 1;
        int beta = INF;

        for (const auto &move : moves)
        {
            board.makeMove(move);
            int moveValue = -negamax(board, iter - 1, -beta, -alpha, time_limit);
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
            if (alpha >= beta)
            {
                break;
            }
        }
        std::cout << "depth " << iter++ << " currmove " << uci::moveToUci(bestMove) <<"\n";
    } while (time_limit > std::chrono::high_resolution_clock::now() && iter <= depth);

    return {.move = bestMove, .eval = bestValue};
}

void go(int wtime, int btime, int winc, int binc)
{
    const auto side_to_move = current_board.sideToMove();
    const auto starttime = std::chrono::high_resolution_clock::now();
    const auto bestmove = findBestMove(current_board, DEPTH, side_to_move == Color::WHITE ? wtime : btime, side_to_move == Color::WHITE ? winc : binc);
    const auto duration = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - starttime)).count();
    std::cout << "info score cp " << bestmove.eval << " time " << duration << "\n";
    std::cout << "bestmove " << uci::moveToUci(bestmove.move) << "\n";
    movetimes.push_back(duration);
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
        if (commands[1] == "infinite"){
            go(INF, INF, INF, INF);
        }

        go(std::stoi(commands[2]) /*wtime*/, std::stoi(commands[4]) /*btime*/, std::stoi(commands[6]) /*winc*/, std::stoi(commands[8]) /*binc*/);
    }
    if (main_command == "stop")
    {
        
    }
    if (main_command == "quit")
    {
        std::cout << std::accumulate(movetimes.begin(), movetimes.end(), 0) / movetimes.size() << "\n";
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
