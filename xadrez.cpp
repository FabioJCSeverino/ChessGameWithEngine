#include <bits/stdc++.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <cstdlib>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

//Garante que terminal usa UTF-8
void setupUTF8(){
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        //cores no windows
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #else
        setlocale(LC_ALL, "");          
        setlocale(LC_CTYPE, "C.UTF-8"); 
    #endif
}
#include<filesystem>
namespace fs = std::filesystem;

fs::path somW;
fs::path somB;
fs::path somCheck;
fs::path somCavalo;
fs::path somCaptura;
fs::path somRoque;
fs::path somOver;

void tocarSom(const fs::path& som) {    if (fs::exists(som)) {
        std::string comando = "pw-play \"" + som.string() + "\"";
        std::system(comando.c_str());
    }
}
pair<char,char>tabuleiro[8][8];

vector<vector<int>>material(2,vector<int>(16,1));

int n = 8,numPecas = 12,numDamas = 2;
int lastPawnMove = 0;
bool continua = 1,erro = 0,turno = 1,roque = 0;

bool moveRW = 0,moveRB = 0;
bool moveRTW = 0, moveLTW = 0;
bool moveRTB = 0, moveLTB = 0;
bool castlesW = 0,castlesB = 0;

char pecasEscolhidas;

uint64_t zobristTable[8][8][12];
uint64_t zobristTurn;
uint64_t zobristCastle[4];

void initZobrist(){
    mt19937_64 rng(123456789ULL);
    for(int y=0;y<8;y++)
        for(int x=0;x<8;x++)
            for(int p=0;p<12;p++)
                zobristTable[y][x][p] = rng();
    zobristTurn = rng();
    for(int i=0;i<4;i++) zobristCastle[i] = rng();
}

int pieceIndex(char piece, char color){
    int base = (color == 'W') ? 0 : 6;
    if(piece=='P') return base+0;
    if(piece=='C') return base+1;
    if(piece=='B') return base+2;
    if(piece=='T') return base+3;
    if(piece=='D') return base+4;
    if(piece=='R') return base+5;
    return -1;
}

uint64_t zobristHash = 0;

vector<pair<char,string>>historico;

void clearScreen(){
    cout << "\033[2J\033[H";
}

string getPiece(char piece, char color){
    if(color == 'W'){
        if(piece == 'R') return "♔";
        if(piece == 'D') return "♕";
        if(piece == 'T') return "♖";
        if(piece == 'B') return "♗";
        if(piece == 'C') return "♘";
        if(piece == 'P') return "♙";
    } else if(color == 'B'){
        if(piece == 'R') return "♚";
        if(piece == 'D') return "♛";
        if(piece == 'T') return "♜";
        if(piece == 'B') return "♝";
        if(piece == 'C') return "♞";
        if(piece == 'P') return "♟";
    }
    return "?";
}

void printaTabuleiro(){
    cout << "--HELP\n";
    char jogador;

    if(pecasEscolhidas == 'T') jogador = turno ? 'W' : 'B';
    else if(pecasEscolhidas == 'B') jogador = 'B';
    else jogador = 'W';

    int rowStart = (jogador == 'W') ? 7 : 0;
    int rowEnd   = (jogador == 'W') ? -1 : 8;
    int rowStep  = (jogador == 'W') ? -1 : 1;

    int colStart = (jogador == 'W') ? 0 : 7;
    int colEnd   = (jogador == 'W') ? 8 : -1;
    int colStep  = (jogador == 'W') ? 1 : -1;

    for(int i = rowStart; i != rowEnd; i += rowStep){
        cout << i + 1 << " ";

        for(int j = colStart; j != colEnd; j += colStep){
            bool lightSquare = (i + j) % 2 != 0;
                if(lightSquare) cout << "\033[48;5;136m";
                else            cout << "\033[48;5;94m";
                
                auto& cell = tabuleiro[i][j];
                if(cell.first == '.'){
                    cout << "  ";
                } else {
                    if(cell.second == 'W') cout << "\033[1;97m";
                    else                   cout << "\033[30m";
                    cout << getPiece(cell.first, cell.second) << " ";
                }
                cout << "\033[0m";
        }

        cout << '\n';
    }

    if(jogador == 'W')
        cout << "  A B C D E F G H\n";
    else
        cout << "  H G F E D C B A\n";
}


void ERROR(){
    if(!continua) return;
    clearScreen();
    printaTabuleiro();
    cout << "Jogada impossível" << endl;
    erro = 1;
}

void criaTabuleiro(){
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 8; j++){
            tabuleiro[i][j] = {'.', 'V'};
        }
    }

    char pecasOrdem[] = {'T','C','B','D','R','B','C','T'};
    for(int j = 0; j < 8; j++){
        tabuleiro[0][j] = {pecasOrdem[j], 'W'};
        tabuleiro[1][j] = {'P', 'W'};
    }

    for(int j = 0; j < 8; j++){
        tabuleiro[7][j] = {pecasOrdem[j], 'B'};
        tabuleiro[6][j] = {'P', 'B'};
    }

    zobristHash = 0;
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
            if(tabuleiro[y][x].first != '.')
                zobristHash ^= zobristTable[y][x][pieceIndex(tabuleiro[y][x].first, tabuleiro[y][x].second)];

    printaTabuleiro();
}

bool squareAttacked(int yTrgt,int xTrgt,char jogador){
    bool atkd = false;
    auto slide = [&](int x,int y,int dx,int dy){
        int cx=x+dx, cy=y+dy;

        while(cx>=0&&cx<8&&cy>=0&&cy<8){
            if(cy == yTrgt && cx == xTrgt && tabuleiro[cy][cx].second != jogador) {atkd = true;break;}
            if(tabuleiro[cy][cx].first != '.') break;
            cx+=dx;
            cy+=dy;
        }
    };

    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){

            auto cell = tabuleiro[y][x];

            if(cell.first=='.')
                continue;

            char piece = cell.first;
            
            if(cell.second != jogador) continue; 

            if(piece=='P'){
                int dir = (jogador=='W') ? 1 : -1;

                if(yTrgt == y+dir && xTrgt == x-1) atkd = true;
                if(yTrgt == y+dir && xTrgt == x+1) atkd = true;
            }

            else if(piece=='C'){

                int dx[] = {1,2,2,1,-1,-2,-2,-1};
                int dy[] = {2,1,-1,-2,-2,-1,1,2};

                for(int k=0;k<8;k++)
                    if(x+dx[k] == xTrgt && y+dy[k] == yTrgt) atkd = true;
            }
            
            else if(piece=='B'){
                slide(x,y,1,1);
                slide(x,y,1,-1);
                slide(x,y,-1,1);
                slide(x,y,-1,-1);
            }

            else if(piece=='T'){
                slide(x,y,1,0);
                slide(x,y,-1,0);
                slide(x,y,0,1);
                slide(x,y,0,-1);
            }

            else if(piece=='D'){
                slide(x,y,1,0);
                slide(x,y,-1,0);
                slide(x,y,0,1);
                slide(x,y,0,-1);
                slide(x,y,1,1);
                slide(x,y,1,-1);
                slide(x,y,-1,1);
                slide(x,y,-1,-1);
            }
            
            else if(piece=='R'){
                for(int dx=-1;dx<=1;dx++)
                    for(int dy=-1;dy<=1;dy++)
                        if(dx||dy)
                            if(x+dx == xTrgt && y+dy == yTrgt) atkd = true;
            }
            if(atkd){
                atkd = false;
                return true;
            }
        }
    }
    return false;
}

bool detectaCheck(char jogador){
    char oponente = (jogador == 'W') ? 'B' : 'W';

    for(int y = 0;y < 8;y++){
        for(int x = 0;x < 8;x++){
            if(tabuleiro[y][x].second != jogador) continue;
            if(tabuleiro[y][x].first != 'R') continue;
            return squareAttacked(y,x,oponente);
        }
    }
    return false;
}


