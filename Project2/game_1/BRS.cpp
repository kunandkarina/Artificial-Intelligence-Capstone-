#include "STcpClient.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <stdlib.h>
#include <vector>
#define BOARD_SIZE 12

const int dir[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}},
          pre_player_id[5] = {0, 4, 1, 2, 3}, next_player_id[5] = {0, 2, 3, 4, 1};

/***
 *      ____            _     _____            _          _____                 _
 *     |  _ \          | |   |  __ \          | |        / ____|               | |
 *     | |_) | ___  ___| |_  | |__) |___ _ __ | |_   _  | (___   ___  __ _  ___| |__
 *     |  _ < / _ \/ __| __| |  _  // _ \ '_ \| | | | |  \___ \ / _ \/ _` |/ __| '_ \
 *     | |_) |  __/\__ \ |_  | | \ \  __/ |_) | | |_| |  ____) |  __/ (_| | (__| | | |
 *     |____/ \___||___/\__| |_|  \_\___| .__/|_|\__, | |_____/ \___|\__,_|\___|_| |_|
 *                                      | |       __/ |
 *                                      |_|      |___/
 */

class BRSAgent {
  private:
    int my_pid;

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

    double getScore(const int mapStat[BOARD_SIZE][BOARD_SIZE], int pid)
    {
        // change array to vector
        std::vector<std::vector<int>> tmpmap(BOARD_SIZE, std::vector<int>(BOARD_SIZE, 0));
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++)
                tmpmap[i][j] = mapStat[i][j];
        }

        // go through whole board and get the sum of each player
        double score = 0;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (tmpmap[i][j] == pid) {
                    int tmp = cntboard(tmpmap, i, j, 1) + 1;
                    score += std::pow(tmp, 1.25);
                }
            }
        }
        return score;
    }

    std::vector<std::vector<int>> getLegalActions(const int mapStat[BOARD_SIZE][BOARD_SIZE],
                                                  const int sheepStat[BOARD_SIZE][BOARD_SIZE], int pid)
    {
        std::vector<std::vector<int>> legal_actions;
        int exist = 0;
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                exist = mapStat[i][j] == pid ? 1 : exist;

                if (mapStat[i][j] != pid || sheepStat[i][j] < 2)
                    continue;

                for (int k = 0; k < 9; ++k) {
                    if (k == 4)
                        continue;
                    if (i + dir[k][0] >= 0 && i + dir[k][0] < BOARD_SIZE && j + dir[k][1] >= 0 &&
                        j + dir[k][1] < BOARD_SIZE && mapStat[i + dir[k][0]][j + dir[k][1]] == 0)
                        legal_actions.push_back({i, j, sheepStat[i][j] / 2, k + 1});
                }
            }
        }

        return legal_actions;
    }

    std::pair<int, int> move_as_far_as_possible(int mapStat[BOARD_SIZE][BOARD_SIZE], std::vector<int> &action)
    {
        int new_x = action[0] + dir[action[3] - 1][0], new_y = action[1] + dir[action[3] - 1][1];
        while (new_x + dir[action[3] - 1][0] >= 0 && new_x + dir[action[3] - 1][0] < BOARD_SIZE &&
               new_y + dir[action[3] - 1][1] >= 0 && new_y + dir[action[3] - 1][1] < BOARD_SIZE &&
               mapStat[new_x + dir[action[3] - 1][0]][new_y + dir[action[3] - 1][1]] == 0) {
            new_x += dir[action[3] - 1][0];
            new_y += dir[action[3] - 1][1];
        }

        return {new_x, new_y};
    }

  public:
    BRSAgent(int _my_pid) : my_pid(_my_pid) {}

    // best reply search algorithm
    std::pair<double, std::vector<int>> brs(int mapStat[BOARD_SIZE][BOARD_SIZE], int sheepStat[BOARD_SIZE][BOARD_SIZE],
                                            int depth, int turn, double alpha, double beta)
    {
        // turn = 0 => my turn, turn = 1 => opponent turn
        if (depth <= 0)
            return {evaluate(mapStat, sheepStat, my_pid), {-1, -1, -1, -1}};

        std::vector<int> v = {-1, -1, -1, -1};
        std::vector<std::vector<std::vector<int>>> legal_actions;
        legal_actions.reserve(4);

        if (!turn)
            legal_actions.emplace_back(getLegalActions(mapStat, sheepStat, my_pid));
        else {
            for (int i = 1; i < 5; ++i) {
                if (i == my_pid)
                    continue;
                legal_actions.emplace_back(getLegalActions(mapStat, sheepStat, i));
            }
        }
        turn = ~turn;
        for (auto &legal_action : legal_actions) {
            for (auto &action : legal_action) {

                // update mapStat and sheepStat
                int new_mapStat[BOARD_SIZE][BOARD_SIZE], new_sheepStat[BOARD_SIZE][BOARD_SIZE];
                memcpy(new_mapStat, mapStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
                memcpy(new_sheepStat, sheepStat, BOARD_SIZE * BOARD_SIZE * sizeof(int));
                int pid = new_mapStat[action[0]][action[1]];
                new_sheepStat[action[0]][action[1]] -= action[2];

                std::pair<int, int> new_pos = move_as_far_as_possible(new_mapStat, action);
                new_mapStat[new_pos.first][new_pos.second] = pid;
                new_sheepStat[new_pos.first][new_pos.second] = action[2];

                std::pair<double, std::vector<int>> new_v =
                    brs(new_mapStat, new_sheepStat, depth - 1, turn, -beta, -alpha);
                new_v.first = -new_v.first;

                if (new_v.first >= beta)
                    return {new_v.first, action};

                if (new_v.first >= alpha) {
                    alpha = new_v.first;
                    v = action;
                }
            }
        }

        return {alpha, v};
    }

    // Evaluation function.
    // NOTE : this should be well designed
    double evaluate(int mapStat[BOARD_SIZE][BOARD_SIZE], int sheepStat[BOARD_SIZE][BOARD_SIZE], int cur_pid)
    {
        return getScore(mapStat, cur_pid);
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
std::vector<int> InitPos(int playerID, int mapStat[12][12], BRSAgent &agent)
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
int time_cnt = 0, d = 5;
std::vector<int> GetStep(int playerID, int mapStat[12][12], int sheepStat[12][12], BRSAgent &agent)
{
    // std::vector<int> step;
    // step.resize(4);

    /*
        Write your code here
    */
    if (++time_cnt > 7)
        ++d;
    std::pair<double, std::vector<int>> ret = agent.brs(mapStat, sheepStat, d, 0, INT_MIN, INT_MAX);
    return ret.second;
}

int main()
{
    int id_package;
    int playerID;
    int mapStat[12][12];
    int sheepStat[12][12];

    // player initial
    GetMap(id_package, playerID, mapStat);
    BRSAgent agent(playerID);
    std::vector<int> init_pos = InitPos(playerID, mapStat, agent);
    SendInitPos(id_package, init_pos);

    while (true) {
        if (GetBoard(id_package, mapStat, sheepStat))
            break;

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<int> step = GetStep(playerID, mapStat, sheepStat, agent);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0
                  << "s" << std::endl;

        SendStep(id_package, step);
    }
}
