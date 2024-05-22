#include"front/lexical.h"

#include<map>
#include<cassert>
#include<string>

#define TODO assert(0 && "todo")

// #define DEBUG_DFA
// #define DEBUG_SCANNER

std::string frontend::toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::Ident: return "Ident";
    case State::IntLiteral: return "IntLiteral";
    case State::FloatLiteral: return "FloatLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

std::set<std::string> frontend::keywords= {
    "const", "int", "float", "if", "else", "while", "continue", "break", "return", "void"
};

bool  isoperator(char  c)  {
        if (c == '+' || c=='-'|| c=='*' || c=='/' || c=='%' || c == '<' || 
        c == '>' || c==':'|| c=='='|| c==';'|| c==','|| c=='('|| c==')' 
        || c=='['|| c==']'|| c=='{'|| c=='}'|| c=='!'|| c=='|'|| c=='&') return true;
         return false;
}

bool is2operator(std::string s){
        if(s == "<=" || s==">=" || s == "==" || s== "!=" || s=="&&" || s=="||") return true;
         return false;
         }
        
bool  isdigit(char  c)  {
        if (c>='0' && c<='9') return true;
        else return false;
}

bool  isletter(char  c)  {
        if ((c>='a' && c<='z')||(c>='A' && c<='Z') || c=='_') return true;
        return false;
}

frontend::TokenType  get_op_type(std::string  s)  {
        if(s == "+") return frontend::TokenType::PLUS;
        else if(s == "-") return frontend::TokenType::MINU;
        else if(s == "*") return frontend::TokenType::MULT;
        else if(s == "/") return frontend::TokenType::DIV;
        else if(s == "%") return frontend::TokenType::MOD;
        else if(s == "<") return frontend::TokenType::LSS;
        else if(s == ">") return frontend::TokenType::GTR;
        else if(s == ":") return frontend::TokenType::COLON;
        else if(s == "=") return frontend::TokenType::ASSIGN;
        else if(s == ";") return frontend::TokenType::SEMICN;
        else if(s == ",") return frontend::TokenType::COMMA;
        else if(s == "(") return frontend::TokenType::LPARENT;
        else if(s == ")") return frontend::TokenType::RPARENT;
        else if(s == "[") return frontend::TokenType::LBRACK;
        else if(s == "]") return frontend::TokenType::RBRACK;
        else if(s == "{") return frontend::TokenType::LBRACE;
        else if(s == "}") return frontend::TokenType::RBRACE;
        else if(s == "!") return frontend::TokenType::NOT;
        else if(s == "<=") return frontend::TokenType::LEQ;
        else if(s == ">=") return frontend::TokenType::GEQ;
        else if(s == "==") return frontend::TokenType::EQL;
        else if(s == "!=") return frontend::TokenType::NEQ;
        else if(s == "&&") return frontend::TokenType::AND;
        else if(s == "||") return frontend::TokenType::OR;
        return frontend::TokenType::INTLTR;
}

// indent string to tokenType
frontend::TokenType  get_indent_type(std::string  s)  {
        if(s == "const") return frontend::TokenType::CONSTTK;
        else if(s == "void") return frontend::TokenType::VOIDTK;
        else if(s == "int") return frontend::TokenType::INTTK;
        else if(s == "float") return frontend::TokenType::FLOATTK;
        else if(s == "if") return frontend::TokenType::IFTK;
        else if(s == "else") return frontend::TokenType::ELSETK;
        else if(s == "while") return frontend::TokenType::WHILETK;
        else if(s == "continue") return frontend::TokenType::CONTINUETK;
        else if(s == "break") return frontend::TokenType::BREAKTK;
        else if(s == "return") return frontend::TokenType::RETURNTK;
        return frontend::TokenType::IDENFR;
}