bool movimentoValido(char piece,char jogador,int x1,int x2,int y1,int y2){
    if(x1<0||x1>7||x2<0||x2>7||y1<0||y1>7||y2>7||y2<0) return false;
    
    piece = toupper(piece);

    char oponente = (jogador == 'W') ? 'B' : 'W';

    if(piece == 'P'){
        if(abs(y2-y1) == 2 && x1 != x2) return false;
        if(y2 == y1 || abs(y2-y1) > 2) return false;
        if(jogador == 'W' && y1 > y2) return false;
        if(jogador == 'B' && y1 < y2) return false;
        
        if(x2 != x1){
            if(abs(x2-x1) > 1) return false;
            
            if(tabuleiro[y2][x2].first != '.' && tabuleiro[y2][x2].second != jogador) return true;

            if (historico.empty()) return false;

            int n = historico.size() - 1;
            if (tabuleiro[y1][x2].first == 'P' && tabuleiro[y1][x2].second != jogador) {
                
                char linhaOrigem  = historico[n].second[1]; 
                char linhaDestino = historico[n].second[5]; 
                
                string casaInimigaDestino = std::string(1, 'A' + x2) + std::string(1, '1' + y1);
                
                string ultimoDestino = historico[n].second.substr(4, 2);

                if (historico[n].first == 'P' && ultimoDestino == casaInimigaDestino) {
                    if (abs(linhaOrigem - linhaDestino) == 2) {
                        return true;
                    }
                }
            }
            return false; 
        } 
                
        if(abs(y2 - y1) == 2){
            if(jogador == 'W' && y1 == 1 && tabuleiro[y1+1][x1].first == '.' && tabuleiro[y2][x2].first == '.') return true;
            if(jogador == 'B' && y1 == 6 && tabuleiro[y1-1][x1].first == '.' && tabuleiro[y2][x2].first == '.') return true;
            return false;
        }
        
        if(tabuleiro[y2][x2].first != '.') return false;
        
        return true;
    }

    if(piece == 'T'){
        if(x1 != x2 && y1 != y2) return false;
        if(x1 == x2 && y1 == y2) return false;
        
        bool cima = (x1==x2);
        int dir = cima ? (y2-y1) : (x2-x1);
        dir = (dir > 0) ? 1 : 0;
        
        int n = cima ? abs(y2-y1) : abs(x2-x1);
        for(int i = 1;i < n;i++){
            if(cima){
                if(dir){
                    if(tabuleiro[y1+i][x1].first != '.') return false;
              }  else{
                  if(tabuleiro[y1-i][x1].first != '.') return false;
                }
            } else{
                if(dir){
                    if(tabuleiro[y1][x1+i].first != '.') return false; 
                } else{
                  if(tabuleiro[y1][x1-i].first != '.') return false;
                }
            }
        }
        
        if(tabuleiro[y2][x2].second == jogador) return false;
        
        return true;
    }
    
    if(piece == 'B'){
        if(x1 == x2 || y1 == y2) return false;
        if(abs(x2-x1) != abs(y2-y1)) return false;
        
        int dx = (x2 > x1) ? 1 : -1;
        int dy = (y2 > y1) ? 1 : -1;
        
        int cx = x1 + dx, cy = y1 + dy;
        while(cx != x2 || cy != y2){
            if(tabuleiro[cy][cx].first != '.') return false;
            cx += dx;
            cy += dy;
        }
        
        if(tabuleiro[y2][x2].second == jogador) return false;
        return true;
    }
    
    if(piece == 'D'){
        return movimentoValido('T',jogador,x1,x2,y1,y2) || movimentoValido('B',jogador,x1,x2,y1,y2);
    }
    
    if(piece == 'C'){
        if(abs(x2-x1) == 2 && abs(y2-y1) == 1 && tabuleiro[y2][x2].second != jogador) return true;
        if(abs(x2-x1) == 1 && abs(y2-y1) == 2 && tabuleiro[y2][x2].second != jogador) return true;
        return false;
    }
    
    if(piece == 'R'){
        if(tabuleiro[y2][x2].second == jogador) return false;
        if(abs(x2-x1) == 1 && abs(y2-y1) == 1) return true;
        if(abs(x2-x1) == 1 && abs(y2-y1) == 0) return true;
        if(abs(x2-x1) == 0 && abs(y2-y1) == 1) return true;
        
        if(y1 == y2 && abs(x2-x1) == 2){
            int direc = x2 - x1;
            bool direita = (direc > 0);
            int step = direita ? 1 : -1;
            int rookX = direita ? 7 : 0;
            
            if(jogador == 'W'){
                if(moveRW) return false;
                if(direita  && moveRTW) return false;
                if(!direita && moveLTW) return false;
                
            } else {
                if(moveRB) return false;
                if(direita  && moveRTB) return false;
                if(!direita && moveLTB) return false;
            }
            for(int x = x1+step; x != rookX && x>=0 && x<8; x += step){
                if(tabuleiro[y1][x].first != '.') return false;
            }

            if(detectaCheck(jogador)) return false;

            for(int cx = x1+step; cx != x1+2*step; cx += step)
             if(squareAttacked(y1, cx, oponente)) return false;

            if(squareAttacked(y1, x2, oponente)) return false;

            return true;
        }
    }
    return false;
}


int converte(char c){
    string alph = "ABCDEFGH";
    for(int i = 0;i < n;i++){
        if(c == alph[i]){
            erro = 0;
            return i;
        }
    }
    ERROR();
    return 0;
}

const int pawnTable[8][8] = {
        { 50,  50,  50,  50,  50,  50,  50,  50},
        {50, 50, 50, 50, 50, 50, 50, 50},
        {10, 10, 20, 30, 30, 20, 10, 10},
        { 5,  5, 10, 25, 25, 10,  5,  5},
        { 0,  0,  0, 26, 26,  0,  0,  0},
        { 5, -5,-10,  -4,  -4,-10, -5,  5},
        { 5, 10, 10,-20,-20, 25, 10,  5},
        { 0,  0,  0,  0,  0,  0,  0,  0}
    };

    const int knightTable[8][8] = {
        {-50,-40,-30,-30,-30,-30,-40,-50},
        {-40,-20,  3,  10,  10,  3,-20,-40},
        {-30,  7, 10, 15, 15, 10,  7,-30},
        {-30,  5, 15, 20, 20, 15,  5,-30},
        {-30,  0, 15, 20, 20, 15,  0,-30},
        {-30,  5, 10, 15, 15, 10,  5,-30},
        {-40,-20,  0,  5,  5,  0,-20,-40},
        {-50,-30,-30,-30,-30,-30,-30,-50}
    };

    const int bishopTable[8][8] = {
        {-20,-10,-10,-10,-10,-10,-10,-20},
        {-10,  0,  0,  0,  0,  0,  0,-10},
        {-10,  0,  5, 10, 10,  5,  0,-10},
        {-10,  5,  5, 15, 15,  5,  10,-10},
        {0,  0, 20, 10, 10, 20,  0,0},
        {0, 10, 10, 10, 10, 10, 10,0},
        {-10,  15,  0,  0,  3,  0,  15,-10},
        {-20,-10,-10,-10,-10,-10,-10,-20}
    };

    const int rookTable[8][8] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 5, 10, 10, 20, 20, 10, 10,  5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        {-5,  0,  0,  0,  0,  0,  0, -5},
        { 0,  0,  30,  25,  25,  30,  0,  0}
    };

    const int kingTable[8][8] = {
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-20,-30,-30,-40,-40,-30,-30,-20},
        {-10,-20,-20,-20,-20,-20,-20,-10},
        { 20, 20,  -1,  -1,  -1,  -1, 20, 20},
        { 20, 50, 5,  -1,  -1, 3, 50, 20}
    };

    const int kingEndgameTable[8][8] = {
        {-50,-40,-30,-20,-20,-30,-40,-50},
        {-30,-20,-10,  0,  0,-10,-20,-30},
        {-30,-10, 20, 30, 30, 20,-10,-30},
        {-30,-10, 30, 40, 40, 30,-10,-30},
        {-30,-10, 30, 40, 40, 30,-10,-30},
        {-30,-10, 20, 30, 30, 20,-10,-30},
        {-30,-30,  0,  0,  0,  0,-30,-30},
        {-50,-30,-30,-30,-30,-30,-30,-50}
    };

    const int pawnEndgameTable[8][8] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        {50, 50, 50, 50, 50, 50, 50, 50}, 
        {30, 30, 30, 40, 40, 30, 30, 30}, 
        {20, 20, 20, 30, 30, 20, 20, 20}, 
        {10, 10, 10, 20, 20, 10, 10, 10}, 
        { 5,  5,  5, 10, 10,  5,  5,  5}, 
        { 0,  0,  0,-10,-10,  0,  0,  0}, 
        { 0,  0,  0,  0,  0,  0,  0,  0}  
    };

    const int rookEndgameTable[8][8] = {
        { 0,  0,  0,  0,  0,  0,  0,  0},
        {20, 20, 20, 20, 20, 20, 20, 20}, 
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0},
        { 0,  0,  0,  0,  0,  0,  0,  0}
    };

    const int knightEndgameTable[8][8] = {
        {-50,-40,-30,-30,-30,-30,-40,-50},
        {-40,-20,  0,  0,  0,  0,-20,-40},
        {-30,  0, 10, 15, 15, 10,  0,-30},
        {-30,  5, 15, 20, 20, 15,  5,-30},
        {-30,  0, 15, 20, 20, 15,  0,-30},
        {-30,  5, 10, 15, 15, 10,  5,-30},
        {-40,-20,  0,  5,  5,  0,-20,-40},
        {-50,-40,-30,-30,-30,-30,-40,-50}
    };

    const int bishopEndgameTable[8][8] = {
        {-20,-10,-10,-10,-10,-10,-10,-20},
        {-10,  0,  0,  0,  0,  0,  0,-10},
        {-10,  0,  5, 10, 10,  5,  0,-10},
        {-10,  5,  5, 10, 10,  5,  5,-10},
        {-10,  0, 10, 10, 10, 10,  0,-10},
        {-10, 10, 10, 10, 10, 10, 10,-10},
        {-10,  5,  0,  0,  0,  0,  5,-10},
        {-20,-10,-10,-10,-10,-10,-10,-20}
    };


