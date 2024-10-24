#include "TypeCheck.h"

// global tabels
// typeMap func2retType; // function name to return type

// global token ids to type
typeMap g_token2Type;
;

// // local token ids to type, since func param can override global param
// typeMap funcparam_token2Type;
vector<typeMap *> local_token2Type;

paramMemberMap func2Param;
paramMemberMap struct2Members;

// private util functions
void error_print(std::ostream &out, A_pos p, string info)
{
    out << "Typecheck error in line " << p->line << ", col " << p->col << ": " << info << std::endl;
    exit(0);
}

void print_token_map(typeMap *map)
{
    for (auto it = map->begin(); it != map->end(); it++)
    {
        std::cout << it->first << " : ";
        if (it->second->type == nullptr)
        {
            std::cout << "undefined";
        }
        else
            switch (it->second->type->type)
            {
            case A_dataType::A_nativeTypeKind:
                switch (it->second->type->u.nativeType)
                {
                case A_nativeType::A_intTypeKind:
                    std::cout << "int";
                    break;
                default:
                    break;
                }
                break;
            case A_dataType::A_structTypeKind:
                std::cout << *(it->second->type->u.structType);
                break;
            default:
                break;
            }
        switch (it->second->isVarArrFunc)
        {
        case 0:
            std::cout << " scalar";
            break;
        case 1:
            std::cout << " array";
            break;
        case 2:
            std::cout << " function";
            break;
        }
        std::cout << std::endl;
    }
}

void print_token_maps()
{
    std::cout << "global token2Type:" << std::endl;
    print_token_map(&g_token2Type);
    std::cout << "local token2Type:" << std::endl;
    for (auto it = local_token2Type.begin(); it != local_token2Type.end(); ++it)
    {
        print_token_map(*it);
    }
}

bool comp_aA_type(aA_type target, aA_type t)
{
    if (!target || !t)
        return false;
    if (target->type != t->type)
        return false;
    if (target->type == A_dataType::A_nativeTypeKind)
        if (target->u.nativeType != t->u.nativeType)
            return false;
    if (target->type == A_dataType::A_structTypeKind)
        if (target->u.structType != t->u.structType)
            return false;
    return true;
}

bool comp_tc_type(tc_type target, tc_type t)
{
    if (!target || !t)
        return false;

    // arr kind first
    if (target->isVarArrFunc && t->isVarArrFunc == 0)
        return false;

    // if target type is nullptr, alwayse ok
    return comp_aA_type(target->type, t->type);
}

tc_type check_RightVal_Type(std::ostream &out, aA_rightVal rightVal)
{
    if (!rightVal)
        return nullptr;
    switch (rightVal->kind)
    {
    case A_rightValType::A_arithExprValKind:
        return check_ArithExpr(out, rightVal->u.arithExpr);
        break;
    case A_rightValType::A_boolExprValKind:
        check_BoolExpr(out, rightVal->u.boolExpr);
        tc_type int4bool = new tc_type_;
        int4bool->type = new aA_type_;
        int4bool->type->type = A_dataType::A_nativeTypeKind;
        int4bool->type->u.nativeType = A_nativeType::A_intTypeKind;
        int4bool->isVarArrFunc = 0;
        return int4bool;
        break;
    }
    return nullptr;
}

tc_type tc_Type(aA_type t, uint isVarArrFunc)
{
    tc_type ret = new tc_type_;
    ret->type = t;
    ret->isVarArrFunc = isVarArrFunc;
    ret->array_length = 0;
    return ret;
}

tc_type tc_Type(aA_varDecl vd)
{
    if (vd->kind == A_varDeclType::A_varDeclScalarKind)
        return tc_Type(vd->u.declScalar->type, 0);
    else if (vd->kind == A_varDeclType::A_varDeclArrayKind)
        return tc_Type(vd->u.declArray->type, 1);
    return nullptr;
}

