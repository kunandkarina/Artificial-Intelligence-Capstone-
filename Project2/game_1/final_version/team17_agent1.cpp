#include "STcpClient.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <stdlib.h>
#include <thread>
#include <vector>
#define BOARD_SIZE 12

// team_name : nuguseyo
// team_id : 17
// team_member : 110550090 王昱力, 110550093 蔡師睿, 110550110 林書愷

const int dir[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}},
          pre_player_id[5] = {0, 4, 1, 2, 3}, next_player_id[5] = {0, 2, 3, 4, 1};

/***
 *      __  __  _____ _______ _____   _   _           _
 *     |  \/  |/ ____|__   __/ ____| | \ | |         | |
 *     | \  / | |       | | | (___   |  \| | ___   __| | ___
 *     | |\/| | |       | |  \___ \  | . ` |/ _ \ / _` |/ _ \
 *     | |  | | |____   | |  ____) | | |\  | (_) | (_| |  __/
 *     |_|  |_|\_____|  |_| |_____/  |_| \_|\___/ \__,_|\___|
 *
 *
 */

class MCTSNode {
  public:
    const double c = 0.75;
    int player_id, expand_id, visit_count, mapStat[BOARD_SIZE][BOARD_SIZE], sheepStat[BOARD_SIZE][BOARD_SIZE];
    double win_score;
    bool end_state;
    MCTSNode *parent;
    std::vector<int> action;
    std::vector<MCTSNode *> children;