int getPieceSquareValue(char piece, char color, int x, int y){
    int row = (color == 'W') ? (7 - y) : y;

    if(numPecas <= 4 && numDamas == 0){
        if(piece == 'R') return kingEndgameTable[row][x];
        if(piece == 'P') return pawnEndgameTable[row][x];
        if(piece == 'T') return rookEndgameTable[row][x];
        if(piece == 'C') return knightEndgameTable[row][x];
        if(piece == 'B') return bishopEndgameTable[row][x];
    }

    if(piece == 'P') return pawnTable[row][x];
    if(piece == 'C') return knightTable[row][x];
    if(piece == 'B') return bishopTable[row][x];
    if(piece == 'T') return rookTable[row][x];
    if(piece == 'R') return kingTable[row][x];
    return 0;
}

map<uint64_t, int> positionCount;

string boardHash() {
    string h;
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++){
            h += tabuleiro[y][x].first;
            h += tabuleiro[y][x].second;
        }

    h += (moveRW ? '1' : '0');
    h += (moveRB ? '1' : '0');
    h += (moveRTW ? '1' : '0');
    h += (moveLTW ? '1' : '0');
    h += (moveRTB ? '1' : '0');
    h += (moveLTB ? '1' : '0');
    h += (turno   ? '1' : '0');
    return h;
}

struct lance {
    int x1,y1,x2,y2;
    char piece;
    bool moveRW,moveRB;
    bool moveRTW, moveLTW;
    bool moveRTB, moveLTB;
    char cap, corCap,cor;
    
    bool operator==(const lance&) const = default;
};

enum TTFlag { EXACT, LOWERBOUND, UPPERBOUND };

struct TTEntry {
    int value;
    int depth;
    TTFlag flag;
};

struct BoardState {
    pair<char,char> tab[8][8];
    bool moveRW, moveRB, moveRTW, moveLTW, moveRTB, moveLTB, turno, castlesW, castlesB;
    uint64_t hash;
};

BoardState saveState(){
    BoardState s;
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
            s.tab[y][x] = tabuleiro[y][x];
    s.moveRW = moveRW; s.moveRB = moveRB;
    s.moveRTW = moveRTW; s.moveLTW = moveLTW;
    s.moveRTB = moveRTB; s.moveLTB = moveLTB;
    s.turno = turno;
    s.castlesW = castlesW; s.castlesB = castlesB;
    s.hash = zobristHash;
    return s;
}

void restoreState(BoardState& s){
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
            tabuleiro[y][x] = s.tab[y][x];
    moveRW = s.moveRW; moveRB = s.moveRB;
    moveRTW = s.moveRTW; moveLTW = s.moveLTW;
    moveRTB = s.moveRTB; moveLTB = s.moveLTB;
    turno = s.turno;
    castlesW = s.castlesW; castlesB = s.castlesB;
    zobristHash = s.hash;
}

void applyMove(lance m, char jogador){
    char oponente = (jogador == 'W') ? 'B' : 'W';

    zobristHash ^= zobristTable[m.y1][m.x1][pieceIndex(m.piece, jogador)];

    if(tabuleiro[m.y2][m.x2].first != '.')
        zobristHash ^= zobristTable[m.y2][m.x2][pieceIndex(tabuleiro[m.y2][m.x2].first, oponente)];

    if(m.piece == 'R' && abs(m.x2 - m.x1) == 2){
        bool direita = (m.x2 > m.x1);
        int rookX = direita ? 7 : 0;
        int rookDest = direita ? m.x2-1 : m.x2+1;

        zobristHash ^= zobristTable[m.y1][rookX][pieceIndex('T', jogador)];
        zobristHash ^= zobristTable[m.y1][rookDest][pieceIndex('T', jogador)];

        tabuleiro[m.y1][rookDest] = {'T', jogador};
        tabuleiro[m.y1][rookX]   = {'.', 'V'};
        if(jogador=='W'){ direita ? moveRTW=1 : moveLTW=1; castlesW=1; }
        else            { direita ? moveRTB=1 : moveLTB=1; castlesB=1; }
    }

    if(m.piece=='T'){
        if(m.y1==0&&m.x1==7) moveRTW=1;
        if(m.y1==0&&m.x1==0) moveLTW=1;
        if(m.y1==7&&m.x1==7) moveRTB=1;
        if(m.y1==7&&m.x1==0) moveLTB=1;
    }
    if(m.y2==0&&m.x2==7) moveRTW=1;
    if(m.y2==0&&m.x2==0) moveLTW=1;
    if(m.y2==7&&m.x2==7) moveRTB=1;
    if(m.y2==7&&m.x2==0) moveLTB=1;

    if(m.piece=='P' && (m.y2==7 || m.y2==0)) m.piece='D';

    if(m.piece=='P' && m.x2!=m.x1 && tabuleiro[m.y2][m.x2].first=='.'){
        zobristHash ^= zobristTable[m.y1][m.x2][pieceIndex('P', oponente)];
        tabuleiro[m.y1][m.x2] = {'.','V'};
    }

    zobristHash ^= zobristTable[m.y2][m.x2][pieceIndex(m.piece, jogador)];

    tabuleiro[m.y2][m.x2] = {m.piece, jogador};
    tabuleiro[m.y1][m.x1] = {'.','V'};
    if(m.piece=='R') jogador=='W' ? moveRW=1 : moveRB=1;

    zobristHash ^= zobristTurn;
}


unordered_map<uint64_t, TTEntry> transpositionTable;

bool precisaChecarCheck(int x1, int y1, int x2, int y2, char jogador) {
    int rx = -1, ry = -1;
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
            if(tabuleiro[y][x].first == 'R' && tabuleiro[y][x].second == jogador)
                { rx = x; ry = y; }

    if(x1 == rx && y1 == ry) return true;
    if(y1 == ry) return true;
    if(x1 == rx) return true;
    if(abs(x1 - rx) == abs(y1 - ry)) return true;

    return false;
}

vector<lance>geraCapturas(char jogador){
    vector<lance>captures;
    char oponente = (jogador == 'W') ? 'B' : 'W';

    auto addMove = [&](int x, int y, int x2, int y2, char piece){
        if(x2<0||x2>7||y2<0||y2>7) return;
        if(tabuleiro[y2][x2].second == jogador) return;
        if(tabuleiro[y2][x2].first != '.')
            captures.emplace_back(x,y,x2,y2,piece);
    };

    auto addSliding = [&](int x, int y, char piece, int dx, int dy){
        int cx = x+dx, cy = y+dy;
        while(cx>=0&&cx<8&&cy>=0&&cy<8){
            if(tabuleiro[cy][cx].second == jogador) break;
            if(tabuleiro[cy][cx].first != '.'){
                captures.emplace_back(x,y,cx,cy,piece);
                break;
            }
            cx+=dx; cy+=dy;
        }
    };

    for(int y = 0; y < 8; y++){
        for(int x = 0; x < 8; x++){
            auto& cell = tabuleiro[y][x];
            if(cell.first == '.' || cell.second != jogador) continue;
            char piece = cell.first;

            if(piece == 'P'){
                int dir = (jogador == 'W') ? 1 : -1;
                int startRank = (jogador == 'W') ? 1 : 6;

                if(tabuleiro[y+dir][x].first == '.'){
                    addMove(x,y,x,y+dir,piece);
                    if(y == startRank && tabuleiro[y+2*dir][x].first == '.')
                        addMove(x,y,x,y+2*dir,piece);
                }

                for(int dx : {-1, 1}){
                    int nx = x+dx, ny = y+dir;
                    if(nx<0||nx>7||ny<0||ny>7) continue;

                    if(tabuleiro[ny][nx].first != '.' && tabuleiro[ny][nx].second != jogador){
                        captures.emplace_back(x,y,nx,ny,piece);
                        continue;
                    }

                    if(!historico.empty()){
                        int n = historico.size()-1;
                        if(tabuleiro[y][nx].first == 'P' && tabuleiro[y][nx].second == oponente){
                            string casaDestino = string(1,'A'+nx) + string(1,'1'+y);
                            string ultimoDestino = historico[n].second.substr(4,2);
                            char linhaOrigem  = historico[n].second[1];
                            char linhaDestino = historico[n].second[5];
                            if(historico[n].first == 'P' && ultimoDestino == casaDestino
                               && abs(linhaOrigem - linhaDestino) == 2)
                                captures.emplace_back(x,y,nx,ny,piece);
                        }
                    }
                }
            }

            else if(piece == 'C'){
                int dx[] = {1,2,2,1,-1,-2,-2,-1};
                int dy[] = {2,1,-1,-2,-2,-1,1,2};
                for(int k=0;k<8;k++) addMove(x,y,x+dx[k],y+dy[k],piece);
            }

            else if(piece == 'B'){
                for(auto [dx,dy] : vector<pair<int,int>>{{1,1},{1,-1},{-1,1},{-1,-1}})
                    addSliding(x,y,piece,dx,dy);
            }

            else if(piece == 'T'){
                for(auto [dx,dy] : vector<pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}})
                    addSliding(x,y,piece,dx,dy);
            }

            else if(piece == 'D'){
                for(auto [dx,dy] : vector<pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}})
                    addSliding(x,y,piece,dx,dy);
            }

            else if(piece == 'R'){
                for(int dx=-1;dx<=1;dx++)
                    for(int dy=-1;dy<=1;dy++)
                        if(dx||dy) addMove(x,y,x+dx,y+dy,piece);
            }
        }
    }
    return captures;
}


