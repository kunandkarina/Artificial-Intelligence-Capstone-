import STcpClient
import numpy as np
import random

dir = ((-1, -1), (0, -1), (1, -1), (-1, 0), (0, 0), (1, 0), (-1, 1), (0, 1), (1, 1))

class Random:
    def __init__(self, playerID):
        self.playerID = playerID
        self.sheeps = 16

    def random_sheeps(self, sheeps):
        return random.randint(1, sheeps-1)
'''
    選擇起始位置
    選擇範圍僅限場地邊緣(至少一個方向為牆)
    
    return: init_pos
    init_pos=[x,y],代表起始位置
    
'''

def InitPos(mapStat, agent):
    init_pos = [0, 0]
    '''
        Write your code here

    '''
    legal_pos = []
    best = float('-inf')
    for i in range(12):
        for j in range(12):
            if mapStat[i][j] != 0:
                continue
            valid = 0
            for k in dir:
                if k == (0, 0):
                    continue
                if(i + k[0] >= 0 and i + k[0] < 12 and j + k[1] >= 0 and j + k[1] < 12 and mapStat[i + k[0]][j + k[1]] == -1):
                    valid = 1
                    break
                
            if valid == 0:
                continue
            legal_pos.append((i, j))
    init_pos = random.choice(legal_pos)
    return init_pos


'''
    產出指令
    
    input: 
    playerID: 你在此局遊戲中的角色(1~4)
    mapStat : 棋盤狀態(list of list), 為 12*12矩陣, 
              0=可移動區域, -1=障礙, 1~4為玩家1~4佔領區域
    sheepStat : 羊群分布狀態, 範圍在0~16, 為 12*12矩陣

    return Step
    Step : 3 elements, [(x,y), m, dir]
            x, y 表示要進行動作的座標 
            m = 要切割成第二群的羊群數量
            dir = 移動方向(1~9),對應方向如下圖所示
            1 2 3
            4 X 6
            7 8 9
'''
def GetStep(playerID, mapStat, sheepStat, agent):
    step = [(0, 0), 0, 1]
    '''
    Write your code here
    
    '''
    legalMoves = []
    for i, j in np.ndindex(mapStat.shape):
        if mapStat[i][j] == playerID and sheepStat[i][j] > 1:
            valid = 0
            for k in dir:
                if k == (0,0):
                    continue
                x, y = i + k[0], j + k[1]
                if 0 <= x < 12 and 0 <= y < 12 and mapStat[x][y] == 0:
                    valid = 1
                    break
            if valid:
                legalMoves.append((i, j))
    if not legalMoves:
        return None
    move = random.choice(legalMoves)
    legaldir = []
    for k in dir:
        if k == (0, 0):
            continue
        x, y = move[0] + k[0], move[1] + k[1]
        if 0 <= x < 12 and 0 <= y < 12 and mapStat[x][y] == 0:
            legaldir.append(k)
    direction = random.choice(legaldir)
    tp_move = list(move)
    while(0 <= tp_move[0] + direction[0] < 12 and 0 <= tp_move[1] + direction[1] < 12 and mapStat[tp_move[0] + direction[0]][tp_move[1] + direction[1]] == 0):
        tp_move[0] += direction[0]
        tp_move[1] += direction[1]
    tp_sheeps = agent.random_sheeps(sheepStat[move[0]][move[1]])
    direction_index = dir.index(direction)
    step = [move, tp_sheeps, direction_index + 1]
    mapStat[tp_move[0]][tp_move[1]] = playerID
    sheepStat[tp_move[0]][tp_move[1]] = tp_sheeps
    sheepStat[move[0]][move[1]] -= tp_sheeps
    return step


# player initial
(id_package, playerID, mapStat) = STcpClient.GetMap()
agent = Random(playerID)
init_pos = InitPos(mapStat, agent)
STcpClient.SendInitPos(id_package, init_pos)

# start game
while (True):
    (end_program, id_package, mapStat, sheepStat) = STcpClient.GetBoard()
    if end_program:
        STcpClient._StopConnect()
        break
    Step = GetStep(playerID, mapStat, sheepStat, agent)

    STcpClient.SendStep(id_package, Step)
