#include"front/syntax.h"

#include<iostream>
#include<cassert>

using frontend::Parser;

// #define DEBUG_PARSER
#define TODO assert(0 && "todo")
#define CUR_TOKEN_IS(tk_type) (token_stream[index].type == TokenType::tk_type)
//根据下一个 Token 类型的类型选择处理的产生式
#define PARSE_TOKEN(tk_type) root->children.push_back(parseTerm(root, TokenType::tk_type))
//如果是终结符，则调用 pasrseTerm 函数，并将其挂在 root 节点上
#define PARSE(name, type) auto name = new type(root); assert(parse##type(name)); root->children.push_back(name);
//如果是非终结符，则调用其 pasrse 函数，并将其挂在 root 节点上

Parser::Parser(const std::vector<frontend::Token>& tokens): index(0), token_stream(tokens) {}

Parser::~Parser() {}

frontend::CompUnit* Parser::get_abstract_syntax_tree(){
    // main function
    frontend::CompUnit* root = new frontend::CompUnit();
    parseCompUnit(root);
    return root;
}
/**
     * @brief recursive descent parsing to build a tree 
     * @param root: root node 
*/
//1.CompUnit -> (Decl | FuncDef) [CompUnit]
bool Parser::parseCompUnit(CompUnit* root){
    log(root);

    //Decl [CompUnit]
    if(CUR_TOKEN_IS(CONSTTK)){
        PARSE(decl,Decl);
    }
    //Decl [CompUnit] 需要向前看两位
    else if((CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) && index+2 < token_stream.size() 
    && !(token_stream[index+2].type == TokenType::LPARENT)){
    PARSE(decl,Decl);
}

    //FuncDef [CompUnit]
    else{
        PARSE(funcdef,FuncDef);
    }
    //(Decl | FuncDef) CompUnit
    if(CUR_TOKEN_IS(CONSTTK) || CUR_TOKEN_IS(VOIDTK)||CUR_TOKEN_IS(INTTK)||CUR_TOKEN_IS(FLOATTK)){
        PARSE(compunit,CompUnit);
    }
    return true;
}
//2.Decl -> ConstDecl | VarDecl
bool Parser::parseDecl(Decl* root){
    log(root);
    // ConstDecl 
    if(CUR_TOKEN_IS(CONSTTK)){
        PARSE(constdecl,ConstDecl);
    }
    // varDecl
    else{
        PARSE(vardecl,VarDecl);
    }
    return true;
}
//3.FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
bool Parser::parseFuncDef(FuncDef* root) {
    log(root);

    PARSE(functype, FuncType);
    PARSE_TOKEN(IDENFR);
    PARSE_TOKEN(LPARENT);

    // no [FuncFParams], FuncType Ident '(' ')' Block
    if(CUR_TOKEN_IS(RPARENT)) {
        PARSE_TOKEN(RPARENT);
    }
    // FuncType Ident '(' FuncFParams ')' Block
    else {
        PARSE(funcfparams, FuncFParams);
        PARSE_TOKEN(RPARENT);
    }

    PARSE(block, Block);
    return true;
}