pair<vector<lance>,vector<lance>> geraLances(char jogador){
    vector<lance> captures, quiet;
    char oponente = (jogador == 'W') ? 'B' : 'W';
    bool RW = moveRW,RB = moveRB,RTW = moveRTW,LTW = moveLTW, RTB = moveRTB, LTB = moveLTB;
    char cor = jogador;

    auto addMove = [&](int x, int y, int x2, int y2, char piece){
        if(x2<0||x2>7||y2<0||y2>7) return;
        if(tabuleiro[y2][x2].second == jogador) return;
        if(tabuleiro[y2][x2].first != '.')
            captures.emplace_back(x,y,x2,y2,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[y2][x2].first,tabuleiro[y2][x2].second,cor);
        else
            quiet.emplace_back(x,y,x2,y2,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[y2][x2].first,tabuleiro[y2][x2].second,cor);
    };

    auto addSliding = [&](int x, int y, char piece, int dx, int dy){
        int cx = x+dx, cy = y+dy;
        while(cx>=0&&cx<8&&cy>=0&&cy<8){
            if(tabuleiro[cy][cx].second == jogador) break;
            if(tabuleiro[cy][cx].first != '.'){
                captures.emplace_back(x,y,cx,cy,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[cy][cx].first,tabuleiro[cy][cx].second,cor);
                break;
            }
            quiet.emplace_back(x,y,cx,cy,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[cy][cx].first,tabuleiro[cy][cx].second,cor);
            cx+=dx; cy+=dy;
        }
    };

    for(int y = 0; y < 8; y++){
        for(int x = 0; x < 8; x++){
            auto& cell = tabuleiro[y][x];
            if(cell.first == '.' || cell.second != jogador) continue;
            char piece = cell.first;

            if(piece == 'P'){
                int dir = (jogador == 'W') ? 1 : -1;
                int startRank = (jogador == 'W') ? 1 : 6;

                if(tabuleiro[y+dir][x].first == '.'){
                    addMove(x,y,x,y+dir,piece);
                    if(y == startRank && tabuleiro[y+2*dir][x].first == '.')
                        addMove(x,y,x,y+2*dir,piece);
                }

                for(int dx : {-1, 1}){
                    int nx = x+dx, ny = y+dir;
                    if(nx<0||nx>7||ny<0||ny>7) continue;

                    if(tabuleiro[ny][nx].first != '.' && tabuleiro[ny][nx].second != jogador){
                        captures.emplace_back(x,y,nx,ny,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[ny][nx].first,tabuleiro[ny][nx].second,cor);
                        continue;
                    }

                    if(!historico.empty()){
                        int n = historico.size()-1;
                        if(tabuleiro[y][nx].first == 'P' && tabuleiro[y][nx].second == oponente){
                            string casaDestino = string(1,'A'+nx) + string(1,'1'+y);
                            string ultimoDestino = historico[n].second.substr(4,2);
                            char linhaOrigem  = historico[n].second[1];
                            char linhaDestino = historico[n].second[5];
                            if(historico[n].first == 'P' && ultimoDestino == casaDestino
                               && abs(linhaOrigem - linhaDestino) == 2)
                                captures.emplace_back(x,y,nx,ny,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[ny][nx].first,tabuleiro[ny][nx].second,cor);
                        }
                    }
                }
            }

            else if(piece == 'C'){
                int dx[] = {1,2,2,1,-1,-2,-2,-1};
                int dy[] = {2,1,-1,-2,-2,-1,1,2};
                for(int k=0;k<8;k++) addMove(x,y,x+dx[k],y+dy[k],piece);
            }

            else if(piece == 'B'){
                for(auto [dx,dy] : vector<pair<int,int>>{{1,1},{1,-1},{-1,1},{-1,-1}})
                    addSliding(x,y,piece,dx,dy);
            }

            else if(piece == 'T'){
                for(auto [dx,dy] : vector<pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}})
                    addSliding(x,y,piece,dx,dy);
            }

            else if(piece == 'D'){
                for(auto [dx,dy] : vector<pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}})
                    addSliding(x,y,piece,dx,dy);
            }

            else if(piece == 'R'){
                for(int dx=-1;dx<=1;dx++)
                    for(int dy=-1;dy<=1;dy++)
                        if(dx||dy) addMove(x,y,x+dx,y+dy,piece);

                auto tryRoque = [&](bool direita){
                    int rookX = direita ? 7 : 0;
                    int step  = direita ? 1 : -1;
                    if(jogador=='W'){ if(moveRW) return; if(direita&&moveRTW) return; if(!direita&&moveLTW) return; }
                    else            { if(moveRB) return; if(direita&&moveRTB) return; if(!direita&&moveLTB) return; }
                    for(int cx=x+step; cx!=rookX; cx+=step)
                        if(tabuleiro[y][cx].first != '.') return;
                
                    if(squareAttacked(y,x,oponente)) return;

                    for(int cx = x+step; cx != x+2*step; cx += step)
                        if(squareAttacked(y, cx, oponente)) return;

                    if(squareAttacked(y, x + 2*step, oponente)) return;

                    quiet.emplace_back(x,y,x+2*step,y,piece,RW,RB,RTW,LTW,RTB,LTB,tabuleiro[y][x].first,tabuleiro[y][x].second,cor);
                };
                tryRoque(true);
                tryRoque(false);
            }
        }
    }

    vector<lance> moves = captures;
    moves.insert(moves.end(), quiet.begin(), quiet.end());

    vector<lance> legal;
    bool emCheck = detectaCheck(jogador);

    for(auto& m : moves){
        if(!emCheck && !precisaChecarCheck(m.x1, m.y1, m.x2, m.y2, jogador)){
            legal.emplace_back(m);
            continue;
        }
        BoardState state = saveState();
        applyMove(m, jogador);
        bool ilegal = detectaCheck(jogador);
        restoreState(state);
        if(!ilegal) legal.emplace_back(m);
    }

    // Ordenação original mantida (será refeita em alphaBeta com score completo)
    sort(legal.begin(), legal.end(), [&](const lance& a, const lance& b){
        auto val = [](char p) -> int {
            if(p=='D') return 6; if(p=='T') return 5;
            if(p=='B') return 4; if(p=='C') return 3;
            if(p=='P') return 1; return 0;
        };
        return val(tabuleiro[a.y2][a.x2].first) > val(tabuleiro[b.y2][b.x2].first);
    });

    return {legal, captures};
}

pair<bool,bool> attacked[8][8];

void attackedSquares(){
    for(int y=0;y<8;y++)
        for(int x=0;x<8;x++)
            attacked[y][x] = {false,false};

    auto mark = [&](int x,int y,char jogador){
        if(x<0||x>7||y<0||y>7) return;
        if(jogador=='W') attacked[y][x].first = true;
        else             attacked[y][x].second = true;
    };

    auto slide = [&](int x,int y,int dx,int dy,char jogador){
        int cx=x+dx, cy=y+dy;
        while(cx>=0&&cx<8&&cy>=0&&cy<8){
            mark(cx,cy,jogador);
            if(tabuleiro[cy][cx].first != '.') break;
            cx+=dx; cy+=dy;
        }
    };

    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            auto cell = tabuleiro[y][x];
            if(cell.first=='.') continue;
            char piece = cell.first;
            char jogador = cell.second;

            if(piece=='P'){
                int dir = (jogador=='W') ? 1 : -1;
                mark(x-1,y+dir,jogador);
                mark(x+1,y+dir,jogador);
            }
            else if(piece=='C'){
                int dx[] = {1,2,2,1,-1,-2,-2,-1};
                int dy[] = {2,1,-1,-2,-2,-1,1,2};
                for(int k=0;k<8;k++) mark(x+dx[k],y+dy[k],jogador);
            }
            else if(piece=='B'){
                slide(x,y,1,1,jogador); slide(x,y,1,-1,jogador);
                slide(x,y,-1,1,jogador); slide(x,y,-1,-1,jogador);
            }
            else if(piece=='T'){
                slide(x,y,1,0,jogador); slide(x,y,-1,0,jogador);
                slide(x,y,0,1,jogador); slide(x,y,0,-1,jogador);
            }
            else if(piece=='D'){
                slide(x,y,1,0,jogador); slide(x,y,-1,0,jogador);
                slide(x,y,0,1,jogador); slide(x,y,0,-1,jogador);
                slide(x,y,1,1,jogador); slide(x,y,1,-1,jogador);
                slide(x,y,-1,1,jogador); slide(x,y,-1,-1,jogador);
            }
            else if(piece=='R'){
                for(int dx=-1;dx<=1;dx++)
                    for(int dy=-1;dy<=1;dy++)
                        if(dx||dy) mark(x+dx,y+dy,jogador);
            }
        }
    }
}

int pawnShieldBonus(int kingX, int kingY, char side){
    int bonus = 0;
    int dir = (side == 'W') ? -1 : 1;
    int shieldRank = kingY + dir;
    if(shieldRank < 0 || shieldRank > 7) return 0;
    for(int dx = -1; dx <= 1; dx++){
        int file = kingX + dx;
        if(file < 0 || file > 7) continue;
        auto &sq = tabuleiro[shieldRank][file];
        if(sq.first == 'P' && sq.second == side) bonus += 15;
        else bonus -= 15;
    }
    return bonus;
}

