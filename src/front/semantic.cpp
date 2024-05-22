#include "front/semantic.h"

#include <cassert>
#include <numeric>
#include <iostream>
#include <cmath>

using ir::Function;
using ir::Instruction;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

// 获取一个树节点的指定类型子节点的指针，若获取失败，使用 assert 断言来停止程序的执行
#define GET_CHILD_PTR(node, node_type, index)                     \
    auto node = dynamic_cast<node_type *>(root->children[index]); \
    assert(node);

// 获取一个树节点的指定类型子节点的指针，并调用一个名为 analysis<type> 的函数来对这个子节点进行分析
#define ANALYSIS(node, type, index)                          \
    auto node = dynamic_cast<type *>(root->children[index]); \
    assert(node);                                            \
    analysis##type(node, buffer);

// 将一个表达式节点的信息复制到另一个表达式节点中
#define COPY_EXP_NODE(from, to)              \
    to->is_computable = from->is_computable; \
    to->v = from->v;                         \
    to->t = from->t;

// 判断节点是否为指定类型
#define NODE_IS(node_type, index) root->children[index]->type == NodeType::node_type

// 获取库函数
map<std::string, Function *> *frontend::get_lib_funcs()
{
    static map<std::string, Function *> lib_funcs = {
        {"getint", new Function("getint", Type::Int)},
        {"getch", new Function("getch", Type::Int)},
        {"getfloat", new Function("getfloat", Type::Float)},
        {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
        {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
        {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
        {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
        {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
        {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
        {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
    };
    return &lib_funcs;
}

void frontend::SymbolTable::add_scope()
{
    scope_stack.push_back(ScopeInfo());
}

void frontend::SymbolTable::exit_scope()
{
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const
{
    return id + "_" + std::to_string(scope_stack.size());
}

Operand frontend::SymbolTable::get_operand(string id) const
{
    return get_ste(id).operand;
}

void frontend::SymbolTable::add_operand(std::string name, STE ste)
{
    scope_stack.back().table[name] = ste;
}

std::string frontend::Analyzer::get_temp_name()
{
    return "temp_" + std::to_string(tmp_cnt++);
}

void frontend::Analyzer::delete_temp_name()
{
    tmp_cnt--;
    return;
}

frontend::STE frontend::SymbolTable::get_ste(string id) const
{
    for (auto i = scope_stack.rbegin(); i != scope_stack.rend(); i++)
    {
        if (i->table.find(id) != i->table.end())
        {
            return i->table.at(id);
        }
    }
}

frontend::Analyzer::Analyzer() : tmp_cnt(0), symbol_table()
{
    symbol_table.add_scope();
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit *root)
{

    ir::Program program;

    analysisCompUnit(root);

    // 添加全局变量
    for (auto &p : symbol_table.scope_stack.back().table) // 遍历最外层作用域的符号表
    {
        if (p.second.dimension.size()) // 如果是数组
        {
            // 计算数组大小
            int size = std::accumulate(p.second.dimension.begin(), p.second.dimension.end(), 1, std::multiplies<int>());
            // 添加数组
            program.globalVal.push_back({p.second.operand, size});
            continue;
        }
        if (p.second.operand.type == ir::Type::FloatLiteral || p.second.operand.type == ir::Type::IntLiteral)
            p.second.operand.name = p.first;
        // 添加变量
        program.globalVal.push_back({p.second.operand});
    }

    // 把全局变量的初始化指令添加到 _global 函数中
    Function g("_global", ir::Type::null);
    for (auto &i : g_init_inst) // 遍历全局变量的初始化指令
    {
        g.addInst(i);
    }
    g.addInst(new Instruction({}, {}, {}, Operator::_return));
    program.addFunction(g);

    auto call_global = new ir::CallInst(Operand("_global", ir::Type::null), Operand());

    // 把函数添加到 ir::Program 中
    for (auto &f : symbol_table.functions)
    {
        if (f.first == "main") // 如果是main函数
        {
            // 在main函数前面添加一个调用 _global 函数的指令
            f.second->InstVec.insert(f.second->InstVec.begin(), call_global);
        }
        program.functions.push_back(*f.second);//将所有函数添加到 IR 程序中
    }

    std::cout <<"---------输出IR中间代码"<<std::endl ;
       for (auto &i : g_init_inst) // 输出IR中间代码
    {
        std::cout <<i->draw()<<std::endl ;
    }
    return program;
}


/*
CompUnit -> (Decl | FuncDef) [CompUnit]
Decl -> ConstDecl | VarDecl
ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
BType -> 'int' | 'float'
ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
VarDecl -> BType VarDef { ',' VarDef } ';'
VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
FuncType -> 'void' | 'int' | 'float'
FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
FuncFParams -> FuncFParam { ',' FuncFParam }
Block -> '{' { BlockItem } '}'
BlockItem -> Decl | Stmt
Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
Exp -> AddExp
Cond -> LOrExp
LVal -> Ident {'[' Exp ']'}
Number -> IntConst | floatConst
PrimaryExp -> '(' Exp ')' | LVal | Number
UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
UnaryOp -> '+' | '-' | '!'
FuncRParams -> Exp { ',' Exp }
MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
AddExp -> MulExp { ('+' | '-') MulExp }
RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
EqExp -> RelExp { ('==' | '!=') RelExp }
LAndExp -> EqExp [ '&&' LAndExp ]
LOrExp -> LAndExp [ '||' LOrExp ]
ConstExp -> AddExp
*/


// CompUnit -> (Decl | FuncDef) [CompUnit]
void frontend::Analyzer::analysisCompUnit(CompUnit *root)
{
    std::cout<<"analysisCompUnit "<<std::endl;
    if (NODE_IS(DECL, 0)) // 如果是声明
    {
        GET_CHILD_PTR(decl, Decl, 0);
        // 生成全局变量的初始化指令
        analysisDecl(decl, g_init_inst);
    }
    else // 如果是函数定义
    {
        GET_CHILD_PTR(funcdef, FuncDef, 0);
        symbol_table.add_scope();//添加一个块
        // 生成一个新的函数
        analysisFuncDef(funcdef);
        symbol_table.exit_scope();
    }

    if (root->children.size() == 2)
    {
        GET_CHILD_PTR(compunit, CompUnit, 1);
        analysisCompUnit(compunit);
    }
}

// Decl -> ConstDecl | VarDecl
void frontend::Analyzer::analysisDecl(Decl *root, vector<Instruction *> &instructions)
{
    std::cout<<"analysisDecl "<<std::endl;
    if (NODE_IS(VARDECL, 0)) // 如果是变量声明
    {
        GET_CHILD_PTR(vardecl, VarDecl, 0);
        // 向instructions中添加变量声明的指令
        analysisVarDecl(vardecl, instructions);
    }
    else // 如果是常量声明
    {
        GET_CHILD_PTR(constdecl, ConstDecl, 0);
        // 向instructions中添加常量声明的指令
        analysisConstDecl(constdecl, instructions);
    }
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
// ConstDecl.t
void frontend::Analyzer::analysisConstDecl(ConstDecl *root, vector<Instruction *> &instructions)
{ std::cout<<"analysisConstDecl "<<std::endl;
    
    GET_CHILD_PTR(btype, BType, 1);
    analysisBType(btype);
    root->t = btype->t;

    GET_CHILD_PTR(constdef, ConstDef, 2);
    analysisConstDef(constdef, instructions, root->t);

    for (size_t index = 4; index < root->children.size(); index += 2)
    {
        GET_CHILD_PTR(constdef, ConstDef, index);
        analysisConstDef(constdef, instructions, root->t);
    }
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
// ConstDef.arr_name
void frontend::Analyzer::analysisConstDef(ConstDef *root, vector<Instruction *> &instructions, ir::Type type)
{
    std::cout<<"analysisConstDef "<<std::endl;
    GET_CHILD_PTR(ident, Term, 0);
    string var_name=ident->token.value;
    root->arr_name = symbol_table.get_scoped_name(ident->token.value);

    std::vector<int> dimension;

    for (size_t index = 2; index < root->children.size(); index += 3)
    {
        if (!(NODE_IS(CONSTEXP, index)))
            break;
        GET_CHILD_PTR(constexp, ConstExp, index);
        constexp->v = root->arr_name;
        constexp->t = type;
        analysisConstExp(constexp, instructions);
        dimension.push_back(std::stoi(constexp->v));
    }

    int size = std::accumulate(dimension.begin(), dimension.end(), 1, std::multiplies<int>());
    ir::Type res_type = type;
            ir::Type curr_type = type;
        if (curr_type == ir::Type::Int){
            curr_type = ir::Type::IntPtr;
        }
        else if (curr_type == ir::Type::Float){
            curr_type = ir::Type::FloatPtr;
        }
        else{
            assert(0 && "Invalid Type");
        }

    if (dimension.empty())
    {
        switch (type)
        {
        case ir::Type::Int:
            // def:定义整形变量，op1:立即数或变量，op2:不使用，des:被赋值变量
            instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::Int},
                                                   Operator::def));std::cout<<"def-302 "<<std::endl;
            res_type = ir::Type::IntLiteral;
            break;
        case ir::Type::Float:
            // fdef:定义浮点数变量，op1:立即数或变量，op2:不使用，des:被赋值变量
            instructions.push_back(new Instruction({"0.0", ir::Type::FloatLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::Float},
                                                   Operator::fdef));
            res_type = ir::Type::FloatLiteral;
            break;
        default:
            break;
        }
    }
    else
    {

        switch (type)
        {
        case ir::Type::Int:
            // alloc:内存分配，op1:数组长度，op2:不使用，des:数组名，数组名被视为一个指针
            instructions.push_back(new Instruction({std::to_string(size), ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::IntPtr},
                                                   Operator::alloc));
            res_type = ir::Type::IntPtr;
            break;
        case ir::Type::Float:
            // alloc:内存分配，op1:数组长度，op2:不使用，des:数组名，数组名被视为一个指针
            instructions.push_back(new Instruction({std::to_string(size), ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::FloatPtr},
                                                   Operator::alloc));
            res_type = ir::Type::FloatPtr;
            break;
        default:
            break;
        }
    }

    GET_CHILD_PTR(constinitval, ConstInitVal, root->children.size() - 1);
    constinitval->v = root->arr_name;
    constinitval->t = type;
    analysisConstInitVal(constinitval, instructions,curr_type,var_name);

    // 如果是常量，constinitval->v是一个立即数，Type应该为Literal; 如果是数组，constinitval->v是数组名，Type应该为Ptr
    symbol_table.add_operand(ident->token.value,
                             {Operand(constinitval->v, res_type),
                              dimension});
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
// ConstInitVal.v
// ConstInitVal.t
void frontend::Analyzer::analysisConstInitVal(ConstInitVal *root, vector<Instruction *> &instructions,ir::Type curr_type,string var_name)
{
    std::cout<<"analysisConstInitVal "<<std::endl;
    if (NODE_IS(CONSTEXP, 0))
    {
       GET_CHILD_PTR(constexp, ConstExp, 0);
        constexp->v = get_temp_name();
        constexp->t = root->t; // Int or Float
        analysisConstExp(constexp, instructions);
        
        if (!(constexp->v == "0" || constexp->v == "0.0"))
        {
            
            switch (root->t)
            {
            case ir::Type::Int:
                if (constexp->t == ir::Type::FloatLiteral)
                {
                    std::cout<<"constexp->t == ir::Type::FloatLiteral"<<std::endl;
                    constexp->t = ir::Type::IntLiteral;
                    constexp->v = std::to_string((int)std::stof(constexp->v));
                }
                // mov:赋值，op1:立即数或变量，op2:不使用，des:被赋值变量
                instructions.push_back(new Instruction({constexp->v, ir::Type::IntLiteral},
                                                       {},
                                                       {root->v, ir::Type::Int},
                                                       Operator::mov));                std::cout<<"mov-383"<<std::endl;

                break;
            case ir::Type::Float:
                if (constexp->t == ir::Type::IntLiteral)
                {
                    std::cout<<"constexp->t == ir::Type::IntLiteral,ir::Type::Float"<<std::endl;
                    constexp->t = ir::Type::FloatLiteral;
                    constexp->v = std::to_string((float)std::stoi(constexp->v));
                }
                // fmov:浮点数赋值，op1:立即数或变量，op2:不使用，des:被赋值变量
                instructions.push_back(new Instruction({constexp->v, ir::Type::FloatLiteral},
                                                       {},
                                                       {root->v, ir::Type::Float},
                                                       Operator::fmov));
                            std::cout<<"fmov-395"<<std::endl;
                break;
            default:
                break;
            }
        }
        root->v = constexp->v;
        root->t = constexp->t;
        delete_temp_name();
        // // ConstInitVal
        // int cnt = 0;
        // for (int i=1; i<root->children.size()-1; i+=2, cnt+=1){
        //     // ConstInitVal
        //     ConstInitVal* child_constinit = dynamic_cast<ConstInitVal*>(root->children[i]);
        //     assert(child_constinit != nullptr);
        //     // ConstExp
        //     ConstExp* constexp = dynamic_cast<ConstExp*>(child_constinit->children[0]);
        //     assert(constexp != nullptr);
        //     // 计算ConstExp的值
        //     analysisConstExp(constexp, instructions);
        //     if (curr_type == ir::Type::IntPtr){
        //         // 允许隐式类型转化 Float -> Int
        //         assert(constexp->t == ir::Type::FloatLiteral || constexp->t == ir::Type::IntLiteral);
        //         int val;
        //         if (constexp->t == ir::Type::Float){
        //             val = std::stof(constexp->v);
        //         }
        //         else{
        //             val = std::stoi(constexp->v);
        //         }
        //         ir::Instruction* storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_name),ir::Type::IntPtr),
        //                                                         ir::Operand(std::to_string(cnt),ir::Type::IntLiteral),
        //                                                         ir::Operand(std::to_string(val),ir::Type::IntLiteral),ir::Operator::store);
        //         instructions.push_back(storeInst);
        //     }
        //     else if (curr_type == ir::Type::FloatPtr){
        //         // 允许类型提升 Int -> Float
        //         assert(constexp->t == ir::Type::FloatLiteral || constexp->t == ir::Type::IntLiteral);
        //         float val = std::stof(constexp->v);
        //         ir::Instruction* storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_name),ir::Type::FloatPtr),
        //                                                         ir::Operand(std::to_string(cnt),ir::Type::IntLiteral),
        //                                                         ir::Operand(std::to_string(val),ir::Type::FloatLiteral),ir::Operator::store);
        //         instructions.push_back(storeInst);
        //     }
        //     else{
        //         assert(0 && "Invalid analyze Type");
        //     }
        // }
    }
    else
    {
        int insert_index = 0;
        for (size_t index = 1; index < root->children.size() - 1; index += 2)
        {
            if (NODE_IS(CONSTINITVAL, index))
            {
                GET_CHILD_PTR(constinitval, ConstInitVal, index);
                constinitval->v = get_temp_name();
                constinitval->t = root->t; // Int or Float
                analysisConstInitVal(constinitval, instructions,curr_type,var_name );

                switch (root->t)
                {
                case ir::Type::Int:
                    // store:存储，op1:数组名，op2:下标，des:存入的数
                    instructions.push_back(new Instruction({root->v, ir::Type::IntPtr},
                                                           {std::to_string(insert_index), ir::Type::IntLiteral},
                                                           {constinitval->v, ir::Type::IntLiteral},
                                                           Operator::store));
                    break;
                case ir::Type::Float:
                    // store:存储，op1:数组名，op2:下标，des:存入的数
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatPtr},
                                                           {std::to_string(insert_index), ir::Type::IntLiteral},
                                                           {constinitval->v, ir::Type::FloatLiteral},
                                                           Operator::store));
                    break;
                default:
                    break;
                }
                insert_index++;
                delete_temp_name();
            }
            else
            {
                break;
            }
        }
    }
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
// VarDecl.t
void frontend::Analyzer::analysisVarDecl(VarDecl *root, vector<Instruction *> &instructions)
{
    std::cout<<"analysisVarDecl "<<std::endl;
    GET_CHILD_PTR(btype, BType, 0);
    analysisBType(btype);
    root->t = btype->t;

    GET_CHILD_PTR(vardef, VarDef, 1);
    analysisVarDef(vardef, instructions, btype->t);

    for (size_t index = 3; index < root->children.size(); index += 2)
    {
        GET_CHILD_PTR(vardef, VarDef, index);
        analysisVarDef(vardef, instructions, btype->t);
    }
}

// BType -> 'int' | 'float'
// BType.t
void frontend::Analyzer::analysisBType(BType *root)
{
     std::cout<<"analysisBType "<<std::endl;
    GET_CHILD_PTR(term, Term, 0);
    switch (term->token.type)
    {
    case TokenType::INTTK:
        root->t = ir::Type::Int;
        break;
    case TokenType::FLOATTK:
        root->t = ir::Type::Float;
        break;
    default:
        break;
    }
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
// VarDef.arr_name
void frontend::Analyzer::analysisVarDef(VarDef *root, vector<Instruction *> &instructions, ir::Type type)
{
    std::cout<<"analysisVarDef "<<std::endl;
    GET_CHILD_PTR(ident, Term, 0);
    root->arr_name = symbol_table.get_scoped_name(ident->token.value);

    std::vector<int> dimension;

    for (size_t index = 2; index < root->children.size(); index += 3)
    {
        if (!(NODE_IS(CONSTEXP, index)))
            break;
        GET_CHILD_PTR(constexp, ConstExp, index);
        constexp->v = root->arr_name;
        constexp->t = type;
        analysisConstExp(constexp, instructions);
        dimension.push_back(std::stoi(constexp->v));
    }

    int size = std::accumulate(dimension.begin(), dimension.end(), 1, std::multiplies<int>());

    if (dimension.empty())
    {//常量
    std::cout<<"analysisVarDef-常量 "<<std::endl;
        switch (type)
        {
        case ir::Type::Int:
            // def:定义整形变量，op1:立即数或变量，op2:不使用，des:被定义变量
            instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::Int},
                                                   Operator::def));std::cout<<"def-560 "<<std::endl;
            break;
        case ir::Type::Float:
            //  fdef:定义浮点数变量，op1:立即数或变量，op2:不使用，des:被定义变量
            instructions.push_back(new Instruction({"0.0", ir::Type::FloatLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::Float},
                                                   Operator::fdef));
            break;
        default:
            break;
        }
    }
    else
    {//数组
    std::cout<<"analysisVarDef-数组 "<<std::endl;
        switch (type)
        {
        case ir::Type::Int:
        std::cout <<"analysisVarDef-整数数组"<< std::endl;
            // alloc:内存分配，op1:数组长度，op2:不使用，des:数组名，数组名被视为一个指针。
            instructions.push_back(new Instruction({std::to_string(size), ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::IntPtr},
                                                   Operator::alloc));
            type = ir::Type::IntPtr;
            // 初始化数组
            for (int insert_index = 0; insert_index < size; insert_index++)
            {
                // store:存储，op1:数组名，op2:下标，des:存入的数
                instructions.push_back(new Instruction({root->arr_name, ir::Type::IntPtr},
                                                       {std::to_string(insert_index), ir::Type::IntLiteral},
                                                       {"0", ir::Type::IntLiteral},
                                                       Operator::store));
            }
            break;
        case ir::Type::Float:
        std::cout <<"analysisVarDef-浮点数数组"<< std::endl;
            // alloc:内存分配，op1:数组长度，op2:不使用，des:数组名，数组名被视为一个指针。
            instructions.push_back(new Instruction({std::to_string(size), ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::FloatPtr},
                                                   Operator::alloc));
            type = ir::Type::FloatPtr;
            // 初始化数组
            for (int insert_index = 0; insert_index < size; insert_index++)
            {
                // store:存储，op1:数组名，op2:下标，des:存入的数
                instructions.push_back(new Instruction({root->arr_name, ir::Type::FloatPtr},
                                                       {std::to_string(insert_index), ir::Type::IntLiteral},
                                                       {"0.0", ir::Type::FloatLiteral},
                                                       Operator::store));
            }
            break;
        default:
            break;
        }
    }
    //---------------------记录curr_type
        ir::Type curr_type = type;
        if (curr_type == ir::Type::IntLiteral)std::cout<<"analysisVarDef-curr_type:IntLiteral"<<std::endl;
        if (curr_type == ir::Type::FloatLiteral)std::cout<<"analysisVarDef-curr_type:FloatLiteral"<<std::endl;
        if (curr_type == ir::Type::Int)std::cout<<"analysisVarDef-curr_type:Int"<<std::endl;
        if (curr_type == ir::Type::Float)std::cout<<"analysisVarDef-curr_type:Float"<<std::endl;

        if (curr_type == ir::Type::Int && !dimension.empty()){
            curr_type = ir::Type::IntPtr;
            std::cout<<"analysisVarDef-change_curr_type:Int->IntPtr"<<std::endl;
        }
        else if (curr_type == ir::Type::Float && !dimension.empty()){
            curr_type = ir::Type::FloatPtr;
            std::cout<<"analysisVarDef-change_curr_type:Float->FloatPtr"<<std::endl;
        }
        
    if (NODE_IS(INITVAL, root->children.size() - 1)) // 如果有初始化值
    {
        GET_CHILD_PTR(initval, InitVal, root->children.size() - 1);
        initval->v = root->arr_name;
        initval->t = type;
        std::cout<<"analysisVarDef-analysisInitVal-有初始值"<<std::endl;
        analysisInitVal(initval, instructions,curr_type);        

    }

    symbol_table.add_operand(ident->token.value,
                             {Operand(root->arr_name, curr_type),
                              dimension});
}

// ConstExp -> AddExp
// ConstExp.is_computable: true
// ConstExp.v
// ConstExp.t
void frontend::Analyzer::analysisConstExp(ConstExp *root, vector<Instruction *> &instructions){
    std::cout<<"analysisConstExp "<<std::endl;
    GET_CHILD_PTR(addexp, AddExp, 0);
    COPY_EXP_NODE(root, addexp);
    analysisAddExp(addexp, instructions);
    COPY_EXP_NODE(addexp, root);
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
// InitVal.is_computable
// InitVal.v
// InitVal.t
void frontend::Analyzer::analysisInitVal(InitVal *root, vector<Instruction *> &instructions,ir::Type curr_type)
{
    std::cout<<"analysisInitVal "<<std::endl;
    //记录有curr_type
    if (NODE_IS(EXP, 0))
    {
        // std::cout<<"analysisInitVal NODE_IS(EXP, 0)"<<std::endl;
        GET_CHILD_PTR(exp, Exp, 0);
        
        string str=get_temp_name();
        exp->v = str;//临时变量
        //  std::cout<<"analysisInitVal-get_temp_name:"<<str<<std::endl;
        
        if(curr_type==ir::Type::IntPtr||curr_type==ir::Type::Int){
            root->t=ir::Type::Int;
        }if(curr_type==ir::Type::FloatPtr||curr_type==ir::Type::Float){
            root->t=ir::Type::Float;
        }
        exp->t = root->t; // Int or Float
        
        
        analysisExp(exp, instructions);//

         if( exp->t==ir::Type::IntPtr|| exp->t==ir::Type::Int||exp->t==ir::Type::IntLiteral){
            std::cout<<"analysisInitVal-exp->t:Int类"<<std::endl;
        }if( exp->t==ir::Type::FloatPtr|| exp->t==ir::Type::Float||exp->t==ir::Type::FloatLiteral){
            std::cout<<"analysisInitVal-exp->t:Float类"<<std::endl;
        }

        std::cout<<"analysisInitVal-exp->v:"<<exp->v<<std::endl;
        if (!(exp->v == "0" || exp->v == "0.0"))
        {
            switch (root->t)
            {
            case ir::Type::Int:
                if (exp->t == ir::Type::FloatLiteral)
                {
                    // std::cout<<"analysisInitVal-root->t:FloatLiteral700"<<std::endl;
                    exp->t = ir::Type::IntLiteral;
                    exp->v = std::to_string((int)std::stof(exp->v));
                }
                else if (exp->t == ir::Type::Float)
                { 

                    
                    // cvt_f2i:浮点数转整数，op1:浮点数，op2:不使用，des:整数
                    instructions.push_back(new Instruction({exp->v, ir::Type::Float},
                                                           {},
                                                           {exp->v, ir::Type::Int},
                                                           Operator::cvt_f2i));std::cout<<"cvt_f2i-695"<<std::endl;
                    exp->t = ir::Type::Int;
                }
                // mov:赋值，op1:立即数或变量，op2:不使用，des:被赋值变量
     
                instructions.push_back(new ir::Instruction({exp->v, exp->t},
                                                       {},
                                                       {root->v, ir::Type::Int},
                                                       Operator::mov));std::cout<<"mov-681"<<std::endl;
                break;
            case ir::Type::Float:
                if (exp->t == ir::Type::IntLiteral)
                {
                    std::cout<<"analysisInitVal-exp->t == ir::Type::IntLiteral-710 "<<str<<std::endl;
                    exp->t = ir::Type::FloatLiteral;
                    exp->v = std::to_string((float)std::stoi(exp->v));
                }
                else if (exp->t == ir::Type::Int)
                {std::cout<<"analysisInitVal-exp->t == ir::Type::Int-715 "<<str<<std::endl;
                    // cvt_i2f:整数转浮点数，op1:整数，op2:不使用，des:浮点数
                    instructions.push_back(new Instruction({exp->v, ir::Type::Int},
                                                           {},
                                                           {exp->v, ir::Type::Float},
                                                           Operator::cvt_i2f));std::cout<<"cvt_i2f-722"<<std::endl;
                    exp->t = ir::Type::Float;
                }else if (exp->t == ir::Type::FloatLiteral){
                    std::cout<<"analysisInitVal-exp->t == ir::Type::FloatLiteral"<<std::endl;
                    exp->t = ir::Type::Float;
                }else if (exp->t == ir::Type::Float){
                    std::cout<<"analysisInitVal-exp->t == ir::Type::Float"<<std::endl;
                }

                std::cout<<"analysisInitVal-exp->v:"<< exp->v<<std::endl;
                // fmov:浮点数赋值，op1:立即数或变量，op2:不使用，des:被赋值变量
                instructions.push_back(new Instruction({exp->v, exp->t},
                                                       {},
                                                       {root->v, ir::Type::Float},
                                                       Operator::fmov));
                    std::cout<<"fmov-710 "<<str<<std::endl;
                break;
            default:
                break;
            }
        }

        root->v = exp->v;
        root->t = exp->t;
        delete_temp_name();
    }
    else
    {
        std::cout<<"analysisInitVal 数组赋值"<<std::endl;
        //数组赋值
        int insert_index = 0;
        for (size_t index = 1; index < root->children.size(); index += 2)
        {
            
            if (NODE_IS(INITVAL, index))
            {
                GET_CHILD_PTR(initval, InitVal, index);
                        string str1=get_temp_name();
                        initval->v = str1;

                if (curr_type == ir::Type::IntPtr){
                // // 允许隐式类型转化 Float -> Int
                // //assert(initval->t == ir::Type::FloatLiteral || initval->t == ir::Type::IntLiteral);
                // int val;
                // if (initval->t == ir::Type::Float){
                //     val = std::stof(initval->v);
                // }
                // else{
                //     val = std::stoi(initval->v);
                // }
                // initval->t==ir::Type::IntLiteral;
                // initval->v=val;
                ;
            }
            else if (curr_type == ir::Type::FloatPtr){
                // 允许类型提升 Int -> Float
              if(initval->t == ir::Type::IntLiteral){
                string val0=initval->v;
                std::cout<<val0<<"!";
                int val1=stoi(val0);
                float val = val1;
                initval->t==ir::Type::FloatLiteral;
                initval->v=val;
                root->t==ir::Type::FloatLiteral;
                     }
            ;
            }
            else{
                assert(0 && "Invalid analyze Type");
            }
                //initval->t = root->t; // IntLiteral or FloatLiteral
                std::cout<<"analysisInitVal773:"<<std::endl;
                analysisInitVal(initval, instructions,curr_type);
                // store:存储，op1:数组名，op2:下标，des:存入的数
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {std::to_string(insert_index), ir::Type::IntLiteral},
                                                       {initval->v, initval->t},
                                                       Operator::store));
                insert_index += 1;
                delete_temp_name();
            }
            else
            {
                break;
            }
        }
    }
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
// FuncDef.n;
// FuncDef.t;
void frontend::Analyzer::analysisFuncDef(FuncDef *root)
{
    std::cout<<"analysisFuncDef "<<std::endl;
    // 生成函数返回值类型
    GET_CHILD_PTR(functype, FuncType, 0);
    analysisFuncType(functype, root->t);

    // 生成函数名
    GET_CHILD_PTR(ident, Term, 1);
    root->n = ident->token.value;

    auto params = new vector<Operand>();
    if (NODE_IS(FUNCFPARAMS, 3)) // 如果有参数
    {
        // 生成函数参数列表
        GET_CHILD_PTR(funcfparams, FuncFParams, 3);
        analysisFuncFParams(funcfparams, *params);
    }

    symbol_table.functions[root->n] = new Function(root->n, *params, root->t);

    // 生成函数体指令到InstVec
    GET_CHILD_PTR(block, Block, root->children.size() - 1);
    analysisBlock(block, symbol_table.functions[root->n]->InstVec);
    if (symbol_table.functions[root->n]->InstVec.back()->op != Operator::_return) // 如果函数没有return语句
        symbol_table.functions[root->n]->InstVec.push_back(new Instruction({}, {}, {}, Operator::_return));
}

// FuncType -> 'void' | 'int' | 'float'
void frontend::Analyzer::analysisFuncType(FuncType *root, ir::Type &returnType)
{

    std::cout<<"analysisFuncType "<<std::endl;
    GET_CHILD_PTR(term, Term, 0);

    switch (term->token.type)
    {
    case TokenType::VOIDTK:
        returnType = ir::Type::null;
        break;
    case TokenType::INTTK:
        returnType = ir::Type::Int;
        break;
    case TokenType::FLOATTK:
        returnType = ir::Type::Float;
        break;
    default:
        break;
    }
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
void frontend::Analyzer::analysisFuncFParams(FuncFParams *root, vector<Operand> &params)
{
    std::cout<<"analysisFuncFParams "<<std::endl;
    for (size_t i = 0; i < root->children.size(); i++)
    {
        if (NODE_IS(FUNCFPARAM, i))
        {
            GET_CHILD_PTR(funcfparam, FuncFParam, i);
            analysisFuncFParam(funcfparam, params);
        }
    }
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
void frontend::Analyzer::analysisFuncFParam(FuncFParam *root, vector<Operand> &params)
{
    std::cout<<"analysisFuncFParam "<<std::endl;
    GET_CHILD_PTR(btype, BType, 0);
    analysisBType(btype);
    auto type = btype->t;

    GET_CHILD_PTR(ident, Term, 1);

    vector<int> dimension;
    if (root->children.size() > 2)
    {
        if (root->children.size() == 4) // 维度为1
        {
            switch (type)
            {
            case ir::Type::Int:
            case ir::Type::IntLiteral:
                type = ir::Type::IntPtr;
                break;
            case ir::Type::Float:
            case ir::Type::FloatLiteral:
                type = ir::Type::FloatPtr;
                break;
            default:
                break;
            }
            dimension.push_back(-1);
        }
        else
        {
            TODO;
        }
    }

    params.push_back(Operand(symbol_table.get_scoped_name(ident->token.value), type));
    symbol_table.add_operand(ident->token.value,
                             {Operand(symbol_table.get_scoped_name(ident->token.value), type), dimension});
}

// Block -> '{' { BlockItem } '}'
void frontend::Analyzer::analysisBlock(Block *root, vector<Instruction *> &instructions)

{
    std::cout<<"analysisBlock "<<std::endl;
    for (size_t i = 1; i < root->children.size() - 1; i++)
    {
        // 生成指令到instructions中
        GET_CHILD_PTR(blockitem, BlockItem, i);
        analysisBlockItem(blockitem, instructions);

    }
      std::cout <<"---------输出函数中间代码"<<std::endl ;
       for (auto &i : instructions) // 输出IR中间代码
    {
        std::cout <<i->draw()<<std::endl ;
    }
}

// BlockItem -> Decl | Stmt
void frontend::Analyzer::analysisBlockItem(BlockItem *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisBlockItem "<<std::endl;
    if (NODE_IS(DECL, 0)) // 如果是声明
    {
        GET_CHILD_PTR(decl, Decl, 0);
        analysisDecl(decl, instructions);
    }
    else // 如果是语句
    {
        GET_CHILD_PTR(stmt, Stmt, 0);
        analysisStmt(stmt, instructions);
    }
}

// Stmt -> LVal '=' Exp ';'
//       | Block
//       | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
//       | 'while' '(' Cond ')' Stmt
//       | 'break' ';'
//       | 'continue' ';'
//       | 'return' [Exp] ';'
//       | [Exp] ';'
// Stmt.jump_eow;
// Stmt.jump_bow;
void frontend::Analyzer::analysisStmt(Stmt *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisStmt "<<std::endl;
    if (NODE_IS(LVAL, 0)) // 如果是赋值语句
    {
        GET_CHILD_PTR(exp, Exp, 2);
        exp->v = get_temp_name();
        exp->t = ir::Type::Int; // 初始化为int
        // 获取右值
        analysisExp(exp, instructions);

        GET_CHILD_PTR(lval, LVal, 0);
        // 获取左值
        COPY_EXP_NODE(exp, lval);
        std::cout<<"analysisStmt-analysisLVal-is_left_true,获取左值"<<std::endl;
        analysisLVal(lval, instructions, true);

        delete_temp_name();
    }
    else if (NODE_IS(BLOCK, 0)) // 如果是复合语句
    {
        symbol_table.add_scope();
        GET_CHILD_PTR(block, Block, 0);
        analysisBlock(block, instructions);
        symbol_table.exit_scope();
    }
    else if (NODE_IS(EXP, 0)) // 如果是表达式语句
    {
        GET_CHILD_PTR(exp, Exp, 0);
        exp->v = get_temp_name();
        exp->t = ir::Type::Int; // 初始化为int
        analysisExp(exp, instructions);
        delete_temp_name();
    }
    else if (NODE_IS(TERMINAL, 0)) // 如果是标识符
    {
        GET_CHILD_PTR(term, Term, 0);

        switch (term->token.type)
        {
        case TokenType::IFTK: // 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
        {
            GET_CHILD_PTR(cond, Cond, 2);
            cond->v = get_temp_name();
            cond->t = ir::Type::null; // 初始化为null
            analysisCond(cond, instructions);
            // op1:跳转条件，整形变量(条件跳转)或null(无条件跳转)
            // op2:不使用
            // des:常量，值为跳转相对目前pc的偏移量

            // 当条件为真时跳转到if_true_stmt（下下条指令）
            // 否则继续执行下一条指令，下一条指令也应为跳转指令，指向if_true_stmt结束之后的指令
            instructions.push_back(new Instruction({cond->v, cond->t},
                                                   {},
                                                   {"2", ir::Type::IntLiteral},
                                                   Operator::_goto));

            vector<Instruction *> if_true_instructions;
            GET_CHILD_PTR(if_true_stmt, Stmt, 4);
            analysisStmt(if_true_stmt, if_true_instructions);

            vector<Instruction *> if_false_instructions;
            if (root->children.size() == 7)
            {
                GET_CHILD_PTR(if_false_stmt, Stmt, 6);
                analysisStmt(if_false_stmt, if_false_instructions);
                // 执行完if_true_stmt后跳转到if_false_stmt后面
                if_true_instructions.push_back(new Instruction({},
                                                               {},
                                                               {std::to_string(if_false_instructions.size() + 1), ir::Type::IntLiteral},
                                                               Operator::_goto));
            }

            // 执行完if_true_stmt后跳转到if_false_stmt/if-else代码段（此时无if_false_stmt）后面
            instructions.push_back(new Instruction({},
                                                   {},
                                                   {std::to_string(if_true_instructions.size() + 1), ir::Type::IntLiteral},
                                                   Operator::_goto));
            instructions.insert(instructions.end(), if_true_instructions.begin(), if_true_instructions.end());
            instructions.insert(instructions.end(), if_false_instructions.begin(), if_false_instructions.end());

            delete_temp_name();
            break;
        }
        case TokenType::WHILETK: // 'while' '(' Cond ')' Stmt
        {
            GET_CHILD_PTR(cond, Cond, 2);
            cond->v = get_temp_name();
            cond->t = ir::Type::null; // 初始化为null
            vector<Instruction *> cond_instructions;
            analysisCond(cond, cond_instructions);
            // op1:跳转条件，整形变量(条件跳转)或null(无条件跳转)
            // op2:不使用
            // des:常量，值为跳转相对目前pc的偏移量

            // 先执行cond，如果为真则执行while_stmt，跳转到下下条（跳过下一条指令）
            // 否则执行下一条，跳转到while_stmt后面
            instructions.insert(instructions.end(), cond_instructions.begin(), cond_instructions.end());
            instructions.push_back(new Instruction({cond->v, cond->t},
                                                   {},
                                                   {"2", ir::Type::IntLiteral},
                                                   Operator::_goto));

            GET_CHILD_PTR(stmt, Stmt, 4);
            vector<Instruction *> while_instructions;
            analysisStmt(stmt, while_instructions);

            // 执行完while_stmt后跳回到cond的第一条
            while_instructions.push_back(new Instruction({},
                                                         {},
                                                         {std::to_string(-int(while_instructions.size() + 2 + cond_instructions.size())), ir::Type::IntLiteral},
                                                         // +2是因为while_stmt前面分别还有2条跳转指令
                                                         Operator::_goto));

            // 跳转到while_stmt后面
            instructions.push_back(new Instruction({},
                                                   {},
                                                   {std::to_string(while_instructions.size() + 1), ir::Type::IntLiteral},
                                                   Operator::_goto));

            for (size_t i = 0; i < while_instructions.size(); i++)
            {
                if (while_instructions[i]->op == Operator::__unuse__ && while_instructions[i]->op1.name == "1")
                {
                    while_instructions[i] = new Instruction({},
                                                            {},
                                                            {std::to_string(int(while_instructions.size()) - i), ir::Type::IntLiteral},
                                                            Operator::_goto);
                }
                else if (while_instructions[i]->op == Operator::__unuse__ && while_instructions[i]->op1.name == "2")
                {
                    while_instructions[i] = new Instruction({},
                                                            {},
                                                            {std::to_string(-int(i + 2 + cond_instructions.size())), ir::Type::IntLiteral},
                                                            Operator::_goto);
                }
            }

            instructions.insert(instructions.end(), while_instructions.begin(), while_instructions.end());
            delete_temp_name();
            break;
        }
        case TokenType::BREAKTK: // 'break' ';'
        {
            instructions.push_back(new Instruction({"1", ir::Type::IntLiteral},
                                                   {},
                                                   {},
                                                   Operator::__unuse__));
            break;
        }
        case TokenType::CONTINUETK: // 'continue' ';'
        {
            instructions.push_back(new Instruction({"2", ir::Type::IntLiteral},
                                                   {},
                                                   {},
                                                   Operator::__unuse__));
            break;
        }
        case TokenType::RETURNTK: // 'return' [Exp] ';'
        {
            if (NODE_IS(EXP, 1)) // 如果有返回值
            {
                GET_CHILD_PTR(exp, Exp, 1);
                exp->v = get_temp_name();
                exp->t = ir::Type::null; // 初始化为null
                analysisExp(exp, instructions);
                // return: op1:返回值，op2:不使用，des:不使用
                instructions.push_back(new Instruction({exp->v, exp->t},
                                                       {},
                                                       {},
                                                       Operator::_return));
                delete_temp_name();
            }
            else
            {
                instructions.push_back(new Instruction({"0", ir::Type::null},
                                                       {},
                                                       {},
                                                       Operator::_return));
            }
            break;
        }
        default:
            break;
        }
    }
}

// Cond -> LOrExp
// Cond.is_computable
// Cond.v
// Cond.t
void frontend::Analyzer::analysisCond(Cond *root, std::vector<Instruction *> &instructions)
{
     std::cout<<"analysisCond "<<std::endl;
    GET_CHILD_PTR(lorexp, LOrExp, 0);
    COPY_EXP_NODE(root, lorexp);
    analysisLOrExp(lorexp, instructions);
    COPY_EXP_NODE(lorexp, root);
}

// LOrExp -> LAndExp [ '||' LOrExp ]
// LOrExp.is_computable
// LOrExp.v
// LOrExp.t
void frontend::Analyzer::analysisLOrExp(LOrExp *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisLOrExp "<<std::endl;
    GET_CHILD_PTR(landexp, LAndExp, 0);
    COPY_EXP_NODE(root, landexp);
    analysisLAndExp(landexp, instructions);
    COPY_EXP_NODE(landexp, root);

    // [ '||' LOrExp ]
    if (root->children.size() == 1){
        return ;
    }
    else{
        // LOrExp
        LOrExp* lorexp = dynamic_cast<LOrExp*>(root->children[2]);
        assert(lorexp != nullptr);
        vector<Instruction *> second_and_instructions;
        analysisLOrExp(lorexp, second_and_instructions);
        // 处理操作数1 (计算其是否为真 or 假)
        if (root->t == Type::Float){
            string tmp_name =get_temp_name();;
            ir::Instruction* cvtInst = new ir::Instruction(ir::Operand(root->v,Type::Float),
                                                           ir::Operand("0.0",Type::FloatLiteral),
                                                           ir::Operand(tmp_name,Type::Int),ir::Operator::fneq);
            instructions.push_back(cvtInst);
            root->v = tmp_name;
            root->t = Type::Int;
        }
        else if (root->t == Type::FloatLiteral){
            float val = std::stof(root->v);
            root->t = Type::IntLiteral;
            root->v = std::to_string(val != 0);
        }

        // 处理操作数2 (计算其是否为真 or 假)
        if (lorexp->t == Type::Float){
            string tmp_name =get_temp_name();
            ir::Instruction* cvtInst = new ir::Instruction(ir::Operand(lorexp->v,Type::Float),
                                                           ir::Operand("0.0",Type::FloatLiteral),
                                                           ir::Operand(tmp_name,Type::Int),ir::Operator::fneq);
            second_and_instructions.push_back(cvtInst);
            lorexp->v = tmp_name;
            lorexp->t = Type::Int;
        }
        else if (lorexp->t == Type::FloatLiteral){
            float val = std::stof(lorexp->v);
            lorexp->t = Type::IntLiteral;
            lorexp->v = std::to_string(val != 0);
        }

        // 计算LAndExp || LOrExp
        if (root->t == Type::IntLiteral && lorexp->t == Type::IntLiteral){
            root->v = std::to_string(std::stoi(root->v) || std::stoi(lorexp->v));
        }
        else{
            // 注意短路规则: 左侧为真则不计算右侧
            string tmp_cal_name =get_temp_name();;

            // 真逻辑: 跳转PC+2,tmp_cal_name赋为1, 然后跳转至结束
            Instruction * root_true_goto = new Instruction(ir::Operand(root->v,root->t),
                                ir::Operand(),
                                ir::Operand("2",Type::IntLiteral),ir::Operator::_goto);
            Instruction * root_true_assign = new Instruction(ir::Operand("1",Type::IntLiteral),
                                                             ir::Operand(),
                                                             ir::Operand(tmp_cal_name,ir::Type::Int),ir::Operator::mov);

            // 假逻辑: 跳转PC+(1+2)
            Instruction * root_false_goto = new Instruction(ir::Operand(),
                                                            ir::Operand(),
                                                            ir::Operand("3",Type::IntLiteral),ir::Operator::_goto);

            // 假逻辑中需要使用的or指令
            Instruction * calInst = new Instruction(ir::Operand(root->v,root->t),
                                                    ir::Operand(lorexp->v,lorexp->t),
                                                    ir::Operand(tmp_cal_name,ir::Type::Int),ir::Operator::_or);
            second_and_instructions.push_back(calInst);

            Instruction * true_logic_goto = new Instruction(ir::Operand(),
                                                            ir::Operand(),
                                                            ir::Operand(std::to_string(second_and_instructions.size()+1),Type::IntLiteral),ir::Operator::_goto);

            instructions.push_back(root_true_goto);
            instructions.push_back(root_false_goto);
            instructions.push_back(root_true_assign);
            instructions.push_back(true_logic_goto);
            instructions.insert(instructions.end(),second_and_instructions.begin(),second_and_instructions.end());

            root->v = tmp_cal_name;
            root->t = Type::Int;
        }
    }
}
// LAndExp -> EqExp [ '&&' LAndExp ]
// LAndExp.is_computable
// LAndExp.v
// LAndExp.t
void frontend::Analyzer::analysisLAndExp(LAndExp *root, vector<Instruction *> &instructions)
{
    std::cout<<"analysisLAndExp "<<std::endl;
    GET_CHILD_PTR(eqexp, EqExp, 0);
    COPY_EXP_NODE(root, eqexp);
    analysisEqExp(eqexp, instructions);
    COPY_EXP_NODE(eqexp, root);
    if (root->children.size() == 1){
        return ;
    }
    else
    {
        // LAndExp
        LAndExp* landexp = dynamic_cast<LAndExp*>(root->children[2]);
        assert(landexp);
         // 并不立即加入到instructions中，在确定跳转后再加入
        vector<Instruction *> second_and_instructions;
        analysisLAndExp(landexp, second_and_instructions);
        // 处理操作数1 (计算其是否为真 or 假)
        if (root->t == Type::Float){
            string tmp_name = get_temp_name();
            ir::Instruction* cvtInst = new ir::Instruction(ir::Operand(root->v,Type::Float),
                                                           ir::Operand("0.0",Type::FloatLiteral),
                                                           ir::Operand(tmp_name,Type::Int),ir::Operator::fneq);
            instructions.push_back(cvtInst);
            root->v = tmp_name;
            root->t = Type::Int;
        }
        else if (root->t == Type::FloatLiteral){
            float val = std::stof(root->v);
            root->t = Type::IntLiteral;
            root->v = std::to_string(val != 0);
        }

        // 处理操作数2 (计算其是否为真 or 假)
        if (landexp->t == Type::Float){
            string tmp_name = get_temp_name();
            ir::Instruction* cvtInst = new ir::Instruction(ir::Operand(landexp->v,Type::Float),
                                                           ir::Operand("0.0",Type::FloatLiteral),
                                                           ir::Operand(tmp_name,Type::Int),ir::Operator::fneq);
            second_and_instructions.push_back(cvtInst);
            landexp->v = tmp_name;
            landexp->t = Type::Int;
        }
        else if (landexp->t == Type::FloatLiteral){
            float val = std::stof(landexp->v);
            landexp->t = Type::IntLiteral;
            landexp->v = std::to_string(val != 0);
        }

        // 计算EqExp && LAndExp
        if (root->t == Type::IntLiteral && landexp->t == Type::IntLiteral){
            root->v = std::to_string(std::stoi(root->v) && std::stoi(landexp->v));
        }
        else{
            // 注意短路规则: 左侧为假，则不计算右侧！！！
            string tmp_cal_name =get_temp_name();
            Instruction * root_true_goto = new Instruction(ir::Operand(root->v,root->t),
                                                           ir::Operand(),
                                                           ir::Operand("2",Type::IntLiteral),ir::Operator::_goto);
            instructions.push_back(root_true_goto);
            // 真逻辑部分，此时不短路，开始计算右侧并赋值
            vector<Instruction *> true_logic_inst = second_and_instructions;
            Instruction * calInst = new Instruction(ir::Operand(root->v,root->t),
                                                    ir::Operand(landexp->v,landexp->t),
                                                    ir::Operand(tmp_cal_name,ir::Type::Int),ir::Operator::_and);
            true_logic_inst.push_back(calInst);
            // 真逻辑结束后，需跳出假逻辑部分
            Instruction * true_logic_goto = new Instruction(ir::Operand(),
                                                            ir::Operand(),
                                                            ir::Operand("2",Type::IntLiteral),ir::Operator::_goto);
            true_logic_inst.push_back(true_logic_goto);
            // 如果root为假，则不进行右侧计算，直接跳转出结束
            Instruction * root_false_goto = new Instruction(ir::Operand(),
                                                            ir::Operand(),
                                                            ir::Operand(std::to_string(true_logic_inst.size()+1),Type::IntLiteral),ir::Operator::_goto);
            // root为假时，tmp_cal_name为0
            Instruction * root_false_assign = new Instruction(ir::Operand("0",Type::IntLiteral),
                                ir::Operand(),
                                ir::Operand(tmp_cal_name,ir::Type::Int),ir::Operator::mov);
            instructions.push_back(root_false_goto);
            instructions.insert(instructions.end(),true_logic_inst.begin(),true_logic_inst.end());
            instructions.push_back(root_false_assign);
            root->v = tmp_cal_name;
            root->t = Type::Int;
        }
    }
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
// EqExp.is_computable
// EqExp.v
// EqExp.t
void frontend::Analyzer::analysisEqExp(EqExp *root, vector<Instruction *> &instructions)
{
    std::cout<<"analysisEqExp "<<std::endl;
    GET_CHILD_PTR(relexp, RelExp, 0);
    COPY_EXP_NODE(root, relexp);
    analysisRelExp(relexp, instructions);
    COPY_EXP_NODE(relexp, root);

    for (size_t i = 2; i < root->children.size(); i += 2) // 如果有多个RelExp
    {
        GET_CHILD_PTR(relexp, RelExp, i);
        analysisRelExp(relexp, instructions);

        GET_CHILD_PTR(term, Term, i - 1);
        if (root->t == ir::Type::IntLiteral && relexp->t == ir::Type::IntLiteral)
        {
            switch (term->token.type)
            {
            case TokenType::EQL:
                root->v = std::to_string(std::stoi(root->v) == std::stoi(relexp->v));
                break;
            case TokenType::NEQ:
                root->v = std::to_string(std::stoi(root->v) != std::stoi(relexp->v));
                break;
            default:
                assert(false);
                break;
            }
        }
        else
        {
            auto temp_name = get_temp_name();

            switch (term->token.type) // TODO: 只考虑整型
            {
            case TokenType::EQL: // 整型变量==运算，逻辑运算结果用变量表示。
            {
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {relexp->v, relexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::eq));
                break;
            }
            case TokenType::NEQ:
            {
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {relexp->v, relexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::neq));
                break;
            }
            default:
                assert(false);
                break;
            }
            root->v = temp_name;
            root->t = ir::Type::Int;
        }
    }
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
// RelExp.is_computable
// RelExp.v
// RelExp.t
void frontend::Analyzer::analysisRelExp(RelExp *root, vector<Instruction *> &instructions)
{
    std::cout<<"analysisRelExp "<<std::endl;
    // std::cout << "RelExp: " + toString(root->t) + " " + root->v << std::endl;
    GET_CHILD_PTR(addexp, AddExp, 0);
    COPY_EXP_NODE(root, addexp);
    analysisAddExp(addexp, instructions);
    COPY_EXP_NODE(addexp, root);

    // std::cout << "RelExp: " + toString(root->t) + " " + root->v << std::endl;

    for (size_t i = 2; i < root->children.size(); i += 2) // 如果有多个AddExp
    {
        GET_CHILD_PTR(addexp, AddExp, i);
        analysisAddExp(addexp, instructions);

        GET_CHILD_PTR(term, Term, i - 1);
        // std::cout << toString(root->t) + " " + root->v + " " + toString(term->token.type) + " " + toString(addexp->t) + " " + addexp->v << std::endl;
        if (root->t == ir::Type::IntLiteral && addexp->t == ir::Type::IntLiteral)
        {

            switch (term->token.type)
            {
            case TokenType::LSS:
                root->v = std::to_string(std::stoi(root->v) < std::stoi(addexp->v));
                break;
            case TokenType::LEQ:
                root->v = std::to_string(std::stoi(root->v) <= std::stoi(addexp->v));
                break;
            case TokenType::GTR:
                root->v = std::to_string(std::stoi(root->v) > std::stoi(addexp->v));
                break;
            case TokenType::GEQ:
                root->v = std::to_string(std::stoi(root->v) >= std::stoi(addexp->v));
                break;
            default:
                break;
            }
        }
        else
        {
            auto temp_name = get_temp_name();
            switch (term->token.type) // TODO: 暂时只考虑整型
            {
            case TokenType::LSS: // 整型变量<运算，逻辑运算结果用变量表示
            {
                if(root->t==Type::FloatLiteral||root->t==Type::Float){
                    if(addexp->t==Type::IntLiteral||addexp->t==Type::Int){
                        //类型转换
                        std::cout<<"LSS-类型转换前 "<<addexp->v<<std::endl;;
                        int val=std::stoi(addexp->v);
                        addexp->t=Type::FloatLiteral;
                        addexp->v=std::to_string((float)(val));
                        std::cout<<"LSS-类型转换 "<<addexp->v<<" "<< toString(addexp->t)<<std::endl;

                    }
                    instructions.push_back(new Instruction({root->v, root->t},
                                                       {addexp->v, addexp->t},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::flss));
                    // std::cout<<"LSS-flss "<<std::endl;
                                                       root->t = ir::Type::Float;
                }else if(root->t==Type::IntLiteral||root->t==Type::Int)
                    {
                        
                    instructions.push_back(new Instruction({root->v, root->t},
                                                       {addexp->v, addexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::lss));
                                                       root->t = ir::Type::Int;
                }


                // if(root->t==Type::FloatLiteral) std::cout<<"LSS-root->t==Type::FloatLiteral "<<std::endl;
                // if(root->t==Type::IntLiteral) std::cout<<"LSS-root->t==Type::IntLiteral "<<std::endl;
                // if(root->t==Type::Float) std::cout<<"LSS-root->t==Type::Float "<<std::endl;
                // if(root->t==Type::Int) std::cout<<"LSS-root->t==Type::Int "<<std::endl;
          
                
                break;
            }
            case TokenType::LEQ: // 整型变量<=运算，逻辑运算结果用变量表示
            {
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {addexp->v, addexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::leq));
                                                       root->t = ir::Type::Int;
                break;
            }
            case TokenType::GTR: // 整型变量>运算，逻辑运算结果用变量表示
            {

                instructions.push_back(new Instruction({root->v, root->t},
                                                       {addexp->v, addexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::gtr));
                                                       root->t = ir::Type::Int;
                break;
            }
            case TokenType::GEQ: // 整型变量>=运算，逻辑运算结果用变量表示
            {
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {addexp->v, addexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::geq));
                                                       root->t = ir::Type::Int;
                break;
            }
            default:
                assert(false);
                break;
            }
            root->v = temp_name;
            
        }
    }
}

// Exp -> AddExp
// Exp.is_computable
// Exp.v
// Exp.t
void frontend::Analyzer::analysisExp(Exp *root, vector<Instruction *> &instructions)
{
     std::cout<<"analysisExp "<<std::endl;
    GET_CHILD_PTR(addexp, AddExp, 0);
    COPY_EXP_NODE(root, addexp);
    analysisAddExp(addexp, instructions);
    std::cout<<"analysisExp-analysisAddExp->v:"<<addexp->v<<std::endl;
               if (addexp->t == ir::Type::Float){
        std::cout<<"analysisExp-analysisAddExp-->t = ir::Type::Float;"<<std::endl;
        }
        else if (addexp->t == ir::Type::Int)
        {
        std::cout<<"analysisExp-analysisAddExp-->t = ir::Type::Int;"<<std::endl;
        }else if (addexp->t == ir::Type::IntLiteral)
        {
        std::cout<<"analysisExp-analysisAddExp-->t = ir::Type::IntLiteral;"<<std::endl;
        }else if (addexp->t == ir::Type::FloatLiteral )
        {
        std::cout<<"analysisExp-analysisAddExp-->t = ir::Type::FloatLiteral;"<<std::endl;
        }
    COPY_EXP_NODE(addexp, root);
}

// AddExp -> MulExp { ('+' | '-') MulExp }
// AddExp.is_computable
// AddExp.v
// AddExp.t
void frontend::Analyzer::analysisAddExp(AddExp *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisAddExp "<<std::endl;

    // 是否需要类型提升？-----------------------------

    GET_CHILD_PTR(mulexp, MulExp, 0);
    COPY_EXP_NODE(root, mulexp);
    analysisMulExp(mulexp, instructions);
    std::cout<<"analysisAddExp-analysisMulExp->v:"<<mulexp->v<<std::endl;

    COPY_EXP_NODE(mulexp, root);//不能注释

       if (mulexp->t == ir::Type::Float){
             std::cout<<"analysisAddExp-mulexp->t = ir::Type::Float;"<<std::endl;
        }else if (mulexp->t == ir::Type::Int)
        {
            std::cout<<"analysisAddExp-mulexp->t = ir::Type::Int;"<<std::endl;
        }else if (mulexp->t == ir::Type::IntLiteral)
        {
            std::cout<<"analysisAddExp-mulexp->t = ir::Type::IntLiteral;"<<std::endl;
        }else if (mulexp->t == ir::Type::FloatLiteral )
             {
            std::cout<<"analysisAddExp-mulexp->t  = ir::Type::FloatLiteral;"<<std::endl;
        }
        //看看root的情况
        
       if (root->t == ir::Type::Float){
             std::cout<<"analysisAddExp-root->t = ir::Type::Float;"<<std::endl;
        }
            
        else if (root->t == ir::Type::Int)
        {
            std::cout<<"analysisAddExp-root->t = ir::Type::Int;"<<std::endl;
        }else if (root->t == ir::Type::IntLiteral)
        {
            std::cout<<"analysisAddExp-root->t = ir::Type::IntLiteral;"<<std::endl;
        }else if (root->t == ir::Type::FloatLiteral )
             {
            std::cout<<"analysisAddExp-root->t  = ir::Type::FloatLiteral;"<<std::endl;
        }

    

    // 如果有多个MulExp
    for (size_t i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(mulexp, MulExp, i);
        mulexp->v = get_temp_name();
        mulexp->t = root->t;
        analysisMulExp(mulexp, instructions);
        std::cout<<"analysisAddExp-analysisMulEx1-->v:"<<mulexp->v<<std::endl;
           if (mulexp->t == ir::Type::Float){
        std::cout<<"analysisAddExp-analysisMul1-->t = ir::Type::Float;"<<std::endl;
        }
        else if (mulexp->t == ir::Type::Int)
        {
        std::cout<<"analysisAddExp-analysisMul1-->t = ir::Type::Int;"<<std::endl;
        }else if (mulexp->t == ir::Type::IntLiteral)
        {
        std::cout<<"analysisAddExp-analysisMul1-->t = ir::Type::IntLiteral;"<<std::endl;
        }else if (mulexp->t == ir::Type::FloatLiteral )
        {
        std::cout<<"analysisAddExp-analysisMul1-->t = ir::Type::FloatLiteral;"<<std::endl;
        }

        GET_CHILD_PTR(term, Term, i - 1);

    if (root->t == ir::Type::Int) {
        auto temp_name = get_temp_name();
        std::cout << "analysisAddExp-Int" << std::endl;
        if (mulexp->t == ir::Type::Float || mulexp->t == ir::Type::FloatLiteral) {
            instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                   {},
                                                   {temp_name, ir::Type::Int},
                                                   Operator::cvt_f2i));
            std::cout << "cvt_f2i-1447" << std::endl;
        } else {
            instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                   {},
                                                   {temp_name, ir::Type::Int},
                                                   Operator::mov));
            std::cout << "mov-1421" << std::endl;
        }

        if (term->token.type == TokenType::PLUS) {
            instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                   {temp_name, ir::Type::Int},
                                                   {temp_name, ir::Type::Int},
                                                   Operator::add));
        } else {
            instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                   {temp_name, ir::Type::Int},
                                                   {temp_name, ir::Type::Int},
                                                   Operator::sub));
        }
        root->v = temp_name;
        root->t = ir::Type::Int;
    } else if (root->t == ir::Type::IntLiteral) {
        std::cout << "analysisAddExp-root->t=IntLiteral" << std::endl;
        if (mulexp->t == ir::Type::IntLiteral) {
            if (term->token.type == TokenType::PLUS) {
                root->v = std::to_string(std::stoi(root->v) + std::stoi(mulexp->v));
            } else if (term->token.type == TokenType::MINU){
                root->v = std::to_string(std::stoi(root->v) - std::stoi(mulexp->v));
            }
        } else {//root为IntLiteral
            
            if (mulexp->t == ir::Type::FloatLiteral) {
                //root为IntLiteral,mulexp为FloatLiteral
                int val1 = stof(mulexp->v);
                mulexp->v = std::to_string(val1);
                mulexp->t == ir::Type::IntLiteral;
            }else{//root为IntLiteral,mulexp不为FloatLiteral和IntLiteral
                auto temp_name = get_temp_name();
                instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                   {},
                                                   {temp_name, ir::Type::Int},
                                                   Operator::mov));
            std::cout << "mov-1465" << std::endl;

            if (term->token.type == TokenType::PLUS) {
                instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::add));
            } else {
                instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::sub));
            }
            root->v = temp_name;
            root->t = ir::Type::Int;

            }

            
        }
    } else if (root->t == ir::Type::Float) {//root为Float
        std::cout << "analysisAddExp-root->t=Float" << std::endl;
        auto temp_name = get_temp_name();
        if (mulexp->t == ir::Type::Int || mulexp->t == ir::Type::IntLiteral) {
            instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                   {},
                                                   {temp_name, ir::Type::Float},
                                                   Operator::cvt_i2f));
        } else if (mulexp->t != ir::Type::FloatLiteral) {
            instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                   {},
                                                   {temp_name, ir::Type::Float},
                                                   Operator::fmov));
            std::cout << "fmov-1500" << std::endl;
        }

        if (term->token.type == TokenType::PLUS) {
            instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                   {temp_name, ir::Type::Float},
                                                   {temp_name, ir::Type::Float},
                                                   Operator::fadd));
            std::cout << "term->token.type == TokenType::PLUS)" << std::endl;
        } else {
            instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                   {temp_name, ir::Type::Float},
                                                   {temp_name, ir::Type::Float},
                                                   Operator::fsub));
            std::cout << "term->token.type == TokenType::SUB" << std::endl;
        }
        root->v = temp_name;
        root->t = ir::Type::Float;
    } else if (root->t == ir::Type::FloatLiteral) {
        std::cout << "analysisAddExp-FloatLiteral-1600" << std::endl;
        if (mulexp->t == ir::Type::FloatLiteral) {
            std::cout << "analysisAddExp-mulexp->t == ir::Type::FloatLiteral-1603" << std::endl;
            if (term->token.type == TokenType::PLUS) {
                std::cout << "analysisAddExp-PLUS-root->v:" << std::stof(root->v) << ","
                          << "analysisAddExp-PLUS-mulexp->v:" << std::stof(mulexp->v) << std::endl;
                float res = std::stof(root->v) + std::stof(mulexp->v);
                root->v = std::to_string(res);
                root->t = ir::Type::FloatLiteral;
                std::cout << "analysisAddExp-PLUSres:" << res << std::endl;
            } else {
                std::cout << "analysisAddExp-SUB" << std::endl;
                float val1 = std::stof(root->v);
                float val2 = std::stof(mulexp->v);
                float res = val1 - val2;
                std::cout << "analysisAddExp-SUB-root->v:" << val1 << "," << "analysisAddExp-SUB-root->v:" << val2 << std::endl;
                root->v = std::to_string(res);
                std::cout << "analysisAddExp-SUBres:" << res << std::endl;
            }
        } else {
            std::cout << "analysisAddExp-mulexp->t == ir::Type::Int类-1620" << std::endl;
            auto temp_name = get_temp_name();
            if (mulexp->t == ir::Type::Int || mulexp->t == ir::Type::IntLiteral) {
                instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::cvt_i2f));
                std::cout << "cvt_i2f-1647" << std::endl;

                mulexp->t = ir::Type::FloatLiteral;
                mulexp->v = std::to_string(static_cast<float>(mulexp->t));
            }

            instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                   {},
                                                   {temp_name, ir::Type::Float},
                                                   Operator::fmov));
            std::cout << "fmov-1542" << std::endl;

            if (term->token.type == TokenType::PLUS) {
                instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fadd));
            } else {
                instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fsub));
            }
            root->v = temp_name;
            root->t = ir::Type::Float;
        }
    } else {
        std::cout << "Unsupported type in analysisAddExp" << std::endl;
    }
    }

    std::cout<<"analysisAddExp-finalroot->v:"<<root->v<<std::endl;
    // delete_temp_name();//?
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
// MulExp.is_computable
// MulExp.v
// MulExp.t
void frontend::Analyzer::analysisMulExp(MulExp *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisMulExp "<<std::endl;
    GET_CHILD_PTR(unaryexp, UnaryExp, 0);
    COPY_EXP_NODE(root, unaryexp);
    analysisUnaryExp(unaryexp, instructions);
    std::cout<<"analysisMulExp-analysisUnaryExp->v:"<<unaryexp->v<<std::endl;
   if (unaryexp->t == ir::Type::Float){
        std::cout<<"analysisMulExp-unaryexp->t = ir::Type::Float;"<<std::endl;
        }
        else if (unaryexp->t == ir::Type::Int)
        {
        std::cout<<"analysisMulExp-unaryexp->t = ir::Type::Int;"<<std::endl;
        }else if (unaryexp->t == ir::Type::IntLiteral)
        {
        std::cout<<"analysisMulExp-unaryexp->t = ir::Type::IntLiteral;"<<std::endl;
        }else if (unaryexp->t == ir::Type::FloatLiteral )
        {
        std::cout<<"analysisMulExp-unaryexp->t = ir::Type::FloatLiteral;"<<std::endl;
        }


    
    COPY_EXP_NODE(unaryexp, root);

    // 如果有多个UnaryExp
    for (size_t i = 2; i < root->children.size(); i += 2)
    {

        GET_CHILD_PTR(unaryexp, UnaryExp, i);
        unaryexp->v = get_temp_name();
        unaryexp->t = root->t;
        analysisUnaryExp(unaryexp, instructions);
        std::cout<<"analysisMulExp-analysisUnaryExp-1697->v:"<<unaryexp->v<<std::endl;
        GET_CHILD_PTR(term, Term, i - 1);

        auto temp_name = get_temp_name();
        switch (root->t)
        {
        case ir::Type::Int:
            if (unaryexp->t == ir::Type::Float || unaryexp->t == ir::Type::FloatLiteral)
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::cvt_f2i));std::cout<<"cvt_f2i-1633"<<std::endl;
            }
            else
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mov));
                                                       std::cout<<"1607"<<std::endl;
            }
            switch (term->token.type)
            {
            case TokenType::MULT:
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mul));
                break;
            case TokenType::DIV:
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::div));
                break;
            case TokenType::MOD:
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mod));
                break;
            default:
                break;
            }
            root->v = temp_name;
            root->t = ir::Type::Int;
            break;

        case ir::Type::IntLiteral:
            if (unaryexp->t == ir::Type::IntLiteral)
            {
                switch (term->token.type)
                {
                case TokenType::MULT:
                    root->v = std::to_string(std::stoi(root->v) * std::stoi(unaryexp->v));
                    break;
                case TokenType::DIV:
                    root->v = std::to_string(std::stoi(root->v) / std::stoi(unaryexp->v));
                    break;
                case TokenType::MOD:
                    root->v = std::to_string(std::stoi(root->v) % std::stoi(unaryexp->v));
                    break;
                default:
                    break;
                }
                root->t = ir::Type::IntLiteral;
            }
            else
            {// (unaryexp->t != ir::Type::IntLiteral),IntLiteral
                if (unaryexp->t == ir::Type::Float || unaryexp->t == ir::Type::FloatLiteral)
                {
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::cvt_f2i));std::cout<<"cvt_f2i-1696"<<std::endl;
                }
                else
                {
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mov));
                                                           std::cout<<"mov-1671"<<std::endl;
                }
                switch (term->token.type)//IntLiteral
                {
                case TokenType::MULT:
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mul));
                    break;
                case TokenType::DIV:
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::div));
                    break;
                case TokenType::MOD:
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mod));
                    break;
                default:
                    break;
                }
                root->v = temp_name;
                root->t = ir::Type::Int;
            }
            break;

        case ir::Type::Float:
            if (unaryexp->t == ir::Type::Int || unaryexp->t == ir::Type::IntLiteral)
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::cvt_i2f));std::cout<<"cvt_i2f-1822"<<std::endl;
                root->t = ir::Type::FloatLiteral;
            }
            else
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fmov));std::cout<<"fmov-1721 "<<temp_name<<std::endl;
            }
            switch (term->token.type)
            {
            case TokenType::MULT:
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fmul));
                                                       std::cout<<"fmul-1731 "<<temp_name<<std::endl;
                break;
            case TokenType::DIV:
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fdiv));
                break;
            // case TokenType::MOD:
            //     instructions.push_back(new Instruction({root->v, ir::Type::Float},
            //                                            {temp_name, ir::Type::Float},
            //                                            {temp_name, ir::Type::Float},
            //                                            Operator::fmod));
            //     break;
            default:
                break;
                
            }
            root->v = temp_name;
            root->t = ir::Type::Float;
            break;
        case ir::Type::FloatLiteral:
            if (unaryexp->t == ir::Type::FloatLiteral)
            {
                switch (term->token.type)
                {
                case TokenType::MULT:
                    root->v = std::to_string(std::stof(root->v) * std::stof(unaryexp->v));
                    break;
                case TokenType::DIV:
                    root->v = std::to_string(std::stof(root->v) / std::stof(unaryexp->v));
                    break;
                case TokenType::MOD:
                    root->v = std::to_string(std::fmod(std::stof(root->v), std::stof(unaryexp->v)));
                    break;
                }
                root->t = ir::Type::FloatLiteral;
            }else{// (unaryexp->t != ir::Type::FloatLiteral)
                if(unaryexp->t == ir::Type::Int || unaryexp->t == ir::Type::IntLiteral){
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::cvt_i2f));std::cout<<"cvt_i2f-1881"<<std::endl;
                }else{
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fmov));
                        std::cout<<"fmov-1766"<<std::endl<<temp_name<<std::endl;
                }
                switch (term->token.type)
                {
                case TokenType::MULT:
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fmul));
                                std::cout<<"fmul-1785"<<std::endl;
                    break;
                case TokenType::DIV:
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fdiv));
                    break;
                // case TokenType::MOD:
                //     instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                //                                            {temp_name, ir::Type::Float},
                //                                            {temp_name, ir::Type::Float},
                //                                            Operator::mod));
                    // break;
                default:
                    break;
                }
                root->v = temp_name;
                root->t = ir::Type::Float;
            }
            break;
        default:
            break;
        }
    }
}

