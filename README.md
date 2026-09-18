# ChessGameWithEngine
Chess terminal game with enemy engine written in c++. Engine uses AlphaBeta-Prunning to search for maximized score positions for each player, MVV-LVA (Most Valuable Victim - Least Valuable Attacker) heuristics based move sort, Killer Moves, Quiescence function, Iterative Deepening with Transposition Table using ZobristHash, Opening book (Sqlite Db-Conection) etc. Complex heuristics based eval function with concepts such as pawn islands, piece placement, attacked squares and king safety.

The engine utilizes a well crafted evaluation function. An NNUE was trained in python and implemented in C++ just as a learning goal, but not integrated in the engine yet. NNUE transforms FEN encoded chess position to features in a 769 sized vector (64 squares * 12 pieces * 2 players + side to move) and outputs predicted stockfish evaluation of position, although the NNUE model needs more training since it seemed to have almost randomic predictions. I plan on continuing this project in the future.

The engine has a performance around 1800-1900 ELO with consisting results. Major improvements include bitboard representation for increased search depth and NNUE implementing.