bool peaoPassado(int y, int x, char jogador) {
    if (jogador == 'W') {
        // Brancas olham para frente 
        for (int ny = y + 1; ny < 8; ny++) {
            // Mesma coluna
            if (tabuleiro[ny][x].first == 'P' && tabuleiro[ny][x].second != jogador) return false;
            //Coluna da direita
            if (x + 1 < 8 && tabuleiro[ny][x + 1].first == 'P' && tabuleiro[ny][x + 1].second != jogador) return false;
            //Coluna da esquerda
            if (x - 1 >= 0 && tabuleiro[ny][x - 1].first == 'P' && tabuleiro[ny][x - 1].second != jogador) return false;
        }
    } 
    else {
        // Pretas olham para trás/baixo (até a linha 0)
        for (int ny = y - 1; ny >= 0; ny--) {
            //Mesma coluna
            if (tabuleiro[ny][x].first == 'P' && tabuleiro[ny][x].second != jogador) return false;
            //Coluna da direita
            if (x + 1 < 8 && tabuleiro[ny][x + 1].first == 'P' && tabuleiro[ny][x + 1].second != jogador) return false;
            
            //Coluna da esquerda
            if (x - 1 >= 0 && tabuleiro[ny][x - 1].first == 'P' && tabuleiro[ny][x - 1].second != jogador) return false;
            
        }
    }
    return true;
}

int evaluate(){
    int valW = 0, valB = 0, numBisposW = 0,numBisposB = 0,numReis = 0;
    numDamas = 0; numPecas = 0;

    //Avalia material
    for(int y = 0; y < 8; y++){
        for(int x = 0; x < 8; x++){
            auto& cell = tabuleiro[y][x];
            if(cell.first == '.') continue;
            int val = 0;
            if(cell.first == 'P') val = 100;
            else if(cell.first == 'C') val = 320;
            else if(cell.first == 'B') {val = 335; cell.second == 'W' ? numBisposW++ : numBisposB++;}
            else if(cell.first == 'T') val = 510;
            else if(cell.first == 'D') {val = 900; numDamas++;}
            else if(cell.first == 'R') {val = 20000; numReis++;}

            if(cell.first != 'P' && cell.first != 'D') numPecas++;

            val += getPieceSquareValue(cell.first, cell.second, x, y);
            
            if(cell.second == 'W') valW += val;
            else valB += val;
        }
    }

    //Está em cheque
    for(int y = 0; y < 8; y++){
        for(int x = 0; x < 8; x++){
            if(tabuleiro[y][x].first == 'R'){
                char cor = tabuleiro[y][x].second;
                char oponente = (cor == 'W') ? 'B' : 'W';
                if(squareAttacked(y,x, oponente)){
                    if(cor == 'W') valW -= 50;
                    else           valB -= 50;
                }
            }
        }
    }


    int whiteFiles[8] = {};
    int blackFiles[8] = {};

    //Número de peões na coluna
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            if(tabuleiro[y][x].first != 'P') continue;
            if(tabuleiro[y][x].second == 'W') whiteFiles[x]++;
            else blackFiles[x]++;
        }
    }

    //Peões dobrados
    for(int f=0;f<8;f++){
        if(whiteFiles[f] > 1) valW -= 10 * (whiteFiles[f] - 1);
        if(blackFiles[f] > 1) valB -= 10 * (blackFiles[f] - 1);
    }

    for(int f = 0;f < 8;f++){
        int uf = f+1,df = f-1;

        //Se existem colunas à esquerda e à direita
        if(df >= 0 && uf <= 7){
            if(whiteFiles[f] > 0){
                //Penaliza se a ilha estiver danificada
                if(whiteFiles[uf] == 0 && whiteFiles[df] == 0) valW -= 15;
            }
            if(blackFiles[f] > 0){
                if(blackFiles[uf] == 0 && blackFiles[df] == 0) valB -= 15;
            }
            //Se existem colunas à esquerda
        } else if(df >= 0){
            if(whiteFiles[f] > 0){
                //penaliza
                if(whiteFiles[df] == 0) valW -= 10;
            }
            if(blackFiles[f] > 0){
                if(blackFiles[df] == 0) valB -= 10;
            }
            //Colunas à direita
        } else if(uf <= 7){
            if(whiteFiles[f] > 0){
                if(whiteFiles[uf] == 0) valW -= 10;
            }
            if(blackFiles[f] > 0){
                if(blackFiles[uf] == 0) valB -= 10;
            }
        }
    }

    //Rei protegido por peões
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            if(tabuleiro[y][x].first != 'R') continue;
            if(tabuleiro[y][x].second == 'W') valW += pawnShieldBonus(x,y,'W');
            else valB += pawnShieldBonus(x,y,'B');
        }
    }

    for(int y = 0;y < 8;y++){
        for(int x = 0;x < 8;x++){
            if(tabuleiro[y][x].first != 'P') continue;
            char jogador = tabuleiro[y][x].second;
            if(peaoPassado(y,x,jogador)) (jogador == 'W') ? valW += 20 + ((y+1)*3): valB += 20 + ((8-y) * 3);
        }
    }

    if(castlesW) valW += 60;
    if(castlesB) valB += 60;
    if(numBisposB >= 2) valB += 30;
    if(numBisposW >= 2) valW += 30;

    attackedSquares();


    auto slide = [&](int x,int y,int dx,int dy,char jogador){
        int cx=x+dx, cy=y+dy;
        while(cx>=0&&cx<8&&cy>=0&&cy<8){
            if(tabuleiro[cy][cx].first != '.') break;
            if(jogador == 'W') valW +=1;
            else valB += 1;
            cx+=dx; cy+=dy;
        }
    };

    for(int y = 0; y < 8; y++){
        for(int x = 0; x < 8; x++){
            if(tabuleiro[y][x].first == 'B'){
                char jogador = tabuleiro[y][x].second;
                slide(x,y,1,1,jogador); slide(x,y,1,-1,jogador);
                slide(x,y,-1,1,jogador); slide(x,y,-1,-1,jogador);
            }
            if(tabuleiro[y][x].first == 'D'){
                char jogador = tabuleiro[y][x].second;
                slide(x,y,1,0,jogador); slide(x,y,-1,0,jogador);
                slide(x,y,0,1,jogador); slide(x,y,0,-1,jogador);
                slide(x,y,1,1,jogador); slide(x,y,1,-1,jogador);
                slide(x,y,-1,1,jogador); slide(x,y,-1,-1,jogador);
            }
            if(tabuleiro[y][x].first == 'T'){
                char jogador = tabuleiro[y][x].second;
                slide(x,y,1,0,jogador); slide(x,y,-1,0,jogador);
                slide(x,y,0,1,jogador); slide(x,y,0,-1,jogador);
            }
        }
    }

    for(int y = 0;y < 8;y++){
        for(int x = 0;x < 8;x++){
            if(tabuleiro[y][x].first != 'R') continue;
            int cor = tabuleiro[y][x].second;

            int dx[8] = {1,-1,0,0,1,1,-1,-1};
            int dy[8] = {0,0,1,-1,1,-1,1,-1};

            int penalty = 0;
            if(numPecas > 4 && numDamas > 0){
                int attackedNearCountW = 0,attackedNearCountB = 0;
                for(int i = 0;i < 8;i++){
                    int nx = x+dx[i],ny = y+dy[i];
                    if(nx <= 7 && nx >= 0 && ny <= 7 && ny >= 0){
                        if(cor == 'W') {if(attacked[ny][nx].second) attackedNearCountW++;}
                        else if(attacked[ny][nx].first) attackedNearCountB++;
                    }
                }
                int near = max(attackedNearCountB,attackedNearCountW);
                penalty = near * near * 4;
                
            }
            (cor == 'W') ? (valW -= penalty) : (valB -= penalty);
        }
    }

    return valW - valB;
}

int evaluateQuiesce(){
    int score = evaluate();
    attackedSquares();

    int valW = 0, valB = 0;

    for(int y = 0;y < 8;y++){
        for(int x = 0;x < 8;x++){
            if(tabuleiro[y][x].first != 'R') continue;
            int cor = tabuleiro[y][x].second;

            int dx[8] = {1,-1,0,0,1,1,-1,-1};
            int dy[8] = {0,0,1,-1,1,-1,1,-1};

            int attackedNearCountW = 0,attackedNearCountB = 0;
            for(int i = 0;i < 8;i++){
                int nx = x+dx[i],ny = y+dy[i];
                if(nx <= 7 && nx >= 0 && ny <= 7 && ny >= 0){
                    if(cor == 'W') {if(attacked[ny][nx].second) attackedNearCountW++;}
                    else if(attacked[ny][nx].first) attackedNearCountB++;
                }
            }
            int near = max(attackedNearCountB,attackedNearCountW);
            int penalty = near * near * 4;
            (cor == 'W') ? (valW -= penalty) : (valB -= penalty);
        }
    }
    return score + (valW - valB);
}

void printaAttacked(char jogador){
    string ALPH = "ABCDEFGH";
    for(int y = 0;y < 8;y++){
        for(int x = 0;x < 8;x++){
            char coord = ALPH[x];
            if(squareAttacked(y,x,jogador)) cout << "Atacado(" << jogador << ") : {" << coord << " , " << y+1 << "}\n"; 
        }
    }
}

