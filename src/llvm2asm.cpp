#include "llvm_ir.h"
#include "asm_arm.h"
#include "temp.h"
#include "llvm2asm.h"
#include "allocReg.h"
#include <queue>
#include <cassert>
#include <iostream>
#include "printASM.h"
#include "printLLVM.h"
#include "register_rules.h"
#include <sstream>
#include <iostream>
using namespace std;
using namespace LLVMIR;
using namespace ASM;

#define INSERT1() printf("%s:%d\n", __FILE__, __LINE__);
static int stack_frame;
static bool alloc_frame = false;
struct StructDef
{
    std::vector<int> offset;
    int size;
    StructDef(std::vector<int> _offset, int _size) : offset(_offset), size(_size) {}
    // 打印函数
    void print() const
    {
        std::cerr << "Offsets: ";
        for (const auto &val : offset)
        {
            std::cerr << val << " ";
        }
        std::cerr << "\nSize: " << size << std::endl;
    }
};

static AS_reg *sp = new AS_reg(AS_type::SP, -1);
static unordered_map<int, AS_address *> fpOffset;
static unordered_map<int, AS_relopkind> condMap;
static unordered_map<string, StructDef *> structLayout;
int getMemLength(TempDef &members)
{
    switch (members.kind)
    {
    case TempType::INT_PTR:
        return INT_LENGTH * members.len;
        break;
    case TempType::INT_TEMP:
        return INT_LENGTH;
        break;
    case TempType::STRUCT_PTR:
        // printf("%d\n", structLayout[members.structname]->size);
        return structLayout[members.structname]->size * members.len;
        break;
    case TempType::STRUCT_TEMP:
        // printf("%d\n", structLayout[members.structname]->size);

        return structLayout[members.structname]->size;
        break;
    default:
        assert(0);
    }
}
void structLayoutInit(vector<L_def *> &defs)
{
    // TODO:存储结构体信息
}

void set_stack(L_func &func)
{
    // TODO: 记录局部变量在栈中的位置，设置fpOffset
    //  Fixme: add here

    for (auto &x : fpOffset)
    {
        printf("%r %d %s\n", x.first, printAS_add(x.second).c_str());
    }
}

void new_frame(list<AS_stm *> &as_list, L_func &func)
{
    // Fixme: add here
    // TODO: 调整栈指针

    // 取得函数参数并存入虚拟寄存器
    int i = 0;

    for (int i = 0; i < 8 && i < func.args.size(); i++)
    {
        switch (func.args[i]->type)
        {
        case TempType::INT_TEMP:
            as_list.emplace_back(AS_Mov(new AS_reg(AS_type::Wn, i), new AS_reg(AS_type::Wn, func.args[i]->num)));
            break;

        case TempType::INT_PTR:
        case TempType::STRUCT_PTR:
        case TempType::STRUCT_TEMP:
            as_list.emplace_back(AS_Mov(new AS_reg(AS_type::Xn, i), new AS_reg(AS_type::Xn, func.args[i]->num)));
            break;

        default:
            break;
        }
    }
    int offset = 0;
    for (int i = 8; i < func.args.size(); i++)
    {
        AS_type type;
        switch (func.args[i]->type)
        {
        case TempType::INT_TEMP:
            type = AS_type::Wn;
            break;

        case TempType::INT_PTR:
        case TempType::STRUCT_PTR:
        case TempType::STRUCT_TEMP:
            type = AS_type::Xn;
            break;

        default:
            break;
        }
        auto tep = new AS_reg(type, Temp_newtemp_int()->num);
        as_list.emplace_back(AS_Ldr(tep, new AS_reg(AS_type::ADR, new AS_address(new AS_reg(AS_type::Xn, FPR), offset))));
        offset += 8;
        as_list.emplace_back(AS_Mov(tep, new AS_reg(type, func.args[i]->num)));
    }
}