//4.ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
bool Parser::parseConstDecl(ConstDecl* root){
    log(root);
    // 'const' BType ConstDef
    PARSE_TOKEN(CONSTTK);
    PARSE(btype,BType);
    PARSE(constdef,ConstDef);
    // 'const' BType ConstDef { ',' ConstDef } 
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        PARSE(constdef,ConstDef);
    }
    // 'const' BType ConstDef { ',' ConstDef } ';'
    PARSE_TOKEN(SEMICN);
    return true;
}
//5.BType -> 'int' | 'float'
bool Parser::parseBType(BType* root){
    log(root);
    // 'int'
    if(CUR_TOKEN_IS(INTTK)){
        PARSE_TOKEN(INTTK);
    }
    // 'float'
    else{
        PARSE_TOKEN(FLOATTK);
    }
    return true;
}
//6.ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
bool Parser::parseConstDef(ConstDef* root){
    log(root);
    // Ident
    PARSE_TOKEN(IDENFR);
    // Ident { '[' ConstExp ']' }
    while(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        PARSE(constexp,ConstExp);
        PARSE_TOKEN(RBRACK);
    }
    PARSE_TOKEN(ASSIGN);
    PARSE(constinitval,ConstInitVal);
    return true;
}
//7.ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
bool Parser::parseConstInitVal(ConstInitVal* root){
    log(root);
    // '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
    if(CUR_TOKEN_IS(LBRACE)){
        PARSE_TOKEN(LBRACE);
        // '{'  '}'
        if(CUR_TOKEN_IS(RBRACE)){
            PARSE_TOKEN(RBRACE);
        }
        // '{'  ConstInitVal { ',' ConstInitVal }  '}'
        else{
            PARSE(constinitval,ConstInitVal);
            // { ',' ConstInitVal }
            while(CUR_TOKEN_IS(COMMA)){
                PARSE_TOKEN(COMMA);
                PARSE(constinitval,ConstInitVal);
            }
            PARSE_TOKEN(RBRACE);
        }
        // PARSE_TOKEN(RBRACE);
    }
    else{
        PARSE(constexp,ConstExp);
    }
    return true;
}
//8.VarDecl -> BType VarDef { ',' VarDef } ';'
bool Parser::parseVarDecl(VarDecl* root){
    log(root);
    PARSE(btype,BType);
    PARSE(vardef,VarDef);
    // { ',' VarDef }
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        PARSE(vardef,VarDef);
    }
    PARSE_TOKEN(SEMICN);
    return true;
}
//9.VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
bool Parser::parseVarDef(VarDef* root){
    log(root);
    PARSE_TOKEN(IDENFR);
    // { '[' ConstExp ']' }
    while(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        PARSE(constexp,ConstExp);
        PARSE_TOKEN(RBRACK);
    }
    // [ '=' InitVal ]
    if(CUR_TOKEN_IS(ASSIGN)){
        PARSE_TOKEN(ASSIGN);
        PARSE(initval,InitVal);
    }
    return true;
}
//10.InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
bool Parser::parseInitVal(InitVal* root){
    log(root);
    // '{' [ InitVal { ',' InitVal } ] '}'
    if(CUR_TOKEN_IS(LBRACE)){
        PARSE_TOKEN(LBRACE);
        // '{'  '}'
        if(CUR_TOKEN_IS(RBRACE)){
            PARSE_TOKEN(RBRACE);
        }
        // '{'  InitVal { ',' InitVal }  '}'
        else{
           PARSE(initval,InitVal);
        //    { ',' InitVal }
           while(CUR_TOKEN_IS(COMMA)) {
               PARSE_TOKEN(COMMA); 
               PARSE(initval,InitVal);
           }
           PARSE_TOKEN(RBRACE);
        }
    }
    // Exp
    else{
        PARSE(exp,Exp);
    }
    return true;

}
//11.FuncType -> 'void' | 'int' | 'float'
bool Parser::parseFuncType(FuncType* root){
    log(root);
    //'void'
    if(CUR_TOKEN_IS(VOIDTK)){
        PARSE_TOKEN(VOIDTK);
    }
    //'int'
    else if(CUR_TOKEN_IS(INTTK)){
        PARSE_TOKEN(INTTK);
    }
    //'float'
    else{
        PARSE_TOKEN(FLOATTK);
    }
    return true;
}
//12.FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
bool Parser::parseFuncFParam(FuncFParam* root){
    log(root);

    PARSE(btype,BType);
    PARSE_TOKEN(IDENFR);
    // BType Ident '[' ']' { '[' Exp ']' }
    if(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        PARSE_TOKEN(RBRACK);
        // { '[' Exp ']' }
        while(CUR_TOKEN_IS(LBRACK)){
            PARSE_TOKEN(LBRACK);
            PARSE(exp,Exp);
            PARSE_TOKEN(RBRACK);
        }
    }
    return true;
}
//13.FuncFParams -> FuncFParam { ',' FuncFParam }
bool Parser::parseFuncFParams(FuncFParams* root){
    log(root);
    
    PARSE(funcfparam,FuncFParam);
    // { ',' FuncFParam }
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        PARSE(funcfparam,FuncFParam);
    }
    return true;
}
//14.Block -> '{' { BlockItem } '}'
bool Parser::parseBlock(Block* root){
    log(root);

    PARSE_TOKEN(LBRACE);
    // { BlockItem }
    while(!CUR_TOKEN_IS(RBRACE)){
        PARSE(blockitem,BlockItem);
    }
    PARSE_TOKEN(RBRACE);
    return true;
}
//15.BlockItem -> Decl | Stmt
bool Parser::parseBlockItem(BlockItem* root){
    log(root);  
    // Decl
    if(CUR_TOKEN_IS(CONSTTK) || CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)){
        PARSE(decl,Decl);
    }
    // Stmt
    else{
        PARSE(stmt,Stmt);
    }
    return true;
}
//16.Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
bool Parser::parseStmt(Stmt* root){
    log(root);
    // LVal '=' Exp ';'
    if(CUR_TOKEN_IS(IDENFR) && index+1 < token_stream.size() && !(token_stream[index+1].type == TokenType::LPARENT)){
        //if(CUR_TOKEN_IS(IDENFR) && index+1 < token_stream.size() && !token_stream[index+1].type == TokenType::LPARENT)
        PARSE(Lval,LVal);
        PARSE_TOKEN(ASSIGN);
        PARSE(exp,Exp);
        PARSE_TOKEN(SEMICN);
    }
    // Block
    else if(CUR_TOKEN_IS(LBRACE)){
        PARSE(block,Block);
    }
    // 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
    else if(CUR_TOKEN_IS(IFTK)){
        PARSE_TOKEN(IFTK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond,Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt,Stmt);
        // 'if' '(' Cond ')' Stmt  'else' Stmt 
        if(CUR_TOKEN_IS(ELSETK)){
            PARSE_TOKEN(ELSETK);
            PARSE(stmt,Stmt);
        }
    }
    // 'while' '(' Cond ')' Stmt
    else if(CUR_TOKEN_IS(WHILETK)){
        PARSE_TOKEN(WHILETK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond,Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt,Stmt);
    }
    // 'break' ';'
    else if(CUR_TOKEN_IS(BREAKTK)){
        PARSE_TOKEN(BREAKTK);
        PARSE_TOKEN(SEMICN);
    }
    // 'continue' ';'
    else if(CUR_TOKEN_IS(CONTINUETK)){
        PARSE_TOKEN(CONTINUETK);
        PARSE_TOKEN(SEMICN);
    }
    // 'return' [Exp] ';'
    else if(CUR_TOKEN_IS(RETURNTK)){
        PARSE_TOKEN(RETURNTK);
        // 'return' ';'
        if(CUR_TOKEN_IS(SEMICN)){
            PARSE_TOKEN(SEMICN);
        }
        // 'return' Exp ';'
        else{
            PARSE(exp,Exp);
            PARSE_TOKEN(SEMICN);
        }
    }
    // ';'
    else if(CUR_TOKEN_IS(SEMICN)){
        PARSE_TOKEN(SEMICN);
    }
    // Exp ';'
    else{
        PARSE(exp,Exp);
        PARSE_TOKEN(SEMICN);
    }
    return true;
}
//17.Exp -> AddExp
bool Parser::parseExp(Exp* root){
    log(root);
    PARSE(addexp,AddExp);
    return true;
}
//18.Cond -> LOrExp
bool Parser::parseCond(Cond* root){
    log(root);
    PARSE(lorexp,LOrExp);
    return true;
}
//19.LVal -> Ident {'[' Exp ']'}
bool Parser::parseLVal(LVal* root){
    log(root);
    PARSE_TOKEN(IDENFR);
    // {'[' Exp ']'}
    while(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        PARSE(exp,Exp);
        PARSE_TOKEN(RBRACK);
    }
    return true;
}
//20.Number -> IntConst | floatConst
bool Parser::parseNumber(Number* root){
    log(root);
    //IntConst
    if(CUR_TOKEN_IS(INTLTR)){
        PARSE_TOKEN(INTLTR);
    }
    //floatConst
    else{
        PARSE_TOKEN(FLOATLTR);
    }
    return true;
}
//21.PrimaryExp -> '(' Exp ')' | LVal | Number
bool Parser::parsePrimaryExp(PrimaryExp* root){
    log(root);
    // '(' Exp ')'
    if(CUR_TOKEN_IS(LPARENT)){
        PARSE_TOKEN(LPARENT);
        PARSE(exp,Exp);
        PARSE_TOKEN(RPARENT);
    }
    // LVal
    else if(CUR_TOKEN_IS(IDENFR)){
        PARSE(lval,LVal);
    }
    //  Number
    else{
        PARSE(number,Number);
    }
    return true;
}
//22.UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
bool Parser::parseUnaryExp(UnaryExp* root){
    log(root);
    // Ident '(' [FuncRParams] ')'
    if(CUR_TOKEN_IS(IDENFR) && index+1 < token_stream.size()&&(token_stream[index+1].type == TokenType::LPARENT)){
        PARSE_TOKEN(IDENFR);
        PARSE_TOKEN(LPARENT);
        // Ident '('  ')'
        if(CUR_TOKEN_IS(RPARENT)){
            PARSE_TOKEN(RPARENT);
        }
        // Ident '(' FuncRParams ')'
        else{
            PARSE(funcrparams,FuncRParams);
            PARSE_TOKEN(RPARENT);
        }
    }
    // UnaryOp UnaryExp
    else if(CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU) || CUR_TOKEN_IS(NOT)){
        PARSE(unaryop,UnaryOp);
        PARSE(unaryexp,UnaryExp);
    }
    // PrimaryExp
    else{
        PARSE(primaryexp,PrimaryExp);
    }
    return true;
}
//23.UnaryOp -> '+' | '-' | '!'
bool Parser::parseUnaryOp(UnaryOp* root){
    log(root);
    //'+'
    if(CUR_TOKEN_IS(PLUS)){
        PARSE_TOKEN(PLUS);
    }
    //'-'
    else if(CUR_TOKEN_IS(MINU)){
        PARSE_TOKEN(MINU);
    }
    // '!'
    else{
      PARSE_TOKEN(NOT);  
    }
    return true;
}
//24.FuncRParams -> Exp { ',' Exp }
bool Parser::parseFuncRParams(FuncRParams* root){
    log(root);
    PARSE(exp,Exp);
    // { ',' Exp }
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        PARSE(exp,Exp);
    }
    return true;
}
//25.MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
bool Parser::parseMulExp(MulExp* root){
    log(root);
    PARSE(unaryexp,UnaryExp);
    // { ('*' | '/' | '%') UnaryExp }
    while(CUR_TOKEN_IS(MULT) || CUR_TOKEN_IS(DIV) || CUR_TOKEN_IS(MOD)){
        // '*' UnaryExp
        if(CUR_TOKEN_IS(MULT)){
            PARSE_TOKEN(MULT);
        }
        // '/' UnaryExp
        else if(CUR_TOKEN_IS(DIV)){
            PARSE_TOKEN(DIV);
        }
        // '%' UnaryExp
        else{
            PARSE_TOKEN(MOD);
        }
        PARSE(unaryexp,UnaryExp);
    }
    return true;
}
//26.AddExp -> MulExp { ('+' | '-') MulExp }
bool Parser::parseAddExp(AddExp* root){
    log(root);
    PARSE(mulexp,MulExp);
    // { ('+' | '-') MulExp }
    while(CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)){
        // '+' MulExp
        if(CUR_TOKEN_IS(PLUS)){
            PARSE_TOKEN(PLUS);
        }
        // '-' MulExp
        else{
            PARSE_TOKEN(MINU);  
        }
        PARSE(mulexp,MulExp);
    }
    return true;
}
//27.RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
bool Parser::parseRelExp(RelExp* root){
    log(root);
    PARSE(addexp,AddExp);
    // { ('<' | '>' | '<=' | '>=') AddExp }
    while(CUR_TOKEN_IS(LSS) || CUR_TOKEN_IS(GTR) || CUR_TOKEN_IS(LEQ) || CUR_TOKEN_IS(GEQ)){
        // '<' AddExp
        if(CUR_TOKEN_IS(LSS)){
            PARSE_TOKEN(LSS);
        }
        // '>' AddExp
        else if(CUR_TOKEN_IS(GTR)){
            PARSE_TOKEN(GTR);
        }
        // '<=' AddExp
        else if(CUR_TOKEN_IS(LEQ)){
            PARSE_TOKEN(LEQ);
        }
        // '>=' AddExp
        else{
            PARSE_TOKEN(GEQ);
        }
        PARSE(addexp,AddExp);
    }
    return true;
}
//28.EqExp -> RelExp { ('==' | '!=') RelExp }
bool Parser::parseEqExp(EqExp* root){
    log(root);
    PARSE(relexp,RelExp);
    // { ('==' | '!=') RelExp }
    while(CUR_TOKEN_IS(EQL) || CUR_TOKEN_IS(NEQ)){
        // '==' RelExp
        if(CUR_TOKEN_IS(EQL)){
            PARSE_TOKEN(EQL);
        }
        // '!=' RelExp
        else {
            PARSE_TOKEN(NEQ);
        }
        PARSE(relexp,RelExp);
    }
    return true;
}
//29.LAndExp -> EqExp [ '&&' LAndExp ]
bool Parser::parseLAndExp(LAndExp* root){
    log(root);
    PARSE(eqexp,EqExp);
    //  '&&' LAndExp 
    if(CUR_TOKEN_IS(AND)){
        PARSE_TOKEN(AND);
        PARSE(landexp,LAndExp);
    }
    return true;
}
//30.LOrExp -> LAndExp [ '||' LOrExp ]
bool Parser::parseLOrExp(LOrExp* root){
    log(root);
    PARSE(landexp,LAndExp);
    //  '||' LOrExp 
    if(CUR_TOKEN_IS(OR)){
        PARSE_TOKEN(OR);
        PARSE(lorexp,LOrExp);
    }
    return true;
}
//31.ConstExp -> AddExp
bool Parser::parseConstExp(ConstExp* root){
    log(root);
    PARSE(addexp,AddExp);
    return true;
}
//32.Term
frontend::Term* Parser::parseTerm(AstNode* parent, TokenType expected) {
    if(token_stream[index].type != expected){
        throw std::runtime_error("Unexpected token or end of stream. Expected: " + toString(expected) + ", got: " + toString(token_stream[index].type));
    }
    frontend::Term* term = new frontend::Term(token_stream[index], parent);
    index++;
    return term;
}


void Parser::log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
}