    MCTSNode(const int (*_mapStat)[BOARD_SIZE], const int (*_sheepStat)[BOARD_SIZE], int _player_id)
        : player_id(_player_id), visit_count(0), win_score(0), expand_id(0), end_state(false), parent(nullptr)
    {
        memcpy(mapStat, _mapStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
        memcpy(sheepStat, _sheepStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
    }

    MCTSNode(const int (*_mapStat)[BOARD_SIZE], const int (*_sheepStat)[BOARD_SIZE], int _player_id, MCTSNode *_parent,
             std::vector<int> &_action, bool _end_state)
        : player_id(_player_id), visit_count(0), win_score(0), expand_id(0), end_state(_end_state), action(_action),
          parent(_parent)
    {
        memcpy(mapStat, _mapStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
        memcpy(sheepStat, _sheepStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
    }

    ~MCTSNode()
    {
        for (auto &child : children)
            delete child;
    }

    double UCB1()
    {
        // Calculate UCB score
        if (visit_count == 0)
            return 1e9;
        return win_score / visit_count + c * sqrt(log(parent->visit_count) / visit_count);
    }
};

/***
 *      __  __  _____ _______ _____                            _
 *     |  \/  |/ ____|__   __/ ____|     /\                   | |
 *     | \  / | |       | | | (___      /  \   __ _  ___ _ __ | |_
 *     | |\/| | |       | |  \___ \    / /\ \ / _` |/ _ \ '_ \| __|
 *     | |  | | |____   | |  ____) |  / ____ \ (_| |  __/ | | | |_
 *     |_|  |_|\_____|  |_| |_____/  /_/    \_\__, |\___|_| |_|\__|
 *                                             __/ |
 *                                            |___/
 */

class MCTSAgent {
  private:
    int player_id, parallel_num;
    std::vector<MCTSNode *> roots;

    std::vector<std::vector<int>> getLegalActions(const int (*mapStat)[BOARD_SIZE], const int (*sheepStat)[BOARD_SIZE],
                                                  int pid)
    {
        std::vector<std::vector<int>> legal_actions;
        legal_actions.reserve(1024);
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (mapStat[i][j] != pid || sheepStat[i][j] < 2)
                    continue;

                for (int k = 0; k < 9; ++k) {
                    if (i + dir[k][0] >= 0 && i + dir[k][0] < BOARD_SIZE && j + dir[k][1] >= 0 &&
                        j + dir[k][1] < BOARD_SIZE && mapStat[i + dir[k][0]][j + dir[k][1]] == 0) {
                        for (int s = 1; s < sheepStat[i][j]; s++)
                            legal_actions.push_back({i, j, s, k + 1});
                    }
                }
            }
        }

        return legal_actions;
    }

    // count the connected land size with dfs
    int cntboard(std::vector<std::vector<int>> &board, int r, int c, int id)
    {
        board[r][c] = -1; // change entered pos to -1
        int sz = 0;
        if (c - 1 >= 0 && board[r][c - 1] == id)
            sz += cntboard(board, r, c - 1, id) + 1;
        if (c + 1 < BOARD_SIZE && board[r][c + 1] == id)
            sz += cntboard(board, r, c + 1, id) + 1;
        if (r - 1 >= 0 && board[r - 1][c] == id)
            sz += cntboard(board, r - 1, c, id) + 1;
        if (r + 1 < BOARD_SIZE && board[r + 1][c] == id)
            sz += cntboard(board, r + 1, c, id) + 1;

        return sz;
    }

    int getWinner(const int (*mapStat)[BOARD_SIZE])
    {
        // change array to vector
        std::vector<std::vector<int>> tmpmap(BOARD_SIZE, std::vector<int>(BOARD_SIZE, 0));
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++)
                tmpmap[i][j] = mapStat[i][j];
        }

        // go through whole board and get the sum of each player
        std::vector<double> id(5);
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (tmpmap[i][j] < 1)
                    continue;
                id[tmpmap[i][j]] += pow(cntboard(tmpmap, i, j, tmpmap[i][j]) + 1, 1.25);
            }
        }

        return std::max_element(id.begin(), id.end()) - id.begin();
    }

    // Select the child node with the highest UCB1 score
    MCTSNode *select(MCTSNode *node)
    {
        MCTSNode *selected_node = nullptr;
        double best_score = -1e9;
        for (auto &child : node->children) {
            double score = child->UCB1();
            if (score > best_score) {
                best_score = score;
                selected_node = child;
            }
        }
        return selected_node;
    }

    void expand(MCTSNode *node, std::mt19937 &thread_gen)
    {
        int next_p = next_player_id[node->player_id], end = 1;
        std::vector<std::vector<int>> legal_actions;
        for (int i = 0; i < 4; i++) {
            legal_actions = getLegalActions(node->mapStat, node->sheepStat, next_p);
            if (legal_actions.size() != 0) {
                end = 0;
                break;
            }
            next_p = next_player_id[next_p];
        }

        if (end) {
            node->end_state = true;
            return;
        }

        std::shuffle(legal_actions.begin(), legal_actions.end(), thread_gen);
        for (auto &action : legal_actions) {

            // copy the current state
            int new_mapStat[BOARD_SIZE][BOARD_SIZE], new_sheepStat[BOARD_SIZE][BOARD_SIZE];
            memcpy(new_mapStat, node->mapStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
            memcpy(new_sheepStat, node->sheepStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));

            // update the new state
            new_mapStat[action[0]][action[1]] = next_p;
            new_sheepStat[action[0]][action[1]] -= action[2];

            int new_x = action[0] + dir[action[3] - 1][0], new_y = action[1] + dir[action[3] - 1][1];
            while (new_x + dir[action[3] - 1][0] >= 0 && new_x + dir[action[3] - 1][0] < BOARD_SIZE &&
                   new_y + dir[action[3] - 1][1] >= 0 && new_y + dir[action[3] - 1][1] < BOARD_SIZE &&
                   new_mapStat[new_x + dir[action[3] - 1][0]][new_y + dir[action[3] - 1][1]] == 0) {
                new_x += dir[action[3] - 1][0];
                new_y += dir[action[3] - 1][1];
            }
            new_mapStat[new_x][new_y] = next_p;
            new_sheepStat[new_x][new_y] = action[2];

            // create a new node
            MCTSNode *new_node = new MCTSNode(new_mapStat, new_sheepStat, next_p, node, action, false);
            node->children.push_back(new_node);
        }
    }

    int rollout(MCTSNode *node, std::mt19937 &thread_gen)
    {
        if (node->end_state)
            return getWinner(node->mapStat);

        MCTSNode *cur_node = node->children[node->expand_id++];
        int player = next_player_id[cur_node->player_id], cur_state[BOARD_SIZE][BOARD_SIZE],
            cur_sheep[BOARD_SIZE][BOARD_SIZE];
        memcpy(cur_state, cur_node->mapStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
        memcpy(cur_sheep, cur_node->sheepStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));

        // simulate
        int passID = 0;
        while (true) {
            if (passID & (1 << player)) {
                player = next_player_id[player];
                continue;
            }

            auto legal_actions = getLegalActions(cur_state, cur_sheep, player);
            if (legal_actions.size() == 0) {
                passID |= 1 << player;

                // no more move
                if (passID == 0b11110)
                    return getWinner(cur_state);
            }
            else {
                std::shuffle(legal_actions.begin(), legal_actions.end(), thread_gen);
                auto action = legal_actions[0];

                // update the new state
                cur_state[action[0]][action[1]] = player;
                cur_sheep[action[0]][action[1]] -= action[2];

                int new_x = action[0] + dir[action[3] - 1][0], new_y = action[1] + dir[action[3] - 1][1];
                while (new_x + dir[action[3] - 1][0] >= 0 && new_x + dir[action[3] - 1][0] < BOARD_SIZE &&
                       new_y + dir[action[3] - 1][1] >= 0 && new_y + dir[action[3] - 1][1] < BOARD_SIZE &&
                       cur_state[new_x + dir[action[3] - 1][0]][new_y + dir[action[3] - 1][1]] == 0) {
                    new_x += dir[action[3] - 1][0];
                    new_y += dir[action[3] - 1][1];
                }
                cur_state[new_x][new_y] = player;
                cur_sheep[new_x][new_y] = action[2];
            }

            player = next_player_id[player];
        }
    }

    // Update the win score and visit count of the nodes in the path
    void backpropagate(MCTSNode *node, int winner_id)
    {
        MCTSNode *cur_node = node;
        while (cur_node != nullptr) {
            cur_node->visit_count++;
            if (cur_node->player_id == winner_id)
                cur_node->win_score++;
            cur_node = cur_node->parent;
        }
    }

    void growTree(const int (*mapStat)[BOARD_SIZE], const int (*sheepStat)[BOARD_SIZE], int thread_id)
    {
        std::mt19937 thread_gen;
        thread_gen.seed(thread_id);
        const auto time_limit = std::chrono::milliseconds(2800); // 2.8s

        delete roots[thread_id];
        roots[thread_id] = new MCTSNode(mapStat, sheepStat, pre_player_id[player_id]);

        auto start_time = std::chrono::high_resolution_clock::now();
        int cnt = 0;
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() -
                                                                     start_time) < time_limit) {
            cnt++;
            MCTSNode *node = roots[thread_id];

            // select
            while (node->children.size() != 0 && node->expand_id == node->children.size())
                node = select(node);

            // expand
            if (node->end_state == false && node->children.size() == 0)
                expand(node, thread_gen);

            // rollout
            int winner_id = rollout(node, thread_gen);

            // backpropagation
            backpropagate(node, winner_id);
        }
        std::cout << cnt << std::endl;
    }

  public:
    MCTSAgent(int _player_id, int _parallel_num, const int mapStat[BOARD_SIZE][BOARD_SIZE],
              const int sheepStat[BOARD_SIZE][BOARD_SIZE])
        : player_id(_player_id), parallel_num(_parallel_num)
    {
        roots.resize(parallel_num);
        for (int i = 0; i < parallel_num; i++)
            roots[i] = new MCTSNode(mapStat, sheepStat, pre_player_id[player_id]);
    }

    ~MCTSAgent()
    {
        for (auto &root : roots)
            delete root;
    }

    std::vector<int> runMCTS(const int mapStat[BOARD_SIZE][BOARD_SIZE], const int sheepStat[BOARD_SIZE][BOARD_SIZE])
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < parallel_num; ++i)
            threads.push_back(std::thread(&MCTSAgent::growTree, this, mapStat, sheepStat, i));
        for (auto &thread : threads)
            thread.join();

        // Merge the results of the parallel MCTS
        int max_visits = 0;
        std::vector<int> best_action;
        std::map<std::vector<int>, int> action_visit_count;

        for (MCTSNode *root : roots) {
            for (MCTSNode *child : root->children)
                action_visit_count[child->action] += child->visit_count;
        }

        for (auto &action : action_visit_count) {
            if (action.second >= max_visits) {
                max_visits = action.second;
                best_action = action.first;
            }
        }

        return best_action;
    }
};