// public functions
void check_Prog(std::ostream &out, aA_program p)
{
    for (auto ele : p->programElements)
    {
        if (ele->kind == A_programVarDeclStmtKind)
        {
            check_VarDecl(out, ele->u.varDeclStmt);
        }
        else if (ele->kind == A_programStructDefKind)
        {
            check_StructDef(out, ele->u.structDef);
        }
    }

    for (auto ele : p->programElements)
    {
        if (ele->kind == A_programFnDeclStmtKind)
        {
            check_FnDeclStmt(out, ele->u.fnDeclStmt);
        }
        else if (ele->kind == A_programFnDefKind)
        {
            check_FnDecl(out, ele->u.fnDef->fnDecl);
        }
    }

    for (auto ele : p->programElements)
    {
        if (ele->kind == A_programFnDefKind)
        {
            check_FnDef(out, ele->u.fnDef);
        }
        else if (ele->kind == A_programNullStmtKind)
        {
            // do nothing
        }
    }

    out << "Typecheck passed!" << std::endl;

    return;
}

bool findToken(string name)
{
    string native_types[] = {"int"};
    for (string native_type : native_types)
    {
        if (name == native_type)
            return true;
    }
    bool global, local, funcParam, structId;

    if (!g_token2Type.empty()) // 检查 map 是否为空
    {
        global = g_token2Type.find(name) != g_token2Type.end();
    }
    if (!local_token2Type.empty())
    {
        local = false;
        for (auto it = local_token2Type.begin(); it != local_token2Type.end(); ++it)
        {
            typeMap &currentScope = **it;
            if (currentScope.find(name) != currentScope.end())
            {
                local = true;
                break;
            }
        }
    }
    if (!func2Param.empty())
    {
        funcParam = func2Param.find(name) != func2Param.end();
    }

    if (!struct2Members.empty())
    {
        structId = struct2Members.find(name) != struct2Members.end();
    }

    return global || local || funcParam || structId;
}

tc_type findTokenType(string name)
{
    if (!g_token2Type.empty()) // 检查 map 是否为空
    {
        if (g_token2Type.find(name) != g_token2Type.end())
        {
            return g_token2Type[name];
        }
    }
    if (!local_token2Type.empty())
    {
        for (auto it = local_token2Type.rbegin(); it != local_token2Type.rend(); ++it)
        {
            typeMap &currentScope = **it;
            if (currentScope.find(name) != currentScope.end())
            {
                return currentScope[name];
            }
        }
    }
    return nullptr;
}

void deducted_token_type(string name, tc_type type)
{
    tc_type temp = new tc_type_;
    temp->isVarArrFunc = type->isVarArrFunc;
    temp->type = type->type;
    if (!g_token2Type.empty()) // 检查 map 是否为空
    {
        if (g_token2Type.find(name) != g_token2Type.end())
        {
            g_token2Type[name] = temp;
        }
    }
    if (!local_token2Type.empty())
    {
        for (auto it = local_token2Type.rbegin(); it != local_token2Type.rend(); ++it)
        {
            typeMap &currentScope = **it;
            if (currentScope.find(name) != currentScope.end())
            {
                currentScope[name] = temp;
            }
        }
    }
}

