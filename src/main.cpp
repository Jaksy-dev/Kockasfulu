#include <iostream>
#include <string>
#include <iosfwd>
#include "chess.hpp"

using namespace chess;

constexpr auto STARTER_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static Board current_board = Board(STARTER_FEN);

std::vector<std::string> split_by_space(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string word;
    while (iss >> word) {
        tokens.push_back(word);
    }
    return tokens;
}

bool isOver(const Board& board, const Movelist& moves){
    if (board.isHalfMoveDraw()){
        // return board.getHalfMoveDrawType().first == GameResultReason::CHECKMATE ? 
        //     MATED_SCORE : DRAW_SCORE;
        return true;
    }

    if (board.isRepetition()){
        //return DRAW_SCORE;
        return true;
    }

    // no moves means game over
    if (moves.empty()){
        //return board.inCheck() ? MATED_SCORE : DRAW_SCORE;
        return true;
    }
    return false;
}

void bestMove(){
        Movelist moves;
        movegen::legalmoves(moves, current_board);
        if (isOver(current_board, moves)){
            return;
        }
        // pick the first legal move
        std::cout << "bestmove " << uci::moveToUci(moves.front()) << "\n";
}

void parseCommand(const std::string & input){
    auto commands = split_by_space(input);
    auto main_command = commands.front();
    if (main_command == "uci") {
        std::cout << "id name kockasfulu\n";
        std::cout << "uciok\n";
    }
    if (main_command == "isready"){
        std::cout << "readyok\n";
    }
    if (main_command == "ucinewgame"){
        current_board = Board(STARTER_FEN);
    }
    if (main_command == "position"){
        if (commands[1] == "fen"){
            // fenstring is 6 segments long
            current_board = Board(commands[2] + commands[3] + commands[4] + commands[5] + commands[6]);
if (commands.size() > 7){
            for (auto it = commands.begin() + 9; it != commands.end(); ++it){

                current_board.makeMove(uci::uciToMove(current_board, *it));
            }
		}
			
        }
        else if (commands[1] == "startpos"){
            current_board = Board(STARTER_FEN);
			if (commands.size() > 2){
            for (auto it = commands.begin() + 3; it != commands.end(); ++it){
                current_board.makeMove(uci::uciToMove(current_board, *it));
            }
		}
        }
    }
    if (main_command == "moves"){
        for (auto it = commands.begin(); it != commands.end(); ++it){
                current_board.makeMove(uci::uciToMove(current_board, *it));
            }
    }

    if (main_command == "go"){
        bestMove();
    }
    if (main_command == "stop"){
        bestMove();
    }
    if (main_command == "quit"){
        exit(0);
    }
}





int main() {

    while (true){
        std::string command;
        std::getline(std::cin, command);
        if (!command.empty()) {
            parseCommand(command);
        }
    }
}
