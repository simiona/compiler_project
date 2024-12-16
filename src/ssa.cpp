#include "ssa.h"
#include "bg_llvm.h"
#include "graph.hpp"
#include "liveness.h"
#include "printLLVM.h"
#include <cassert>
#include <iostream>
#include <list>
#include <queue>
#include <stack>
#include <string>
#include <iterator>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;
using namespace LLVMIR;
using namespace GRAPH;
struct imm_Dominator
{
    LLVMIR::L_block *pred;
    unordered_set<LLVMIR::L_block *> succs;
};

unordered_map<L_block *, unordered_set<L_block *>> dominators;
unordered_map<L_block *, imm_Dominator> tree_dominators;
unordered_map<L_block *, unordered_set<L_block *>> DF_array;
unordered_map<L_block *, Node<LLVMIR::L_block *> *> revers_graph; // 存储block到node的映射
unordered_map<Temp_temp *, AS_operand *> temp2ASoper;

static void init_table()
{
    dominators.clear();
    tree_dominators.clear();
    DF_array.clear();
    revers_graph.clear();
    temp2ASoper.clear();
}

LLVMIR::L_prog *SSA(LLVMIR::L_prog *prog)
{
    for (auto &fun : prog->funcs)
    {
        init_table();
        combine_addr(fun);
        mem2reg(fun);
        auto RA_bg = Create_bg(fun->blocks);
        // printL_block(cout,RA_bg.mynodes[0]->info);
        SingleSourceGraph(RA_bg.mynodes[0], RA_bg, fun);
        Liveness(RA_bg.mynodes[0], RA_bg, fun->args);
        Dominators(RA_bg);
        // printf_domi();
        tree_Dominators(RA_bg);
        // printf_D_tree();
        // 默认0是入口block
        computeDF(RA_bg, RA_bg.mynodes[0]);
        // Show_graph(stdout,RA_bg);
        // printf_DF();
        Place_phi_fu(RA_bg, fun);
        Rename(RA_bg);
        combine_addr(fun);
    }
    return prog;
}

static bool is_mem_variable(L_stm *stm)
{
    return stm->type == L_StmKind::T_ALLOCA &&
           stm->u.ALLOCA->dst->kind == OperandKind::TEMP &&
           stm->u.ALLOCA->dst->u.TEMP->type == TempType::INT_PTR &&
           stm->u.ALLOCA->dst->u.TEMP->len == 0;
}

// 保证相同的AS_operand,地址一样 。常量除外
void combine_addr(LLVMIR::L_func *fun)
{
    unordered_map<Temp_temp *, unordered_set<AS_operand **>> temp_set;
    unordered_map<Name_name *, unordered_set<AS_operand **>> name_set;
    for (auto &block : fun->blocks)
    {
        for (auto &stm : block->instrs)
        {
            auto AS_operand_list = get_all_AS_operand(stm);
            for (auto AS_op : AS_operand_list)
            {
                if ((*AS_op)->kind == OperandKind::TEMP)
                {
                    temp_set[(*AS_op)->u.TEMP].insert(AS_op);
                }
                else if ((*AS_op)->kind == OperandKind::NAME)
                {
                    name_set[(*AS_op)->u.NAME].insert(AS_op);
                }
            }
        }
    }
    for (auto temp : temp_set)
    {
        AS_operand *fi_AS_op = **temp.second.begin();
        for (auto AS_op : temp.second)
        {
            *AS_op = fi_AS_op;
        }
    }
    for (auto name : name_set)
    {
        AS_operand *fi_AS_op = **name.second.begin();
        for (auto AS_op : name.second)
        {
            *AS_op = fi_AS_op;
        }
    }
}