void check_VarDecl(std::ostream &out, aA_varDeclStmt vd)
{

    if (!vd)
    {
        return;
    }
    std::string name;
    tc_type variable = new tc_type_;
    if (vd->kind == A_varDeclStmtType::A_varDeclKind)
    {
        // decl only
        aA_varDecl vdecl = vd->u.varDecl;
        if (vdecl->kind == A_varDeclType::A_varDeclScalarKind)
        {
            name = *vdecl->u.declScalar->id;
            // global scalar variable
            variable->type = vdecl->u.declScalar->type;
            variable->isVarArrFunc = 0;
        }
        else if (vdecl->kind == A_varDeclType::A_varDeclArrayKind)
        {
            name = *vdecl->u.declArray->id;
            // // global array variable
            variable->type = vdecl->u.declArray->type;
            variable->isVarArrFunc = 1;
            variable->array_length = vdecl->u.declArray->len;
        }
    }
    else if (vd->kind == A_varDeclStmtType::A_varDefKind)
    {
        // decl and def
        aA_varDef vdef = vd->u.varDef;
        if (vdef->kind == A_varDefType::A_varDefScalarKind)
        {
            name = *vdef->u.defArray->id;
            variable->type = vdef->u.defScalar->type;
            if (!vdef->u.defScalar->type)
            {
                aA_type a = new aA_type_();
                a->type = A_dataType::A_nativeTypeKind;
                variable->type = a;
            }
            variable->isVarArrFunc = 0;
            aA_rightVal val = vdef->u.defScalar->val;
            if (!comp_tc_type(variable, check_RightVal_Type(out, val)))
                error_print(out, val->pos, "type and value conflict!");
        }
        else if (vdef->kind == A_varDefType::A_varDefArrayKind)
        {
            name = *vdef->u.defArray->id;
            variable->type = vdef->u.defArray->type;
            if (!vdef->u.defArray->type)
            {
                aA_type a = new aA_type_();
                a->type = A_dataType::A_nativeTypeKind;
                variable->type = a;
            }
            variable->isVarArrFunc = 0;
            variable->array_length = vdef->u.defArray->len;
            vector<aA_rightVal> vals = vdef->u.defArray->vals;
            for (auto val : vals)
            {
                if (!comp_tc_type(variable, check_RightVal_Type(out, val)))
                    error_print(out, val->pos, "Type and value conflict!");
            }
            if (vdef->u.defArray->len != vals.size())
            {
                error_print(out, vdef->pos, "Array length mismatched!");
            }
            variable->isVarArrFunc = 1;
        }
    }

    if (findToken(name))
    {
        error_print(out, vd->pos, "the varible '" + name + "' has been declared!");
    }
    if (local_token2Type.empty())
    {
        g_token2Type[name] = variable;
    }
    else
    {
        typeMap *currentScope = local_token2Type.back();
        (*currentScope)[name] = variable;
    }

    return;
}

void check_StructDef(std::ostream &out, aA_structDef sd)
{
    if (!sd)
        return;
    string name = *sd->id;
    if (struct2Members.find(name) != struct2Members.end())
        error_print(out, sd->pos, "This id is already defined!");
    struct2Members[name] = &(sd->varDecls);
    return;
}

void check_FnDecl(std::ostream &out, aA_fnDecl fd)
{
    if (!fd)
        return;
    string name = *fd->id;

    // if already declared, should match
    if (func2Param.find(name) != func2Param.end())
    {
        // is function ret val matches
        tc_type function = g_token2Type[name];
        if (!comp_aA_type(function->type, fd->type))
        {
            error_print(out, fd->pos, "Function '" + name + "' has a mismatched return type.");
        }
        // is function params matches decl
        paramMemberMap::iterator it = func2Param.find(name);
        if (it != func2Param.end())
        {
            const vector<aA_varDecl> &declaredParams = *(it->second);          // 获取之前声明的参数
            const vector<aA_varDecl> &currentParams = fd->paramDecl->varDecls; // 当前的参数列表

            if (declaredParams.size() != currentParams.size())
            {
                error_print(out, fd->pos, "Function '" + name + "' has a mismatched number of parameters.");
            }

            for (size_t i = 0; i < declaredParams.size(); ++i)
            {
                if (!comp_tc_type(tc_Type(declaredParams[i]), tc_Type(currentParams[i])))
                {
                    error_print(out, fd->pos, "Function '" + name + "' has a mismatched parameter type at position " + std::to_string(i + 1));
                }
            }
        }
    }
    else
    {
        for (auto &paramDecl : fd->paramDecl->varDecls)
        {
            string paramName = paramDecl->kind == A_varDeclType::A_varDeclScalarKind ? *paramDecl->u.declScalar->id : *paramDecl->u.declArray->id;
            if (g_token2Type.find(paramName) != g_token2Type.end())
            {
                error_print(out, fd->pos, "Parameter '" + paramName + "' in function '" + name + "' conflicts with a global variable.");
            }
        }
        func2Param[name] = &(fd->paramDecl->varDecls); // 存入 func2Param
        g_token2Type[name] = tc_Type(fd->type, 2);
    }
    return;
}