// UnaryExp -> PrimaryExp
//           | Ident '(' [FuncRParams] ')'
//           | UnaryOp UnaryExp
// UnaryExp.is_computable
// UnaryExp.v
// UnaryExp.t
void frontend::Analyzer::analysisUnaryExp(UnaryExp *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisUnaryExp "<<std::endl;
    if (NODE_IS(PRIMARYEXP, 0)) // 如果是PrimaryExp
    {std::cout<<"analysisUnaryExp-PrimaryExp "<<std::endl;

        GET_CHILD_PTR(primaryexp, PrimaryExp, 0);
        COPY_EXP_NODE(root, primaryexp);
        analysisPrimaryExp(primaryexp, instructions);
        COPY_EXP_NODE(primaryexp, root);
    }
    else if (NODE_IS(TERMINAL, 0)) // 如果是函数调用
    {std::cout<<"analysisUnaryExp-函数调用 "<<std::endl;
        GET_CHILD_PTR(ident, Term, 0);
        Function *func;
        if (symbol_table.functions.count(ident->token.value)) // 如果是用户自定义函数
        {
            func = symbol_table.functions[ident->token.value];
        }
        else if (get_lib_funcs()->count(ident->token.value)) // 如果是库函数
        {
            func = (*get_lib_funcs())[ident->token.value];
        }
        else
        {
            assert(0);
        }
        root->v = get_temp_name();
        root->t = func->returnType;
if (NODE_IS(FUNCRPARAMS, 2)) // 如果有参数
        {  
            GET_CHILD_PTR(funcrparams, FuncRParams, 2);
            std::vector<Operand> params;
            for (size_t i = 0; i < func->ParameterList.size(); i++)
            {
                Operand param = {get_temp_name(), (func->ParameterList)[i].type};
                params.push_back(param);
                std::cout << "(func->ParameterList)[" << i << "].type: " << toString((func->ParameterList)[i].type) << std::endl;
                              if (params[i].type == ir::Type::Float){
                                    std::cout<<"params[i].type = ir::Type::Float;"<<std::endl;
                                    }
                                    else if (params[i].type == ir::Type::Int)
                                    {
                                    std::cout<<"params[i].type = ir::Type::Int;"<<std::endl;
                                    }else if (params[i].type == ir::Type::IntLiteral)
                                    {
                                    std::cout<<"params[i].type = ir::Type::IntLiteral;"<<std::endl;
                                    }else if (params[i].type == ir::Type::FloatLiteral )
                                    {
                                    std::cout<<"params[i].type = ir::Type::FloatLiteral;"<<std::endl;
                                    }
                                    

                                    if (func->ParameterList[i].type == ir::Type::Float){
                                    std::cout<<"func->ParameterList[i].type = ir::Type::Float;"<<std::endl;
                                    }
                                    else if (func->ParameterList[i].type == ir::Type::Int)
                                    {
                                    std::cout<<"func->ParameterList[i].type = ir::Type::Int;"<<std::endl;
                                    }else if (func->ParameterList[i].type == ir::Type::IntLiteral)
                                    {
                                    std::cout<<"func->ParameterList[i].type = ir::Type::IntLiteral;"<<std::endl;
                                    }else if (func->ParameterList[i].type == ir::Type::FloatLiteral )
                                    {
                                    std::cout<<"func->ParameterList[i].type = ir::Type::FloatLiteral;"<<std::endl;
                                    }
            }
            analysisFuncRParams(funcrparams, params, instructions,func);

            instructions.push_back(new ir::CallInst({ident->token.value, ir::Type::null}, params, {root->v, root->t}));
        }
        else // 如果没有参数
        {
            instructions.push_back(new ir::CallInst({ident->token.value, ir::Type::null}, {root->v, root->t}));
        }
    }
    else // 如果是一元运算符
    {std::cout<<"analysisUnaryExp-如果是一元运算符 "<<std::endl;
        GET_CHILD_PTR(unaryop, UnaryOp, 0);
        analysisUnaryOp(unaryop, instructions);

        GET_CHILD_PTR(unaryexp, UnaryExp, 1);
        COPY_EXP_NODE(root, unaryexp);
        analysisUnaryExp(unaryexp, instructions);
        COPY_EXP_NODE(unaryexp, root);

        if (unaryop->op == TokenType::MINU)
        {
            auto temp_name = get_temp_name();
            switch (root->t)
            {
            case ir::Type::Int:
                instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                                       {root->v, root->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::sub));
                root->t = ir::Type::Int;
                root->v = temp_name;
                break;
            case ir::Type::IntLiteral:
                root->t = ir::Type::IntLiteral;
                root->v = std::to_string(-std::stoi(root->v));
                break;
            case ir::Type::Float:
                instructions.push_back(new Instruction({"0.0", ir::Type::FloatLiteral},
                                                       {root->v, root->t},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fsub));
                root->t = ir::Type::Float;
                root->v = temp_name;
                break;
            case ir::Type::FloatLiteral:
                root->t = ir::Type::FloatLiteral;
                root->v = std::to_string(-std::stof(root->v));
                break;
            default:
                assert(0);
            }
        }
        else if (unaryop->op == TokenType::NOT)
        {
            auto temp_name = get_temp_name();
            switch (root->t)
            {
            case ir::Type::Int:
            case ir::Type::IntLiteral:
                instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                                       {root->v, root->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::eq));
                root->t = ir::Type::Int;
                break;
            case ir::Type::Float:
            case ir::Type::FloatLiteral:
                instructions.push_back(new Instruction({"0.0", ir::Type::FloatLiteral},
                                                       {root->v, root->t},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::feq));
                root->t = ir::Type::Float;
                break;
            default:
                assert(0);
            }
            root->v = temp_name;
        }
    }
}