void mem2reg(L_func *fun)
{
    if (fun->blocks.empty())
    {
        return;
    }
    queue<L_block *> q;
    set<L_block *> used;
    unordered_map<Temp_label *, L_block *> label2block;
    for (auto block : fun->blocks)
    {
        label2block[block->label] = block;
    }
    q.push(fun->blocks.front());
    used.insert(fun->blocks.front());
    while (!q.empty())
    {
        auto block = q.front();
        q.pop();
        for (auto succ : block->succs)
        {
            if (label2block.find(succ) == label2block.end())
            {
                continue;
            }
            auto succ_block = label2block[succ];
            if (used.find(succ_block) == used.end())
            {
                q.push(succ_block);
                used.insert(succ_block);
            }
        }
    }

    for (auto it = fun->blocks.begin(); it != fun->blocks.end();)
    {
        if (used.find(*it) == used.end())
        {
            it = fun->blocks.erase(it);
        }
        else
        {
            ++it;
        }
    }
    unordered_map<AS_operand *, AS_operand *> scalars, load_replacements;

    for (auto &stm : fun->blocks.front()->instrs)
    {
        if (is_mem_variable(stm))
        {
            auto temp = Temp_newtemp_int();
            auto AS_op = new AS_operand();
            AS_op->kind = OperandKind::TEMP;
            AS_op->u.TEMP = temp;
            scalars[stm->u.ALLOCA->dst] = AS_op;
            temp2ASoper[AS_op->u.TEMP] = AS_op;
        }
    }

    for (auto &block : fun->blocks)
    {
        std::list<L_stm *> new_instrs;
        for (auto &stm : block->instrs)
        {
            switch (stm->type)
            {
            case L_StmKind::T_LOAD:
            {
                if (scalars.find(stm->u.LOAD->ptr) != scalars.end())
                {
                    load_replacements[stm->u.LOAD->dst] = scalars[stm->u.LOAD->ptr];
                }
                else
                {
                    new_instrs.push_back(stm);
                }
            }
            break;
            case L_StmKind::T_STORE:
            {
                if (scalars.find(stm->u.STORE->ptr) != scalars.end())
                {
                    if (load_replacements.find(stm->u.STORE->src) != load_replacements.end())
                    {
                        new_instrs.push_back(L_Move(load_replacements[stm->u.STORE->src], scalars[stm->u.STORE->ptr]));
                    }
                    else
                    {
                        new_instrs.push_back(L_Move(stm->u.STORE->src, scalars[stm->u.STORE->ptr]));
                    }
                }
                else
                {
                    auto AS_operand_list = get_all_AS_operand(stm);
                    for (auto AS_op : AS_operand_list)
                    {
                        if (load_replacements.find(*AS_op) != load_replacements.end())
                        {
                            *AS_op = load_replacements[*AS_op];
                        }
                    }
                    new_instrs.push_back(stm);
                }
            }
            break;
            case L_StmKind::T_ALLOCA:
            {
                if (scalars.find(stm->u.ALLOCA->dst) != scalars.end())
                {
                    new_instrs.push_back(L_Move(AS_Operand_Const(0), scalars[stm->u.ALLOCA->dst]));
                }
                else
                {
                    new_instrs.push_back(stm);
                }
            }
            break;
            default:
            {
                auto AS_operand_list = get_all_AS_operand(stm);
                for (auto AS_op : AS_operand_list)
                {
                    if (load_replacements.find(*AS_op) != load_replacements.end())
                    {
                        *AS_op = load_replacements[*AS_op];
                    }
                }
                new_instrs.push_back(stm);
            }
            break;
            }
        }
        block->instrs = new_instrs;
    }
}

void Dominators(GRAPH::Graph<LLVMIR::L_block *> &bg)
{
    // 标签到节点
    std::unordered_map<int, Node<LLVMIR::L_block *> *> labelToNodeMap;
    // 节点到其支配者集合
    std::unordered_map<Node<LLVMIR::L_block *> *, std::unordered_set<Node<LLVMIR::L_block *> *>> nodeDominators;

    // 构建标签到节点的映射
    for (auto &nodePair : bg.mynodes)
    {
        labelToNodeMap[nodePair.first] = nodePair.second;
    }

    if (labelToNodeMap.empty())
    {
        return;
    }

    Node<LLVMIR::L_block *> *entryNode = nullptr; // 入口节点
    for (auto &nodePair : bg.mynodes)
    {
        if (nodePair.first == 0)
        {
            entryNode = nodePair.second;
            break;
        }
    }

    // 收集所有节点
    std::unordered_set<Node<LLVMIR::L_block *> *> allNodes;
    for (const auto &pair : labelToNodeMap)
    {
        allNodes.insert(pair.second);
    }

    // 初始化支配者集合
    for (auto node : allNodes)
    {
        if (node == entryNode)
        {
            nodeDominators[entryNode] = {entryNode}; // 入口节点支配自身
        }
        else
        {
            nodeDominators[node] = allNodes; // 其他节点初始支配集合为所有节点
        }
    }

    bool hasChanged = true;
    while (hasChanged)
    {
        hasChanged = false;

        for (auto currentNode : allNodes)
        {
            if (currentNode == entryNode)
            {
                continue;
            }

            // 当前节点的新的支配集合
            std::unordered_set<Node<LLVMIR::L_block *> *> newDominators = allNodes;
            for (auto predecessor : *(currentNode->pred()))
            {
                auto predNode = bg.mynodes[predecessor];
                auto predDominators = nodeDominators[predNode];

                // 交集运算
                std::unordered_set<Node<LLVMIR::L_block *> *> tempSet;
                for (auto node : newDominators)
                {
                    if (predDominators.find(node) != predDominators.end())
                    {
                        tempSet.insert(node);
                    }
                }
                newDominators = tempSet;
            }

            // 将当前节点自身加入到支配者集合
            newDominators.insert(currentNode);

            // 检查支配者集合是否发生变化
            if (newDominators != nodeDominators[currentNode])
            {
                nodeDominators[currentNode] = newDominators;
                hasChanged = true;
            }
        }
    }

    for (const auto &pair : nodeDominators)
    {
        dominators[pair.first->info] = {};
        for (auto dominatorNode : pair.second)
        {
            dominators[pair.first->info].insert(dominatorNode->info);
        }
    }
}

