//2025-09-26
// 三维井字棋最优摆放模型 version 1.1
/*主要改进：
1. 支持复合移动（同时放置两个相邻方块）
2. 添加推下操作策略（只在必要时使用）
3. 考虑KFS资源限制
4. 优化评估函数，考虑时间效率
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <limits>
#include <string>
#include <unordered_set>

using namespace std;

enum class Player { NONE, US, OPPONENT };

// 移动类型
enum class MoveType { 
    SINGLE,     // 单个放置
    DOUBLE,     // 同时放置两个方块
    PUSH        // 推下对方KFS
};

// 移动表示
struct Move {
    MoveType type;
    pair<int, int> pos1;  // 第一个位置
    pair<int, int> pos2;  // 第二个位置（仅DOUBLE类型使用）
    int timeCost;         // 时间代价
    
    Move(MoveType t, pair<int, int> p1, pair<int, int> p2 = {-1, -1}) 
        : type(t), pos1(p1), pos2(p2) {
        // 设置时间代价
        if (type == MoveType::PUSH) timeCost = 2;
        else timeCost = 1; // SINGLE和DOUBLE时间代价相同
    }
};

class ThreeDTicTacToe {
private:
    // 3层×3列的棋盘（总共9个格子）
    vector<vector<Player>> board; // [层][列]
    
    // 资源限制
    int usKFS_R1 = 3;  // 我方R1 KFS数量
    int usKFS_R2 = 3;  // 我方R2 KFS数量
    bool weaponUsed = false; // 兵器是否已使用
    
    // 得分规则
    const int SCORE_BOTTOM = 30;  // 底层每个KFS
    const int SCORE_MIDDLE = 40; // 中层每个KFS  
    const int SCORE_TOP = 80;    // 顶层每个KFS
    const int WIN_SCORE = 10000; // 获胜的极大值
    
    // 获胜模式：垂直列和对角线
    vector<vector<pair<int, int>>> winningPatterns;

public:
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
            return abs(col1 - col2) <= 1;
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
    
    // 评估函数：综合考虑获胜可能性和得分
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
            
            // 如果我方有获胜机会
            if (playerCount == 2 && emptyCount == 1) {
                threatScore += 500; // 即将获胜，高优先级
            }
            // 如果对方有获胜威胁
            else if (opponentCount == 2 && emptyCount == 1) {
                threatScore -= 600; // 必须阻止，更高优先级
            }
            // 如果我方有发展潜力
            else if (playerCount == 1 && emptyCount == 2) {
                threatScore += 100;
            }
        }
        
        return threatScore;
    }
    
    // 获取所有可用移动（包括复合移动和推下操作）
    vector<Move> getAvailableMoves(Player player, const string& robotType) {
        vector<Move> moves;
        Player opponent = (player == Player::US) ? Player::OPPONENT : Player::US;
        
        // 根据机器人类型确定可放置的层
        int allowedLayer = -1;
        if (robotType == "R1") allowedLayer = 0;        // R1只能放底层
        else if (robotType == "R2") allowedLayer = 1;   // R2只能放中层  
        else if (robotType == "R2_LIFTED") allowedLayer = 2; // 被举起的R2放顶层
        
        // 检查资源限制
        bool canPlaceR1 = (robotType == "R1" || robotType == "R2_LIFTED") && usKFS_R1 > 0;
        bool canPlaceR2 = (robotType == "R2" || robotType == "R2_LIFTED") && usKFS_R2 > 0;
        
        // 生成单个放置移动
        for (int layer = 0; layer < 3; layer++) {
            for (int col = 0; col < 3; col++) {
                if (board[layer][col] == Player::NONE) {
                    // 检查层限制
                    if (allowedLayer != -1 && layer != allowedLayer) continue;
                    
                    // 检查资源限制
                    if ((layer == 0 || layer == 2) && !canPlaceR1) continue; // 底层和顶层需要R1 KFS
                    if (layer == 1 && !canPlaceR2) continue; // 中层需要R2 KFS
                    
                    moves.push_back(Move(MoveType::SINGLE, {layer, col}));
                }
            }
        }
        
        // 如果是被举起的R2，可以生成复合移动（同时放置两个方块）
        if (robotType == "R2_LIFTED" && usKFS_R1 >= 1 && usKFS_R2 >= 1) {
            // 生成所有可能的复合移动
            for (int i = 0; i < moves.size(); i++) {
                for (int j = i+1; j < moves.size(); j++) {
                    auto pos1 = moves[i].pos1;
                    auto pos2 = moves[j].pos1;
                    
                    // 检查位置是否相邻
                    if (arePositionsAdjacent(pos1, pos2)) {
                        moves.push_back(Move(MoveType::DOUBLE, pos1, pos2));
                    }
                }
            }
        }
        
        // 生成推下移动（只在必要时）
        if (!weaponUsed && evaluateThreats(player, opponent) < -500) {
            // 只在对方有获胜威胁时考虑推下
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
    
    // 执行移动
    bool executeMove(const Move& move, Player player) {
        switch (move.type) {
            case MoveType::SINGLE: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                if (board[layer][col] != Player::NONE) return false;
                
                // 更新资源
                if (layer == 0 || layer == 2) { // 底层或顶层使用R1 KFS
                    if (usKFS_R1 <= 0) return false;
                    usKFS_R1--;
                } else { // 中层使用R2 KFS
                    if (usKFS_R2 <= 0) return false;
                    usKFS_R2--;
                }
                
                board[layer][col] = player;
                break;
            }
                
            case MoveType::DOUBLE: {
                int layer1 = move.pos1.first, col1 = move.pos1.second;
                int layer2 = move.pos2.first, col2 = move.pos2.second;
                
                if (board[layer1][col1] != Player::NONE || 
                    board[layer2][col2] != Player::NONE) return false;
                
                // 更新资源
                if (layer1 == 0 || layer1 == 2) {
                    if (usKFS_R1 <= 0) return false;
                    usKFS_R1--;
                } else {
                    if (usKFS_R2 <= 0) return false;
                    usKFS_R2--;
                }
                
                if (layer2 == 0 || layer2 == 2) {
                    if (usKFS_R1 <= 0) return false;
                    usKFS_R1--;
                } else {
                    if (usKFS_R2 <= 0) return false;
                    usKFS_R2--;
                }
                
                board[layer1][col1] = player;
                board[layer2][col2] = player;
                break;
            }
                
            case MoveType::PUSH: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                if (board[layer][col] != Player::OPPONENT) return false;
                
                // 标记兵器已使用
                weaponUsed = true;
                board[layer][col] = Player::NONE;
                break;
            }
        }
        
        return true;
    }
    
    // 撤销移动
    void undoMove(const Move& move, Player player) {
        switch (move.type) {
            case MoveType::SINGLE: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                board[layer][col] = Player::NONE;
                
                // 恢复资源
                if (layer == 0 || layer == 2) {
                    usKFS_R1++;
                } else {
                    usKFS_R2++;
                }
                break;
            }
                
            case MoveType::DOUBLE: {
                int layer1 = move.pos1.first, col1 = move.pos1.second;
                int layer2 = move.pos2.first, col2 = move.pos2.second;
                
                board[layer1][col1] = Player::NONE;
                board[layer2][col2] = Player::NONE;
                
                // 恢复资源
                if (layer1 == 0 || layer1 == 2) {
                    usKFS_R1++;
                } else {
                    usKFS_R2++;
                }
                
                if (layer2 == 0 || layer2 == 2) {
                    usKFS_R1++;
                } else {
                    usKFS_R2++;
                }
                break;
            }
                
            case MoveType::PUSH: {
                int layer = move.pos1.first;
                int col = move.pos1.second;
                
                board[layer][col] = Player::OPPONENT;
                weaponUsed = false; // 恢复兵器状态
                break;
            }
        }
    }
    
    // 评估移动的优先级（用于排序）
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
        else if (move.type == MoveType::DOUBLE) {
            // 检查是否有一个放置能立即获胜
            board[move.pos1.first][move.pos1.second] = player;
            bool wins1 = checkWin(player);
            board[move.pos1.first][move.pos1.second] = Player::NONE;
            
            board[move.pos2.first][move.pos2.second] = player;
            bool wins2 = checkWin(player);
            board[move.pos2.first][move.pos2.second] = Player::NONE;
            
            if (wins1 || wins2) priority += 10000;
        }
        
        // 优先高层放置
        if (move.type == MoveType::SINGLE) {
            priority += move.pos1.first * 10; // 层越高优先级越高
        }
        else if (move.type == MoveType::DOUBLE) {
            priority += (move.pos1.first + move.pos2.first) * 5;
        }
        
        // 优先时间效率高的移动
        priority -= move.timeCost * 5;
        
        return priority;
    }
    
    // 极小化极大算法（修改版，支持复合移动）
    int minimax(int depth, bool isMaximizing, int alpha, int beta, Player player, const string& robotType) {
        if (depth == 0 || isGameOver()) {
            return evaluate(player);
        }
        
        Player currentPlayer = isMaximizing ? player : 
                             (player == Player::US ? Player::OPPONENT : Player::US);
        
        vector<Move> moves = getAvailableMoves(currentPlayer, robotType);
        
        // 按优先级排序移动
        sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
            return evaluateMovePriority(a, currentPlayer) > evaluateMovePriority(b, currentPlayer);
        });
        
        if (isMaximizing) {
            int maxEval = numeric_limits<int>::min();
            
            for (const auto& move : moves) {
                // 模拟移动
                executeMove(move, player);
                
                int eval = minimax(depth - 1, false, alpha, beta, player, robotType);
                maxEval = max(maxEval, eval);
                
                // 撤销移动
                undoMove(move, player);
                
                alpha = max(alpha, eval);
                if (beta <= alpha) break;
            }
            return maxEval;
        } else {
            int minEval = numeric_limits<int>::max();
            
            for (const auto& move : moves) {
                // 模拟移动
                executeMove(move, (player == Player::US) ? Player::OPPONENT : Player::US);
                
                int eval = minimax(depth - 1, true, alpha, beta, player, robotType);
                minEval = min(minEval, eval);
                
                // 撤销移动
                undoMove(move, (player == Player::US) ? Player::OPPONENT : Player::US);
                
                beta = min(beta, eval);
                if (beta <= alpha) break;
            }
            return minEval;
        }
    }
    
    bool isGameOver() {
        return checkWin(Player::US) || checkWin(Player::OPPONENT) || 
               (usKFS_R1 <= 0 && usKFS_R2 <= 0); // 或者没有KFS可用了
    }
    
    // 最优移动决策（考虑机器人放置限制和复合移动）
    Move findOptimalMove(Player player, const string& robotType, int depth = 3) {
        vector<Move> moves = getAvailableMoves(player, robotType);
        
        if (moves.empty()) {
            return Move(MoveType::SINGLE, {-1, -1}); // 无效移动
        }
        
        // 按优先级排序
        sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
            return evaluateMovePriority(a, player) > evaluateMovePriority(b, player);
        });
        
        Move bestMove = moves[0];
        int bestScore = numeric_limits<int>::min();
        
        for (const auto& move : moves) {
            // 模拟移动
            executeMove(move, player);
            
            int score = minimax(depth - 1, false, 
                              numeric_limits<int>::min(),
                              numeric_limits<int>::max(), player, robotType);
            
            // 考虑时间效率
            score -= move.timeCost * 10;
            
            // 撤销移动
            undoMove(move, player);
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
        
        return bestMove;
    }
    
    // 放置KFS（兼容原有接口）
    bool placeKFS(int layer, int col, Player player) {
        if (layer < 0 || layer >= 3 || col < 0 || col >= 3) {
            return false;
        }
        if (board[layer][col] != Player::NONE) {
            return false;
        }
        
        // 检查资源限制
        if ((layer == 0 || layer == 2) && usKFS_R1 <= 0) return false;
        if (layer == 1 && usKFS_R2 <= 0) return false;
        
        // 更新资源
        if (layer == 0 || layer == 2) usKFS_R1--;
        else usKFS_R2--;
        
        board[layer][col] = player;
        return true;
    }
    
    // 显示棋盘
    void displayBoard() {
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
        cout << "剩余R1 KFS: " << usKFS_R1 << ", 剩余R2 KFS: " << usKFS_R2 << endl;
        cout << "兵器状态: " << (weaponUsed ? "已使用" : "未使用") << endl;
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
        
        // 1. 检查R1（底层）的最优移动
        auto r1Move = game.findOptimalMove(Player::US, "R1", 3);
        if (r1Move.pos1.first != -1) {
            cout << "R1最优移动: ";
            printMove(r1Move);
        }
        
        // 2. 检查R2（中层）的最优移动  
        auto r2Move = game.findOptimalMove(Player::US, "R2", 3);
        if (r2Move.pos1.first != -1) {
            cout << "R2最优移动: ";
            printMove(r2Move);
        }
        
        // 3. 检查被举起R2（顶层）的最优移动
        auto r2LiftedMove = game.findOptimalMove(Player::US, "R2_LIFTED", 3);
        if (r2LiftedMove.pos1.first != -1) {
            cout << "被举起R2最优移动: ";
            printMove(r2LiftedMove);
        }
        
        // 优先级建议
        cout << "\n=== 放置优先级建议 ===" << endl;
        cout << "1. 阻止对方立即获胜" << endl;
        cout << "2. 创造我方获胜机会" << endl; 
        cout << "3. 优先复合移动（同时放置两个方块）" << endl;
        cout << "4. 只在必要时使用推下操作" << endl;
    }
    
    void printMove(const Move& move) {
        vector<string> layerNames = {"底层", "中层", "顶层"};
        
        switch (move.type) {
            case MoveType::SINGLE:
                cout << "单个放置 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列" << endl;
                break;
                
            case MoveType::DOUBLE:
                cout << "复合放置 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列 和 "
                     << layerNames[move.pos2.first] 
                     << "第" << (move.pos2.second + 1) << "列" << endl;
                break;
                
            case MoveType::PUSH:
                cout << "推下操作 - " << layerNames[move.pos1.first] 
                     << "第" << (move.pos1.second + 1) << "列的对方KFS" << endl;
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
    
    cout << "=== 三维井字棋最优摆放模型 v1.1 ===" << endl;
    cout << "棋盘结构: 3层(顶层/中层/底层) × 3列" << endl;
    cout << "得分规则: 顶层80分, 中层40分, 底层30分" << endl;
    cout << "获胜条件: 垂直列或对角线连成一线" << endl;
    cout << "新增功能: 支持复合移动和推下操作" << endl << endl;
    
    // 示例初始局面
    game.placeKFS(0, 1, Player::US);    // 我方在底层中间

    cout << "当前棋盘状态:" << endl;
    game.displayBoard();
    cout << endl;
    
    // 生成策略建议
    manager.makeStrategicDecision();
    
    return 0;
}