// UnaryOp -> '+' | '-' | '!'
// TokenType op;
void frontend::Analyzer::analysisUnaryOp(UnaryOp *root, std::vector<Instruction *> &instructions)
{
     std::cout<<"analysisUnaryOp "<<std::endl;
    GET_CHILD_PTR(term, Term, 0);
    root->op = term->token.type;
}

// FuncRParams -> Exp { ',' Exp }
//analysisFuncRParams(funcrparams, params, instructions);funcrparams对应要求的
void frontend::Analyzer::analysisFuncRParams(FuncRParams *root, vector<Operand> &params, vector<Instruction *> &instructions,Function * func)
{
       //可能需要类型的转换
     std::cout<<"analysisFuncRParams "<<std::endl;
    size_t index = 0;
    for (size_t i = 0; i < root->children.size(); i += 2)
    {
        if (NODE_IS(EXP, i))
        {
            GET_CHILD_PTR(exp, Exp, i);
            exp->v = params[index].name;
            exp->t = params[index].type;
            
            analysisExp(exp, instructions);
            std::cout<<"analysisFuncRParams-exp->v "<<exp->v<<std::endl;
            std::cout<<"analysisFuncRParams-exp->t "<<toString(exp->t)<<std::endl;
            std::cout<<"params["<<i<<"].type:"<<toString(params[i].type)<<std::endl;
            std::cout<<"func->ParameterList["<<i<<"].type:"<<toString(func->ParameterList[i].type)<<std::endl;
            if(exp->t==ir::Type::FloatLiteral &&func->ParameterList[i].type==ir::Type::Int){
                auto temp_name = get_temp_name();
                // exp->v=std::to_string(int(stof(exp->v)));
                // exp->t=ir::Type::Int;
            instructions.push_back(new Instruction({exp->v, exp->t}, {}, {temp_name, ir::Type::Int}, Operator::cvt_f2i));
                    exp->v = temp_name;
                    exp->t = ir::Type::Int;
            std::cout<<"类型转换后analysisFuncRParams-exp->v "<<exp->v<<std::endl;
            std::cout<<"类型转换后analysisFuncRParams-exp->t "<<toString(exp->t)<<std::endl;
            }
            params[index] = {exp->v, exp->t};
            index++;
        }
    }
}