void free_frame(list<AS_stm *> &as_list)
{
    // TODO:调整sp指针
}
void allignPtr(AS_reg *&op_reg, AS_operand *as_operand, list<AS_stm *> &as_list)
{
    switch (as_operand->kind)
    {
    case OperandKind::TEMP:
    {
        if (fpOffset.find(as_operand->u.TEMP->num) != fpOffset.end())
        {
            auto ptr_fp = fpOffset[as_operand->u.TEMP->num];
            auto teg = new AS_reg(AS_type::Xn, Temp_newtemp_int()->num);
            auto base = new AS_reg(AS_type::Xn, Temp_newtemp_int()->num);

            as_list.push_back(AS_Mov(new AS_reg(AS_type::IMM, ptr_fp->imm), teg));
            as_list.push_back(AS_Binop(AS_binopkind::ADD_, new AS_reg(AS_type::Xn, FPR), teg, base));
            op_reg = new AS_reg(AS_type::ADR, new AS_address(base, 0));
        }
        else
        {
            op_reg = new AS_reg(AS_type::ADR, new AS_address(new AS_reg(AS_type::Xn, as_operand->u.TEMP->num), 0));
            // 计算出来的和函数参数中的
        }
        break;
    }
    case OperandKind::NAME:
    {
        auto Xn = new AS_reg(AS_type::Xn, Temp_newtemp_int()->num);
        op_reg = new AS_reg(AS_type::ADR, new AS_address(Xn, 0));
        as_list.push_back(AS_Adr(new AS_label(as_operand->u.NAME->name->name), Xn));
        break;
    }
    case OperandKind::ICONST:
    {
        assert(0);
        break;
    }
    default:
        assert(0);

        break;
    }
    // printf("%s\n", printAS_reg(op_reg).c_str());
}

void llvm2asmBinop(list<AS_stm *> &as_list, L_stm *binop_stm)
{
}

void llvm2asmLoad(list<AS_stm *> &as_list, L_stm *load_stm)
{
}

void llvm2asmStore(list<AS_stm *> &as_list, L_stm *store_stm)
{
}

void llvm2asmCmp(list<AS_stm *> &as_list, L_stm *cmp_stm)
{
}
void llvm2asmMov(list<AS_stm *> &as_list, L_stm *mov_stm)
{
}
void llvm2asmCJmp(list<AS_stm *> &as_list, L_stm *cjmp_stm)
{
}

void llvm2asmRet(list<AS_stm *> &as_list, L_stm *ret_stm)
{
}

void llvm2asmGep(list<AS_stm *> &as_list, L_stm *gep_stm)
{
}

