//2025-09-24
// 三维井字棋最优摆放模型 version 1.0
/*策略：
1. 优先阻止对方立即获胜
2. 优先创造我方获胜机会
3. 优先拉大得分差
实现：给每一步的局面进行评分，寻找最优解
不足：
1. 没有考虑对方的策略，评分时默认对方采取最优策略，并且默认摆放时是你一个KFS，我一个KFS轮流摆放
2. 未考虑将对方的KFS推下

ds查错误
1. 未模拟兵器攻击移除对手KFS的机制
规则依据：规则4.5.3规定，R1可以用组装好的兵器试图移走对方占据九宫格的KFS。每次攻击后，兵器视为“已用”（规则4.5.5）。
代码漏洞：代码完全未模拟兵器攻击功能。棋盘状态只能放置或空置，无法移除对手的KFS。这导致策略评估缺失关键防御和攻击维度，无法反映实际比赛中的对抗性。
影响：AI可能建议被动放置KFS，而忽略主动破坏对手阵型的机会，导致策略次优。

2. 移动决策未考虑机器人协作顺序
规则依据：规则4.5.17要求R2必须被R1举起才能放置顶层KFS；R1和R2的移动有顺序依赖（如R1必须先离开武馆，R2才能离开）。
代码漏洞：StrategyManager::makeStrategicDecision() 同时计算R1、R2和被举起R2的最优移动，但未考虑执行顺序。代码假设移动是独立的，而实际中放置顺序会影响棋盘状态和后续决策。
影响：可能推荐矛盾的移动（如R2放置中层后，R1却阻塞了其顶层放置路径），或忽略协作带来的连锁收益。

3. 忽略KFS收集限制和可用性
规则依据：规则3.6和4.4规定，机器人必须从梅林收集KFS后才能带入对抗区。KFS数量有限（R1有3个，R2有4个）。
代码漏洞：代码假设KFS无限可用，允许任意放置。未模拟KFS收集过程或数量限制。
影响：AI可能建议放置不存在的KFS，或过度优化高层放置而忽略实际KFS库存，导致策略不可行。

4. 未处理兵器使用次数和状态
规则依据：规则4.5.4和4.5.6规定，每件兵器只能用一次，使用后必须放入“已用兵器区”。
代码漏洞：代码未跟踪兵器状态（未使用/已用），也未模拟兵器使用对棋盘的影响（如移除对手KFS）。
影响：AI无法评估兵器资源的消耗和攻击机会，可能重复使用“已用兵器”或浪费攻击时机。

6. 层奖励可能导致双重计分
代码漏洞：在findOptimalMove()中，额外添加层奖励（中层+10、顶层+20），但评估函数evaluate()已通过getCurrentScore()计算层得分。这可能导致高层放置被过度优先。
影响：AI可能过分追求高层放置，而忽略更紧急的威胁或获胜机会，破坏策略平衡。

    解决方案：移除findOptimalMove()中的层奖励，完全依赖evaluate()的得分计算。

7. 未模拟对手攻击和动态棋盘变化
规则依据：对手同样可以使用兵器攻击我方KFS，棋盘状态动态变化。
代码漏洞：代码的极小化极大算法假设对手只放置KFS，不模拟对手攻击移除我方KFS的行为。
影响：AI评估过于乐观，无法应对对手攻击造成的棋盘状态突变，策略鲁棒性差。

8. 初始化和示例局面不具代表性
代码漏洞：main()函数中只初始化了一个我方KFS在底层中间，未模拟实际比赛中的复杂初始状态（如对手KFS放置）。
影响：测试用例过于简单，可能掩盖策略缺陷，无法验证真实场景下的决策质量。

*/
#include <iostream>
#include <vector>
#include <algorithm>
#include <limits>
#include <string>

using namespace std;

enum class Player { NONE, US, OPPONENT };