// PrimaryExp -> '(' Exp ')' | LVal | Number
// PrimaryExp.is_computable
// PrimaryExp.v
// PrimaryExp.t
void frontend::Analyzer::analysisPrimaryExp(PrimaryExp *root, std::vector<Instruction *> &instructions)
{
    std::cout<<"analysisPrimaryExp "<<std::endl;
    if (NODE_IS(NUMBER, 0))
    {std::cout<<"analysisPrimaryExp-NUMBER "<<std::endl;
        GET_CHILD_PTR(number, Number, 0);
        COPY_EXP_NODE(root, number);
        analysisNumber(number, instructions);
        COPY_EXP_NODE(number, root);
    }
    else if (NODE_IS(LVAL, 0))
    {std::cout<<"analysisPrimaryExp-LVAL-is_left_false "<<std::endl;
        GET_CHILD_PTR(lval, LVal, 0);
        COPY_EXP_NODE(root, lval);
        analysisLVal(lval, instructions, false);
        COPY_EXP_NODE(lval, root);
    }
    else
    {std::cout<<"analysisPrimaryExp-exp "<<std::endl;
        GET_CHILD_PTR(exp, Exp, 1);
        COPY_EXP_NODE(root, exp);
        analysisExp(exp, instructions);
        COPY_EXP_NODE(exp, root);
    }
}