void printMove(const lance& m) {
    string alph = "ABCDEFGH";
    cout << m.piece << alph[m.x1] << (m.y1+1) << "->" << alph[m.x2] << (m.y2+1) << "\n";
}


bool ajuda = 0;
void help(){
    clearScreen();
    printaTabuleiro();

    ajuda = 1;

    cout << endl;
    cout << "•Ex: PE2->E4 \n"                         << 
    "•ROQUE HABILITADO (Ex: RE1->G1)\n"               << 
    "•'RESIGN' PARA DESISTIR" << endl;

    string quit;
    while(quit != "Q"){
        cout << "\n•'Q' PARA SAIR DO MENU" << endl;
        cin >> quit;
        clearScreen();
        printaTabuleiro();
    }    
    ajuda = 0;
    ERROR();
    return;
}

void testMove(lance m){
    string ALPH = "ABCDEFGH";
    cout << m.piece << ALPH[m.x1] << m.y1 << "->" << ALPH[m.x2] << m.y1 << '\n';
}

lance salvaEstado(lance m){
    lance e;
    e.piece = m.piece;
    e.x1 = m.x1; e.x2 = m.x2;
    e.y1 = m.y1; e.y2 = m.y2;
    e.moveRW = m.moveRW; e.moveRB = m.moveRB;
    e.moveRTW = m.moveRTW; e.moveLTW = m.moveLTW;
    e.moveRTB = m.moveRTB; e.moveLTB = m.moveLTB;
    e.cap = m.cap; e.corCap = m.corCap;
    e.cor = m.cor;
    return e;
}

void takeBackMove(lance m){
    moveRW = m.moveRW; moveRB = m.moveRB;
    moveRTW = m.moveRTW; moveLTW = m.moveLTW;
    moveRTB = m.moveRTB; moveLTB = m.moveLTB;
    tabuleiro[m.y2][m.x2].first = m.cap;
    tabuleiro[m.y2][m.x2].second = m.corCap; 
    tabuleiro[m.y1][m.x1].first = m.piece;
    tabuleiro[m.y1][m.x1].second = m.cor; 
}
bool capturou = false;

void move(){
    cout << "Move: " << endl;
    string move; 
    cin >> move;

    if(move == "RESIGN" || move == "resign" || move == "desistir" || move == "DESISTIR"){
        continua = false;
        return;
    }

    if(move == "--HELP"){
        help();
        return;
    }

    if(move.size() != 7){
        ERROR();
        return;
    }

    int xAnterior = converte(toupper(move[1]));
    int yAnterior = move[2] - '0';
    yAnterior--;

    int xNovo = converte(toupper(move[5]));
    int yNovo = move[6] - '0';
    yNovo--;
    char piece = toupper(move[0]);
    char jogador = tabuleiro[yAnterior][xAnterior].second;
    char oponente = jogador == 'W' ? 'B' : 'W';
    bool check = detectaCheck(jogador);

    if(turno && jogador != 'W'){ ERROR(); return; }
    if(!turno && jogador != 'B'){ ERROR(); return; }

    if(yNovo > n-1 || yNovo < 0 || yAnterior > n-1 || yAnterior < 0){ ERROR(); return; }
    if(tabuleiro[yAnterior][xAnterior].first != piece){ ERROR(); return; }
    if(!movimentoValido(piece,jogador,xAnterior,xNovo,yAnterior,yNovo)){ ERROR(); return; }
    
    if(check){
        check = false;
        pair<vector<lance>,vector<lance>> lancesPossiveisSort = geraLances(jogador);
        vector<lance> lancesPossiveis = lancesPossiveisSort.first;
        
        vector<lance> validos;
        for(lance l : lancesPossiveis){            
            BoardState state = saveState();
            applyMove(l,jogador);
            if(!detectaCheck(jogador)) validos.emplace_back(l);
            restoreState(state);
        }
        if(validos.empty()) {continua = 0;return;}

        bool evadeCheck = false;
        for(lance l : validos){
            if((l.x1 == xAnterior) && (l.x2 == xNovo) && (l.y1 == yAnterior) && (l.y2 == yNovo) && (l.piece == piece)) {evadeCheck = true; break;}
        }

        if(!evadeCheck){ERROR();return;}
    }

    int torre = 0,dama = 0,peao = 0,cavalo = 0,bispo = 0;
    for(int y = 0;y < 8;y++){
        for(int x = 0;x < 8;x++){
            if(tabuleiro[y][x].second != jogador) continue;
            if(tabuleiro[y][x].first == 'P') peao = 1;
            if(tabuleiro[y][x].first == 'T') torre = 1;
            if(tabuleiro[y][x].first == 'B') bispo = 1;
            if(tabuleiro[y][x].first == 'C') cavalo = 1;
            if(tabuleiro[y][x].first == 'D') dama = 1;
        }
    }

    //Material insuficiente
    if(!dama && !torre && !peao && !(cavalo && bispo)) {continua = 0;return;}

    lance l;
    l.x1 = xAnterior; l.y1 = yAnterior;
    l.x2 = xNovo;     l.y2 = yNovo;
    l.piece = piece;
    l.moveRW = moveRW; l.moveRB = moveRB;
    l.moveRTW = moveRTW; l.moveLTW = moveLTW;
    l.moveRTB = moveRTB; l.moveLTB = moveLTB;
    l.cap = tabuleiro[yNovo][xNovo].first;
    l.corCap = tabuleiro[yNovo][xNovo].second;
    l.cor = jogador;

    if(!check){
        if(precisaChecarCheck(xAnterior, yAnterior, xNovo, yNovo, jogador)){
            BoardState state = saveState();
            applyMove(l, jogador);
            bool entraEmCheck = detectaCheck(jogador);
            restoreState(state);
            if(entraEmCheck){ ERROR(); return; }
        }
    }

    if(piece == 'P' && yNovo == 7 || piece == 'P' && yNovo == 0){
        piece = 'D';
    }

    if(piece == 'R' && abs(xNovo - xAnterior) == 2){
        bool direita = (xNovo > xAnterior);
        int y = yAnterior;

        zobristHash ^= zobristTable[y][xAnterior][pieceIndex('R', jogador)];
        zobristHash ^= zobristTable[y][xNovo][pieceIndex('R', jogador)];

        if(direita){
            int rookX = 7;
            tabuleiro[y][xNovo-1] = {'T', jogador};
            tabuleiro[y][rookX]   = {'.', 'V'};
            zobristHash ^= zobristTable[yAnterior][rookX][pieceIndex('T', jogador)];
            zobristHash ^= zobristTable[yAnterior][xNovo-1][pieceIndex('T', jogador)];
        } else {
            int rookX = 0;
            tabuleiro[y][xNovo+1] = {'T', jogador};
            tabuleiro[y][rookX]   = {'.', 'V'};
            zobristHash ^= zobristTable[yAnterior][rookX][pieceIndex('T', jogador)];
            zobristHash ^= zobristTable[yAnterior][xNovo+1][pieceIndex('T', jogador)];
        }

        tabuleiro[y][xNovo]     = {'R', jogador};
        tabuleiro[y][xAnterior] = {'.', 'V'};

        if(jogador == 'W'){ moveRW = 1; direita ? moveRTW=1 : moveLTW=1; }
        else               { moveRB = 1; direita ? moveRTB=1 : moveLTB=1; }

        turno ^= 1;
        zobristHash ^= zobristTurn;
        return; 
    }

    if(piece == 'R') jogador == 'W' ? moveRW = 1 : moveRB = 1;

    turno ^= 1;
    char captura = tabuleiro[yNovo][xNovo].first;
    if(captura != '.') capturou = true;
    if(capturou && captura != 'P') numPecas--;

    zobristHash ^= zobristTable[yAnterior][xAnterior][pieceIndex(piece, jogador)];

    if(capturou) 
    zobristHash ^= zobristTable[yNovo][xNovo][pieceIndex(captura, oponente)];

    if(piece == 'P' && xNovo != xAnterior && tabuleiro[yNovo][xNovo].first == '.'){
        zobristHash ^= zobristTable[yAnterior][xNovo][pieceIndex('P', oponente)];
        tabuleiro[yAnterior][xNovo] = {'.','V'};
    }

    if(piece == 'P') lastPawnMove = 0;
    else lastPawnMove++;

    if(captura == 'R') continua = 0;

    tabuleiro[yNovo][xNovo].first = piece;
    tabuleiro[yNovo][xNovo].second = jogador;
    tabuleiro[yAnterior][xAnterior] = {'.','V'};

    zobristHash ^= zobristTable[yNovo][xNovo][pieceIndex(piece, jogador)];
    
    historico.emplace_back(piece, std::string{
    (char)toupper(move[1]), move[2], '-', '>', 
    (char)toupper(move[5]), move[6]
    });

    if(piece == 'T'){
        if(yAnterior == 0 && xAnterior == 7) moveRTW = 1;
        if(yAnterior == 0 && xAnterior == 0) moveLTW = 1;
        if(yAnterior == 7 && xAnterior == 7) moveRTB = 1;
        if(yAnterior == 7 && xAnterior == 0) moveLTB = 1;
    }
    if(yNovo == 0 && xNovo == 7) moveRTW = 1;
    if(yNovo == 0 && xNovo == 0) moveLTW = 1;
    if(yNovo == 7 && xNovo == 7) moveRTB = 1;
    if(yNovo == 7 && xNovo == 0) moveLTB = 1;
    
    uint64_t hash = zobristHash;
    positionCount[hash]++;
    if(positionCount[hash] >= 3 || lastPawnMove >= 50){
        continua = 0;
        clearScreen();
        printaTabuleiro();
        cout << "EMPATE\n";
    }

    char oponenteCheck = turno ? 'W' : 'B';
    vector<lance> lancesOponente = geraLances(oponenteCheck).first;
    vector<lance> validosOponente;
    for(lance m : lancesOponente){
        BoardState state = saveState();
        applyMove(m, oponenteCheck);
        if(!detectaCheck(oponenteCheck)) validosOponente.emplace_back(m);
        restoreState(state);
    }
    if(validosOponente.empty()){
        continua = 0;
        clearScreen();
        printaTabuleiro();
        if(detectaCheck(oponenteCheck)){ cout << "CHEQUEMATE\n";tocarSom(somCheck);}
        else cout << "AFOGAMENTO\n";
        return;
    }
    cout << '\a' << '\n';
}