/***
 *       _____                         _____ _             _
 *      / ____|                       / ____| |           | |
 *     | |  __  __ _ _ __ ___   ___  | (___ | |_ __ _ _ __| |_
 *     | | |_ |/ _` | '_ ` _ \ / _ \  \___ \| __/ _` | '__| __|
 *     | |__| | (_| | | | | | |  __/  ____) | || (_| | |  | |_
 *      \_____|\__,_|_| |_| |_|\___| |_____/ \__\__,_|_|   \__|
 *
 *
 */

/*
    選擇起始位置
    選擇範圍僅限場地邊緣(至少一個方向為牆)

    return: init_pos
    init_pos=<x,y>,代表你要選擇的起始位置

*/
std::vector<int> InitPos(int mapStat[12][12])
{
    std::vector<int> init_pos;
    init_pos.resize(2);

    /*
        Write your code here
    */
    int best = INT_MIN;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (mapStat[i][j] != 0)
                continue;

            int valid = 0;
            for (int k = 1; k < 9; k += 2) {
                if (i + dir[k][0] >= 0 && i + dir[k][0] < 12 && j + dir[k][1] >= 0 && j + dir[k][1] < 12 &&
                    mapStat[i + dir[k][0]][j + dir[k][1]] == -1) {
                    valid = 1;
                    break;
                }
            }
            if (!valid)
                continue;

            int score = 0;
            for (int k = 0; k < 9; k++) {
                if (k == 4)
                    continue;

                int x = i, y = j, times = 0;
                while (++times < 3 && x + dir[k][0] >= 0 && x + dir[k][0] < BOARD_SIZE && y + dir[k][1] >= 0 &&
                       y + dir[k][1] < BOARD_SIZE && mapStat[x + dir[k][0]][y + dir[k][1]] == 0) {
                    x += dir[k][0];
                    y += dir[k][1];
                    score++;
                }
            }

            int step[8][2] = {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}, {2, 1}, {2, -1}, {-2, 1}, {-2, -1}};
            for (int k = 0; k < 8; k++) {
                int x = i + step[k][0], y = j + step[k][1];
                if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && mapStat[x][y] == 0)
                    score++;
            }

            // std::cout << i << " " << j << " " << score << "\n";
            if (score > best) {
                best = score;
                init_pos[0] = i;
                init_pos[1] = j;
            }
        }
    }
    return init_pos;
}