// LVal -> Ident {'[' Exp ']'}
// LVal.is_computable
// LVal.v
// LVal.t
// LVal.i array index, legal if t is IntPtr or FloatPtr
void frontend::Analyzer::analysisLVal(LVal *root, vector<Instruction *> &instructions, bool is_left = false)
{
    std::cout<<"analysisLVal "<<std::endl;
    GET_CHILD_PTR(ident, Term, 0);
    auto var = symbol_table.get_ste(ident->token.value);

    std::cout << "LVal: " << toString(var.operand.type) << " "<< var.operand.name << std::endl;
    // 普通变量 (可能的类型: Int, IntLiteral, Float, FloatLiteral, IntPtr, FloatPtr)
    if (root->children.size() == 1) // 如果没有下标
    {std::cout<<"analysisLVal-普通变量 "<<std::endl;
        if (is_left)
        {
            std::cout<<"analysisLVal-普通变量-is_left "<<std::endl;
            // root->t = var.operand.type;
            // root->v = var.operand.name;
            switch (root->t)
            {
            case ir::Type::Int:
            case ir::Type::IntLiteral:
                switch (var.operand.type)
                {
                case ir::Type::Float:
                case ir::Type::FloatLiteral:
                    instructions.push_back(new Instruction({root->v, ir::Type::Float}, {}, {root->v, root->t}, Operator::cvt_f2i));std::cout<<"cvt_f2i-2135"<<std::endl;
                    break;
                default:
                    break;
                }
                break;
            case ir::Type::Float:
            case ir::Type::FloatLiteral:
                switch (var.operand.type)
                {
                case ir::Type::Int:
                case ir::Type::IntLiteral:
                    instructions.push_back(new Instruction({root->v, ir::Type::Int}, {}, {root->v, root->t}, Operator::cvt_i2f));std::cout<<"cvt_i2f-2064"<<std::endl;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }

            instructions.push_back(new Instruction({root->v, root->t},
                                                   {},
                                                   {var.operand.name, var.operand.type},
                                                   Operator::mov));
                                                   std::cout<<"2037"<<std::endl;
            return;
        }

        switch (var.operand.type)
        {
        case ir::Type::IntPtr:
            std::cout << "IntPtr" << std::endl;
            root->is_computable = false;
                root->t = var.operand.type;
                 root->v = var.operand.name;
             //   root->v=symbol_table.get_scoped_name(ident->token.value);
                break;
        case ir::Type::FloatPtr:
            std::cout << "FloatPtr" << std::endl;
            root->is_computable = false;
            root->t = var.operand.type;
             root->v = var.operand.name;
            root->v=symbol_table.get_scoped_name(ident->token.value);
            break;
        case ir::Type::IntLiteral:
        case ir::Type::FloatLiteral:
            root->is_computable = true;
            root->t = var.operand.type;
            root->v = var.operand.name;
            break;
        case ir::Type::Float:
            root->t = var.operand.type;
            root->v = get_temp_name();
            instructions.push_back(new Instruction({var.operand.name, var.operand.type},
                                                   {},
                                                   {root->v, var.operand.type},
                                                   Operator::fmov));
                                                   std::cout<<"fmov-2217-varop_type"<< toString(var.operand.type)<<std::endl;
            break;
        case ir::Type::Int:
                    root->t = var.operand.type;
            root->v = get_temp_name();
            instructions.push_back(new Instruction({var.operand.name, var.operand.type},
                                                   {},
                                                   {root->v, var.operand.type},
                                                   Operator::mov));
                                                   std::cout<<"mov-2226-varop_type"<< toString(var.operand.type)<<std::endl;
            break;
        default:
            root->t = var.operand.type;
            root->v = get_temp_name();
            instructions.push_back(new Instruction({var.operand.name, var.operand.type},
                                                   {},
                                                   {root->v, var.operand.type},
                                                   Operator::mov));
                                                   std::cout<<"mov-2235-varop_type"<< toString(var.operand.type)<<std::endl;
            break;
        }
    }
    else // 如果有下标
    {
        std::vector<Operand> load_index;
        for (size_t index = 2; index < root->children.size(); index += 3)
        {
            if (!(NODE_IS(EXP, index)))
                break;

            GET_CHILD_PTR(exp, Exp, index);
            analysisExp(exp, instructions);
            load_index.push_back({exp->v, exp->t});
        }

        auto res_index = Operand{get_temp_name(), ir::Type::Int};
        instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                               {},
                                               res_index,
                                               Operator::def));std::cout<<"def-2239 "<<std::endl;
        for (size_t i = 0; i < load_index.size(); i++)
        {
            int mul_dim = std::accumulate(var.dimension.begin() + i + 1, var.dimension.end(), 1, std::multiplies<int>());
            auto temp_name = get_temp_name();
            instructions.push_back(new Instruction(load_index[i],
                                                   {std::to_string(mul_dim), ir::Type::IntLiteral},
                                                   {temp_name, ir::Type::Int},
                                                   Operator::mul));
            instructions.push_back(new Instruction(res_index,
                                                   {temp_name, ir::Type::Int},
                                                   res_index,
                                                   Operator::add));
        }

        if (is_left)
        {
            // store:存数指令，op1:数组名，op2:下标，des:存放变量
            instructions.push_back(new Instruction(var.operand,
                                                   res_index,
                                                   {root->v, root->t},
                                                   Operator::store));
        }
        else
        {
            root->t = (var.operand.type == ir::Type::IntPtr) ? ir::Type::Int : ir::Type::Float;
            // load:取数指令，op1:数组名，op2:下标，des:存放变量
            instructions.push_back(new Instruction(var.operand,
                                                   res_index,
                                                   {root->v, root->t},
                                                   Operator::load));
        }
    }
}