int Quiesce(int alpha, int beta, char jogador) {
    int eval = evaluate();

    if(jogador == 'W') {
        if(eval >= beta) return beta;
        alpha = max(alpha, eval);
    } else {
        if(eval <= alpha) return alpha;
        beta = min(beta, eval);
    }

    vector<lance> captures = geraCapturas(jogador);

    for(auto& m : captures) {

        int victim = 0;
        char cap = tabuleiro[m.y2][m.x2].first;

        if(cap=='P') victim=100;
        else if(cap=='C'||cap=='B') victim=300;
        else if(cap=='T') victim=500;
        else if(cap=='D') victim=900;

        int attacker = 0;
        if(m.piece=='P') attacker=100;
        else if(m.piece=='C'||m.piece=='B') attacker=300;
        else if(m.piece=='T') attacker=500;
        else if(m.piece=='D') attacker=900;

        if(victim < attacker) continue;

        BoardState state = saveState();
        applyMove(m, jogador);
        int score = Quiesce(alpha, beta, jogador == 'W' ? 'B' : 'W');
        restoreState(state);

        if(jogador == 'W') {
            alpha = max(alpha, score);
            if(alpha >= beta) return beta;
        } else {
            beta = min(beta, score);
            if(beta <= alpha) return alpha;
        }
    }

    return (jogador == 'W') ? alpha : beta;
}

static vector<lance> moveLists[16];

lance killerMoves[16][2];
bool  killerValid[16][2];
int   historyTable[2][6][8][8]; // [cor][peça][x][y]

// índice de peça para historyTable
inline int pidx(char p){
    if(p=='P') return 0; if(p=='C') return 1; if(p=='B') return 2;
    if(p=='T') return 3; if(p=='D') return 4; return 5;
}

void storeKiller(int depth, lance& m){
    // Só lances quietos
    if(tabuleiro[m.y2][m.x2].first != '.') return;
    // Evita duplicar slot 0
    if(killerValid[depth][0] &&
       killerMoves[depth][0].x1 == m.x1 && killerMoves[depth][0].y1 == m.y1 &&
       killerMoves[depth][0].x2 == m.x2 && killerMoves[depth][0].y2 == m.y2) return;
    killerMoves[depth][1] = killerMoves[depth][0];
    killerValid[depth][1] = killerValid[depth][0];
    killerMoves[depth][0] = m;
    killerValid[depth][0] = true;
}

int scoreLance(const lance& m, int depth, char jogador){
    char cap = tabuleiro[m.y2][m.x2].first;

    //  Capturas: MVV-LVA
    if(cap != '.'){
        auto val = [](char p)->int{ if(p=='D')return 900;if(p=='T')return 500;if(p=='B'||p=='C')return 300;if(p=='P')return 100;return 0; };
        auto atk = [](char p)->int{ if(p=='D')return 6;if(p=='T')return 5;if(p=='B')return 4;if(p=='C')return 3;if(p=='P')return 1;return 0; };
        return 10000 + val(cap) - atk(m.piece);
    }

    //Killer moves
    if(killerValid[depth][0] &&
       killerMoves[depth][0].x1==m.x1 && killerMoves[depth][0].y1==m.y1 &&
       killerMoves[depth][0].x2==m.x2 && killerMoves[depth][0].y2==m.y2) return 9000;
    if(killerValid[depth][1] &&
       killerMoves[depth][1].x1==m.x1 && killerMoves[depth][1].y1==m.y1 &&
       killerMoves[depth][1].x2==m.x2 && killerMoves[depth][1].y2==m.y2) return 8000;

    //  History heuristic
    int cor = (jogador=='W') ? 0 : 1;
    return historyTable[cor][pidx(m.piece)][m.x2][m.y2];
}

int alphaBeta(int depth, int alpha, int beta, char jogador, int ply = 0){
    uint64_t hash = zobristHash;

    // Detecção de repetição dentro da busca:
    // Se a posição já ocorreu na partida real, qualquer nova visita é repetição -> empate
    if(ply > 0){
        auto it = positionCount.find(hash);
        if(it != positionCount.end() && it->second >= 2) return 0;
    }

    if(lastPawnMove >= 50) return 0;

    auto it = transpositionTable.find(hash);
    if(it != transpositionTable.end() && it->second.depth >= depth){
        TTEntry& entry = it->second;
        if(entry.flag == EXACT)      return entry.value;
        if(entry.flag == LOWERBOUND) alpha = max(alpha, entry.value);
        if(entry.flag == UPPERBOUND) beta  = min(beta,  entry.value);
        if(alpha >= beta) return entry.value;
    }

    if(depth == 0) return Quiesce(alpha, beta, jogador);

    char oponente = (jogador == 'W') ? 'B' : 'W';

    // reutiliza vetor pré-alocado
    moveLists[depth].clear();
    moveLists[depth] = geraLances(jogador).first;
    vector<lance>& lances = moveLists[depth];

    if(lances.empty()) {
        if(detectaCheck(jogador)) return (jogador == 'W') ? -99999 + ply : 99999 - ply; // xeque-mate
        return 0; // afogamento
    }

    // ordena com killer + history
    sort(lances.begin(), lances.end(), [&](const lance& a, const lance& b){
        return scoreLance(a, depth, jogador) > scoreLance(b, depth, jogador);
    });

    int originalAlpha = alpha;
    int best = (jogador == 'W') ? -99999 : 99999;
    int cor = (jogador=='W') ? 0 : 1;

    if(jogador == 'W'){
        for(auto& m : lances){
            BoardState state = saveState();
            applyMove(m, jogador);
            int val = alphaBeta(depth-1, alpha, beta, oponente, ply+1);
            restoreState(state);
            best = max(best, val);
            alpha = max(alpha, val);
            if(alpha >= beta){
                storeKiller(depth, const_cast<lance&>(m));
                if(tabuleiro[m.y2][m.x2].first == '.')
                    historyTable[cor][pidx(m.piece)][m.x2][m.y2] += depth * depth;
                break;
            }
        }
    } else {
        for(auto& m : lances){
            BoardState state = saveState();
            applyMove(m, jogador);
            int val = alphaBeta(depth-1, alpha, beta, oponente, ply+1);
            restoreState(state);
            best = min(best, val);
            beta = min(beta, val);
            if(alpha >= beta){
                storeKiller(depth, const_cast<lance&>(m));
                if(tabuleiro[m.y2][m.x2].first == '.')
                    historyTable[cor][pidx(m.piece)][m.x2][m.y2] += depth * depth;
                break;
            }
        }
    }

    TTEntry entry;
    entry.value = best;
    entry.depth = depth;
    if(best <= originalAlpha)      entry.flag = UPPERBOUND;
    else if(best >= beta)          entry.flag = LOWERBOUND;
    else                           entry.flag = EXACT;
    transpositionTable[hash] = entry;

    return best;
}

pair<lance,int> melhorLance(char jogador, int maxDepth){
    //limpa tabelas a cada nova busca
    memset(killerMoves, 0, sizeof(killerMoves));
    memset(killerValid, 0, sizeof(killerValid));
    memset(historyTable, 0, sizeof(historyTable));

    vector<lance> lances = geraLances(jogador).first;
    
    lance best;
    int bestVal;    

    if(lances.empty()) {
    return {lance{}, evaluate()};
}
    //Iterative deepening
    for(int i = 1;i <= maxDepth;i++){
        int alpha = -99999, beta = 99999;
        best = lances[0];
        bestVal = (jogador == 'W') ? alpha : beta;
        
        for(auto& m : lances){
            BoardState state = saveState();
            applyMove(m, jogador);
    
            uint64_t hash = zobristHash;
            auto it = positionCount.find(zobristHash);
            bool isRepetition = (it != positionCount.end() && it->second >= 2);
            
            int val;
            if(isRepetition) val = 0;
            else val = alphaBeta(i-1, alpha, beta, jogador == 'W' ? 'B' : 'W', 1);

            restoreState(state);
            
            if(jogador == 'W') {
                if(val > bestVal) { bestVal = val; best = m; }
                alpha = max(alpha, bestVal);   // Diminui janela para proxima busca
            } else {
                if(val < bestVal) { bestVal = val; best = m; }
                beta = min(beta, bestVal);
            }
        }
        auto itBest = find(lances.begin(), lances.end(), best);
        if(itBest != lances.end())
            swap(lances[0], *itBest);
    }
    return {best, bestVal};
}

