//2025-10-09
// 三维井字棋最优摆放模型 version 1.2

/*
本代码基于minimax算法评估局面，同时剪枝提高效率
待优化：
minimax算法为回合制游戏设计，当前版本未考虑时间，速度等因素  -- 未来版本将引入蒙特卡洛树搜索（MCTS）等更复杂算法，或修改代码逻辑

*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <limits>
#include <string>
#include <unordered_set>
#include <cmath>

using namespace std;

enum class Player { NONE, US, OPPONENT };

// 移动类型
enum class MoveType { 
    SINGLE,               // 单个放置(R1 R2 R2Lifted)
    R2_LIFTED_DOUBLE,     // 被举起的R2同时放置两个方块(R2Lifted)
    R2_DOUBLE,            // R2单独在中层放置两个相邻方块(R2)
    PUSH,                 // 推下对方KFS(R1)
    COMBINED,             // R1和R2同时放置(R1andR2)
    PUSH_PLACE,           // R1推下同时R2放置(R1andR2)
    COMBINED_R2_DOUBLE,   // R1放置底层同时R2在中层放置两个方块(R1andR2)
    PUSH_R2_DOUBLE        // R1推下同时R2在中层放置两个方块(R1andR2)
};

// 移动表示
struct Move {
    MoveType type;
    pair<int, int> pos1;  // 第一个位置 在combined中为R1摆放或推下位置
    pair<int, int> pos2;  // 第二个位置 在combined中为R2摆放位置
    pair<int, int> pos3;  // 第三个位置 在COMBINED_R2_DOUBLE和PUSH_R2_DOUBLE中为R2的第二个摆放位置
    int timeCost;         // 时间代价 只在findOptions中使用，并未影响决策
    
    Move(MoveType t, pair<int, int> p1, pair<int, int> p2 = {-1, -1}, pair<int, int> p3 = {-1, -1}) 
        : type(t), pos1(p1), pos2(p2), pos3(p3) {
        // 设置时间代价
        if (type == MoveType::PUSH || type == MoveType::PUSH_PLACE || 
            type == MoveType::PUSH_R2_DOUBLE) {
            timeCost = 2;
        } else {
            timeCost = 1;
        }
    }
    
    // 重载==运算符用于比较移动
    bool operator==(const Move& other) const {
        return type == other.type && pos1 == other.pos1 && pos2 == other.pos2 && pos3 == other.pos3;
    }
};

class ThreeDTicTacToe {
private:
    // 3层×3列的棋盘（总共9个格子）
    vector<vector<Player>> board; // [层][列]
    
    // 资源限制
    int usKFS_R1 = 3;  // 我方R1 KFS数量
    int usKFS_R2 = 3;  // 我方R2 KFS数量
    int opponentKFS_R1 = 3;  // 对方R1 KFS数量
    int opponentKFS_R2 = 3;  // 对方R2 KFS数量
    bool usWeaponUsed = false; // 我方兵器是否已使用
    bool opponentWeaponUsed = false; // 对方兵器是否已使用

    // 得分规则
    const int SCORE_BOTTOM = 30;  // 底层每个KFS
    const int SCORE_MIDDLE = 40; // 中层每个KFS  
    const int SCORE_TOP = 80;    // 顶层每个KFS
    const int WIN_SCORE = 10000; // 获胜的极大值
    
    // 获胜模式：垂直列和对角线
    vector<vector<pair<int, int>>> winningPatterns;

public:
    // 设置我方兵器状态
    void Set_usWeaponUsed(bool used) {
        usWeaponUsed = used;
    }

    // 设置对方兵器状态
    void Set_opponentWeaponUsed(bool used) {
        opponentWeaponUsed = used;
    }

    //棋盘初始化，构造进行
    ThreeDTicTacToe() {
        // 初始化3层×3列棋盘
        board.resize(3, vector<Player>(3, Player::NONE));
        
        // 生成获胜模式
        generateWinningPatterns();
    }
    
    void generateWinningPatterns() {
        // 垂直列获胜（同一列的3层）
        for (int col = 0; col < 3; col++) {
            winningPatterns.push_back({
                {0, col}, // 底层
                {1, col}, // 中层  
                {2, col}  // 顶层
            });
        }
        
        // 对角线获胜（空间对角线）
        // 从(0,0)到(2,2)的对角线
        winningPatterns.push_back({
            {0, 0}, // 底层左下
            {1, 1}, // 中层中
            {2, 2}  // 顶层右上
        });
        
        // 从(0,2)到(2,0)的对角线
        winningPatterns.push_back({
            {0, 2}, // 底层右下
            {1, 1}, // 中层中
            {2, 0}  // 顶层左上
        });
    }
    
    // 检查位置是否相邻（用于复合移动）
    bool arePositionsAdjacent(pair<int, int> pos1, pair<int, int> pos2) {
        int layer1 = pos1.first, col1 = pos1.second;
        int layer2 = pos2.first, col2 = pos2.second;
        
        // 同层相邻：水平、垂直或对角线
        if (layer1 == layer2) {
            return abs(col1 - col2) <= 1; 
        }
        // 跨层相邻：同一列或对角线
        else if (abs(layer1 - layer2) == 1) {
            return (col1 == col2) || (abs(col1 - col2) == 1);
        }
        
        return false;
    }
    
    // 检查是否获胜
    bool checkWin(Player player) {
        for (const auto& pattern : winningPatterns) {
            bool win = true;
            for (const auto& pos : pattern) {
                int layer = pos.first;
                int col = pos.second;
                if (board[layer][col] != player) {
                    win = false;
                    break;
                }
            }
            if (win) return true;
        }
        return false;
    }
    
    // 获取当前得分
    int getCurrentScore(Player player) {
        int score = 0;
        for (int layer = 0; layer < 3; layer++) {
            for (int col = 0; col < 3; col++) {
                if (board[layer][col] == player) {
                    switch (layer) {
                        case 0: score += SCORE_BOTTOM; break;  // 底层
                        case 1: score += SCORE_MIDDLE; break; // 中层
                        case 2: score += SCORE_TOP; break;    // 顶层
                    }
                }
            }
        }
        return score;
    }
    
    // 评估函数：综合考虑获胜可能性和得分，只在思考深度为零或者对局结束调用
    int evaluate(Player player) {
        Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
        
        // 如果我方获胜，返回极大值
        if (checkWin(player)) return WIN_SCORE;
        
        // 如果对方获胜，返回极小值  
        if (checkWin(opponent)) return -WIN_SCORE;
        
        // 计算当前得分差
        int scoreDiff = getCurrentScore(player) - getCurrentScore(opponent);
        
        // 评估威胁和机会
        int threatScore = evaluateThreats(player, opponent);
        
        return scoreDiff + threatScore;
    }
    
    // 评估威胁和机会
    int evaluateThreats(Player player, Player opponent) {
        int threatScore = 0;
        
        // 检查每个获胜模式
        for (const auto& pattern : winningPatterns) {
            int playerCount = 0;
            int opponentCount = 0;
            int emptyCount = 0;
            
            for (const auto& pos : pattern) {
                int layer = pos.first;
                int col = pos.second;
                
                if (board[layer][col] == player) playerCount++;
                else if (board[layer][col] == opponent) opponentCount++;
                else emptyCount++;
            }
            
            // // 如果我方有获胜机会
            // if (playerCount == 2 && emptyCount == 1) {
            //     threatScore += 500; // 即将获胜，高优先级
            // }
            // // 如果对方有获胜威胁
            // else if (opponentCount == 2 && emptyCount == 1) {
            //     threatScore -= 600; // 必须阻止，更高优先级
            // }
            // // 如果我方有发展潜力
            // else if (playerCount == 1 && emptyCount == 2) {
            //     threatScore += 100;
            // }

            // 如果我方有获胜机会
            if (playerCount == 2 && emptyCount == 1) {
                threatScore += 50; // 即将获胜，高优先级
            }
            // 如果对方有获胜威胁
            else if (opponentCount == 2 && emptyCount == 1) {
                threatScore -= 60; // 必须阻止，更高优先级
            }
            // 如果我方有发展潜力
            else if (playerCount == 1 && emptyCount == 2) {
                threatScore += 10;
            }
        }
        
        return threatScore;
    }
    
    // 获取所有可用移动
    vector<Move> getAvailableMoves(Player player, const string& robotType = "ALL") {
        vector<Move> moves;
        Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
        
        // 如果指定了特定机器人类型，只生成该类型的移动
        if (robotType != "ALL") {
            return getSingleRobotMoves(player, robotType);
        }
        
        // 生成所有单个机器人的移动
        vector<Move> r1Moves = getSingleRobotMoves(player, "R1");
        vector<Move> r2Moves = getSingleRobotMoves(player, "R2");
        vector<Move> r2LiftedMoves = getSingleRobotMoves(player, "R2_LIFTED");
        
        // 添加单个机器人移动
        moves.insert(moves.end(), r1Moves.begin(), r1Moves.end());
        moves.insert(moves.end(), r2Moves.begin(), r2Moves.end());
        moves.insert(moves.end(), r2LiftedMoves.begin(), r2LiftedMoves.end());
        
        // 生成R1和R2的组合移动
        vector<Move> combinedMoves = getCombinedMoves(player, r1Moves, r2Moves);
        moves.insert(moves.end(), combinedMoves.begin(), combinedMoves.end());
        
        // 生成R1推下和R2放置的组合移动
        vector<Move> pushPlaceMoves = getPushPlaceMoves(player, r2Moves);
        moves.insert(moves.end(), pushPlaceMoves.begin(), pushPlaceMoves.end());
        
        // 生成R1放置底层同时R2在中层放置两个方块的组合移动
        vector<Move> combinedR2DoubleMoves = getCombinedR2DoubleMoves(player, r1Moves);
        moves.insert(moves.end(), combinedR2DoubleMoves.begin(), combinedR2DoubleMoves.end());
        
        // 生成R1推下同时R2在中层放置两个方块的组合移动
        vector<Move> pushR2DoubleMoves = getPushR2DoubleMoves(player);
        moves.insert(moves.end(), pushR2DoubleMoves.begin(), pushR2DoubleMoves.end());
        
        return moves;
    }
    
    // 获取单个机器人的移动
    vector<Move> getSingleRobotMoves(Player player, const string& robotType) {
        vector<Move> moves;
        Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
        
        // 根据机器人类型确定可放置的层和所需的KFS类型
        int allowedLayer = -1;
        bool useR1KFS = false;
        bool useR2KFS = false;
        
        if (robotType == "R1") {
            allowedLayer = 0;        // R1只能放底层
            useR1KFS = true;
        } else if (robotType == "R2") {
            allowedLayer = 1;        // R2只能放中层
            useR2KFS = true;
        } else if (robotType == "R2_LIFTED") {
            allowedLayer = 2;        // 被举起的R2放顶层
            useR2KFS = true;
        }
        
        // 检查资源限制
        bool canPlace = false;
        if (useR1KFS) {
            if (player == Player::US && usKFS_R1 > 0) canPlace = true;
            else if (player == Player::OPPONENT && opponentKFS_R1 > 0) canPlace = true;
        }
        if (useR2KFS) {
            if (player == Player::US && usKFS_R2 > 0) canPlace = true;
            else if (player == Player::OPPONENT && opponentKFS_R2 > 0) canPlace = true;
        }
        
        // 生成单个放置移动
        for (int layer = 0; layer < 3; layer++) {
            for (int col = 0; col < 3; col++) {
                if (board[layer][col] == Player::NONE) {
                    // 检查层限制
                    if (allowedLayer != -1 && layer != allowedLayer) continue;
                    
                    // 检查资源限制
                    if (!canPlace) continue;
                    
                    moves.push_back(Move(MoveType::SINGLE, {layer, col}));
                }
            }
        }
        
        // 如果是R2（中层），可以生成R2_DOUBLE移动（在中层同时放置两个相邻方块）
        if (robotType == "R2") {
            // 检查R2 KFS资源是否足够
            bool hasEnoughR2 = (player == Player::US && usKFS_R2 >= 2) || 
                            (player == Player::OPPONENT && opponentKFS_R2 >= 2);
            
            if (hasEnoughR2) {
                // 生成所有可能的R2_DOUBLE移动（两个位置都在中层且相邻）
                for (int col1 = 0; col1 < 3; col1++) {
                    if (board[1][col1] != Player::NONE) continue;
                    
                    for (int col2 = col1 + 1; col2 < 3; col2++) {
                        if (board[1][col2] != Player::NONE) continue;
                        
                        // 检查位置是否相邻（在中层，水平相邻或对角线相邻）
                        if (arePositionsAdjacent({1, col1}, {1, col2})) {
                            moves.push_back(Move(MoveType::R2_DOUBLE, {1, col1}, {1, col2}));
                        }
                    }
                }
            }
        }
        
        // 如果是被举起的R2，可以生成R2_LIFTED_DOUBLE移动（同时放置两个方块）
        if (robotType == "R2_LIFTED") {
            // 检查R2 KFS资源是否足够
            bool hasEnoughR2 = (player == Player::US && usKFS_R2 >= 2) || 
                            (player == Player::OPPONENT && opponentKFS_R2 >= 2);
            
            if (hasEnoughR2) {
                // 生成所有可能的R2_LIFTED_DOUBLE移动（两个位置都在顶层或一个顶层一个中层）
                for (int layer1 = 1; layer1 <= 2; layer1++) { // 中层或顶层
                    for (int col1 = 0; col1 < 3; col1++) {
                        if (board[layer1][col1] != Player::NONE) continue;
                        
                        for (int layer2 = 1; layer2 <= 2; layer2++) { // 中层或顶层
                            for (int col2 = 0; col2 < 3; col2++) {
                                // 确保第一个位置"小于"第二个位置，避免重复
                                if (make_pair(layer1, col1) >= make_pair(layer2, col2)) continue;
                                
                                if (board[layer2][col2] != Player::NONE) continue;
                                
                                // 检查位置是否相邻
                                if (arePositionsAdjacent({layer1, col1}, {layer2, col2})) {
                                    moves.push_back(Move(MoveType::R2_LIFTED_DOUBLE, {layer1, col1}, {layer2, col2}));
                                }
                            }
                        }
                    }
                }
            }
        }
        
        bool weaponUsed = (player == Player::US) ? usWeaponUsed : opponentWeaponUsed;
        // 生成推下移动（只在必要时）
        if (robotType=="R1" && !weaponUsed && evaluateThreats(player, opponent) < -500) {
            for (int layer = 0; layer < 3; layer++) {
                for (int col = 0; col < 3; col++) {
                    if (board[layer][col] == opponent) {
                        moves.push_back(Move(MoveType::PUSH, {layer, col}));
                    }
                }
            }
        }
        
        return moves;
    }
    
    // 获取R1和R2的组合移动
    vector<Move> getCombinedMoves(Player player, const vector<Move>& r1Moves, const vector<Move>& r2Moves) {
        vector<Move> combinedMoves;
        
        // 检查资源是否足够同时进行R1和R2移动
        bool hasR1Resource = (player == Player::US && usKFS_R1 > 0) || 
                           (player == Player::OPPONENT && opponentKFS_R1 > 0);
        bool hasR2Resource = (player == Player::US && usKFS_R2 > 0) || 
                           (player == Player::OPPONENT && opponentKFS_R2 > 0);
        
        if (!hasR1Resource || !hasR2Resource) {
            return combinedMoves;
        }
        
        // 生成所有可能的R1和R2移动组合
        for (const auto& r1Move : r1Moves) {
            if (r1Move.type != MoveType::SINGLE) continue;
            
            for (const auto& r2Move : r2Moves) {
                if (r2Move.type != MoveType::SINGLE) continue;
                
                if (r1Move.pos1 == r2Move.pos1) continue;
                
                combinedMoves.push_back(Move(MoveType::COMBINED, r1Move.pos1, r2Move.pos1));
            }
        }
        
        return combinedMoves;
    }
    
    // 获取R1推下和R2放置的组合移动
    vector<Move> getPushPlaceMoves(Player player, const vector<Move>& r2Moves) {
        vector<Move> pushPlaceMoves;
        Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
        
        bool weaponUsed = (player == Player::US) ? usWeaponUsed : opponentWeaponUsed;
        
        // 检查条件：兵器未使用，且对方有获胜威胁
        if (weaponUsed || evaluateThreats(player, opponent) >= -500) {
            return pushPlaceMoves;
        }
        
        // 检查资源是否足够进行R2放置
        bool hasR2Resource = (player == Player::US && usKFS_R2 > 0) || 
                           (player == Player::OPPONENT && opponentKFS_R2 > 0);
        
        if (!hasR2Resource) {
            return pushPlaceMoves;
        }
        
        // 生成所有可能的推下和放置组合
        for (int pushLayer = 0; pushLayer < 3; pushLayer++) {
            for (int pushCol = 0; pushCol < 3; pushCol++) {
                // 检查是否可以推下这个位置
                if (board[pushLayer][pushCol] != opponent) continue;
                
                // 对于每个可能的推下位置，找到所有R2可以放置的位置（不能是推下的位置）
                for (const auto& r2Move : r2Moves) {
                    if (r2Move.type != MoveType::SINGLE) continue;
                    
                    int placeLayer = r2Move.pos1.first;
                    int placeCol = r2Move.pos1.second;
                    
                    // R2不能放在R1推下的位置
                    if (pushLayer == placeLayer && pushCol == placeCol) continue;
                    
                    // 创建推下+放置组合移动
                    pushPlaceMoves.push_back(Move(MoveType::PUSH_PLACE, {pushLayer, pushCol}, {placeLayer, placeCol}));
                }
            }
        }
        
        return pushPlaceMoves;
    }
    
    // 获取R1放置底层同时R2在中层放置两个方块的组合移动
    vector<Move> getCombinedR2DoubleMoves(Player player, const vector<Move>& r1Moves) {
        vector<Move> combinedR2DoubleMoves;
        
        // 检查资源是否足够
        bool hasR1Resource = (player == Player::US && usKFS_R1 > 0) || 
                           (player == Player::OPPONENT && opponentKFS_R1 > 0);
        bool hasR2Resource = (player == Player::US && usKFS_R2 >= 2) || 
                           (player == Player::OPPONENT && opponentKFS_R2 >= 2);
        
        if (!hasR1Resource || !hasR2Resource) {
            return combinedR2DoubleMoves;
        }
        
        // 生成所有可能的R2_DOUBLE移动
        vector<Move> r2DoubleMoves;
        for (int col1 = 0; col1 < 3; col1++) {
            if (board[1][col1] != Player::NONE) continue;
            
            for (int col2 = col1 + 1; col2 < 3; col2++) {
                if (board[1][col2] != Player::NONE) continue;
                
                // 检查位置是否相邻
                if (arePositionsAdjacent({1, col1}, {1, col2})) {
                    r2DoubleMoves.push_back(Move(MoveType::R2_DOUBLE, {1, col1}, {1, col2}));
                }
            }
        }
        
        // 生成所有可能的组合
        for (const auto& r1Move : r1Moves) {
            if (r1Move.type != MoveType::SINGLE) continue;
            
            for (const auto& r2DoubleMove : r2DoubleMoves) {
                // 确保R1移动在底层
                if (r1Move.pos1.first != 0) continue;
                
                // 确保位置不冲突
                if (r1Move.pos1 == r2DoubleMove.pos1 || r1Move.pos1 == r2DoubleMove.pos2) continue;
                
                // 使用三个位置：R1位置 + R2两个位置
                combinedR2DoubleMoves.push_back(Move(MoveType::COMBINED_R2_DOUBLE, 
                                                   r1Move.pos1, 
                                                   r2DoubleMove.pos1,
                                                   r2DoubleMove.pos2));
            }
        }
        
        return combinedR2DoubleMoves;
    }
    
    // 获取R1推下同时R2在中层放置两个方块的组合移动
    vector<Move> getPushR2DoubleMoves(Player player) {
        vector<Move> pushR2DoubleMoves;
        Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
        
        bool weaponUsed = (player == Player::US) ? usWeaponUsed : opponentWeaponUsed;
        
        // 检查条件：兵器未使用，且对方有获胜威胁
        if (weaponUsed || evaluateThreats(player, opponent) >= -500) {
            return pushR2DoubleMoves;
        }
        
        // 检查资源是否足够进行R2放置
        bool hasR2Resource = (player == Player::US && usKFS_R2 >= 2) || 
                           (player == Player::OPPONENT && opponentKFS_R2 >= 2);
        
        if (!hasR2Resource) {
            return pushR2DoubleMoves;
        }
        
        // 生成所有可能的R2_DOUBLE移动
        vector<Move> r2DoubleMoves;
        for (int col1 = 0; col1 < 3; col1++) {
            if (board[1][col1] != Player::NONE) continue;
            
            for (int col2 = col1 + 1; col2 < 3; col2++) {
                if (board[1][col2] != Player::NONE) continue;
                
                // 检查位置是否相邻
                if (arePositionsAdjacent({1, col1}, {1, col2})) {
                    r2DoubleMoves.push_back(Move(MoveType::R2_DOUBLE, {1, col1}, {1, col2}));
                }
            }
        }
        
        // 生成所有可能的推下和R2_DOUBLE组合
        for (int pushLayer = 0; pushLayer < 3; pushLayer++) {
            for (int pushCol = 0; pushCol < 3; pushCol++) {
                // 检查是否可以推下这个位置
                if (board[pushLayer][pushCol] != opponent) continue;
                
                // 对于每个可能的推下位置，找到所有R2_DOUBLE可以放置的位置
                for (const auto& r2DoubleMove : r2DoubleMoves) {
                    // R2不能放在R1推下的位置
                    if ((pushLayer == 1 && pushCol == r2DoubleMove.pos1.second) ||
                        (pushLayer == 1 && pushCol == r2DoubleMove.pos2.second)) {
                        continue;
                    }
                    
                    // 创建推下+R2_DOUBLE组合移动，使用三个位置
                    pushR2DoubleMoves.push_back(Move(MoveType::PUSH_R2_DOUBLE, 
                                                   {pushLayer, pushCol}, 
                                                   r2DoubleMove.pos1,
                                                   r2DoubleMove.pos2));
                }
            }
        }
        
        return pushR2DoubleMoves;
    }
    
    // 执行移动
    bool executeMove(const Move& move, Player player) {
        bool& weaponUsed = (player == Player::US) ? usWeaponUsed : opponentWeaponUsed;
        
        switch (move.type) {
            case MoveType::SINGLE: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                if (board[layer][col] != Player::NONE) return false;

                if (layer == 0) {
                    if (player == Player::US) {
                        if (usKFS_R1 <= 0) return false;
                        usKFS_R1--;
                    } else {
                        if (opponentKFS_R1 <= 0) return false;
                        opponentKFS_R1--;
                    }
                } else { // 中层和顶层使用R2 KFS
                    if (player == Player::US) {
                        if (usKFS_R2 <= 0) return false;
                        usKFS_R2--;
                    } else {
                        if (opponentKFS_R2 <= 0) return false;
                        opponentKFS_R2--;
                    }
                }
                board[layer][col] = player;
                break;
            }
                
            case MoveType::R2_LIFTED_DOUBLE: {
                int layer1 = move.pos1.first, col1 = move.pos1.second;
                int layer2 = move.pos2.first, col2 = move.pos2.second;
                
                if (board[layer1][col1] != Player::NONE || 
                    board[layer2][col2] != Player::NONE) return false;
                
                if (player == Player::US) {
                    if (usKFS_R2 < 2) return false;
                    usKFS_R2 -= 2;
                } else {
                    if (opponentKFS_R2 < 2) return false;
                    opponentKFS_R2 -= 2;
                }
                
                board[layer1][col1] = player;
                board[layer2][col2] = player;
                break;
            }
                
            case MoveType::R2_DOUBLE: {
                int layer1 = move.pos1.first, col1 = move.pos1.second;
                int layer2 = move.pos2.first, col2 = move.pos2.second;
                
                // 确保两个位置都在中层
                if (layer1 != 1 || layer2 != 1) return false;
                
                if (board[layer1][col1] != Player::NONE || 
                    board[layer2][col2] != Player::NONE) return false;
                
                if (player == Player::US) {
                    if (usKFS_R2 < 2) return false;
                    usKFS_R2 -= 2;
                } else {
                    if (opponentKFS_R2 < 2) return false;
                    opponentKFS_R2 -= 2;
                }
                
                board[layer1][col1] = player;
                board[layer2][col2] = player;
                break;
            }
                
            case MoveType::PUSH: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                if (board[layer][col] != ((player == Player::US) ? Player::OPPONENT : Player::US)) 
                    return false;
                
                // 标记兵器已使用
                weaponUsed = true;
                board[layer][col] = Player::NONE;
                // 恢复资源
                if(player == Player::US) {
                   if(layer == 0) {
                       opponentKFS_R1++;
                   } else {
                       if(layer>=1) opponentKFS_R2++;
                   }
                } else {
                    if(layer == 0) {
                       usKFS_R1++;
                   } else {
                       if(layer>=1) usKFS_R2++;
                   }
                }
                break;
            }
                
            case MoveType::COMBINED: {
                // 执行R1移动（底层）
                int r1Layer = move.pos1.first, r1Col = move.pos1.second;
                if (board[r1Layer][r1Col] != Player::NONE) return false;
                
                // 执行R2移动（中层）
                int r2Layer = move.pos2.first, r2Col = move.pos2.second;
                if (board[r2Layer][r2Col] != Player::NONE) return false;
                
                // 检查资源
                if (player == Player::US) {
                    if (usKFS_R1 <= 0 || usKFS_R2 <= 0) return false;
                    usKFS_R1--;
                    usKFS_R2--;
                } else {
                    if (opponentKFS_R1 <= 0 || opponentKFS_R2 <= 0) return false;
                    opponentKFS_R1--;
                    opponentKFS_R2--;
                }
                
                board[r1Layer][r1Col] = player;
                board[r2Layer][r2Col] = player;
                break;
            }
                
            case MoveType::PUSH_PLACE: {
                Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
                
                // 检查兵器是否已使用
                if (weaponUsed) return false;
                
                // 执行推下操作
                int pushLayer = move.pos1.first, pushCol = move.pos1.second;
                if (board[pushLayer][pushCol] != opponent) return false;
                
                // 执行放置操作
                int placeLayer = move.pos2.first, placeCol = move.pos2.second;
                if (board[placeLayer][placeCol] != Player::NONE) return false;
                
                // 检查R2资源
                if (player == Player::US) {
                    if (usKFS_R2 <= 0) return false;
                    usKFS_R2--;
                } else {
                    if (opponentKFS_R2 <= 0) return false;
                    opponentKFS_R2--;
                }
                // 修复资源
                if(player == Player::US) {
                   if(pushLayer == 0) {
                       opponentKFS_R1++;
                   } else {
                       if(pushLayer>=1) opponentKFS_R2++;
                   }
                } else {
                    if(pushLayer == 0) {
                       usKFS_R1++;
                   } else {
                       if(pushLayer >= 1) usKFS_R2++;
                   }
                }
                
                // 执行操作
                board[pushLayer][pushCol] = Player::NONE; // 推下
                board[placeLayer][placeCol] = player;     // 放置
                weaponUsed = true; // 标记兵器已使用
                break;
            }
                
            case MoveType::COMBINED_R2_DOUBLE: {
                // 执行R1移动（底层）
                int r1Layer = move.pos1.first, r1Col = move.pos1.second;
                if (board[r1Layer][r1Col] != Player::NONE) return false;
                
                // 执行R2_DOUBLE移动（中层）
                int r2Layer1 = move.pos2.first, r2Col1 = move.pos2.second;
                int r2Layer2 = move.pos3.first, r2Col2 = move.pos3.second;
                if (board[r2Layer1][r2Col1] != Player::NONE || board[r2Layer2][r2Col2] != Player::NONE) return false;
                
                // 检查资源
                if (player == Player::US) {
                    if (usKFS_R1 <= 0 || usKFS_R2 < 2) return false;
                    usKFS_R1--;
                    usKFS_R2 -= 2;
                } else {
                    if (opponentKFS_R1 <= 0 || opponentKFS_R2 < 2) return false;
                    opponentKFS_R1--;
                    opponentKFS_R2 -= 2;
                }
                
                board[r1Layer][r1Col] = player;
                board[r2Layer1][r2Col1] = player;
                board[r2Layer2][r2Col2] = player;
                break;
            }
                
            case MoveType::PUSH_R2_DOUBLE: {
                Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
                
                // 检查兵器是否已使用
                if (weaponUsed) return false;
                
                // 执行推下操作
                int pushLayer = move.pos1.first, pushCol = move.pos1.second;
                if (board[pushLayer][pushCol] != opponent) return false;
                
                // 执行R2_DOUBLE移动（中层）
                int r2Layer1 = move.pos2.first, r2Col1 = move.pos2.second;
                int r2Layer2 = move.pos3.first, r2Col2 = move.pos3.second;
                if (board[r2Layer1][r2Col1] != Player::NONE || board[r2Layer2][r2Col2] != Player::NONE) return false;
                
                // 检查R2资源
                if (player == Player::US) {
                    if (usKFS_R2 < 2) return false;
                    usKFS_R2 -= 2;
                } else {
                    if (opponentKFS_R2 < 2) return false;
                    opponentKFS_R2 -= 2;
                }
                
                // 修复资源 - 推下后对方恢复资源
                if(player == Player::US) {
                   if(pushLayer == 0) {
                       opponentKFS_R1++;
                   } else {
                       if(pushLayer>=1) opponentKFS_R2++;
                   }
                } else {
                    if(pushLayer == 0) {
                       usKFS_R1++;
                   } else {
                       if(pushLayer >= 1) usKFS_R2++;
                   }
                }
                
                // 执行操作
                board[pushLayer][pushCol] = Player::NONE; // 推下
                board[r2Layer1][r2Col1] = player;        // R2放置第一个
                board[r2Layer2][r2Col2] = player;        // R2放置第二个
                weaponUsed = true; // 标记兵器已使用
                break;
            }
        }
        return true;
    }
    
    // 撤销移动
    void undoMove(const Move& move, Player player) {
        bool& weaponUsed = (player == Player::US) ? usWeaponUsed : opponentWeaponUsed;
        
        switch (move.type) {
            case MoveType::SINGLE: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                board[layer][col] = Player::NONE;
                
                // 恢复资源
                if (layer == 0) { // 底层使用R1 KFS
                    if (player == Player::US) usKFS_R1++;
                    else opponentKFS_R1++;
                } else { // 中层和顶层使用R2 KFS
                    if (player == Player::US) usKFS_R2++;
                    else opponentKFS_R2++;
                }
                break;
            }
                
            case MoveType::R2_LIFTED_DOUBLE: {
                int layer1 = move.pos1.first, col1 = move.pos1.second;
                int layer2 = move.pos2.first, col2 = move.pos2.second;
                
                board[layer1][col1] = Player::NONE;
                board[layer2][col2] = Player::NONE;
                
                // 恢复资源 - R2_LIFTED_DOUBLE移动只发生在中层和顶层，只使用R2 KFS
                if (player == Player::US) usKFS_R2 += 2;
                else opponentKFS_R2 += 2;
                break;
            }
                
            case MoveType::R2_DOUBLE: {
                int layer1 = move.pos1.first, col1 = move.pos1.second;
                int layer2 = move.pos2.first, col2 = move.pos2.second;
                
                board[layer1][col1] = Player::NONE;
                board[layer2][col2] = Player::NONE;
                
                // 恢复资源 - R2_DOUBLE移动只使用R2 KFS
                if (player == Player::US) usKFS_R2 += 2;
                else opponentKFS_R2 += 2;
                break;
            }
                
            case MoveType::PUSH: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                board[layer][col] = (player == Player::US) ? Player::OPPONENT : Player::US;
                weaponUsed = false; // 恢复兵器状态
                // 恢复资源
                if(player == Player::US) {
                   if(layer == 0) {
                       opponentKFS_R1--;
                   } else {
                       if(layer>=1) opponentKFS_R2--;
                   }
                } else {
                    if(layer == 0) {
                       usKFS_R1--;
                   } else {
                       if(layer>=1) usKFS_R2--;
                   }
                }
                 break;
            }
                
            case MoveType::COMBINED: {
                // 撤销R1移动
                int r1Layer = move.pos1.first, r1Col = move.pos1.second;
                board[r1Layer][r1Col] = Player::NONE;
                
                // 撤销R2移动
                int r2Layer = move.pos2.first, r2Col = move.pos2.second;
                board[r2Layer][r2Col] = Player::NONE;
                
                // 恢复资源
                if (player == Player::US) {
                    usKFS_R1++;
                    usKFS_R2++;
                } else {
                    opponentKFS_R1++;
                    opponentKFS_R2++;
                }
                break;
            }
                
            case MoveType::PUSH_PLACE: {
                Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
                
                int pushLayer = move.pos1.first, pushCol = move.pos1.second;
                int placeLayer = move.pos2.first, placeCol = move.pos2.second;
                
                // 恢复推下的棋子
                board[pushLayer][pushCol] = opponent;
                
                // 恢复放置的棋子
                board[placeLayer][placeCol] = Player::NONE;
                
                // 恢复资源
                if (player == Player::US) {
                    usKFS_R2++;
                } else {
                    opponentKFS_R2++;
                }
                
                // 修复资源
                if(player == Player::US) {
                   if(pushLayer == 0) {
                       opponentKFS_R1--;
                   } else {
                       if(pushLayer>=1) opponentKFS_R2--;
                   }
                } else {
                    if(pushLayer == 0) {
                       usKFS_R1--;
                   } else {
                       if(pushLayer >= 1) usKFS_R2--;
                   }
                }
                // 恢复兵器状态
                weaponUsed = false;
                break;
            }
                
            case MoveType::COMBINED_R2_DOUBLE: {
                // 撤销R1移动
                int r1Layer = move.pos1.first, r1Col = move.pos1.second;
                board[r1Layer][r1Col] = Player::NONE;
                
                // 撤销R2_DOUBLE移动
                int r2Layer1 = move.pos2.first, r2Col1 = move.pos2.second;
                int r2Layer2 = move.pos3.first, r2Col2 = move.pos3.second;
                board[r2Layer1][r2Col1] = Player::NONE;
                board[r2Layer2][r2Col2] = Player::NONE;
                
                // 恢复资源
                if (player == Player::US) {
                    usKFS_R1++;
                    usKFS_R2 += 2;
                } else {
                    opponentKFS_R1++;
                    opponentKFS_R2 += 2;
                }
                break;
            }
                
            case MoveType::PUSH_R2_DOUBLE: {
                Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
                
                int pushLayer = move.pos1.first, pushCol = move.pos1.second;
                int r2Layer1 = move.pos2.first, r2Col1 = move.pos2.second;
                int r2Layer2 = move.pos3.first, r2Col2 = move.pos3.second;
                
                // 恢复推下的棋子
                board[pushLayer][pushCol] = opponent;
                
                // 恢复R2_DOUBLE移动
                board[r2Layer1][r2Col1] = Player::NONE;
                board[r2Layer2][r2Col2] = Player::NONE;
                
                // 恢复资源
                if (player == Player::US) {
                    usKFS_R2 += 2;
                } else {
                    opponentKFS_R2 += 2;
                }
                
                // 修复资源 - 撤销推下后对方资源恢复
                if(player == Player::US) {
                   if(pushLayer == 0) {
                       opponentKFS_R1--;
                   } else {
                       if(pushLayer>=1) opponentKFS_R2--;
                   }
                } else {
                    if(pushLayer == 0) {
                       usKFS_R1--;
                   } else {
                       if(pushLayer >= 1) usKFS_R2--;
                   }
                }
                // 恢复兵器状态
                weaponUsed = false;
                break;
            }
        }
    }
    
    // 评估移动的优先级（仅用于排序，未影响minimax等决策函数）
    int evaluateMovePriority(const Move& move, Player player) {
        int priority = 0;
        
        // 优先立即获胜的移动
        if (move.type == MoveType::SINGLE) {
            // 模拟移动
            board[move.pos1.first][move.pos1.second] = player;
            bool wins = checkWin(player);
            board[move.pos1.first][move.pos1.second] = Player::NONE;
            
            if (wins) priority += 10000;
        }
        else if (move.type == MoveType::R2_LIFTED_DOUBLE || move.type == MoveType::R2_DOUBLE) {
            // 检查是否有一个放置能立即获胜
            board[move.pos1.first][move.pos1.second] = player;
            bool wins1 = checkWin(player);
            board[move.pos1.first][move.pos1.second] = Player::NONE;
            
            board[move.pos2.first][move.pos2.second] = player;
            bool wins2 = checkWin(player);
            board[move.pos2.first][move.pos2.second] = Player::NONE;
            
            if (wins1 || wins2) priority += 10000;
        }
        else if (move.type == MoveType::COMBINED) {
            // 检查组合移动是否能立即获胜
            board[move.pos1.first][move.pos1.second] = player;
            board[move.pos2.first][move.pos2.second] = player;
            bool wins = checkWin(player);
            board[move.pos1.first][move.pos1.second] = Player::NONE;
            board[move.pos2.first][move.pos2.second] = Player::NONE;
            
            if (wins) priority += 10000;
        }
        else if (move.type == MoveType::PUSH_PLACE) {
            // 检查推下+放置组合是否能立即获胜
            board[move.pos2.first][move.pos2.second] = player;
            bool wins = checkWin(player);
            board[move.pos2.first][move.pos2.second] = Player::NONE;
            
            if (wins) priority += 10000;
        }
        else if (move.type == MoveType::COMBINED_R2_DOUBLE) {
            // 检查组合移动是否能立即获胜
            board[move.pos1.first][move.pos1.second] = player;
            board[move.pos2.first][move.pos2.second] = player;
            board[move.pos3.first][move.pos3.second] = player;
            bool wins = checkWin(player);
            board[move.pos1.first][move.pos1.second] = Player::NONE;
            board[move.pos2.first][move.pos2.second] = Player::NONE;
            board[move.pos3.first][move.pos3.second] = Player::NONE;
            
            if (wins) priority += 10000;
        }
        else if (move.type == MoveType::PUSH_R2_DOUBLE) {
            // 检查推下+R2_DOUBLE组合是否能立即获胜
            board[move.pos2.first][move.pos2.second] = player;
            board[move.pos3.first][move.pos3.second] = player;
            bool wins = checkWin(player);
            board[move.pos2.first][move.pos2.second] = Player::NONE;
            board[move.pos3.first][move.pos3.second] = Player::NONE;
            
            if (wins) priority += 10000;
        }

        // 优先高层放置
        if (move.type == MoveType::SINGLE) {
            priority += move.pos1.first * 10; // 层越高优先级越高
        }
        else if (move.type == MoveType::R2_LIFTED_DOUBLE || move.type == MoveType::R2_DOUBLE) {
            priority += (move.pos1.first + move.pos2.first) * 10;
        }
        else if (move.type == MoveType::COMBINED) {
            priority += (move.pos1.first + move.pos2.first) * 10; // 组合移动的层数总和
        }
        else if (move.type == MoveType::PUSH_PLACE) {
            priority += move.pos2.first * 10; // 只考虑放置位置的层
        }
        else if (move.type == MoveType::COMBINED_R2_DOUBLE) {
            priority += (move.pos1.first + move.pos2.first + move.pos3.first) * 10;
        }
        else if (move.type == MoveType::PUSH_R2_DOUBLE) {
            priority += (move.pos2.first + move.pos3.first) * 10;
        }
        
        // 优先时间效率高的移动
        priority -= move.timeCost * 5;
        
        return priority;
    }
    
    // 极小化极大算法
    int minimax(int depth, bool isMaximizing, int alpha, int beta, Player player, int maxDepth) {
        if (depth == 0 || isGameOver()) {
            int baseScore = evaluate(player);
            
            // 如果是获胜局面，不应用折扣
            if (abs(baseScore) >= WIN_SCORE - 1000) {
                return baseScore-70*(maxDepth - depth);
            }
            
            // 对非获胜局面应用深度折扣 - 每层折扣10%
            // double discountFactor = 0.9;
            // double discountedScore = baseScore * pow(discountFactor, maxDepth - depth);
            double discountedScore = baseScore;
            
            return static_cast<int>(discountedScore);
        }
        
        Player currentPlayer = isMaximizing ? player : 
                             (player == Player::US ? Player::OPPONENT : Player::US);
        
        // 获取当前玩家所有可用的移动（包括所有机器人类型和组合移动）
        vector<Move> allMoves = getAvailableMoves(currentPlayer, "ALL");
        
        // 如果没有可用移动，直接返回评估值
        if (allMoves.empty()) {
            int baseScore = evaluate(player);
            
            // 如果是获胜局面，不应用折扣
            if (abs(baseScore) >= WIN_SCORE - 1000) {
                return baseScore;
            }
            
            // 对非获胜局面应用深度折扣
            double discountFactor = 0.9;
            double discountedScore = baseScore * pow(discountFactor, maxDepth - depth);
            
            return static_cast<int>(discountedScore);
        }
        
        // 按优先级排序移动（可选，提高剪枝效率）
        // sort(allMoves.begin(), allMoves.end(), [&](const Move& a, const Move& b) {
        //     return evaluateMovePriority(a, currentPlayer) > evaluateMovePriority(b, currentPlayer);
        // });
        
        if (isMaximizing) {
            int maxEval = numeric_limits<int>::min();
            
            for (const auto& move : allMoves) {
                // 模拟移动
                executeMove(move, currentPlayer);
                
                int eval = minimax(depth - 1, false, alpha, beta, player, maxDepth);
                maxEval = max(maxEval, eval);
                
                // 撤销移动
                undoMove(move, currentPlayer);
                
                alpha = max(alpha, eval);
                if (beta <= alpha) break;
            }
            return maxEval;
        } else {
            int minEval = numeric_limits<int>::max();
            
            for (const auto& move : allMoves) {
                // 模拟移动
                executeMove(move, currentPlayer);
                
                int eval = minimax(depth - 1, true, alpha, beta, player, maxDepth);
                minEval = min(minEval, eval);
                
                // 撤销移动
                undoMove(move, currentPlayer);
                
                beta = min(beta, eval);
                if (beta <= alpha) break;
            }
            return minEval;
        }
    }
    
    bool isGameOver() {
        // 检查是否有任一方获胜
        if (checkWin(Player::US) || checkWin(Player::OPPONENT)) {
            return true;
        }
        
        // 检查是否还有可执行的移动
        vector<Move> usMoves = getAvailableMoves(Player::US, "ALL");
        vector<Move> opponentMoves = getAvailableMoves(Player::OPPONENT, "ALL");
        
        // 如果任一方还有可执行的移动，游戏继续
        if (!usMoves.empty() || !opponentMoves.empty()) {
            return false;
        }
        
        // 检查棋盘是否已满
        bool boardFull = true;
        for (int layer = 0; layer < 3; layer++) {
            for (int col = 0; col < 3; col++) {
               if (board[layer][col] == Player::NONE) {
                   boardFull = false;
                   break;
               }
            }
            if (!boardFull) break;
        }
    
        return boardFull;
    }
    
    // 最优移动决策（考虑所有移动类型，包括组合移动）
    Move findOptimalMove(Player player, int &outScore , const string& robotType = "ALL", int depth = 3) {
        vector<Move> moves = getAvailableMoves(player, robotType);
        
        if (moves.empty()) {
            return Move(MoveType::SINGLE, {-1, -1}); // 无效移动
        }
        
        //按优先级排序
        // sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        //     return evaluateMovePriority(a, player) > evaluateMovePriority(b, player);
        // });
        
        Move bestMove = moves[0];
        int bestScore = numeric_limits<int>::min();
        
        for (const auto& move : moves) {
            // 模拟移动
            executeMove(move, player);
            
            int score = minimax(depth - 1, false, 
                              numeric_limits<int>::min(),
                              numeric_limits<int>::max(), player, depth);
            
            // 考虑时间效率
            score -= move.timeCost * 10;
            
            // 撤销移动
            undoMove(move, player);
            
            // 修改为 >= 确保找到最优移动
            if (score >= bestScore) {
                // 如果分数相同，选择时间代价更小的移动
                if (score == bestScore && move.timeCost < bestMove.timeCost) {
                    bestScore = score;
                    bestMove = move;
                } 
                // 如果分数更高，直接选择
                else if (score > bestScore) {
                    bestScore = score;
                    bestMove = move;
                }
            }
        }
        outScore = bestScore;
        return bestMove;
    }
    
    // 新增：查找最优和第二优移动
    pair<Move, Move> findTopTwoMoves(Player player, int &bestScore, int &secondBestScore, const string& robotType = "ALL", int depth = 3) {
        vector<Move> moves = getAvailableMoves(player, robotType);
        
        if (moves.empty()) {
            Move invalidMove = Move(MoveType::SINGLE, {-1, -1});
            return make_pair(invalidMove, invalidMove);
        }
        
        Move bestMove = moves[0];
        Move secondBestMove = moves[0];
        bestScore = numeric_limits<int>::min();
        secondBestScore = numeric_limits<int>::min();
        
        for (const auto& move : moves) {
            // 模拟移动
            executeMove(move, player);
            
            int score = minimax(depth - 1, false, 
                              numeric_limits<int>::min(),
                              numeric_limits<int>::max(), player, depth);
            
            // 考虑时间效率
            score -= move.timeCost * 10;
            
            // 撤销移动
            undoMove(move, player);
            
            // 更新最优和第二优移动
            if (score > bestScore) {
                // 更新第二优为原来的最优
                secondBestScore = bestScore;
                secondBestMove = bestMove;
                
                // 更新最优
                bestScore = score;
                bestMove = move;
            } else if (score > secondBestScore && score != bestScore) {
                // 更新第二优
                secondBestScore = score;
                secondBestMove = move;
            }
        }
        
        return make_pair(bestMove, secondBestMove);
    }
    
    // 放置KFS（兼容原有接口）
    bool placeKFS(int layer, int col, Player player) {
        if (layer < 0 || layer >= 3 || col < 0 || col >= 3) {
            return false;
        }
        if (board[layer][col] != Player::NONE) {
            return false;
        }
        switch (player) {
        case Player::US:
            if (layer == 0 && usKFS_R1 <= 0) return false;     // 底层需要R1 KFS
            if (layer >= 1 && usKFS_R2 <= 0) return false;     // 中层和顶层需要R2 KFS
        
            // 更新资源
            if (layer == 0) usKFS_R1--;     // 底层使用R1 KFS
            else usKFS_R2--;               // 中层和顶层使用R2 KFS
        
            board[layer][col] = player;
            return true;
            break;
        
        case Player::OPPONENT:
            if (layer == 0 && opponentKFS_R1 <= 0) return false;     // 底层需要R1 KFS
            if (layer >= 1 && opponentKFS_R2 <= 0) return false;     // 中层和顶层需要R2 KFS

            // 更新资源
            if (layer == 0) opponentKFS_R1--;     // 底层使用R1 KFS
            else opponentKFS_R2--;               // 中层和顶层使用R2 KFS

            board[layer][col] = player;
            return true;
            break;
        
        default:
            return true;
            break;
        }
    }
    
    // 显示棋盘
    void displayBoard(){
        vector<string> layerNames = {"底层", "中层", "顶层"};
        for (int layer = 2; layer >= 0; layer--) { // 从顶层开始显示
            cout << layerNames[layer] << ": ";
            for (int col = 0; col < 3; col++) {
                char symbol = '.';
                if (board[layer][col] == Player::US) symbol = 'U';
                else if (board[layer][col] == Player::OPPONENT) symbol = 'O';
                cout << symbol << " ";
            }
            cout << endl;
        }
        
        // 显示当前得分和资源
        cout << "我方得分: " << getCurrentScore(Player::US) << endl;
        cout << "对方得分: " << getCurrentScore(Player::OPPONENT) << endl;
        cout << "我方剩余R1 KFS: " << usKFS_R1 << ", 剩余R2 KFS: " << usKFS_R2 << endl;
        cout << "对方剩余R1 KFS: " << opponentKFS_R1 << ", 剩余R2 KFS: " << opponentKFS_R2 << endl;
        cout << "我方兵器状态: " << (usWeaponUsed ? "已使用" : "未使用") << endl;
        cout << "对方兵器状态: " << (opponentWeaponUsed ? "已使用" : "未使用") << endl;
    }
    
    // 获取棋盘状态（用于调试）
    vector<vector<Player>> getBoardState() {
        return board;
    }
    
    // 获取资源状态
    pair<int, int> getResourceState() {
        return {usKFS_R1, usKFS_R2};
    }
};

// 策略管理器
class StrategyManager {
private:
    ThreeDTicTacToe game;
    
public:
    // 整体策略决策
    void makeStrategicDecision() {
        cout << "=== 策略分析 ===" << endl;
        
        int bestScore = numeric_limits<int>::min();
        int secondBestScore = numeric_limits<int>::min();
        
        // 使用新的函数获取最优和第二优移动
        auto topTwoMoves = game.findTopTwoMoves(Player::US, bestScore, secondBestScore, "ALL", 3);
        Move bestMove = topTwoMoves.first;
        Move secondBestMove = topTwoMoves.second;
        
        cout << "\n=== 综合决策 ===" << endl;
        if (bestMove.pos1.first != -1) {
            cout << "最优移动: ";
            printMove(bestMove);
            cout << "得分: " << bestScore << endl;
            
            cout << "-------------------" << endl;
            
            // 输出第二优移动
            if (secondBestMove.pos1.first != -1) {
                cout << "第二优移动: ";
                printMove(secondBestMove);
                cout << "得分: " << secondBestScore << endl;
                
                // 分析第二优移动类型优势
               
            } else {
                cout << "没有有效的第二优移动" << endl;
            }
        } else {
            cout << "没有有效的移动" << endl;
        }
        cout << "================" << endl;
        cout << "执行最优移动后局面" << endl;
        game.executeMove(bestMove, Player::US);
        game.displayBoard();
    }
    
    void printMove(const Move& move) {
        vector<string> layerNames = {"底层", "中层", "顶层"};
        
        switch (move.type) {
            case MoveType::SINGLE:
                cout << "单个放置 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列" << endl;
                break;
                
            case MoveType::R2_LIFTED_DOUBLE:
                cout << "被举起R2复合放置 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列 和 "
                     << layerNames[move.pos2.first] 
                     << "第" << (move.pos2.second + 1) << "列" << endl;
                break;
                
            case MoveType::R2_DOUBLE:
                cout << "R2中层双放置 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列 和 "
                     << layerNames[move.pos2.first] 
                     << "第" << (move.pos2.second + 1) << "列" << endl;
                break;
                
            case MoveType::PUSH:
                cout << "推下操作 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列的对方KFS" << endl;
                break;
                
            case MoveType::COMBINED:
                cout << "组合移动 - R1在" << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列, R2在"
                     << layerNames[move.pos2.first] 
                     << "第" << (move.pos2.second + 1) << "列" << endl;
                break;
                
            case MoveType::PUSH_PLACE:
                cout << "推下+放置组合 - R1推下" << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列, R2放置在"
                     << layerNames[move.pos2.first] 
                     << "第" << (move.pos2.second + 1) << "列" << endl;
                break;
                
            case MoveType::COMBINED_R2_DOUBLE:
                cout << "组合移动+R2双放置 - R1在" << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列, R2在" 
                     << layerNames[move.pos2.first] << "第" << (move.pos2.second + 1) << "列"
                     << "和" << layerNames[move.pos3.first] << "第" << (move.pos3.second + 1) << "列" << endl;
                break;
                
            case MoveType::PUSH_R2_DOUBLE:
                cout << "推下+R2双放置组合 - R1推下" << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列, R2在" 
                     << layerNames[move.pos2.first] << "第" << (move.pos2.second + 1) << "列"
                     << "和" << layerNames[move.pos3.first] << "第" << (move.pos3.second + 1) << "列" << endl;
                break;
        }
    }
    
    ThreeDTicTacToe& getGame() {
        return game;
    }
};

int main() {
    StrategyManager manager;
    ThreeDTicTacToe& game = manager.getGame();

    game.Set_usWeaponUsed(false);
    game.Set_opponentWeaponUsed(false);
    game.placeKFS(0, 0, Player::NONE);    
    game.placeKFS(0, 1, Player::NONE);      
    game.placeKFS(0, 2, Player::NONE);    
    game.placeKFS(1, 0, Player::NONE);   
    game.placeKFS(1, 1, Player::NONE); 
    game.placeKFS(1, 2, Player::NONE);   
    game.placeKFS(2, 0, Player::NONE);   
    game.placeKFS(2, 1, Player::NONE); 
    game.placeKFS(2, 2, Player::NONE);    

    // game.Set_usWeaponUsed(true);
    // game.Set_opponentWeaponUsed(false);
    // game.placeKFS(0, 0, Player::NONE);    
    // game.placeKFS(0, 1, Player::NONE);      
    // game.placeKFS(0, 2, Player::OPPONENT);    
    // game.placeKFS(1, 0, Player::US);   
    // game.placeKFS(1, 1, Player::OPPONENT); 
    // game.placeKFS(1, 2, Player::NONE);   
    // game.placeKFS(2, 0, Player::NONE);   
    // game.placeKFS(2, 1, Player::NONE); 
    // game.placeKFS(2, 2, Player::NONE);

    cout << "当前棋盘状态:" << endl;
    game.displayBoard();
    cout << endl;
    
    // 生成策略建议
    manager.makeStrategicDecision();
    cout << "testnumber:1" << endl;
    return 0;
}