void llvm2asmStm(list<AS_stm *> &as_list, L_stm &stm, L_func &func)
{

    if (!alloc_frame && stm.type != L_StmKind::T_LABEL)
    {
        new_frame(as_list, func);
        alloc_frame = true;
    }

    switch (stm.type)
    {
    case L_StmKind::T_BINOP:
    {
        llvm2asmBinop(as_list, &stm);
        break;
    }
    case L_StmKind::T_LOAD:
    {
        llvm2asmLoad(as_list, &stm);
        break;
    }
    case L_StmKind::T_STORE:
    {
        llvm2asmStore(as_list, &stm);
        break;
    }
    case L_StmKind::T_LABEL:
    {
        auto label = new AS_label(stm.u.LABEL->label->name);
        as_list.push_back(AS_Label(label));
        break;
    }
    case L_StmKind::T_JUMP:
    {
        auto label = new AS_label(stm.u.JUMP->jump->name);
        as_list.push_back(AS_B(label));
        break;
    }
    case L_StmKind::T_CMP:
    {
        llvm2asmCmp(as_list, &stm);
        break;
    }
    case L_StmKind::T_CJUMP:
    {
        llvm2asmCJmp(as_list, &stm);
        break;
    }
    case L_StmKind::T_MOVE:
    {
        llvm2asmMov(as_list, &stm);
        break;
    }
    case L_StmKind::T_CALL:
    {
        llvm2asmCall(as_list, &stm);
        break;
    }
    case L_StmKind::T_VOID_CALL:
    {
        llvm2asmVoidCall(as_list, &stm);
        break;
    }
    case L_StmKind::T_RETURN:
    {
        llvm2asmRet(as_list, &stm);
        break;
    }
    case L_StmKind::T_ALLOCA:
    {
        // Do nothing
        break;
    }
    case L_StmKind::T_GEP:
    {

        llvm2asmGep(as_list, &stm);

        break;
    }
    case L_StmKind::T_PHI:
    {
        // Do nothing
        break;
    }
    case L_StmKind::T_NULL:
    {
        // Do nothing
        break;
    }
    }
    //
}
int save_register(list<AS_stm *> &as_list)
{
    int sub = 0;
    for (auto it = allocateRegs.begin(); it != allocateRegs.end(); ++it)
    {
        // 获取当前元素
        int first = *it;
        ++it; // 移动到下一个元素

        // 检查是否有下一个元素
        if (it != allocateRegs.end())
        {
            int second = *it;
            as_list.push_back(AS_Stp(new AS_reg(AS_type::Xn, first), new AS_reg(AS_type::Xn, second), sp, -2 * 8));
            sub += 2 * 8;
        }
        else
        {
            // 如果`set`中的元素个数是奇数，最后一个元素将单独处理
            as_list.push_back(AS_Str(new AS_reg(AS_type::Xn, first), sp, -8));
            sub += 8;
            break;
        }
    }
    as_list.push_back(AS_Stp(new AS_reg(AS_type::Xn, FPR), new AS_reg(AS_type::Xn, LNR), sp, -2 * 8));
    sub += 2 * 8;
    return sub;
}
void load_register(list<AS_stm *> &as_list)
{
    // TODO:函数调用完毕，需要加载保存在栈上的寄存器
}
void getCalls(AS_reg *&op_reg, AS_operand *as_operand, list<AS_stm *> &as_list)
{
    switch (as_operand->kind)
    {
    case OperandKind::ICONST:
    {

        // store from the const: str #1, ...
        // move the instant into x2: mov x2, #1
        int instant = as_operand->u.ICONST;
        AS_reg *src_mov = new AS_reg(AS_type::IMM, instant);
        AS_reg *dst_mov = new AS_reg(AS_type::Wn, Temp_newtemp_int()->num);
        as_list.push_back(AS_Mov(src_mov, dst_mov));
        op_reg = dst_mov;

        break;
    }
    case OperandKind::TEMP:
    {
        switch (as_operand->u.TEMP->type)
        {
        case TempType::INT_PTR:
        case TempType::INT_TEMP:
        {
            int src_num = as_operand->u.TEMP->num;
            if (as_operand->u.TEMP->len != 0)
            {
                allignPtr(op_reg, as_operand, as_list);
                op_reg = op_reg->u.add->base;
            }
            else
            {
                op_reg = new AS_reg(AS_type::Wn, src_num);
            }
            break;
        }

        case TempType::STRUCT_PTR:
        case TempType::STRUCT_TEMP:
        {
            allignPtr(op_reg, as_operand, as_list);
            op_reg = op_reg->u.add->base;
            break;
        }
        }
        break;
    }
    case OperandKind::NAME:
    {
        auto Xn = new AS_reg(AS_type::Xn, Temp_newtemp_int()->num);
        auto Xn2 = new AS_reg(AS_type::Wn, Temp_newtemp_int()->num);
        as_list.push_back(AS_Adr(new AS_label(as_operand->u.NAME->name->name), Xn));
        as_list.push_back(AS_Ldr(Xn2, new AS_reg(AS_type::ADR, new AS_address(Xn, 0))));
        op_reg = Xn;
    }
    }
}
void llvm2asmVoidCall(list<AS_stm *> &as_list, L_stm *call)
{

    for (int i = 0; i < 8 && i < call->u.VOID_CALL->args.size(); i++)
    {
        AS_reg *param;
        getCalls(param, call->u.VOID_CALL->args[i], as_list);
        as_list.emplace_back(AS_Mov(param, new AS_reg(param->type, paramRegs[i])));
    }
    vector<AS_reg *> abcd;
    for (int i = 8; i < call->u.VOID_CALL->args.size(); i++)
    {
        AS_reg *param;
        getCalls(param, call->u.VOID_CALL->args[i], as_list);
        abcd.push_back(param);
    }
    if (abcd.size())
    {
        as_list.push_back(AS_Mov(sp, new AS_reg(AS_type::Xn, XXna)));
        int sub = save_register(as_list);
        as_list.push_back(AS_Mov(new AS_reg(AS_type::Xn, XXna), sp));

        int param_sub = 0;
        for (auto &x : abcd)
        {
            param_sub += 8;
            if (-sub - param_sub < -256)
            {
                auto temp = new AS_reg(AS_type::Xn, XXnb);
                as_list.emplace_back(AS_Mov(new AS_reg(AS_type::IMM, -sub - param_sub), temp));

                as_list.emplace_back(AS_Str(x, new AS_reg(AS_type::ADR, new AS_address(sp, temp))));
            }
            else
            {
                as_list.emplace_back(AS_Str(x, new AS_reg(AS_type::ADR, new AS_address(sp, -sub - param_sub))));
            }
        }
        as_list.emplace_back(AS_Binop(AS_binopkind::SUB_, sp, new AS_reg(AS_type::IMM, sub + param_sub), sp));
    }
    else
    {
        save_register(as_list);
    }

    as_list.push_back(AS_Mov(sp, new AS_reg(AS_type::Xn, FPR)));
    as_list.emplace_back(AS_Bl(new AS_label(call->u.VOID_CALL->fun)));
    if (abcd.size())
    {
        as_list.emplace_back(AS_Binop(AS_binopkind::ADD_, sp, new AS_reg(AS_type::IMM, -abcd.size() * 8), sp));
    }
    load_register(as_list);
}
void llvm2asmCall(list<AS_stm *> &as_list, L_stm *call)
{


}