string lookupOpening(SQLite::Database& db, const string& moveSeq) {
    SQLite::Statement q(db,
    "SELECT opening_name FROM openings WHERE INSTR(?, move_sequence) = 1 "
    "ORDER BY LENGTH(move_sequence) DESC LIMIT 1");
    q.bind(1, moveSeq);
    if (q.executeStep())
        return q.getColumn(0).getString(); 
    return "";
}

lance getBookMove(SQLite::Database& db, char jogador) {

    string alph = "ABCDEFGH";

    string moveSeq;
    for(auto& h : historico)
        moveSeq += string(1,h.first) + h.second + " ";

    if(!moveSeq.empty())
        moveSeq.pop_back();

    string pattern = moveSeq.empty() ? "%" : moveSeq + " %";

    string query =
        "SELECT move_sequence "
        "FROM openings "
        "WHERE move_sequence LIKE ? "
        "ORDER BY eval DESC "
        "LIMIT 40";

    SQLite::Statement q(db, query);
    q.bind(1, pattern);

    vector<string> candidates;

    while(q.executeStep()){
        candidates.push_back(q.getColumn(0).getString());
    }

    if(candidates.empty())
        return {-1,-1,-1,-1,'?'};

    static mt19937 rng(
        chrono::steady_clock::now().time_since_epoch().count()
    );

    string bookMoves = candidates[rng() % candidates.size()];

    string nextMove = bookMoves.substr(
        moveSeq.empty() ? 0 : moveSeq.size()+1
    );

    size_t space = nextMove.find(' ');
    if(space != string::npos)
        nextMove = nextMove.substr(0, space);

    if(nextMove.size() == 7){
        char piece = nextMove[0];
        size_t x1 = alph.find(nextMove[1]);
        int y1 = nextMove[2] - '1';
        size_t x2 = alph.find(nextMove[5]);
        int y2 = nextMove[6] - '1';

        lance m = {(int)x1, y1, (int)x2, y2, piece};
        if(
            tabuleiro[y1][x1].first == piece &&
            tabuleiro[y1][x1].second == jogador &&
            movimentoValido(piece,jogador,x1,x2,y1,y2)
        ){
            return m;
        }

        if(x1 != string::npos && x2 != string::npos)
            return {(int)x1,y1,(int)x2,y2,piece};
    }

    return {-1,-1,-1,-1,'?'};
}

void aiMove(pair<lance,int>aiVal,char jogador){
    string alph = "ABCDEFGH";
    lance ai = aiVal.first; 
    char captura = tabuleiro[ai.y2][ai.x2].first;
    if(captura != '.') capturou = true;
    if(capturou && captura != 'P') numPecas--;

    
    applyMove(ai, jogador);
    printMove(ai);
    historico.emplace_back(ai.piece, alph[ai.x1] + to_string(ai.y1+1) + "->" + alph[ai.x2] + to_string(ai.y2+1));

    if(ai.piece == 'P') lastPawnMove = 0;
    else lastPawnMove++;

    uint64_t hash = zobristHash;
    positionCount[zobristHash]++;
    if(positionCount[zobristHash] >= 3){
        continua = 0;
        clearScreen();
        printaTabuleiro();
        cout << "EMPATE\n";
    }   
    turno ^= 1;

    if(captura == 'R') continua = false;

    char oponenteCheck = turno ? 'W' : 'B';
    vector<lance> lancesOponente = geraLances(oponenteCheck).first;
    vector<lance> validosOponente;
    for(lance m : lancesOponente){
        BoardState state = saveState();
        applyMove(m, oponenteCheck);
        if(!detectaCheck(oponenteCheck)) validosOponente.emplace_back(m);
        restoreState(state);
    }
    if(validosOponente.empty()){
        continua = 0;
        clearScreen();
        printaTabuleiro();
        if(detectaCheck(oponenteCheck)){ cout << "CHEQUEMATE\n"; tocarSom(somCheck);}
        else cout << "AFOGAMENTO\n";
        return;
    }
    
    if(lastPawnMove >= 50){
        continua = 0;
        clearScreen();
        printaTabuleiro();
        cout << "EMPATE\n";
    }   
}

void sequencia(string moveSeq){
    for (auto& h : historico)
    moveSeq += string(1, h.first) + h.second + " ";
    if (!moveSeq.empty()) moveSeq.pop_back();
}

int main(){
    fs::path pasta_atual = fs::current_path();
    somW = pasta_atual / "sons" / "White.wav"; 
    somB = pasta_atual / "sons" / "Black.wav"; 
    somCheck = pasta_atual / "sons" / "Check.wav"; 
    somCavalo = pasta_atual / "sons" / "cavalo.mp3"; 
    somCaptura = pasta_atual / "sons" / "captura.mp3"; 
    somRoque = pasta_atual / "sons" / "roque.mp3"; 
    somOver = pasta_atual / "sons" / "over.mp3"; 

    try {
        SQLite::Database db("openings.db", SQLite::OPEN_READONLY);
        string alph = "ABCDEFGH";
        int depth = 7;

        auto getBestMove = [&](char j) -> pair<lance, int> {
            lance book = getBookMove(db, j);
            if (book.x1 != -1) {
                cout << "[Livro de aberturas]\n";
                return {book, 0};
            }
            return melhorLance(j, depth);
        };

        char pecas; 
        cout << "Escolha as peças (W,B)" << endl;
        cin >> pecas;
        pecasEscolhidas = pecas;

        setupUTF8();
        clearScreen();
        initZobrist();
        criaTabuleiro();
    
    while(continua){
        if(pecas == 'M' || pecas == 'B'){
            if(continua){
                pair<lance,int> aiVal = getBestMove('W');
                aiMove(aiVal, 'W');
                clearScreen(); printaTabuleiro();
                if(capturou) {std::thread(tocarSom,somCaptura).detach(); capturou = false;}
                else std::thread(tocarSom,somW).detach();

                string moveSeq = ""; sequencia(moveSeq);
                string opening = lookupOpening(db, moveSeq);
                //if(!opening.empty()) cout << "Abertura: " << opening << "\n";
                if(pecas == 'M') cout << "Vantagem: " << double(aiVal.second)/100 << "\n";
                if(detectaCheck('B')) {
                    cout << "CHECK\n";
                    std::thread(tocarSom,somCheck).detach();
               }
            }
        }

        if(pecas == 'M'){
            if(continua){
                pair<lance,int> aiVal = getBestMove('B');
                aiMove(aiVal, 'B');
                clearScreen(); printaTabuleiro();
                if(capturou) {std::thread(tocarSom,somCaptura).detach(); capturou = false;}
                else std::thread(tocarSom,somB).detach();

                string moveSeq = ""; sequencia(moveSeq);
                string opening = lookupOpening(db, moveSeq);
                //if(!opening.empty()) cout << "Abertura: " << opening << "\n";
                cout << "Vantagem: " << double(aiVal.second)/100 << "\n";

                if(detectaCheck('W')) {
                    cout << "CHECK\n";
                std::thread(tocarSom,somCheck).detach();
               }
            }
            continue;
        }

        if(pecas != 'M')
        {
            char quemMove = turno ? 'W' : 'B';
            cout << (quemMove == 'W' ? "Brancas" : "Pretas") << " jogam:\n";

            do {
                if(!continua) break;
                move();
            } while(erro);

            if(!continua) break;
            
            clearScreen(); printaTabuleiro();

            if(turno){
                if(capturou) {std::thread(tocarSom,somCaptura).detach(); capturou = false;}
                else std::thread(tocarSom,somW).detach();
                } else{
                if(capturou) {std::thread(tocarSom,somCaptura).detach(); capturou = false;}
                else std::thread(tocarSom,somB).detach();
                }

            pair<lance,int> aiVal = getBestMove('B');
            string moveSeq = ""; sequencia(moveSeq);
            string opening = lookupOpening(db, moveSeq);
            //if(!opening.empty()) cout << "Abertura: " << opening << "\n";
            if(pecas != 'T') cout << "Vantagem: " << double(aiVal.second)/100 << "\n";

            char quemVaiMover = turno ? 'W' : 'B';
            if(detectaCheck(quemVaiMover)) {
                cout << "CHECK\n";
                std::thread(tocarSom,somCheck).detach();
            }
        }

        if(pecas == 'W'){
            if(continua){
                pair<lance,int> aiVal = getBestMove('B');
                aiMove(aiVal, 'B');
                clearScreen(); printaTabuleiro();
                if(capturou) {std::thread(tocarSom,somCaptura).detach(); capturou = false;}
                else std::thread(tocarSom,somB).detach();

                string moveSeq = ""; sequencia(moveSeq);
                string opening = lookupOpening(db, moveSeq);
                //if(!opening.empty()) cout << "Abertura: " << opening << "\n";
                cout << "Vantagem: " << double(aiVal.second)/100 << "\n";
                if(detectaCheck('B')) {
                    cout << "CHECK\n";
                    std::thread(tocarSom,somCheck).detach();
               }
            }
        }
    }
    std::thread(tocarSom,somOver).detach();

    } catch (std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
    }
}