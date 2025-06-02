#include <iostream>
#include "chess.hpp"

using namespace chess;

enum class result {MATED, DRAW};
#define MATED_SCORE 1
#define DRAW_SCORE 2

int main() {
    std::cout << "kockasfulu\n";

    auto board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    if (board.isHalfMoveDraw()){
        return board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE ? 
            MATED_SCORE : DRAW_SCORE;
    }

    if (board.isRepetition()){
        return DRAW_SCORE;
    }


    Movelist moves;
    movegen::legalmoves(moves, board);

    for (const auto& move : moves) {
        std::cout << uci::moveToUci(move) << ' ';
    }

    // no moves means game over
    if (moves.empty()){
        return board.inCheck() ? MATED_SCORE : DRAW_SCORE;
    }
}
