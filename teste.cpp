#include <torch/torch.h>
#include <torch/script.h>
#include <iostream>
using namespace std;

#include <string>
#include <unordered_map>
#include <sstream>
#include <cctype>
#include <stdexcept>

// 
const std::unordered_map<char, int> PIECE_INDEX = {
    {'P', 0}, {'N', 1}, {'B', 2}, {'R', 3}, {'Q', 4}, {'K', 5},
    {'p', 6}, {'n', 7}, {'b', 8}, {'r', 9}, {'q', 10}, {'k', 11}
};

torch::Tensor fen_to_features(const std::string& fen) {

    // Separa a FEN em partes (board, side_to_move, ...)
    std::istringstream iss(fen);
    std::string board, side_to_move;
    iss >> board >> side_to_move;

    torch::Tensor features = torch::zeros({769}, torch::kFloat32);

    // Acesso direto aos dados do tensor
    auto features_acc = features.accessor<float, 1>();

    int rank = 7;
    int file = 0;

    for (char c : board) {

        if (c == '/') {
            rank -= 1;
            file = 0;
        }
        else if (std::isdigit(static_cast<unsigned char>(c))) {
            file += (c - '0');
        }
        else {
            auto it = PIECE_INDEX.find(c);
            if (it == PIECE_INDEX.end()) {
                throw std::runtime_error(std::string("Peça inválida na FEN: ") + c);
            }

            int piece_index = it->second;
            int square = rank * 8 + file;
            int feature_index = piece_index * 64 + square;

            features_acc[feature_index] = 1.0f;

            file += 1;
        }
    }

    // Side to move
    if (side_to_move == "w") {
        features_acc[768] = 1.0f;
    }

    return features;
}

int main(int argc, char* argv[]){
    if(argc < 2){
        cerr<< "Missing FileName\n";
        return 1;
    }   

    string filename = argv[1];
    torch::jit::script::Module module;

    try{
        module = torch::jit::load(filename);
        cout << "Model Loaded\n";
    } catch(const c10::Error& e){
        cerr << "Problem Loading Model: " << e.what() << '\n';
        return -1;
    }

    module.eval();
    // Posição de xadrez em FEN
    string fen = "rnbqkbnr/2Kppppp/1p6/1pp5/8/8/8/8 w kq - 1 1";

    torch::Tensor input_tensor = fen_to_features(fen).unsqueeze(0); // shape (1, 769)

    vector<torch::jit::IValue> inputs;
    inputs.push_back(input_tensor);

    torch::NoGradGuard no_grad;
    at::Tensor output = module.forward(inputs).toTensor();

    cout << "Avaliação: " << output.item<float>() << '\n';

    return 0;
}