// Number -> IntConst | floatConst
// Number.is_computable = true;
// Number.v
// Number.t
void frontend::Analyzer::analysisNumber(Number *root, vector<Instruction *> &instructions)
{
     std::cout<<"analysisNumber "<<std::endl;
    root->is_computable = true;

    GET_CHILD_PTR(term, Term, 0);
    
/*struct Term: AstNode {
    Token token;

    Term(Token t, AstNode* p = nullptr);
};*/
            
       if (root->t == ir::Type::Float){
          std::cout<<"Number-root->t = ir::Type::Float;"<<std::endl;
        }
            
        else if (root->t == ir::Type::Int)
        {
             std::cout<<"Number-root->t = ir::Type::Int;"<<std::endl;
        }else if (root->t == ir::Type::IntLiteral)
        {
             std::cout<<"Number-root->t = ir::Type::IntLiteral;"<<std::endl;
        }else if (root->t == ir::Type::FloatLiteral )
             {
             std::cout<<"Number-root->t = ir::Type::FloatLiteral;"<<std::endl;
        }

    switch (term->token.type)
    {
    case TokenType::INTLTR: // 整数常量
    std::cout<<"analysisNumber-TokenType::INTLTR:"<<std::endl;
         
        if (term->token.value.substr(0, 2) == "0x")
        {
            root->v = std::to_string(std::stoi(term->token.value, nullptr, 16));
        }
        else if (term->token.value.substr(0, 2) == "0b")
        {
            root->v = std::to_string(std::stoi(term->token.value, nullptr, 2));
        }
        else if (term->token.value.substr(0, 1) == "0" && !(term->token.value.substr(0, 2) == "0x") && !(term->token.value.substr(0, 2) == "0b"))
        {
            root->v = std::to_string(std::stoi(term->token.value, nullptr, 8));
        }
        else
        {
            root->v = term->token.value;
        }
    
        if(root->t == Type::FloatLiteral){
             std::cout<<"analysisNumber-root->t = Type::FloatLiteral"<<std::endl;

            // int val1=stoi(root->v);
            // float val0=val1;
            // // root->v=std::to_string(val0);
            //     instructions.push_back(new Instruction({root->v,ir::Type::Int},
            //                                            {},
            //                                            {std::to_string(val0), ir::Type::Float},
            //                                            Operator::cvt_i2f));std::cout<<"cvt_i2f-2314"<<std::endl;
                root->t = ir::Type::FloatLiteral;
            

        }else {
            root->t = Type::IntLiteral;
             std::cout<<"analysisNumber-root->t = Type::IntLiteral"<<std::endl;
            }
            // root->t = Type::IntLiteral;
        break;
    case TokenType::FLOATLTR: // 浮点数常量
       root->t = Type::FloatLiteral;
         std::cout<<"analysisNumber-TokenType::FLOATLTR:"<<std::endl;
        root->v = term->token.value;
        break;
    default:
        break;
    }



//需要进行类型转换



}