void printf_domi()
{
    printf("Dominator:\n");
    for (auto x : dominators)
    {
        printf("%s :\n", x.first->label->name.c_str());
        for (auto t : x.second)
        {
            printf("%s ", t->label->name.c_str());
        }
        printf("\n\n");
    }
}

void printf_D_tree()
{
    printf("dominator tree:\n");
    for (auto x : tree_dominators)
    {
        printf("%s :\n", x.first->label->name.c_str());
        for (auto t : x.second.succs)
        {
            printf("%s ", t->label->name.c_str());
        }
        printf("\n\n");
    }
}
void printf_DF()
{
    printf("DF:\n");
    for (auto x : DF_array)
    {
        printf("%s :\n", x.first->label->name.c_str());
        for (auto t : x.second)
        {
            printf("%s ", t->label->name.c_str());
        }
        printf("\n\n");
    }
}

void tree_Dominators(GRAPH::Graph<LLVMIR::L_block *> &bg)
{
    for (auto &nodePair : bg.mynodes)
    {
        if (nodePair.first == 0)
        {
            continue;
        }

        auto &currentNode = nodePair.second;
        auto &currentBlock = currentNode->info;
        auto &currentBlockDominators = dominators[currentBlock];

        for (auto potentialDominator : currentBlockDominators)
        {
            if (potentialDominator == currentBlock)
            {
                continue; // 当前块自身
            }

            bool isImmediateDominator = true;

            for (auto otherDominator : currentBlockDominators)
            {
                if (otherDominator == currentBlock || otherDominator == potentialDominator)
                {
                    continue; // 当前块和潜在支配者
                }

                if (dominators[otherDominator].find(potentialDominator) != dominators[otherDominator].end())
                {
                    isImmediateDominator = false;
                    break; // 如果发现另一个支配者支配潜在支配者，则不是直接支配者
                }
            }

            if (isImmediateDominator)
            {
                tree_dominators[potentialDominator].succs.insert(currentBlock); // 建立后继关系
                tree_dominators[currentBlock].pred = potentialDominator;        // 前驱
                break;
            }
        }
    }
}

void computeDF(GRAPH::Graph<LLVMIR::L_block *> &bg, GRAPH::Node<LLVMIR::L_block *> *r)
{
    for (auto &[id, node] : bg.mynodes)
    {
        revers_graph[node->info] = node;
    }
    // DF
    unordered_set<L_block *> S{};

    for (auto succ_num : r->succs)
    {
        auto succ_block = bg.mynodes[succ_num]->info;

        if (tree_dominators[succ_block].pred != r->info)
        {
            S.insert(succ_block);
        }
    }

    for (auto &child : tree_dominators[r->info].succs)
    {
        computeDF(bg, revers_graph[child]);

        for (auto &w : DF_array[child])
        {
            if (dominators[w].find(r->info) == dominators[w].end() || w == r->info)
            {
                S.insert(w);
            }
        }
    }

    DF_array[r->info] = S;
}

// 只对标量做
void Place_phi_fu(GRAPH::Graph<LLVMIR::L_block *> &bg, L_func *fun)
{
    // 定义变量：存储每个变量的定义位置
    std::unordered_map<Temp_temp *, std::unordered_set<L_block *>> variableDefSites;
    // 存储每个基本块的 phi 函数变量集合
    std::unordered_map<L_block *, std::unordered_set<Temp_temp *>> phiNodes;

    // 收集每个变量的定义点
    for (auto &nodePair : bg.mynodes)
    {
        auto &blockNode = nodePair.second;
        for (auto definedVariable : FG_def(blockNode))
        {
            variableDefSites[definedVariable].insert(blockNode->info);
        }
    }

    // 遍历每个变量的定义点，计算需要插入 phi 函数的位置
    for (auto &defSitePair : variableDefSites)
    {
        auto &variable = defSitePair.first;
        auto &workList = defSitePair.second;

        while (!workList.empty())
        {
            auto currentBlock = *workList.begin();
            workList.erase(currentBlock);

            for (auto dominanceFrontierBlock : DF_array[currentBlock])
            {
                auto dominanceNode = revers_graph[dominanceFrontierBlock];
                auto &dominanceNodeDefs = FG_def(dominanceNode);

                if (phiNodes[dominanceFrontierBlock].find(variable) == phiNodes[dominanceFrontierBlock].end() &&
                    FG_In(dominanceNode).find(variable) != FG_In(dominanceNode).end())
                {
                    phiNodes[dominanceFrontierBlock].insert(variable);

                    if (dominanceNodeDefs.find(variable) == dominanceNodeDefs.end())
                    {
                        workList.insert(dominanceFrontierBlock);
                    }
                }
            }
        }
    }

    // 插入 phi 函数到对应的基本块中
    for (auto &nodePair : bg.mynodes)
    {
        auto &blockNode = nodePair.second;

        for (auto variable : phiNodes[blockNode->info])
        {
            std::vector<std::pair<AS_operand *, Temp_label *>> phiOperands;

            for (auto predecessorLabel : blockNode->preds)
            {
                auto predecessorNode = bg.mynodes[predecessorLabel];
                phiOperands.push_back({temp2ASoper[variable], predecessorNode->info->label});
            }

            // 在当前基本块的指令集的第二位置插入 phi 函数
            blockNode->info->instrs.insert(++blockNode->info->instrs.begin(), L_Phi(temp2ASoper[variable], phiOperands));
        }
    }
}