void check_FnDeclStmt(std::ostream &out, aA_fnDeclStmt fd)
{
    if (!fd)
        return;
    check_FnDecl(out, fd->fnDecl);
    return;
}

void create_Scope()
{
    typeMap *newScope = new typeMap(); // 创建新的 typeMap 对象
    local_token2Type.push_back(newScope);
}

void delete_Scope()
{
    if (!local_token2Type.empty())
    {
        typeMap *lastScope = local_token2Type.back();
        local_token2Type.pop_back(); // 从 vector 中移除最后一个元素
        delete lastScope;            // 释放
    }
}

void check_FnDef(std::ostream &out, aA_fnDef fd)
{
    create_Scope();
    string name = *fd->fnDecl->id;
    if (!fd)
        return;
    // should match if declared
    check_FnDecl(out, fd->fnDecl);
    // add params to local tokenmap, func params cannot override global ones
    for (aA_varDecl vd : fd->fnDecl->paramDecl->varDecls)
    {
        tc_type variable = new tc_type_;
        if (vd->kind == A_varDeclType::A_varDeclScalarKind)
        {
            name = *vd->u.declScalar->id;
            // global scalar variable
            variable->type = vd->u.declScalar->type;
            variable->isVarArrFunc = 0;
        }
        else if (vd->kind == A_varDeclType::A_varDeclArrayKind)
        {
            name = *vd->u.declArray->id;
            // // global array variable
            variable->type = vd->u.declArray->type;
            variable->isVarArrFunc = 1;
            variable->array_length = vd->u.declArray->len;
        }
        typeMap *currentScope = local_token2Type.back();
        (*currentScope)[name] = variable;
    }

    for (aA_codeBlockStmt stmt : fd->stmts)
    {
        check_CodeblockStmt(out, stmt);
        // return value type should match
        // if (stmt->kind == A_codeBlockStmtType::A_returnStmtKind)
        // {
        //     if (!comp_aA_type(fd->fnDecl->type, g_token2Type[*fd->fnDecl->id]->type))
        //     {
        //         error_print(out, stmt->pos, "Return type does not match function declaration.");
        //     }
        // }
    }
    delete_Scope();
    return;
}

void check_CodeblockStmt(std::ostream &out, aA_codeBlockStmt cs)
{
    if (!cs)
        return;
    // variables declared in a code block should not duplicate with outer ones.
    switch (cs->kind)
    {
    case A_codeBlockStmtType::A_varDeclStmtKind:
    {
        check_VarDecl(out, cs->u.varDeclStmt);
        break;
    }
    case A_codeBlockStmtType::A_assignStmtKind:
    {
        check_AssignStmt(out, cs->u.assignStmt);
        break;
    }
    case A_codeBlockStmtType::A_ifStmtKind:
    {
        create_Scope();
        check_IfStmt(out, cs->u.ifStmt);
        delete_Scope();
        break;
    }
    case A_codeBlockStmtType::A_whileStmtKind:
    {
        create_Scope();
        check_WhileStmt(out, cs->u.whileStmt);
        delete_Scope();
        break;
    }
    case A_codeBlockStmtType::A_callStmtKind:
    {
        check_CallStmt(out, cs->u.callStmt);
        break;
    }
    case A_codeBlockStmtType::A_returnStmtKind:
    {
        check_ReturnStmt(out, cs->u.returnStmt);
        break;
    }
    default:
        break;
    }
    return;
}