class ThreeDTicTacToe {
private:
    // 3层×3列的棋盘（总共9个格子）
    vector<vector<Player>> board; // [层][列]
    
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
        //根据评估的玩家，自动确定对手是谁
        
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
            int playerCount = 0;    //当前获胜模式（pattern）中属于“我方玩家”的棋子数量
            int opponentCount = 0;   //当前获胜模式（pattern）中属于“对手玩家”的棋子数量
            int emptyCount = 0;      //当前获胜模式（pattern）中空格的数量
            
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
    
    // 极小化极大算法
    //minimax 的设计目标是“递归评估分数”，它只负责告诉上一层“这一轮最优分数是多少”，不关心具体是哪一步。
    //depth：递归深度，控制AI预判多少步，越大越“聪明”，但计算量也越大。
    //isMaximizing：当前轮到谁行动。true表示AI自己行动（最大化分数），false表示对手行动（最小化分数）。     如果 isMaximizing 为 true，说明当前轮到 player 行动（比如AI自己），此时要让分数最大化（选最优方案）。
    //                                                                                               如果 isMaximizing 为 false，说明当前轮到对手行动，此时要让分数最小化（假设对手会选最坏方案来针对你）。
    //alpha、beta：用于剪枝优化，减少无意义的搜索，提高效率。
    //player：指定本次模拟的“主角”是谁（比如AI或对手），决定评估时以谁为中心。  决定了评估分数时以谁为中心（evaluate(player)），最终返回的分数是“对player来说”的好坏。
    int minimax(int depth, bool isMaximizing, int alpha, int beta, Player player) {
        if (depth == 0 || isGameOver()) {
            return evaluate(player);
        }
        
        Player currentPlayer = isMaximizing ? player : 
                             (player == Player::US ? Player::OPPONENT : Player::US);
        
        vector<pair<int, int>> moves = getAvailableMoves();
        
        if (isMaximizing) {
            int maxEval = numeric_limits<int>::min();   //当前所有可能落子方案中，对主角来说最优的分数
            
            for (const auto& move : moves) {
                int layer = move.first;
                int col = move.second;
                
                // 模拟移动
                board[layer][col] = player;
                
                int eval = minimax(depth - 1, false, alpha, beta, player);
                maxEval = max(maxEval, eval);
                
                // 撤销移动
                board[layer][col] = Player::NONE;
                
                alpha = max(alpha, eval);
                if (beta <= alpha) break;
            }
            return maxEval;
        } else {
            int minEval = numeric_limits<int>::max();
            
            for (const auto& move : moves) {
                int layer = move.first;
                int col = move.second;
                
                board[layer][col] = (player == Player::US) ? Player::OPPONENT : Player::US;
                
                int eval = minimax(depth - 1, true, alpha, beta, player);
                minEval = min(minEval, eval);
                
                board[layer][col] = Player::NONE;
                
                beta = min(beta, eval);
                if (beta <= alpha) break;
            }
            return minEval;
        }
    }
    
    // 获取所有可用移动
    vector<pair<int, int>> getAvailableMoves() {
        vector<pair<int, int>> moves;
        for (int layer = 0; layer < 3; layer++) {
            for (int col = 0; col < 3; col++) {
                if (board[layer][col] == Player::NONE) {
                    moves.push_back({layer, col});
                }
            }
        }
        return moves;
    }
    
    bool isGameOver() {
        return checkWin(Player::US) || checkWin(Player::OPPONENT) || getAvailableMoves().empty();//
    }
    