/*
    產出指令

    input:
    playerID: 你在此局遊戲中的角色(1~4)
    mapStat : 棋盤狀態, 為 12*12矩陣,
                    0=可移動區域, -1=障礙, 1~4為玩家1~4佔領區域
    sheepStat : 羊群分布狀態, 範圍在0~16, 為 12*12矩陣

    return Step
    Step : <x,y,m,dir>
            x, y 表示要進行動作的座標
            m = 要切割成第二群的羊群數量
            dir = 移動方向(1~9),對應方向如下圖所示
            1 2 3
            4 X 6
            7 8 9
*/
std::vector<int> GetStep(int playerID, int mapStat[12][12], int sheepStat[12][12], MCTSAgent &agent)
{
    // std::vector<int> step;
    // step.resize(4);

    /*
        Write your code here
    */
    return agent.runMCTS(mapStat, sheepStat);
}

int main()
{
    int id_package;
    int playerID;
    int mapStat[12][12];
    int sheepStat[12][12];

    // player initial
    GetMap(id_package, playerID, mapStat);
    std::vector<int> init_pos = InitPos(mapStat);
    SendInitPos(id_package, init_pos);

    MCTSAgent agent = MCTSAgent(playerID, 4, mapStat, sheepStat);

    while (true) {
        if (GetBoard(id_package, mapStat, sheepStat))
            break;

        std::vector<int> step = GetStep(playerID, mapStat, sheepStat, agent);
        SendStep(id_package, step);
    }
}