void check_AssignStmt(std::ostream &out, aA_assignStmt as)
{
    if (!as)
        return;
    string name;
    tc_type deduced_type; // deduced type if type is omitted at decl
    tc_type leftVal_type;
    switch (as->leftVal->kind)
    {
    case A_leftValType::A_varValKind:
    {
        name = *as->leftVal->u.id;
        leftVal_type = findTokenType(name);
        if (!findToken(name))
        {
            error_print(out, as->pos, "Variable scalar '" + name + "' is not declared.");
        }
        if (leftVal_type->isVarArrFunc == 1)
            error_print(out, as->leftVal->pos, "the leftVal cannot be an array.");
    }
    break;

    case A_leftValType::A_arrValKind:
    {
        aA_arrayExpr left_arr = as->leftVal->u.arrExpr;
        check_ArrayExpr(out, left_arr);
        name = *left_arr->arr->u.id;
        leftVal_type = findTokenType(name);
        leftVal_type->type = new aA_type_;
        leftVal_type->type->type = A_dataType::A_nativeTypeKind;

        if (!findToken(name))
        {
            error_print(out, as->pos, "Variable '" + name + "' is not declared.");
        }
    }
    break;
    case A_leftValType::A_memberValKind:
    {
        aA_memberExpr left_mem = as->leftVal->u.memberExpr;
        leftVal_type = check_MemberExpr(out, left_mem);
        // if (leftVal_type->isVarArrFunc == 1)
        //     error_print(out, as->pos, "the leftVal cannot be an array.");
    }
    break;
    }
    if (!comp_tc_type(leftVal_type, check_RightVal_Type(out, as->rightVal)))
    {
        if (leftVal_type->type != nullptr)
        {
            error_print(out, as->pos, "the types of leftVal and rightVal don't match.");
        }
        else
        {
            deducted_token_type(name, check_RightVal_Type(out, as->rightVal));
        }
    }
    return;
}

void check_ArrayExpr(std::ostream &out, aA_arrayExpr ae)
{
    if (!ae)
        return;
    string name = *ae->arr->u.id;
    // check array name
    /* fill code here */
    tc_type arrayType = findTokenType(name);
    if (!arrayType)
        return;
    if(arrayType->isVarArrFunc!=1){
        error_print(out,ae->pos,"'"+name+"' is not an array!");
    }
    if(ae->idx->kind==A_indexExprKind::A_numIndexKind & arrayType->array_length<=ae->idx->u.num){
        error_print(out, ae->pos, "'" + name + "' index out of bound!");
    }
    // check index
    /* fill code here */
    return;
}

tc_type check_MemberExpr(std::ostream &out, aA_memberExpr me)
{
    // check if the member exists and return the tyep of the member
    if (!me)
        return nullptr;
    string name = *me->structId->u.id;
    // check struct name
    tc_type type = findTokenType(name);
    if (!type)
    {
        error_print(out, me->pos, "Variable struct '" + name + "' is not declared.");
    }
    if (!type->type->u.structType)
    {
        error_print(out, me->pos, "Variable '" + name + "' is not a struct.");
    }
    // check member name
    vector<aA_varDecl> members = *struct2Members[*type->type->u.structType];
    for (auto mem : members)
    {
        string memId;
        tc_type memType = new tc_type_();
        if (mem->kind == A_varDeclType::A_varDeclScalarKind)
        {
            memId = *mem->u.declScalar->id;
            memType->isVarArrFunc = 0;
            memType->type = mem->u.declScalar->type;
        }
        if (mem->kind == A_varDeclType::A_varDeclArrayKind)
        {
            memId = *mem->u.declArray->id;
            memType->isVarArrFunc = 1;
            memType->array_length = mem->u.declArray->len;
            memType->type = mem->u.declArray->type;
        }
        if (*me->memberId == memId)
            return memType;
        delete memType;
    }
    error_print(out, me->pos, "struct '" + *me->structId->u.id + "' doesn't have a member named '" + *me->memberId + "'.");
    return nullptr;
}