static list<AS_operand **> get_def_int_operand(LLVMIR::L_stm *stm)
{
    list<AS_operand **> ret1 = get_def_operand(stm), ret2;
    for (auto AS_op : ret1)
    {
        if ((**AS_op).kind == OperandKind::TEMP && (**AS_op).u.TEMP->type == TempType::INT_TEMP)
        {
            ret2.push_back(AS_op);
        }
    }
    return ret2;
}

static list<AS_operand **> get_use_int_operand(LLVMIR::L_stm *stm)
{
    list<AS_operand **> ret1 = get_use_operand(stm), ret2;
    for (auto AS_op : ret1)
    {
        if ((**AS_op).kind == OperandKind::TEMP && (**AS_op).u.TEMP->type == TempType::INT_TEMP)
        {
            ret2.push_back(AS_op);
        }
    }
    return ret2;
}

static void Rename_temp(
    GRAPH::Graph<LLVMIR::L_block *> &bg,
    GRAPH::Node<LLVMIR::L_block *> *currentNode,
    std::unordered_map<Temp_temp *, std::stack<Temp_temp *>> &tempStack)
{
    std::vector<Temp_temp *> modifiedTemps;

    // 遍历当前节点的指令列表
    for (auto &instruction : currentNode->info->instrs)
    {
        // 处理非 PHI 指令的使用操作数
        if (instruction->type != L_StmKind::T_PHI)
        {
            auto usedOperands = get_use_int_operand(instruction);
            for (auto operand : usedOperands)
            {
                auto oldTemp = (*operand)->u.TEMP;
                if (tempStack.find(oldTemp) != tempStack.end())
                {
                    *operand = AS_Operand_Temp(tempStack[oldTemp].top());
                }
            }
        }

        auto definedOperands = get_def_int_operand(instruction);
        for (auto operand : definedOperands)
        {
            auto temp = (*operand)->u.TEMP;
            if (tempStack.find(temp) != tempStack.end())
            {
                auto newTemp = Temp_newtemp_int();
                tempStack[temp].push(newTemp);
                modifiedTemps.push_back(temp);
                *operand = AS_Operand_Temp(newTemp);
            }
        }
    }

    // 更新后继节点中的 PHI 函数操作数
    for (auto successorLabel : currentNode->succs)
    {
        auto successorNode = bg.mynodes[successorLabel];
        for (auto &instruction : successorNode->info->instrs)
        {
            if (instruction->type == L_StmKind::T_PHI)
            {
                for (auto &[operand, sourceLabel] : instruction->u.PHI->phis)
                {
                    if (sourceLabel != currentNode->info->label)
                    {
                        continue;
                    }
                    auto tempToReplace = operand->u.TEMP;
                    if (tempStack.find(tempToReplace) != tempStack.end())
                    {
                        auto topTemp = tempStack[tempToReplace].top();
                        operand = AS_Operand_Temp(topTemp);
                    }
                }
            }
        }
    }

    for (auto childLabel : tree_dominators[currentNode->info].succs)
    {
        Rename_temp(bg, revers_graph[childLabel], tempStack);
    }

    for (auto temp : modifiedTemps)
    {
        tempStack[temp].pop();
    }
}

void Rename(GRAPH::Graph<LLVMIR::L_block *> &bg)
{
    // 每个临时变量对应一个栈
    std::unordered_map<Temp_temp *, std::stack<Temp_temp *>> tempStacks;

    for (const auto &[temp, _] : temp2ASoper)
    {
        tempStacks[temp].push(temp);
    }

    // 从0开始重命名变量
    Rename_temp(bg, bg.mynodes[0], tempStacks);
}