frontend::DFA::DFA(): cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token& buf) {
    bool flag = false; // 控制返回值

    switch (cur_state) {
        case frontend::State::Empty:
            if (isspace(input)) {
                reset();
            } else if (isalpha(input) || input == '_') {
                cur_state = frontend::State::Ident;
                cur_str += input;
            } else if (isdigit(input)) {
                cur_state = frontend::State::IntLiteral;
                cur_str += input;
            } else if (input == '.') {
                cur_state = frontend::State::FloatLiteral;
                cur_str += input;
            } else if (isoperator(input)) {
                cur_state = frontend::State::op;
                cur_str += input;
            } else {
                assert(0 && "invalid next State");
            }
            break;

        case frontend::State::Ident:
            if (isspace(input)) {
                buf.type = get_indent_type(cur_str);
                buf.value = cur_str;
                reset();
                flag = true;
            } else if (isalpha(input) || isdigit(input) || input == '_') {
                cur_str += input;
            } else if (isoperator(input)) {
                buf.type = get_indent_type(cur_str);
                buf.value = cur_str;
                cur_state = frontend::State::op;
                cur_str = input;
                flag = true;
            } else {
                assert(0 && "invalid next State");
            }
            break;

        case frontend::State::op:
            if (isspace(input)) {
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;
                reset();
                flag = true;
            } else if (isalpha(input) || input == '_') {
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;
                cur_state = frontend::State::Ident;
                cur_str = input;
                flag = true;
            } else if (isdigit(input)) {
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;
                cur_state = frontend::State::IntLiteral;
                cur_str = input;
                flag = true;
            } else if (input == '.') {
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;
                cur_state = frontend::State::FloatLiteral;
                cur_str = input;
                flag = true;
            } else if (isoperator(input)) {
                if (is2operator(cur_str + input)) {
                    cur_str += input;
                } else {
                    buf.type = get_op_type(cur_str);
                    buf.value = cur_str;
                    cur_state = frontend::State::op;
                    cur_str = input;
                    flag = true;
                }
            } else {
                assert(0 && "invalid next State");
            }
            break;

        case frontend::State::IntLiteral:
            if (isspace(input)) {
                buf.type = frontend::TokenType::INTLTR;
                buf.value = cur_str;
                reset();
                flag = true;
            } else if (isdigit(input) || (input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F') || input == 'x' || input == 'X') {
                cur_str += input;
            } else if (input == '.') {
                cur_state = frontend::State::FloatLiteral;
                cur_str += input;
            } else if (isoperator(input)) {
                buf.type = frontend::TokenType::INTLTR;
                buf.value = cur_str;
                cur_state = frontend::State::op;
                cur_str = input;
                flag = true;
            } else {
                assert(0 && "invalid next State");
            }
            break;

        case frontend::State::FloatLiteral:
            if (isspace(input)) {
                buf.type = frontend::TokenType::FLOATLTR;
                buf.value = cur_str;
                reset();
                flag = true;
            } else if (isdigit(input)) {
                cur_str += input;
            } else if (isoperator(input)) {
                buf.type = frontend::TokenType::FLOATLTR;
                buf.value = cur_str;
                cur_state = frontend::State::op;
                cur_str = input;
                flag = true;
            } else {
                assert(0 && "invalid next State");
            }
            break;

        default:
            assert(0 && "invalid State");
    }

    return flag;
}

void frontend::DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

frontend::Scanner::Scanner(std::string filename): fin(filename) {
    if(!fin.is_open()) {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner() {
    fin.close();
}
std::string preprocess(std::ifstream& fin) {  std::string result;
    std::string line;
    bool inMultiLineComment = false;    // 当前位于多行注释内部
    while (std::getline(fin, line)) {
        size_t pos = 0;
        if (inMultiLineComment) {       // 在多行注释内部时需要查找结束位置
            pos = line.find("*/");
            if (pos != std::string::npos) {
                inMultiLineComment = false;
                line.erase(0, pos + 2); // 移除"*/"前的内容
            } else {
                continue;               // 跳过当前行，继续读取下一行
            }
        }
        while (pos != std::string::npos) {
            // 查找单行注释和多行注释的起始位置
            size_t posSingle = line.find("//", pos);
            size_t posMulti = line.find("/*", pos);
            if (posSingle == pos) {     // 单行注释在本行内
                line.erase(pos);        // 移除"//"后的所有内容
                break;                  // 不再查找其他注释
            }
            else if (posMulti == pos) { // 多行注释在本行内
                size_t posMultiEnd = line.find("*/", posMulti + 2);     // 查找多行注释结束位置
                if (posMultiEnd != std::string::npos) {
                    line.erase(posMulti, posMultiEnd - posMulti + 2);   // 移除多行注释
                }
                else {
                    line.erase(posMulti);           // 移除多行注释后的内容
                    inMultiLineComment = true;
                }
                break;                  // 不再查找其他注释
            }
            pos = std::min(posSingle, posMulti);    // 更新 pos，查找下一个注释的起始位置
        }
        result += line+"\n";            // 将处理后的行添加到结果字符串中
    }
    return result;
}
std::vector<frontend::Token> frontend::Scanner::run() {
        std::vector<Token> ret;
        Token tk;
        DFA dfa;
        std::string s = preprocess(fin); // delete comments
        for (auto c : s) {
            if (dfa.next(c, tk)) {
                ret.push_back(tk);
            }
        }
        return ret;
#ifdef DEBUG_SCANNER
#include<iostream>
            std::cout << "token: " << toString(tk.type) << "\t" << tk.value << std::endl;
#endif
}