void check_IfStmt(std::ostream &out, aA_ifStmt is)
{
    if (!is)
        return;
    check_BoolUnit(out, is->boolUnit);
    /* fill code here, take care of variable scope */

    for (aA_codeBlockStmt s : is->ifStmts)
    {
        check_CodeblockStmt(out, s);
    }

    /* fill code here */
    for (aA_codeBlockStmt s : is->elseStmts)
    {
        check_CodeblockStmt(out, s);
    }
    /* fill code here */
    if (!local_token2Type.empty())
    {
        typeMap *lastScope = local_token2Type.back();
        local_token2Type.pop_back(); // 从 vector 中移除最后一个元素
        delete lastScope;            // 释放
    }
    return;
}

void check_BoolExpr(std::ostream &out, aA_boolExpr be)
{
    if (!be)
        return;
    switch (be->kind)
    {
    case A_boolExprType::A_boolBiOpExprKind:
        check_BoolExpr(out, be->u.boolBiOpExpr->left);
        check_BoolExpr(out, be->u.boolBiOpExpr->right);
        break;
    case A_boolExprType::A_boolUnitKind:
        check_BoolUnit(out, be->u.boolUnit);
        break;
    default:
        break;
    }
    return;
}

tc_type check_ExprUnit(std::ostream &out, aA_exprUnit eu);

void check_CompOperand(std::ostream &out, aA_exprUnit eu)
{
    if (!eu)
        return;
    tc_type temp = check_ExprUnit(out, eu);
    if (temp->isVarArrFunc != 0)
        error_print(out, eu->pos, "The operand is not a scalar.");
    if (temp->type == nullptr)
        return;
    if (temp->type->type != A_dataType::A_nativeTypeKind)
        error_print(out, eu->pos, "the operand is not a native type.");
}

void check_BoolUnit(std::ostream &out, aA_boolUnit bu)
{
    if (!bu)
        return;
    switch (bu->kind)
    {
    case A_boolUnitType::A_comOpExprKind:
    {
        check_CompOperand(out, bu->u.comExpr->left);
        check_CompOperand(out, bu->u.comExpr->right);
    }
    break;
    case A_boolUnitType::A_boolExprKind:
        check_BoolExpr(out, bu->u.boolExpr);
        break;
    case A_boolUnitType::A_boolUOpExprKind:
        check_BoolUnit(out, bu->u.boolUOpExpr->cond);
        break;
    default:
        break;
    }
    return;
}

tc_type check_ExprUnit(std::ostream &out, aA_exprUnit eu)
{
    // return the aA_type of expr eu
    if (!eu)
        return nullptr;
    tc_type ret;
    switch (eu->kind)
    {
    case A_exprUnitType::A_idExprKind:
    {
        ret = findTokenType(*eu->u.id);
        if (ret == nullptr)
            error_print(out, eu->pos, "the varible '" + *eu->u.id + "' is not declared.");
    }
    break;
    case A_exprUnitType::A_numExprKind:
    {
        aA_type numt = new aA_type_;
        numt->pos = eu->pos;
        numt->type = A_dataType::A_nativeTypeKind;
        numt->u.nativeType = A_nativeType::A_intTypeKind;
        ret = tc_Type(numt, 0);
    }
    break;
    case A_exprUnitType::A_fnCallKind:
    {
        check_FuncCall(out, eu->u.callExpr);
        // check_FuncCall will check if the function is defined
        ret = g_token2Type[*eu->u.callExpr->fn];
        if (ret->type == nullptr)
            error_print(out, eu->pos, "Function '" + *eu->u.callExpr->fn + "' does not return a value.");
    }
    break;
    case A_exprUnitType::A_arrayExprKind:
    {
        check_ArrayExpr(out, eu->u.arrayExpr);
        /* fill code here */
        ret = new tc_type_;
        ret = findTokenType(*eu->u.arrayExpr->arr->u.id);
    }
    break;
    case A_exprUnitType::A_memberExprKind:
    {
        ret = check_MemberExpr(out, eu->u.memberExpr);
    }
    break;
    case A_exprUnitType::A_arithExprKind:
    {
        ret = check_ArithExpr(out, eu->u.arithExpr);
    }
    break;
    case A_exprUnitType::A_arithUExprKind:
    {
        ret = check_ExprUnit(out, eu->u.arithUExpr->expr);
    }
    break;
    }
    return ret;
}