    // 最优移动决策（考虑机器人放置限制）
    //findOptimalMove 函数。它会遍历所有可能的走法，调用 minimax 得到每一步的分数，然后用 bestMove 记录分数最高的那个走法。
    pair<int, int> findOptimalMove(Player player, const string& robotType, int depth = 3) {
        vector<pair<int, int>> moves = getAvailableMoves();
        pair<int, int> bestMove = {-1, -1};
        int bestScore = numeric_limits<int>::min();
        
        // 根据机器人类型确定可放置的层
        int allowedLayer = -1;
        if (robotType == "R1") allowedLayer = 0;        // R1只能放底层
        else if (robotType == "R2") allowedLayer = 1;   // R2只能放中层  
        else if (robotType == "R2_LIFTED") allowedLayer = 2; // 被举起的R2放顶层
        
        for (const auto& move : moves) {
            int layer = move.first;
            int col = move.second;
            
            // 检查目前robot是否可以放在这格
            if (allowedLayer != -1 && layer != allowedLayer) {
                continue;
            }
            
            // 层得分加成（例鼓励高层放置）
            //layerBonus 的相对大小决定了机器人对不同层（底层、中层、顶层）落子的偏好。
            //如果你把顶层的 layerBonus 设置得更高，机器人会更倾向于优先放顶层。
            //如果你把底层的 layerBonus 提高，机器人会更愿意放底层。
            int layerBonus = 0;
            switch (layer) {
                // case 0: layerBonus = 0; break;    // 底层
                // case 1: layerBonus = 10; break;   // 中层
                // case 2: layerBonus = 20; break;   // 顶层
            }
            
            // 模拟移动
            board[layer][col] = player;
            
            int score = minimax(depth - 1, false, 
                              numeric_limits<int>::min(),
                              numeric_limits<int>::max(), player);//计算我下出这一步后对手让我获得的最小分数
            
            score += layerBonus; // 添加层奖励
            
            // 撤销移动
            board[layer][col] = Player::NONE;
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
        
        // 如果没有符合限制的移动，返回无效移动（-1，-1）
        return bestMove;
    }
    
    // 放置KFS
    bool placeKFS(int layer, int col, Player player) {
        if (layer < 0 || layer >= 3 || col < 0 || col >= 3) {
            return false;
        }
        if (board[layer][col] != Player::NONE) {
            return false;
        }
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
        
        // 显示当前得分
        cout << "我方得分: " << getCurrentScore(Player::US) << endl;
        cout << "对方得分: " << getCurrentScore(Player::OPPONENT) << endl;
    }
    
    // 获取棋盘状态（用于调试）
    vector<vector<Player>> getBoardState() {
        return board;
    }
};

// 策略管理器
class StrategyManager {
private:
    ThreeDTicTacToe game;
    
public:
    // 整体策略决策
    void makeStrategicDecision() {
        // 1. 检查R1（底层）的最优移动
        auto r1Move = game.findOptimalMove(Player::US, "R1", 3);
        if (r1Move.first != -1) {
            cout << "R1最优放置: 底层第" << (r1Move.second + 1) << "列" << endl;
        }
        
        // 2. 检查R2（中层）的最优移动  
        auto r2Move = game.findOptimalMove(Player::US, "R2", 3);
        if (r2Move.first != -1) {
            cout << "R2最优放置: 中层第" << (r2Move.second + 1) << "列" << endl;
        }
        
        // 3. 检查被举起R2（顶层）的最优移动
        auto r2LiftedMove = game.findOptimalMove(Player::US, "R2_LIFTED", 3);
        if (r2LiftedMove.first != -1) {
            cout << "被举起R2最优放置: 顶层第" << (r2LiftedMove.second + 1) << "列" << endl;
        }
        
        // 优先级建议
        cout << "\n放置优先级建议:" << endl;
        cout << "1. 阻止对方立即获胜" << endl;
        cout << "2. 创造我方获胜机会" << endl; 
        cout << "3. 占据高层位置（得分更高）" << endl;
        //cout << "4. 控制中心列" << endl;
    }
    
    ThreeDTicTacToe& getGame() {
        return game;
    }
};

int main() {
    StrategyManager manager;
    ThreeDTicTacToe& game = manager.getGame();
    
    cout << "=== 三维井字棋最优摆放模型 ===" << endl;
    cout << "棋盘结构: 3层(顶层/中层/底层) × 3列" << endl;
    cout << "得分规则: 顶层80分, 中层40分, 底层30分" << endl;
    cout << "获胜条件: 垂直列或对角线连成一线" << endl << endl;
    
    // 示例初始局面
    game.placeKFS(0, 1, Player::US);    // 我方在底层中间

    cout << "当前棋盘状态:" << endl;
    game.displayBoard();
    cout << endl;
    
    // 生成策略建议
    manager.makeStrategicDecision();
    
    return 0;
}