void allocReg(list<AS_stm *> &as_list, L_func &func)
{
    list<InstructionNode *> liveness;

    forwardLivenessAnalysis(liveness, as_list);

    livenessAnalysis(liveness, as_list);
}
struct BLOCKPHI
{
    string label;
    L_stm *phi;
    BLOCKPHI(string _label, L_stm *_phi) : label(_label), phi(_phi) {}
};

AS_func *llvm2asmFunc(L_func &func)
{
    list<AS_stm *> stms;
    list<BLOCKPHI *> phi;
    unordered_map<string, list<AS_stm *>::iterator> block_map;
    auto p = new AS_func(stms);
    auto func_label = new AS_label(func.name);
    p->stms.push_back(AS_Label(func_label));
    for (auto &x : fpOffset)
    {
        std::ostringstream oss;
        oss << x.first << ":" << printAS_add(x.second).c_str() << endl;
        p->stms.push_back(AS_Llvmir(oss.str()));
    }
    string temp_label = "";
    for (const auto &block : func.blocks)
    {
        for (const auto &instr : block->instrs)
        {
            std::ostringstream oss;
            printL_stm(oss, instr);
            p->stms.push_back(AS_Llvmir(oss.str()));
            llvm2asmStm(p->stms, *instr, func);
            // TODO:遇见phi语句如何处理
        }
    }


    allocReg(p->stms, func);
    return p;
}

void llvm2asmDecl(vector<AS_decl *> &decls, L_def &def)
{
    switch (def.kind)
    {
    case L_DefKind::GLOBAL:
    {
        return;
    }
    case L_DefKind::FUNC:
    {
        AS_decl *decl = new AS_decl(def.u.FUNC->name);
        decls.push_back(decl);
        break;
    }
    case L_DefKind::SRT:
    {
        return;
    }
    }
}

void llvm2asmGlobal(vector<AS_global *> &globals, L_def &def)
{
    switch (def.kind)
    {
    case L_DefKind::GLOBAL:
    {
        switch (def.u.GLOBAL->def.kind)
        {
        case TempType::INT_TEMP:
        {
            AS_label *label = new AS_label(def.u.GLOBAL->name);
            int init;
            if (def.u.GLOBAL->init.size() == 1)
            {
                init = def.u.GLOBAL->init[0];
            }
            else
            {
                init = 0;
            }
            auto global = new AS_global(label, init, 1);
            globals.push_back(global);
            break;
        }
        case TempType::INT_PTR:
        {
            AS_label *label = new AS_label(def.u.GLOBAL->name);
            auto global = new AS_global(label, 0, def.u.GLOBAL->def.len * INT_LENGTH);
            globals.push_back(global);
            break;
        }
        case TempType::STRUCT_TEMP:
        {
            AS_label *label = new AS_label(def.u.GLOBAL->name);
            int struct_len = structLayout[def.u.GLOBAL->def.structname]->size;
            auto global = new AS_global(label, 0, struct_len);
            globals.push_back(global);
            break;
        }
        case TempType::STRUCT_PTR:
        {
            AS_label *label = new AS_label(def.u.GLOBAL->name);
            int struct_len = structLayout[def.u.GLOBAL->def.structname]->size;
            int sum_len = def.u.GLOBAL->def.len * struct_len;
            auto global = new AS_global(label, 0, sum_len);
            globals.push_back(global);
            break;
        }
        }
        break;
    }
    case L_DefKind::FUNC:
    {
        return;
    }
    case L_DefKind::SRT:
    {
        return;
    }
    }
}

AS_prog *llvm2asm(L_prog &prog)
{
    std::vector<AS_global *> globals;
    std::vector<AS_decl *> decls;
    std::vector<AS_func *> func_list;

    auto as_prog = new AS_prog(globals, decls, func_list);

    structLayoutInit(prog.defs);

    // translate function definition
    for (const auto &def : prog.defs)
    {
        llvm2asmDecl(as_prog->decls, *def);
    }

    for (const auto &func : prog.funcs)
    {
        AS_decl *decl = new AS_decl(func->name);
        as_prog->decls.push_back(decl);
    }

    // translate global data
    for (const auto &def : prog.defs)
    {
        llvm2asmGlobal(as_prog->globals, *def);
    }

    // translate each llvm function

    for (const auto &func : prog.funcs)
    {
        alloc_frame = false;

        set_stack(*func);

        as_prog->funcs.push_back(llvm2asmFunc(*func));

        fpOffset.clear();
    }

    return as_prog;
}