tc_type check_ArithExpr(std::ostream &out, aA_arithExpr ae)
{
    if (!ae)
        return nullptr;
    tc_type ret;
    switch (ae->kind)
    {
    case A_arithExprType::A_arithBiOpExprKind:
    {
        ret = check_ArithExpr(out, ae->u.arithBiOpExpr->left);
        tc_type rightType = check_ArithExpr(out, ae->u.arithBiOpExpr->right);
        if (ret->type->type > 0 || ret->type->type != A_dataType::A_nativeTypeKind || ret->type->u.nativeType != A_nativeType::A_intTypeKind ||
            rightType->type->type > 0 || rightType->type->type != A_dataType::A_nativeTypeKind || rightType->type->u.nativeType != A_nativeType::A_intTypeKind)
            error_print(out, ae->pos, "Only int can be arithmetic expression operation values!");
    }
    break;
    case A_arithExprType::A_exprUnitKind:
        ret = check_ExprUnit(out, ae->u.exprUnit);
        break;
    }
    return ret;
}

void check_FuncCall(std::ostream &out, aA_fnCall fc)
{
    if (!fc)
        return;
    // check if function defined
    string func_name = *fc->fn;
    /* fill code here */
    vector<aA_varDecl> varDeclList;
    if (func2Param.find(func_name) != func2Param.end())
    {
        varDeclList = *func2Param[*fc->fn];
    }
    else
    {
        error_print(out, fc->pos, "the function '" + *fc->fn + "' is not declared");
    }
    // check if parameter list matches
    if (varDeclList.size() != fc->vals.size())
    {
        error_print(out, fc->pos, "Param number is not equal!");
    }
    tc_type temp = new tc_type_;
    for (int i = 0; i < fc->vals.size(); i++)
    {
        if (varDeclList[i]->kind == A_varDeclType::A_varDeclScalarKind)
        {
            temp->isVarArrFunc = 0;
            temp->type = varDeclList[i]->u.declScalar->type;
            if (!comp_tc_type(check_RightVal_Type(out, fc->vals[i]), temp))
                error_print(out, fc->vals[i]->pos, "Scalar type not equal!");
        }
        else if (varDeclList[i]->kind == A_varDeclType::A_varDeclArrayKind)
        {
            temp->isVarArrFunc = 1;
            temp->type = varDeclList[i]->u.declArray->type;
            temp->array_length = varDeclList[i]->u.declArray->len;
            if (!comp_tc_type(check_RightVal_Type(out, fc->vals[i]), temp))
                error_print(out, fc->vals[i]->pos, "Array type not equal!");
        }
    }
    return;
}

void check_WhileStmt(std::ostream &out, aA_whileStmt ws)
{
    if (!ws)
        return;
    check_BoolUnit(out, ws->boolUnit);
    /* fill code here, take care of variable scope */

    for (aA_codeBlockStmt s : ws->whileStmts)
    {
        check_CodeblockStmt(out, s);
    }

    return;
}

void check_CallStmt(std::ostream &out, aA_callStmt cs)
{
    if (!cs)
        return;
    check_FuncCall(out, cs->fnCall);
    return;
}

void check_ReturnStmt(std::ostream &out, aA_returnStmt rs)
{
    if (!rs)
        return;

    if (rs->retVal->kind == A_rightValType::A_arithExprValKind)
    {
        check_ArithExpr(out, rs->retVal->u.arithExpr);
    }
    if (rs->retVal->kind == A_rightValType::A_boolExprValKind)
    {
        check_BoolExpr(out, rs->retVal->u.boolExpr);
    }
    return;
}
