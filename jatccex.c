#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdint.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#if (defined _MSC_VER) && (defined _WIN64) || (defined __LP64__)
#define int int64_t
#endif


// 符号表项：支持struct后用struct替换数组实现。
struct Symbol_item
{
    int token;      // 标记，值应该是Token_type类型的，除了关键字之外，一定是Id。
    int hash;       // 根据名称计算出的一个哈希值，加速查找，不需要每次都去遍历名字比较。
    char *name;     // 名称，计算出哈希值之后就不需要用它了，指向源文件某位置的char*指针。
    int class;      // 标识符的类型，Id类型的token才需要，值为Identifier_type中枚举。
    int type;       // 标识符的变量类型或者函数返回值类型，值为Var_type枚举中普通类型与PTR组合得到的值。
    int value;      // 标识符的值。如果标识符是函数，则是函数地址，如果是变量就是相对/绝对地址，如果是枚举常量则是具体的值，标号的话就是地址，自定义类型不使用。
    int gclass, gtype, gvalue;  // 同class,type,value用于全局作用域覆盖局部作用域时存放全局符号。
};

// break和continue列表项
struct Bc_list_item
{
    int *loop;              // 循环入口地址，仅用于唯一标识一个循环
    int *bc_address;        // 需要填充的code段指令中的break或者continue跳转地址的地址
};

// goto列表项
struct Label_list_item
{
    int label_hash;         // 标号的哈希
    int line;               // goto语句的行号
    int *goto_address;      // 需要填充的code段指令中标号地址的地址
};

// 尚未定义的函数调用列表
struct Func_call_item
{
    int hash;               // 函数名哈希
    int line;               // 函数调用的行号
    int *call_addresss;     // 需要填充的code段指令中函数跳转地址的地址
};

// VM
char *data;         // 数据段
int *code,          // 代码段
    *stack;         // 运行栈
int *pc, *sp, *bp, ax, cycle; // 寄存器
int poolsize;       // 各个段分配内存大小

// tokenizer
int token;          // 当前token
int token_val;      // 当前token是常量或字符串字面值时用来记录值
char *src;          // 源码
int line;           // 行号
int last_token;     // 支持记录和回溯token流的功能
int last_token_val; // 支持记录和回溯token流的功能
char* last_src;     // 支持记录和回溯token流的功能
int last_line;      // 支持记录和回溯token流的功能

// parser
struct Symbol_item *symbols;        // 符号表
struct Symbol_item *idmain;         // main函数的符号表记录
struct Symbol_item *current_id;     // 当前标识符的符号表记录
int basetype;                       // 变量、函数和类型定义的基本类型，是指针类型时使用
int expr_type;                      // 表达式类型
int index_of_bp;                    // 函数调用时第一个参数相对bp的位置，函数的参数数量+1
struct Bc_list_item *break_list;    // break语句跳转地址的列表
struct Bc_list_item *continue_list; // continue语句跳转地址列表
int *cur_loop;                      // 保存当前正在解析的循环的地址，用来唯一标识一个循环，for break & continue
struct Label_list_item *label_list; // goto语句跳转地址列表
struct Func_call_item *func_list;   // 待填充的函数调用列表

// debug
int debug;                  // 调试模式
int *last_code;             // 上一次打印至的code段指针

// union or struct domain
struct us_domain
{
    int hash;               // hash of type name or var name
    int type;               // type of user defined struct or domain
    int size;               // size of type or domain
    int offset;             // offset in struct of domain
    struct us_domain* next; // first domain for type or next domain for domain, indicate whether a struct/union has been already defined or not.
};

// 结构体和联合体信息的列表，每个元素都是一个链表的表头，存储struct和union的信息，链表中余下的节点按顺序存储它的域
struct us_domain *struct_symbols_list;
struct us_domain *union_symbols_list;
struct us_domain *us_domains_list;      // 集中存储链表上述两个列表中链表的节点，用malloc单独分配链表节点的话给人感觉就不整体了，不是必要

int cur_struct_type;        // 当前struct类型
int cur_union_type;         // 当前union类型


/*
变量类型，如果是char*则表示为CHAR + PTR，char**则表示为CHAR + PTR + PTR。
枚举最终解释为整型常量，不支持命名枚举。
用最终类型模PTR得到基本类型，整除PTR则是指针层数，只存在指针这一种复合类型情况下很取巧的做法。
union的类型从100开始，struct从500开始，每定义一个类型就+1,各自减去UNION/STRUCT就是他们在
struct_symbols_list/union_symbols_list中的下标。
*/
enum Var_type { CHAR = 0, INT, ENUM, UNION = 100, STRUCT = 500, PTR = 1000 };

/*
标识token的类型，已经能够用单个ASCII表示的某些token其实没有包含在里面，如!~()，这也是从128开始的原因。
*/
enum Token_type
{
    Num = 128,      // number
    Id,             // identifier
    // keywords in lexicographic order
    Break,          // break
    Char,           // char
    Continue,       // Continue
    Do,             // do
    Else,           // else
    Enum,           // enum
    For,            // For
    Goto,           // goto
    If,             // if
    Int,            // int
    Return,         // return
    Sizeof,         // sizeof
    Struct,         // struct
    Union,          // union
    While,          // while
    // operators in precedence order
    Comma,          // ,
    Assign,         // =
    Cond,           // ?
    Lor,            // ||
    Land,           // &&
    Or,             // |
    Xor,            // ^
    And,            // &
    Eq, Ne,         // == !=
    Lt, Le, Gt, Ge, // < <= > >=
    Shl, Shr,       // << >>
    Add, Sub,       // + -
    Mul, Div, Mod,  // * / %
    Inc, Dec,       // ++ --
    Brak, Dot, Gmbp // [] . -> (get member by pointer)
};

// 标识符的类型，用在符号表中的Class域
enum Identifier_type
{
    EnumVal = 200,  // constant enum value
    Fun,            // function
    Sys,            // system call (native-call)
    Glo,            // global variables
    Loc,            // local variables
    EnumType,       // user defined enum type
    UnionType,      // user defined union type
    StructType,     // user defined struct type
    Label           // label of goto
};

// 指令操作码，最多一个操作数
enum Instruction
{
    LEA = 100, IMM, JMP, JSR, JZ, JNZ, ENT, ADJ, // 1个操作数，剩余的都没有操作数
    LEV, LI, LC, SI, SC, PUSH,
    OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
    OPEN, READ, CLOS, WRIT, PRTF, MALC, FREE, MSET, MCMP, MCPY, EXIT
};


/*
做词法分析（tokenize/lex）：从源码获取到下一个token，得到这个token的类型和值。
*/
void next()
{
    int op;
    char *last_pos;
    int hash;
    while (token = *src++)
    {
        if (token == '\n')
        {
            // 调试模式下，如果生成了代码的话，按行打印出中间生成的代码
            if (debug && (last_code <= code))
            {
                printf("line %d:\n", line);
                while (last_code <= code)
                {
                    op = *last_code;
                    printf("0x%.10X: %.4s", (int)last_code++, &"LEA ,IMM ,JMP ,JSR ,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                        "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                        "OPEN,READ,CLOS,WRIT,PRTF,MALC,FREE,MSET,MCMP,MCPY,EXIT,"[(op - LEA) * 5]);
                    if (op >= JMP && op <= JNZ)
                        printf("0x%.10X\n", *last_code++);
                    else if (op <= ADJ)
                        printf("%d\n", *last_code++);
                    else
                        printf("\n");
                }
                last_code = code + 1;
            }
            line++;
        }
        // 不支持宏和预编译指令，遇到#直接视为行注释
        else if (token == '#')
        {
            while (*src != '\0' && *src != '\n')
            {
                src++;
            }
        }
        // 标识符
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || token == '_')
        {
            last_pos = src - 1;
            hash = token;
            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || *src == '_' || (*src >= '0' && *src <= '9'))
            {
                hash = hash * 147 + *src; // 计算字符串的哈希值，用来唯一标识一个标识符，假定不会发生哈希冲突
                src++;
            }
            // 查找已有标识符
            current_id = symbols;
            while (current_id->token)
            {
                if (current_id->hash == hash && !memcmp(current_id->name, last_pos, src - last_pos))
                {
                    // 找到了为已有的标识符
                    token = current_id->token;
                    return;
                }
                current_id++;
            }
            // 在符号表中保存新的标识符
            token = current_id->token = Id;
            current_id->hash = hash;
            current_id->name = last_pos;
            return;
        }
        // 整数字面值，三种类型：0123八进制，123十进制，0x123十六进制
        else if (token >= '0' && token <= '9')
        {
            token_val = token - '0';
            // 十进制
            if (token_val > 0)
            {
                while (*src >= '0' && *src <= '9')
                {
                    token_val = token_val * 10 + (*src - '0');
                    src++;
                }
            }
            else
            {
                // 十六进制
                if (*src == 'x' || *src == 'X')
                {
                    token = *++src;
                    while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || token >= 'A' && token <= 'F')
                    {
                        token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0); // '0'~48,'A'~65,'a'~97
                        token = *++src;
                    }
                }
                // 八进制
                else
                {
                    while (*src >= '0' && *src <= '7')
                    {
                        token_val = token_val * 8 + *src - '0';
                        src++;
                    }
                }
            }
            token = Num;
            return;
        }
        // 字符串和字符字面量，仅支持\n转义，其他转义暂时不进行支持，字面量将存储到data区
        else if (token == '"' || token == '\'')
        {
            last_pos = data;
            while (*src != 0 && *src != token)
            {
                token_val = *src++;
                // 处理转义字符\n，不匹配\n的反斜杠忽略，直接表示其下一个字符
                if (token_val == '\\')
                {
                    token_val = *src++;
                    if (token_val == 'n')
                    {
                        token_val = '\n';
                    }
                }

                if (token == '"')
                {
                    *data++ = token_val; // 字符串末尾补\0不在这里做
                }
            }

            src++;
            if (token == '"')
            {
                token_val = (int)last_pos;
            }
            else
            {
                token = Num; // 字符常量视为整数
            }
            return;
        }
        // 注释和除号, // /**/ /
        else if (token == '/')
        {
            //
            if (*src == '/')
            {
                while (*src != 0 && *src != '\n')
                {
                    src++;
                }
            }
            /* */
            else if (*src == '*')
            {
                src++;
                while (*src != 0 && (*src != '*' || *(src + 1) != '/'))
                {
                    if (*src == '\n') // 注释中的换行
                        line++;
                    src++;
                }
                if (*src == '*' && *(src + 1) == '/')
                {
                    src = src + 2;
                }
            }
            else
            {
                token = Div;
                return;
            }
        }
        // 操作符
        else if (token == ',') { token = Comma; return; }
        else if (token == '=') { if (*src == '=') { src++; token = Eq; } else token = Assign; return; } // = ==
        else if (token == '+') { if (*src == '+') { src++; token = Inc; } else token = Add; return; } // + ++
        else if (token == '-') { if (*src == '-') { src++; token = Dec; } else if (*src == '>') { src++, token = Gmbp; } else token = Sub; return; } // - -- ->
        else if (token == '!') { if (*src == '=') { src++; token = Ne; } return; } // != !
        else if (token == '<') { if (*src == '=') { src++; token = Le; } else if (*src == '<') { src++; token = Shl; } else token = Lt; return; } // < <= <<
        else if (token == '>') { if (*src == '=') { src++; token = Ge; } else if (*src == '>') { src++; token = Shr; } else token = Gt; return; } // > >= >>
        else if (token == '&') { if (*src == '&') { src++; token = Land; } else token = And; return; } // & &&
        else if (token == '|') { if (*src == '|') { src++; token = Lor; } else token = Or; return; } // | ||
        else if (token == '^') { token = Xor; return; } // ^
        else if (token == '%') { token = Mod; return; } // %
        else if (token == '*') { token = Mul; return; } // *
        else if (token == '?') { token = Cond; return; } // ?
        else if (token == '[') { token = Brak; return; } // [
        else if (token == '.') { token = Dot; return; } // .
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ':') return; // tokens are their ASCII
    }
}


/*
记录与回溯token流状态，目前只用于标号的解析，只应该用于测试能否解析，不应该在中间生成任何代码。
*/
void record()
{
    last_src = src;
    last_token = token;
    last_token_val = token_val;
    last_line = line;
}

void backtrack()
{
    src = last_src;
    token = last_token;
    token_val = last_token_val;
    line = last_line;
}

/*
检查下一个token是否是某种特定类型，不匹配报错退出。
*/
void match(int tk)
{
    char* tokens;
    if (token == tk)
    {
        next();
    }
    else
    {
        tokens =
            "Num     "
            "Id      "
            "Break   "
            "Char    "
            "Continue"
            "Do      "
            "Else    "
            "Enum    "
            "For     "
            "Goto    "
            "If      "
            "Int     "
            "Return  "
            "Sizeof  "
            "Struct  "
            "Union   "
            "While   "
            "Comma   "
            "Assign  "
            "Cond    "
            "Lor     "
            "Land    "
            "Or      "
            "Xor     "
            "And     "
            "Eq      "
            "Ne      "
            "Lt      "
            "Gt      "
            "Le      "
            "Ge      "
            "Shl     "
            "Shr     "
            "Add     "
            "Sub     "
            "Mul     "
            "Div     "
            "Mod     "
            "Inc     "
            "Dec     "
            "Brak    "
            "Dot     "
            "Gmbp    ";
        if (tk >= Num)
        {
            printf("%d: expected token : %.8s\n", line, &tokens[8 * (tk - Num)]);
        }
        else
        {
            printf("%d: expected token : '%c'\n", line, tk);
        }
        exit(-1);
    }
}

/*
解析类型：用于一定是类型的场景：局部变量声明、函数参数、类型转换、sizeof()运算符。
1. 全局变量定义和函数定义可能是类型定义，有其他情况，单独解析。
2. 非法类型返回-1，如果类型可以省略，比如函数参数里面，应该在外层加以处理，使用默认的int。
3. 只处理基本类型，指针在外层处理。
*/
int parse_type()
{
    int type;
    type = INT; // 默认类型就是INT
    
    if (token != Int && token != Char && token != Enum && token != Struct && token != Union)
    {
        return -1;
    }

    if (token == Int)
    {
        match(Int);
    }
    else if (token == Char)
    {
        type = CHAR;
        match(Char);
    }
    // 自定义 enum
    else if (token == Enum)
    {
        match(Enum);
        if (token == Id && current_id->class == EnumType)
        {
            type = INT; // 视为int
            match(Id);
        }
        else
        {
            printf("%d: unknown enum type\n", line);
            exit(-1);
        }
    }
    // 自定义 struct
    else if (token == Struct)
    {
        match(Struct);
        if (token == Id && current_id->class == StructType)
        {
            type = current_id->type;
            match(Id);
        }
        else
        {
            printf("%d: unknown struct type\n", line);
            exit(-1);
        }
    }
    // 自定义 union
    else if (token == Union)
    {
        match(Union);
        if (token == Id && current_id->class == UnionType)
        {
            type = current_id->type;
            match(Id);
        }
        else
        {
            printf("%d: unknown union type\n", line);
            exit(-1);
        }
    }
    return type;
}

/*
获取对类型进行++/--/+/-/[]运算符计算的单元大小，对于指针比较特殊。
多个运算符都要用，提取出来复用。
*/
int get_unit_size(int type)
{
    int unit_size;
    if (type > PTR)
    {
        // 一层的struct和union指针++/--时，单元大小是结构体和联合体大小
        if (type < PTR + PTR && type >= UNION + PTR)
        {
            if (type >= STRUCT + PTR) // struct*
            {
                unit_size = struct_symbols_list[type - PTR - STRUCT].size;
            }
            else // union *
            {
                unit_size = union_symbols_list[type - PTR - UNION].size;
            }
        }
        // 多层指针，整型指针
        else
        {
            unit_size = sizeof(int);
        }
    }
    else // 整型、char*指针
    {
        unit_size = sizeof(char);
    }
    return unit_size;
}


/*
解析表达式：

对于每个运算符，递归地向后处理高于当前运算符优先级(左结合的话是高于，右结合的话是高于和等于)的运算符后再回来处理当前的运算。
一元运算符优先级总是高于二元运算符，所以总是先处理一元运算符。

代码生成的逻辑：
1.一元运算符操作ax，二元运算符操作栈顶和ax，然后将结果保存到ax。
2.每次计算完一个子表达式后运算结果都将保存在ax中，然后计算外层表达式。
3.遇到二元运算符则会将保存在ax的左侧子表达式结果压栈，然后计算右侧子表达式结果。
*/
void expression(int level)
{
    struct Symbol_item *id;
    int tmp;
    int *addr;
    struct us_domain* cur_node;
    struct Func_call_item *func_list_pos;

    // 整数字面值
    if (token == Num)
    {
        match(Num);
        *++code = IMM;
        *++code = token_val;
        expr_type = INT;
    }
    // 字符串字面值
    else if (token == '"')
    {
        *++code = IMM;
        *++code = token_val;

        match('"');
        while (token == '"') // 处理多个字符串字面连接的情况"hello""world"
        {
            match('"');
        }
        data = (char*)(((int)data + sizeof(int)) & (-(int)sizeof(int))); // data首地址取int整数倍，同时字符串末尾填充为空位置中的0
        expr_type = CHAR + PTR;
    }
    // sizeof运算符：一元运算符，结果是int类型的右值
    else if (token == Sizeof)
    {
        match(Sizeof);
        match('(');
        expr_type = parse_type();
        if (expr_type == -1)
        {
            printf("%d: invalid type in sizeof()\n", line);
            exit(-1);
        }

        while (token == Mul)
        {
            match(Mul);
            expr_type = expr_type + PTR;
        }

        match(')');

        *++code = IMM;
        if (expr_type == CHAR)
        {
            *++code = sizeof(char);
        }
        else if (expr_type < PTR && expr_type >= UNION)
        {
            if (expr_type >= STRUCT) // struct
            {
                if (struct_symbols_list[expr_type - STRUCT].next == 0)
                {
                    printf("%d: can not get size of undefined struct type\n", line);
                    exit(-1);
                }
                *++code = struct_symbols_list[expr_type - STRUCT].size;
            }
            else // union
            {
                if (union_symbols_list[expr_type - UNION].next == 0)
                {
                    printf("%d: can not get size of undefined union type\n", line);
                    exit(-1);
                }
                *++code = union_symbols_list[expr_type - UNION].size;
            }
        }
        else // pointer/enum/int
        {
            *++code = sizeof(int);
        }
        expr_type = INT;
    }
    // 变量与函数调用：可能是函数调用、enum值、全局/局部变量
    else if (token == Id)
    {
        match(Id);
        // 记录函数或者变量的id
        id = current_id;

        // 函数调用，右值
        if (token == '(')
        {
            match('(');
            tmp = 0;
            // 参数按照顺序依次压栈，标准C语言是按照逆序压栈的
            while (token != ')')
            {
                expression(Assign);
                *++code = PUSH;
                tmp++;
                if (token != ')')
                {
                    match(Comma);
                    if (token == ')')
                    {
                        printf("%d: expected expression after ','\n", line);
                        exit(-1);
                    }
                }
            }
            match(')');

            // 系统调用
            if (id->class == Sys)
            {
                *++code = id->value;
            }
            // 自定义函数调用
            else if (id->class == Fun)
            {
                *++code = JSR;
                // 函数已经定义
                if (id->value)
                {
                    *++code = id->value;
                }
                // 函数声明了但是未定义，记录调用地址，全局声明解析结束后填充
                else
                {
                    for (func_list_pos = func_list; func_list_pos->hash; func_list_pos++);
                    func_list_pos->hash = id->hash;
                    func_list_pos->line = line;
                    func_list_pos->call_addresss = ++code;
                }
            }
            // 未定义的符号
            else
            {
                printf("%d: call undeclared function\n", line);
                exit(-1);
            }

            // 函数调用返回时，清理为参数分配的栈空间
            if (tmp > 0)
            {
                *++code = ADJ;
                *++code = tmp;
            }
            expr_type = id->type; // 返回值类型
        }
        // 枚举常量，右值
        else if (id->class == EnumVal)
        {
            *++code = IMM;
            *++code = id->value;
            expr_type = INT;
        }
        // 全局或局部变量，作为左值，需要先加载地址到ax再通过LC/LI加载值，以上一条指令是否是LC/LI作为判断是否是左值的依据，
        // 需要当左值来使用时，去掉LC/LI后就能够在ax中得到左值的地址，以此来完成取地址、赋值等操作。当做右值使用时和常量、中间结果等右值无异。
        // 左值经过某些运算后还是左值比如=*[].->，经过某些运算后变成了右值+-*/%^&|>><< etc，皆通过这个机制来实现。
        // 可以说是表达式代码生成的最精髓之处！
        else
        {
            // 函数内定义的局部变量或者函数参数，加载与bp的相对地址
            if (id->class == Loc)
            {
                *++code = LEA;
                *++code = index_of_bp - id->value;
            }
            // 全局变量则加载绝对地址
            else if (id->class == Glo)
            {
                *++code = IMM;
                *++code = id->value;
            }
            else
            {
                printf("%d: undefined variable\n", line);
                exit(-1);
            }

            // 加载变量值到ax，地址已经由上面LEA或者IMM加载到了ax中
            expr_type = id->type;
            *++code = (expr_type == CHAR) ? LC : LI;
        }
    }
    // 强制类型转换、括号运算符
    else if (token == '(')
    {
        match('(');
        // 强制类型转换，获取转换类型，并直接修改expr_type中保存的类型
        if (token == Int || token == Char || token == Enum || token == Struct || token == Union)
        {
            tmp = parse_type();
            if (tmp == -1)
            {
                printf("%d: invalid cast target type\n", line);
                exit(-1);
            }
            while (token == Mul)
            {
                match(Mul);
                tmp = tmp + PTR;
            }
            //不支持转换为struct/union类型
            if (tmp < PTR && tmp >= UNION)
            {
                printf("%d: do not support struct/union be cast target type, please consider use pointer\n", line);
                exit(-1);
            }

            match(')');

            expression(Inc); // 强制类型转换优先级同前缀++
            expr_type = tmp;
        }
        // 普通的括号运算符，而不是强制类型转换
        else
        {
            expression(Comma);
            match(')');
        }
    }
    // 指针解引用，得到左值
    else if (token == Mul)
    {
        match(Mul);
        expression(Inc); // 指针解引用和前缀++一个优先级，右结合

        if (expr_type >= PTR)
        {
            expr_type = expr_type - PTR;
        }
        else
        {
            printf("%d: invalid dereference\n", line);
            exit(-1);
        }
        *++code = (expr_type == CHAR) ? LC : LI; // 如果是多重指针就是LI，加载地址到ax
    }
    // 取地址操作，运用于左值，得到右值
    else if (token == And)
    {
        match(And);
        expression(Inc); // 和前缀++一个优先级，右结合
        // 前一个操作如果是取值到ax，那么经过一个取地址操作就将这个操作移除就可以了，相当于还原ax为地址
        // 如果不是，说明不能取地址，因为能取地址的左值操作时都是使用地址然后LC/LI加载到ax来操作。
        if (*code == LC || *code == LI)
        {
            code--;
        }
        else
        {
            printf("%d: invalid operand for address operation\n", line);
            exit(-1);
        }

        expr_type = expr_type + PTR;
    }
    // 逻辑非，得到右值
    else if (token == '!')
    {
        match('!');
        expression(Inc);

        *++code = PUSH;
        *++code = IMM;
        *++code = 0;
        *++code = EQ;

        expr_type = INT;
    }
    // 按位取反，得到右值
    else if (token == '~')
    {
        match('~');
        expression(Inc); // 和前缀++一个优先级

        *++code = PUSH;
        *++code = IMM;
        *++code = -1;
        *++code = XOR; // 和-1(0xFFFFFFFF)做异或，相当于按位取反

        expr_type = INT;
    }
    // 正号，得到右值
    else if (token == Add)
    {
        match(Add);
        expression(Inc);
        expr_type = INT;
    }
    // 负号，一元前缀右结合，得到右值
    else if (token == Sub)
    {
        match(Sub);
        // 常量取相反数
        if (token == Num)
        {
            *++code = IMM;
            *++code = -token_val;
            match(Num);
        }
        // 变量取相反数，乘以-1实现
        else
        {
            *++code = IMM;
            *++code = -1;
            *++code = PUSH;
            expression(Inc); // 和前缀++一个优先级
            *++code = MUL;
        }

        expr_type = INT;
    }
    // 前缀++/--，右结合，得到左值
    // Inc枚举值表示前缀++/--的优先级，后缀++/--左结合优先级高于前缀
    else if (token == Inc || token == Dec)
    {
        tmp = token;
        match(token);
        expression(Inc);

        if (*code == LC)
        {
            *code = PUSH; // 将ax地址再push到栈中，栈顶两个连续栈帧都保存同一个变量地址
            *++code = LC;
        }
        else if (*code == LI)
        {
            *code = PUSH;
            *++code = LI;
        }
        else
        {
            printf("%d: invalid lvalue for pre-increment", line);
            exit(-1);
        }

        *++code = PUSH;
        *++code = IMM;
        *++code = get_unit_size(expr_type); // 需要处理是指针的情况
        *++code = (tmp == Inc) ? ADD : SUB;
        *++code = (expr_type == CHAR) ? SC : SI;
    }
    else
    {
        printf("%d: invalid expression\n", line);
        exit(-1);
    }

    // 处理二元运算符，不断向右扫描，直到遇到优先级小于当前优先级的运算符，参数level指定了当前的优先级
    // 注意结合性的影响，左结合则向右计算优先级更高的运算符，右结合则向右计算优先级相等或者更高的运算符
    // 因为要区分不同运算符，必须使用不同枚举值，用来代指更高优先级的运算符应该选用高一级的同类优先级运算符中枚举值最小的那一个
    while (token >= level)
    {
        tmp = expr_type;
        // struct/union不支持除.,之外的所有运算符，统一检查并报错（->其中会检查，不在这里做）
        if (expr_type >= UNION && expr_type < PTR && token >= Assign && token <= Brak)
        {
            printf("%d: invalid operator for struct/union : %d\n", line, token);
            exit(-1);
        }

        // 逗号表达式，左结合，优先级最低
        if (token == Comma)
        {
            match(Comma);
            // 什么都不做，后面的操作将覆盖ax，不需要特地清理，如果清理了前面的代码，条件跳转的地址(if ?:)可能会出现问题
            expression(Assign);
        }
        // var = expr;
        // 解析=前，已经为var生成了汇编代码，变量地址会保存在ax中
        else if (token == Assign)
        {
            // 右结合，如果有会先计算右边的赋值表达式
            match(Assign);
            // LC/LI表明上一步是加载值到ax（地址存在ax中），也就是=左边是一个左值
            if (*code == LC || *code == LI)
            {
                *code = PUSH; // 取消上一步的变量加载，转而将地址压到栈顶
            }
            else
            {
                printf("%d: invalid lvalue in assignment\n", line);
                exit(-1);
            }
            expression(Assign);

            expr_type = tmp;
            // 最后来将表达式的值存到栈顶地址的位置，实现赋值操作
            *++code = (expr_type == CHAR) ? SC : SI;
        }
        // expr ? a : b 三目运算符，注意中间的a相当于加了括号，需要使用最低优先级，左边的expr和b则就是?:的优先级
        else if (token == Cond)
        {
            match(Cond);
            *++code = JZ;
            addr = ++code;
            expression(Comma);
            if (token == ':') {
                match(':');
            }
            else {
                printf("%d: missing colon in conditional\n", line);
                exit(-1);
            }
            *addr = (int)(code + 3);
            *++code = JMP;
            addr = ++code;
            expression(Cond);
            *addr = (int)(code + 1);
        }
        // ||
        else if (token == Lor)
        {
            match(Lor);
            *++code = JNZ;  // 短路求值，如果已经为true，则直接跳到下一个，多个||表达式则会一直跳到末尾
            addr = ++code;
            expression(Land);
            *addr = (int)(code + 1);
            expr_type = INT;
        }
        // &&
        else if (token == Land)
        {
            match(Land);
            *++code = JZ;   // 短路求值，如果已经为false，则直接跳到下一个，多个&&表达式则会一直跳到末尾
            addr = ++code;
            expression(Or);
            *addr = (int)(code + 1);
            expr_type = INT;
        }
        // |
        else if (token == Or)
        {
            match(Or);
            *++code = PUSH;
            expression(Xor);
            *++code = OR;
            expr_type = INT;
        }
        // ^
        else if (token == Xor)
        {
            match(Xor);
            *++code = PUSH;
            expression(And);
            *++code = XOR;
            expr_type = INT;
        }
        // &
        else if (token == And)
        {
            match(And);
            *++code = PUSH;
            expression(Eq);
            *++code = AND;
            expr_type = INT;
        }
        // ==
        else if (token == Eq)
        {
            match(Eq);
            *++code = PUSH;
            expression(Lt);
            *++code = EQ;
            expr_type = INT;
        }
        // !=
        else if (token == Ne)
        {
            match(Ne);
            *++code = PUSH;
            expression(Lt);
            *++code = NE;
            expr_type = INT;
        }
        // <
        else if (token == Lt)
        {
            match(Lt);
            *++code = PUSH;
            expression(Shl);
            *++code = LT;
            expr_type = INT;
        }
        // <=
        else if (token == Le)
        {
            match(Le);
            *++code = PUSH;
            expression(Shl);
            *++code = LE;
            expr_type = INT;
        }
        // >
        else if (token == Gt)
        {
            match(Gt);
            *++code = PUSH;
            expression(Shl);
            *++code = GT;
            expr_type = INT;
        }
        // >=
        else if (token == Ge)
        {
            match(Ge);
            *++code = PUSH;
            expression(Shl);
            *++code = GE;
            expr_type = INT;
        }
        // <<
        else if (token == Shl)
        {
            match(Shl);
            *++code = PUSH;
            expression(Add);
            *++code = SHL;
            expr_type = INT;
        }
        // >>
        else if (token == Shr)
        {
            match(Shr);
            *++code = PUSH;
            expression(Add);
            *++code = SHR;
            expr_type = INT;
        }
        // +
        else if (token == Add)
        {
            match(Add);
            *++code = PUSH;
            expression(Mul);
            expr_type = tmp;
            if (expr_type > PTR) // pointer but not char*
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = get_unit_size(expr_type);
                *++code = MUL;
            }
            *++code = ADD;
        }
        // -
        else if (token == Sub)
        {
            match(Sub);
            *++code = PUSH;
            expression(Mul);
            if (expr_type > PTR && tmp == expr_type) // 两个指针相减，得到偏移量
            {
                *++code = SUB;
                *++code = PUSH;
                *++code = IMM;
                *++code = get_unit_size(expr_type);
                *++code = DIV;
                expr_type = INT;
            }
            else if (tmp > PTR) // 指针偏移
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = get_unit_size(tmp);
                *++code = MUL;
                *++code = SUB;
                expr_type = tmp;
            }
            else // 整数减法、char*指针
            {
                *++code = SUB;
                expr_type = tmp;
            }
        }
        // *
        else if (token == Mul)
        {
            match(Mul);
            *++code = PUSH;
            expression(Inc);
            *++code = MUL;
            expr_type = tmp;
        }
        // /
        else if (token == Div)
        {
            match(Div);
            *++code = PUSH;
            expression(Inc);
            *++code = DIV;
            expr_type = tmp;
        }
        // %
        else if (token == Mod)
        {
            match(Mod);
            *++code = PUSH;
            expression(Inc);
            *++code = MOD;
            expr_type = tmp;
        }
        // 后缀 ++ -- 最高优先级且左结合一定最先计算
        // 将变量值++或者--后将原值取到ax中
        else if (token == Inc || token == Dec)
        {
            if (*code == LI)
            {
                *code = PUSH;
                *++code = LI;
            }
            else if (*code == LC)
            {
                *code = PUSH;
                *++code = LC;
            }
            else
            {
                printf("%d: invlaid lvalue int post ++/--\n", line);
                exit(-1);
            }
            
            *++code = PUSH;
            *++code = IMM;
            *++code = get_unit_size(expr_type);
            *++code = (token == Inc) ? ADD : SUB;
            *++code = (expr_type == CHAR) ? SC : SI; // 先保存了++/--的结果
            *++code = PUSH;
            *++code = IMM;
            *++code = get_unit_size(expr_type);
            *++code = (token == Inc) ? SUB : ADD; // 再重新恢复原值参与计算
            match(token);
        }
        // []
        else if (token == Brak)
        {
            match(Brak);
            *++code = PUSH;
            expression(Comma);
            match(']');
            if (tmp > PTR) // 并非char*的指针
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = get_unit_size(tmp);
                *++code = MUL;
            }
            else if (tmp < PTR) // 不是指针
            {
                printf("%d: pointer type expected for []\n", line);
                exit(-1);
            }
            expr_type = tmp - PTR;
            *++code = ADD;
            *++code = (expr_type == CHAR) ? LC : LI;
        }
        // . -> 最高优先级，左结合，.需要左值，->不要求，得到左值，一定最先计算，不需要再向后查找更高优先级的表达式
        else if (token == Dot || token == Gmbp)
        {
            // ->
            if (token == Gmbp)
            {
                // 一层struct/union指针，此时ax中就是->左边的表达式地址，地址是否有效由写程序的人负责，这里也无法检测
                if (expr_type >= PTR + UNION && expr_type < PTR + PTR)
                {
                    expr_type = expr_type - PTR;
                }
                else
                {
                    printf("%d: invalid variable type for -> operator\n", line);
                    exit(-1);
                }
            }
            // . 此时需要确保左边一定是左值
            else
            {
                // 确保是左值，则取消加载，还原地址
                if (*code == LC || *code == LI)
                {
                    code--;
                }
                else
                {
                    printf("%d: invalid lvalue for operator .\n", line);
                    exit(-1);
                }
            }
            match(token);

            // 非struct/union类型
            if (expr_type >= PTR || expr_type < UNION)
            {
                printf("%d: invalid lvalue for operator .\n", line);
                exit(-1);
            }
            // 结构体/联合体的域
            if (token != Id)
            {
                printf("%d: excepted member name after operator ./->\n", line);
                exit(-1);
            }
            id = current_id;
            match(Id);

            cur_node = (expr_type >= STRUCT) ? &struct_symbols_list[expr_type - STRUCT] : &union_symbols_list[expr_type - UNION];
            cur_node = cur_node->next; // 从第一个成员开始
            for (; cur_node; cur_node = cur_node->next)
            {
                if (cur_node->hash == current_id->hash)
                {
                    break;
                }
            }
            // 没有这个成员
            if (!cur_node)
            {
                printf("%d: invalid member name for operator ./->, hash: %d\n", line, current_id->hash);
                exit(-1);
            }

            // 计算指针偏移，现在ax中保存结构体的地址，struct才需要偏移，union约定所有成员地址都和union本身一致，可以不做，struct第一个成员也可以不做
            if (expr_type >= STRUCT && cur_node->offset != 0)
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = cur_node->offset;
                *++code = ADD;
            }
            // 结果也是左值，加载结果
            expr_type = cur_node->type;
            *++code = (expr_type == CHAR) ? LC : LI;
        }
        else
        {
            printf("%d: compiler error, token = %d\n", line, token);
            exit(-1);
        }
    }
}

/*
解析语句：
statement = if_statement
        | while_statement
        | for_statement
        | do_while_statement
        | break_statement
        | continue_statement
        | "{", {statement}, "}"
        | return, [expression], ";"
        | [expression], ";";
        | id, ":", statement;
        | goto, id, ";";
if_statement = if, "(", expression, ")", statement, [else, statement];
while_statement = while, "(", expression, ")", statement;
for_statement = for, "(", [expression], ";", [expression], ";", [expression], ")", statement;
do_while_statement = do, statement, while, "(", [expression], ")", ";";
break_statement = break, ";";
continue_statement = continue, ";";

代码生成：
=======================if-else语句=========================
if (condition)
{
    true_statements;
}

if (condition)
{
    true_statements;
}
else
{
    false_statements;
}

[condition]
JZ [end]
[true_statements]
...                  <-----[end]

[condition]
JZ [a]
[true_statements]
JMP [end]
[false_statements]   <-----[a]
...                  <-----[end]

=======================while 语句============================
while (condition)
{
    while_statements;
}

[condition]         <-----[a]
JZ [end]
[while_statements]
JMP [a]
...                 <-----[end]

=======================for 语句=============================
for (init; condition; iter)
{
    for_statements;
}

[init]
[condition]/IMM 1       <------ [a] // has condition/no condition
JNZ [c]
JMP [end]
[iter]                  <------ [b]
JMP [a]
[for_statements]        <------ [c]
JMP [b]
...                     <------ [end]

=========================do while语句=======================
[do_while_statements]   <------ [a]
[condition]
JNZ [a]
...                     <------ [end]

=========================break 语句=========================
JMP [end]

=========================cotinue语句========================
JMP [entry]

*/
void statement()
{
    int *a, *b, *c, *end; // 记录保存跳转地址的code段地址，后续确定后填充
    struct Symbol_item *id;
    struct Bc_list_item *bclist_pos;
    struct Label_list_item *label_list_pos;
    int *tmp_loop;  // 考虑循环嵌套，暂存当前循环，以便结束内层循环后恢复cur_loop，为了实现break和continue
    

    a = b = c = end = 0;
    tmp_loop = 0;
    bclist_pos = 0;

    // 暂存当前循环，进入内层循环时会直接覆盖cur_loop
    tmp_loop = cur_loop;

    // if, "(", expression, ")", statement, [else, statement]
    if (token == If)
    {
        match(If);
        match('(');
        expression(Comma);
        match(')');

        *++code = JZ;
        a = end = ++code;

        statement();
        if (token == Else)
        {
            match(Else);
            *a = (int)(code + 3);
            *++code = JMP;
            end = ++code;
            statement();
        }
        *end = (int)(code + 1);
    }
    // while, "(", expression, ")", statement
    else if (token == While)
    {
        match(While);
        a = code + 1;
        cur_loop = a;   // 保存当前循环，for break & continue

        match('(');
        expression(Comma);
        match(')');

        *++code = JZ;
        end = ++code;

        statement();

        *++code = JMP;
        *++code = (int)a;
        *end = (int)(code + 1);

        // 处理break和continue列表中的跳转地址
        for (bclist_pos = break_list; bclist_pos->loop; bclist_pos++)
        {
            if (bclist_pos->loop == cur_loop)
            {
                *(int*)bclist_pos->bc_address = (int)(code + 1);
                bclist_pos->loop = 0;
                bclist_pos->bc_address = 0;
            }
        }
        for (bclist_pos = continue_list; bclist_pos->loop; bclist_pos++)
        {
            if (bclist_pos->loop == cur_loop)
            {
                *(int*)bclist_pos->bc_address = (int)a;
                bclist_pos->loop = 0;
                bclist_pos->bc_address = 0;
            }
        }
    }
    // for, "(", [expression], ";", [expression], ";", [expression], ")", statement
    else if (token == For)
    {
        match(For);
        match('(');
        // 初始化表达式可能为空
        if (token != ';')
        {
            expression(Comma);
        }
        match(';');

        a = code + 1;
        cur_loop = a;   // 保存当前循环，for break & continue
        // 条件表达式为空
        if (token == ';')
        {
            *++code = IMM;
            *++code = 1;
        }
        else
        {
            expression(Comma);
        }
        match(';');

        *++code = JNZ;
        c = ++code;
        *++code = JMP;
        end = ++code;

        b = code + 1;
        // 迭代表达式
        if (token != ')')
        {
            expression(Comma);
        }
        match(')');
        *++code = JMP;
        *++code = (int)a;
        *c = (int)(code + 1);

        statement();

        *++code = JMP;
        *++code = (int)b;
        *end = (int)(code + 1);

        // 处理break和continue列表中的跳转地址
        for (bclist_pos = break_list; bclist_pos->loop; bclist_pos++)
        {
            if (bclist_pos->loop == cur_loop)
            {
                *(int*)bclist_pos->bc_address = (int)(code + 1);
                bclist_pos->loop = 0;
                bclist_pos->bc_address = 0;
            }
        }
        for (bclist_pos = continue_list; bclist_pos->loop; bclist_pos++)
        {
            if (bclist_pos->loop == cur_loop)
            {
                *(int*)bclist_pos->bc_address = (int)b; // continue will goto iter statement
                bclist_pos->loop = 0;
                bclist_pos->bc_address = 0;
            }
        }
    }
    // do, statement, while, "(", [expression], ")", ";"
    else if (token == Do)
    {
        match(Do);
        a = code + 1;
        cur_loop = a;   // 保存当前循环，for break & continue
        statement();
        match(While);
        match('(');
        b = code + 1;   // for continue
        expression(Comma);
        match(')');
        match(';');
        *++code = JNZ;
        *++code = (int)a;

        // 处理break和continue列表中的跳转地址
        for (bclist_pos = break_list; bclist_pos->loop; bclist_pos++)
        {
            if (bclist_pos->loop == cur_loop)
            {
                *(int*)bclist_pos->bc_address = (int)(code + 1);
                bclist_pos->loop = 0;
                bclist_pos->bc_address = 0;
            }
        }
        for (bclist_pos = continue_list; bclist_pos->loop; bclist_pos++)
        {
            if (bclist_pos->loop == cur_loop)
            {
                *(int*)bclist_pos->bc_address = (int)b; // continue will goto condition
                bclist_pos->loop = 0;
                bclist_pos->bc_address = 0;
            }
        }
    }
    // break, ";"
    else if (token == Break)
    {
        // 当前不在循环中，报错
        if (!cur_loop)
        {
            printf("%d: invalid break statement, not in a loop\n", line);
            exit(-1);
        }
        match(Break);
        match(';');
        
        *++code = JMP;
        // 添加当前需要填充的地址到break列表末尾
        for (bclist_pos = break_list; bclist_pos->loop; bclist_pos++) ;
        bclist_pos->loop = cur_loop;
        bclist_pos->bc_address = ++code;
    }
    // continue, ";"
    else if (token == Continue)
    {
        // 当前不在循环中，报错
        if (!cur_loop)
        {
            printf("%d: invalid continue statement, not in a loop\n", line);
            exit(-1);
        }
        match(Continue);
        match(';');
        
        *++code = JMP;
        // 添加当前需要填充的地址到continue列表末尾
        for (bclist_pos = continue_list; bclist_pos->loop; bclist_pos++) ;
        bclist_pos->loop = cur_loop;
        bclist_pos->bc_address = ++code;
    }
    // "{", {statement}, "}"
    else if (token == '{')
    {
        match('{');
        while (token != '}')
        {
            statement();
        }
        match('}');
    }
    // "return", [expression], ";"
    else if (token == Return)
    {
        match(Return);
        if (token != ';')
        {
            expression(Comma);
        }
        match(';');
        *++code = LEV; // leave subroutine
    }
    // ";"
    else if (token == ';')
    {
        match(';');
    }
    // goto, id, ";";
    else if (token == Goto)
    {
        match(Goto);
        match(Id);
        
        *++code = JMP;
        // 添加当前需要填充的地址到label列表末尾
        for (label_list_pos = label_list; label_list_pos->label_hash; label_list_pos++) ;
        label_list_pos->label_hash = current_id->hash;
        label_list_pos->line = line;
        label_list_pos->goto_address = ++code;

        match(';');
    }
    // expression, ";" | id, ":", statement
    else
    {
        // 记录状态
        record();

        // 尝试解析为标号
        if (token == Id)
        {
            match(Id);
            id = current_id;
            // 是标号
            if (token == ':')
            {
                match(':');

                // 标准C语言标号和变量、函数名是互不冲突的，但这里需要位置来保存，
                // 出于实现方便和代码清晰考虑，直接限制标号不能和类型、函数、变量、系统调用、枚举值同名。
                if (id->class >= EnumVal && id->class < Label)
                {
                    printf("%d: label can not have a same name with types, global vars, local vars, functions, system calls, and enum values\n", line);
                    exit(-1);
                }
                else if (id->class == Label)
                {
                    printf("%d: labels can not have same name\n", line);
                    exit(-1);
                }

                // 定义一个新的标号
                id->class = Label;
                id->value = (int)(code + 1);

                // 标号后必须有语句，位于块末尾的必须加一个空语句;
                if (token == '}')
                {
                    printf("%d: there must a statement after a label, please add a ';'\n", line);
                    exit(-1);
                }
                statement();
                cur_loop = tmp_loop;
                return;
            }
            // 不是标号，回溯状态到匹配标识符前
            else
            {
                backtrack();
            }
        }
        expression(Comma);
        match(';');
    }

    // 恢复当前循环
    cur_loop = tmp_loop;
}

/*
解析函数参数列表
param_decl = type, {"*"}, id, {",", type {"*"}, id};
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;
*/
void function_parameter()
{
    int type;
    int params;
    params = 0;
    while (token != ')')
    {
        type = parse_type();
        if (type == -1) // 没有类型，使用默认的int
        {
            type = INT;
        }
        
        while (token == Mul)
        {
            type = type + PTR;
            match(Mul);
        }

        // 不支持struct/union作为函数参数类型
        if (type < PTR && type >= UNION)
        {
            printf("%d: do not support struct/union to be type of function paramter, please use pointer instead\n", line);
            exit(-1);
        }

        // 参数名称
        if (token != Id)
        {
            printf("%d: invalid parameter declaration\n", line);
            exit(-1);
        }
        // 已经定义同名局部变量
        else if (current_id->class == Loc)
        {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }
        match(Id);

        // 函数参数同局部变量允许和全局变量、函数、枚举值、自定义类型同名，应该覆盖其定义
        if (current_id->class >= EnumVal && current_id->class <= Glo || current_id->class >= EnumType && current_id->class <= StructType)
        {
            current_id->gclass = current_id->class;
            current_id->gtype = current_id->type;
            current_id->gvalue = current_id->value;
        }
        current_id->class = Loc;
        current_id->type = type;
        current_id->value = params++; // 参数的下标，从0开始，最后用index_of_bp减去这个值得到相对bp偏移，为了统一局部变量和参数的处理

        if (token != ')')
        {
            match(Comma);
            if (token == ')')
            {
                printf("%d: expected identifier after ','\n", line);
                exit(-1);
            }
        }
    }
    index_of_bp = params + 1; // 记录第一个参数相对bp位置
}

/*
解析函数体
func_body = {var_decl}, {statement};
var_decl = type {"*"}, id, {",", id}, ";";
type = int | char | user_defiend_type;
user_defined_type = (enum | union | struct), id;

|    ....       | high address
+---------------+
| arg: param_a  |    new_bp + 3
+---------------+
| arg: param_b  |    new_bp + 2
+---------------+
|return address |    new_bp + 1
+---------------+
| old BP        | <- new BP
+---------------+
| local_1       |    new_bp - 1
+---------------+
| local_2       |    new_bp - 2
+---------------+
|    ....       |  low address

代码生成：
函数：
ENT count_of_locals
...
LEV

调用方：
参数按照调用顺序依次压栈
JSR addr_of_func
ADJ count_of_params
...

函数参数相对于新的bp的偏移: ..., +4, +3, +2
函数内局部变量相对于bp的偏移: -1, -2, -3, ...

struct/union局部变量的分配：
int a;
struct node s;
int b;

分配内存:
...             High address        reletive address to bp
variable a                          -1
end of struct s                     -2
...
begin of struct s                   -(1+sizeof(struct node)/sizeof(int)) <---- address of s
variable b
...             Low addresss
*/
void function_body()
{
    int type;
    int local_pos; // 局部变量计数
    local_pos = 0;

    // 局部变量声明，内置类型或者自定义类型，类型不能省略
    while (token == Char || token == Int || token == Enum || token == Struct || token == Union)
    {
        // 一定是有效的类型，while已经做了判断
        basetype = parse_type();

        while (token != ';')
        {
            type = basetype;
            while (token == Mul)
            {
                match(Mul);
                type = type + PTR;
            }

            if (token != Id)
            {
                printf("%d: invalid local declaration\n", line);
                exit(-1);
            }
            if (current_id->class == Loc)
            {
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);

            // 局部变量允许和全局变量、函数、枚举值、自定义类型同名，应该覆盖其定义
            if (current_id->class >= EnumVal && current_id->class <= Glo || current_id->class >= EnumType && current_id->class <= StructType)
            {
                current_id->gclass = current_id->class;
                current_id->gtype = current_id->type;
                current_id->gvalue = current_id->value;
            }
            current_id->class = Loc;
            current_id->type = type;

            // 结构与联合
            if (type < PTR && type >= UNION)
            {
                if (type >= STRUCT) // struct
                {
                    local_pos = local_pos + struct_symbols_list[type - STRUCT].size / sizeof(int);
                    current_id->value = local_pos + index_of_bp;
                }
                else // union
                {
                    local_pos = local_pos + union_symbols_list[type - UNION].size / sizeof(int);
                    current_id->value = local_pos + index_of_bp;
                }
            }
            // 整型与指针
            else
            {
                current_id->value = ++local_pos + index_of_bp; // 用index_of_bp减这个值得到相对bp偏移，为了统一局部变量和参数的处理
            }

            if (token != ';')
            {
                match(Comma);
                if (token == ';')
                {
                    printf("%d: expected identifier after ','\n", line);
                    exit(-1);
                }
            }
        }
        match(';');
    }

    // 生成函数体代码，第一条指令，切换堆栈，局部变量分配内存：ENT n，n是局部变量个数
    *++code = ENT;
    *++code = local_pos;

    // 解析语句直到函数结束
    while (token != '}')
    {
        statement();
    }

    // 离开被调函数返回主调函数的指令LEV
    *++code = LEV;
}

/*
解析函数体，从参数列表开始
func_decl = ret_type, id, "(", param_decl, ")", "{", func_body, "}";
forward_func_decl = ret_type, id, "(", [param_decl], ")", ";";

前向声明返回1，函数定义返回0
*/
int function_declaration()
{
    int forward_decl;
    struct Label_list_item *label_list_pos;
    int find_label;

    cur_loop = 0;
    forward_decl = 0;

    match('(');
    function_parameter();
    match(')');

    if (token == ';')
    {
        forward_decl = 1; // 不消耗;，留到外面
    }
    else
    {
        match('{');
        function_body();
        //match('}'); // 不消耗}，留到global_declaration中用于标识函数解析过程的结束
    }

    // 填充goto的标号地址
    for (label_list_pos = label_list; label_list_pos->label_hash; label_list_pos++)
    {
        find_label = 0;
        for (current_id = symbols; current_id->token; current_id++)
        {
            if (current_id->token == Id && current_id->class == Label && current_id->hash == label_list_pos->label_hash && current_id->value)
            {
                *(int*)label_list_pos->goto_address = current_id->value;
                find_label = 1;
                break;
            }
        }
        if (!find_label)
        {
            printf("%d: undefined label for goto in function, hash: %d\n", label_list_pos->line, label_list_pos->label_hash);
            exit(-1);
        }
        // 填充完后依次清空goto列表
        label_list_pos->label_hash = 0;
        label_list_pos->goto_address = 0;
    }

    // 遍历符号表，恢复全局变量定义，如果没有同名全局变量，则删除后符号表中还有该项，但是class/type/value都会被置空，不会影响
    current_id = symbols;
    while (current_id->token)
    {
        if (current_id->class == Loc || current_id->class == Label)  // 同时清空标号定义
        {
            current_id->class = current_id->gclass;
            current_id->type = current_id->gtype;
            current_id->value = current_id->gvalue;
        }
        current_id++;
    }
    return forward_decl;
}

/*
解析枚举定义：{}内的内容
enum_body = id, ["=", number], {",", id, ["=", number]};
*/
void enum_body()
{
    int enum_val;
    enum_val = 0;
    while (token != '}')
    {
        if (token != Id)
        {
            printf("%d: invalid enum identifier %d\n", line, token);
            exit(-1);
        }
        next();
        if (token == Assign)
        {
            match(Assign);
            if (token != Num)
            {
                printf("%d: invalid enum initializer\n", line);
                exit(-1);
            }
            enum_val = token_val;
            next();
        }

        // 给枚举赋值，视为常量
        current_id->class = EnumVal;
        current_id->type = INT;
        current_id->value = enum_val++;

        if (token != '}')
        {
            match(Comma);
        }
    }
}


/*
处理结构体和联合体的声明列表。
struct_decl = struct, [id], "{", var_decl, {var_decl}, "}", ";";
union_decl = union, [id], "{", var_decl, {var_decl}, "}", ";";
var_decl = type {"*"}, id, {",", {"*"}, id}, ";";
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;

struct_or_union: 1 struct 0 union

union的大小取最大，struct大小累加。
*/
void struct_union_body(int su_type, int struct_or_union)
{
    int domain_type;
    int domain_size;
    int cur_offset;
    int max_size;
    
    struct us_domain *cur_us_symbol;    // 当前类型的结构体/联合体信息表中的记录 
    struct us_domain **next_domain;     // 保存上一个节点的next指针的地址，新建下一个节点时用来连接
    struct us_domain *cur_node;

    cur_us_symbol = struct_or_union ? &struct_symbols_list[su_type - STRUCT] : &union_symbols_list[su_type - UNION];
    next_domain = &cur_us_symbol->next;
    cur_offset = 0;
    max_size = 0;

    if (token == '}')
    {
        printf("%d: struct/union definition can not be empty\n", line);
        exit(-1);
    }

    while (token != '}')
    {
        // 类型
        domain_type = parse_type();
        if (domain_type == -1)
        {
            printf("%d: invalid type in struct/union definition\n", line);
            exit(-1);
        }

        // 指针
        while (token == Mul)
        {
            match(Mul);
            domain_type = domain_type + PTR;
        }

        // 确定域的大小
        // 整型
        if (domain_type >= CHAR && domain_type <= ENUM)
        {
            domain_size = sizeof(int); // char也按int大小对齐，存在优化空间，这里只是为了方便
        }
        // 指针
        else if (domain_type >= PTR)
        {
            domain_size = sizeof(int);
        }
        // 已定义的结构或者联合
        else
        {
            // struct
            if (domain_type >= STRUCT)
            {
                if (struct_symbols_list[domain_type - STRUCT].next == 0)
                {
                    printf("%d: incomplete struct type can not be member of struct/union\n", line);
                    exit(-1);
                }
                else
                {
                    domain_size = struct_symbols_list[domain_type - STRUCT].size;
                }
            }
            // union
            else if (domain_type >= UNION)
            {
                if (union_symbols_list[domain_type - UNION].next == 0)
                {
                    printf("%d: incomplete union type can not be member of struct/union\n", line);
                    exit(-1);
                }
                else
                {
                    domain_size = union_symbols_list[domain_type - UNION].size;
                }
            }
            else
            {
                printf("%d: invalid type in struct/union definition\n", line);
                exit(-1);
            }
        }

        while (token != ';')
        {
            if (token == Id)
            {
                // 结构中成员不会和全局作用域中的符号冲突，只需要哈希值即可，不会修改符号表，但需要检测是否在struct或者union内部有命名冲突
                for (cur_node = cur_us_symbol->next; cur_node; cur_node = cur_node->next)
                {
                    // 已经在struct/union中定义了同名变量
                    if (cur_node->hash == current_id->hash)
                    {
                        printf("%d: struct/union member redefinition\n", line);
                        exit(-1);
                    }
                }

                // 在us_domains_list中新建链表节点，添加到当前节点上
                for (cur_node = us_domains_list; cur_node->hash; cur_node++);
                cur_node->hash = current_id->hash;
                cur_node->type = domain_type;
                cur_node->size = domain_size;
                cur_node->offset = struct_or_union ? cur_offset : 0;
                cur_node->next = 0;

                // 连接到上一个节点
                *next_domain = cur_node;
                next_domain = &cur_node->next;

                match(Id);
            }
            else
            {
                printf("%d: invalid variable declaration in struct/union definition\n", line);
                exit(-1);
            }

            // struct累计偏移，union确定最大的成员
            cur_offset = cur_offset + domain_size;
            max_size = max_size > domain_size ? max_size : domain_size;

            if (token != ';')
            {
                match(Comma);
                if (token == ';')
                {
                    printf("%d: invalid ',' before ';' in struct/union definition\n", line);
                    exit(-1);
                }
            }
        }
        match(';');
    }

    // 最终struct/union大小
    if (struct_or_union)
    {
        struct_symbols_list[su_type - STRUCT].size = cur_offset;
    }
    else
    {
        union_symbols_list[su_type - UNION].size = max_size;
    }
    // } 留到外面
}


/*
处理全局的变量定义、类型定义、以及函数定义、类型和函数的前向声明：
global_decl = enum_decl | func_decl | var_decl | struct_decl | union_decl | forward_type_decl | forward_func_decl;
enum_decl = enum, [id], "{", enum_body ,"}";
func_decl = ret_type, id, "(", [param_decl], ")", "{", func_body, "}";
var_decl = type {"*"}, id, {",", {"*"}, id}, ";";
ret_type = void | type, {"*"};
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;
struct_decl = struct, [id], "{", var_decl, {var_decl}, "}", ";";
union_decl = union, [id], "{", var_decl, {var_decl}, "}", ";";
forward_type_decl = (union | struct), id;
forward_func_decl = ret_type, id, "(", [param_decl], ")", ";"; 
*/
void global_declaration()
{
    int type;
    struct Symbol_item cur_func; // 函数解析时缓存当前符号表项
    struct Symbol_item *id;

    basetype = INT;

    // 解析enum: enum定义或变量声明或者作为返回值定义函数
    if (token == Enum)
    {
        match(Enum);
        // 可能是变量声明或者命名enum类型定义
        if (token == Id)
        {
            id = current_id;
            match(Id);
            // 不识别的新id，那只能是类型定义
            if (id->class == 0)
            {
                id->class = EnumType;
                id->type = Enum;        // 这个域意义不大，这里不区分枚举类型，都解释为int
                match('{');
                enum_body();
                match('}');
            }
            // 已定义的enum类型
            else if (id->class == EnumType)
            {
                basetype = INT;
                goto define_glo_func;
            }
            // 已知标识符，但不是enum类型
            else
            {
                printf("%d: known identifier can not be new enum name in definition\n", line);
                exit(-1);
            }
        }
        // 匿名enum定义
        else
        {
            match('{');
            enum_body();
            match('}');
        }
        match(';');
        return;
    }
    // 解析struct定义、前向声明、struct变量定义、struct相关类型作为函数返回类型
    else if (token == Struct)
    {
        match(Struct);
        if (token == Id)
        {
            id = current_id;
            match(Id);

            // 不识别的新id，那只能是前向声明或者定义，确定类型值，如果是定义就解析定义
            if (id->class == 0)
            {
                id->class = StructType;
                id->type = cur_struct_type;
                cur_struct_type++;
                
                // 填充结构体类型信息
                struct_symbols_list[id->type - STRUCT].hash = id->hash;
                struct_symbols_list[id->type - STRUCT].type = id->type;
                struct_symbols_list[id->type - STRUCT].size = 0;    // 解析后确定
                struct_symbols_list[id->type - STRUCT].offset = 0;
                struct_symbols_list[id->type - STRUCT].next = 0;    // 解析后确定

                // 新的struct定义
                if (token == '{')
                {
                    match('{');
                    struct_union_body(id->type, 1);
                    match('}');
                }
                // 不是前向声明
                else if (token != ';')
                {
                    printf("%d: invalid struct definition or forward declaration\n", line);
                    exit(-1);
                }
                // else ;前向声明
            }
            // 已声明的struct类型
            else if (id->class == StructType)
            {
                // struct定义
                if (token == '{')
                {
                    // 已经被定义
                    if (struct_symbols_list[id->type - STRUCT].next != 0)
                    {
                        printf("%d: duplicate struct definition\n", line);
                        exit(-1);
                    }
                    // 没有定义过那就解析定义
                    else
                    {
                        match('{');
                        struct_union_body(id->type, 1);
                        match('}'); // ;最后会匹配
                    }
                }
                // 不是定义也不是前向声明，那应该就是全局变量或者函数定义
                else if (token != ';')
                {
                    basetype = id->type;
                    goto define_glo_func;
                }
                // else ;就是前向声明，而且已经声明过了，什么都不用做，不管有没有定义
            }
            // 已知标识符，但不是struct类型
            else
            {
                printf("%d: known identifier can not be new struct name in definition\n", line);
                exit(-1);
            }
        }
        else
        {
            printf("%d: invalid use of keyword struct, must be with an identifier\n", line);
            exit(-1);
        }
        match(';');
        return;
    }
    // 解析union定义、前向声明、union变量定义、union相关类型作为函数返回类型，和struct基本一模一样
    else if (token == Union)
    {
        match(Union);
        if (token == Id)
        {
            id = current_id;
            match(Id);

            // 不识别的新id，那只能是前向声明或者定义，确定类型值，如果是定义就解析定义
            if (id->class == 0)
            {
                id->class = UnionType;
                id->type = cur_union_type;
                cur_union_type++;

                // 填充联合体类型信息
                union_symbols_list[id->type - UNION].hash = id->hash;
                union_symbols_list[id->type - UNION].type = id->type;
                union_symbols_list[id->type - UNION].size = 0;    // 解析后确定
                union_symbols_list[id->type - UNION].offset = 0;
                union_symbols_list[id->type - UNION].next = 0;    // 解析后确定

                // 新的union定义
                if (token == '{')
                {
                    match('{');
                    struct_union_body(id->type, 0);
                    match('}');
                }
                // 不是前向声明
                else if (token != ';')
                {
                    printf("%d: invalid struct definition or forward declaration\n", line);
                    exit(-1);
                }
                // else ;前向声明
            }
            // 已声明的union类型
            else if (id->class == UnionType)
            {
                // union定义
                if (token == '{')
                {
                    // 已经被定义
                    if (union_symbols_list[id->type - UNION].next != 0)
                    {
                        printf("%d: duplicate union definition\n", line);
                        exit(-1);
                    }
                    // 没有定义过那就解析定义
                    else
                    {
                        match('{');
                        struct_union_body(id->type, 0);
                        match('}'); // ;最后会匹配
                    }
                }
                // 不是定义也不是前向声明，那应该就是全局变量或者函数定义
                else if (token != ';')
                {
                    basetype = id->type;
                    goto define_glo_func;
                }
                // else ;就是前向声明，而且已经声明过了，什么都不用做，不管有没有定义
            }
            // 已知标识符，但不是struct类型
            else
            {
                printf("%d: known identifier can not be new union name in definition\n", line);
                exit(-1);
            }
        }
        else
        {
            printf("%d: invalid use of struct, must with an identifier\n", line);
            exit(-1);
        }
        match(';');
        return;
    }

    // 内置类型
    if (token == Int)
    {
        match(Int);
    }
    else if (token == Char)
    {
        match(Char);
        basetype = CHAR;
    }

define_glo_func:
    // 变量定义、函数定义、函数声明，直到变量定义函数声明结束;，函数定义结束}
    while (token != ';' && token != '}')
    {
        type = basetype;
        while (token == Mul)
        {
            match(Mul);
            type = type + PTR;
        }

        // 无效的声明
        if (token != Id)
        {
            printf("%d: invalid global declaration\n", line);
            exit(-1);
        }
        id = current_id;
        match(Id);

        // 函数定义或者声明
        if (token == '(')
        {
            // 不支持union或者struct作为函数返回值
            if (type < PTR && type >= UNION)
            {
                printf("%d: do not support struct/union to be type of function return value, please use pointer instead\n", line);
                exit(-1);
            }

            // 缓存当前符号表项
            memcpy(&cur_func, id, sizeof(struct Symbol_item));

            // 先给一个定义，保证递归调用时函数已经有了定义，结束解析后再检查当前函数是否重复定义，返回值是否与声明匹配等
            id->class = Fun;
            id->type = type;
            id->value = (int)(code + 1); // 函数入口

            // 前向声明，不生成代码
            if (function_declaration())
            {
                // 符号已经定义
                if (cur_func.class)
                {
                    // 符号已经声明为其他类型
                    if (cur_func.class != Fun)
                    {
                        printf("%d: duplicate global declaration, identifier has been used\n", line);
                        exit(-1);
                    }
                    // 已经声明或者定义为函数，但返回值不同
                    else if (cur_func.type != type)
                    {
                        printf("%d: invalid function declaration, different return type\n", line);
                        exit(-1);
                    }
                    // 已经声明或定义为函数，还原为缓存项
                    else
                    {
                        id->value = cur_func.value;
                    }
                }
                // 新的函数名，初次声明，值为空
                else
                {
                    id->value = 0;
                }
            }
            // 函数定义
            else
            {
                // 符号已经定义
                if (cur_func.class)
                {
                    // 符号已经声明为其他类型
                    if (cur_func.class != Fun)
                    {
                        printf("%d: duplicate global declaration, identifier has been used\n", line);
                        exit(-1);
                    }
                    // 同名函数已经定义
                    else if (cur_func.value)
                    {
                        printf("%d: redefinition of function\n", line);
                        exit(-1);
                    }
                    // 未定义，返回值和声明不同
                    else if (cur_func.type != type)
                    {
                        printf("%d: invalid function definition, different return type with declaration\n", line);
                        exit(-1);
                    }
                    // else 和声明匹配并且第一次定义，已经定义
                }
                // else 新的函数名，已经定义
            }
        }
        // 全局变量定义，在data区分配内存
        else
        {
            if (id->class)
            {
                printf("%d: duplicate global declaration, identifier has been used\n", line);
                exit(-1);
            }
            id->class = Glo;
            id->type = type;
            id->value = (int)data;

            // 结构与联合
            if (id->type < PTR && id->type >= UNION)
            {
                if (id->type >= STRUCT) // struct
                {
                    data = data + struct_symbols_list[id->type - STRUCT].size;
                }
                else // union
                {
                    data = data + union_symbols_list[id->type - UNION].size;
                }
            }
            // 指针、整型,char不足int大小的也按照int大小对齐，保证地址一定是sizeof(int)整数倍
            else
            {
                data = data + sizeof(int);
            }
            
            // 一行定义多个变量
            if (token != ';')
            {
                match(Comma);
                if (token == ';')
                {
                    printf("%d: expected identifier after ','\n", line);
                    exit(-1);
                }
            }
        }
    }
    next(); // ; }
}


/*
parser入口
支持的C语言子集（EBNF文法）:

program = {global_decl};
global_decl = enum_decl | func_decl | var_decl | struct_decl | union_decl | forward_type_decl | forward_func_decl;
enum_decl = enum, [id], "{", id, ["=", number], {",", id, ["=", number]} ,"}";
func_decl = ret_type, id, "(", [param_decl], ")", "{", func_body, "}";
struct_decl = struct, [id], "{", var_decl, {var_decl}, "}", ";";
union_decl = union, [id], "{", var_decl, {var_decl}, "}", ";";
forward_type_decl = (union | struct), id;
forward_func_decl = ret_type, id, "(", [param_decl], ")", ";";
param_decl = type, {"*"}, id, {",", type {"*"}, id};
ret_type = void | type, {"*"};
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;
func_body = {var_decl}, {statement};
var_decl = type {"*"}, id, {",", id}, ";";
statement = if_statement
        | while_statement
        | for_statement
        | do_while_statement
        | break_statement
        | continue_statement
        | "{", {statement}, "}"
        | return, [expression], ";"
        | [expression], ";";
        | id, ":", statement;
        | goto, id, ";";
if_statement = if, "(", expression, ")", statement, [else, statement];
while_statement = while, "(", expression, ")", statement;
for_statement = for, "(", [expression], ";", [expression], ";", [expression], ")", statement;
do_while_statement = do, statement, while, "(", [expression], ")", ";";
break_statement = break, ";";
continue_statement = continue, ";";
*/
void parse()
{
    struct Func_call_item *func_list_pos;
    int find_func;

    cur_struct_type = STRUCT;
    cur_union_type = UNION;

    next();
    while (token > 0)
    {
        global_declaration();
    }

    // 全局定义解析完成后填充未定义的函数调用
    for (func_list_pos = func_list; func_list_pos->hash; func_list_pos++)
    {
        find_func = 0;
        for (current_id = symbols; current_id->token; current_id++)
        {
            if (current_id->class == Fun && current_id->hash == func_list_pos->hash && current_id->value)
            {
                *func_list_pos->call_addresss = current_id->value;
                find_func = 1;
                break;
            }
        }
        if (!find_func)
        {
            printf("%d: call declared but undefined function\n", func_list_pos->line);
            exit(-1);
        }
        func_list_pos->hash = 0;
        func_list_pos->line = 0;
        func_list_pos->call_addresss = 0;
    }
}

/*
run the virtual machine
memory: data/stack/code
registers: pc/sp/bp/ax/cycle
*/
int run_vm()
{
    int op, *tmp;
    while (1)
    {
        op = *pc++;
        cycle++;
        if (debug == 1)
        {
            printf("%d> %.4s", cycle, &"LEA ,IMM ,JMP ,JSR ,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                "OPEN,READ,CLOS,WRIT,PRTF,MALC,FREE,MSET,MCMP,MCPY,EXIT,"[(op - LEA) * 5]);
            if (op >= JMP && op <= JNZ)
                printf("0x%.10X\n", *pc);
            else if (op <= ADJ)
                printf("%d\n", *pc);
            else
                printf("\n");
        }

        // load & store
        if (op == IMM) { ax = *pc++; }                      // load immediate to ax
        else if (op == LEA) { ax = (int)(bp + *pc++); }     // load address to ax
        else if (op == LC) { ax = *(char*)ax; }             // load char to ax
        else if (op == LI) { ax = *(int*)ax; }              // load int to ax
        else if (op == SC) { *(char*)*sp++ = ax; }          // save char to address in stack top, then pop
        else if (op == SI) { *(int*)*sp++ = ax; }           // save int to address in stack top, then pop
        else if (op == PUSH) *--sp = ax;                    // push ax to stack top

        // jump & function call
        else if (op == JMP) pc = (int*)*pc;                                  // jump to addresss
        else if (op == JZ) { pc = ax ? pc + 1 : (int*)*pc; }                 // jump to address if ax is zero
        else if (op == JNZ) { pc = ax ? (int*)*pc : pc + 1; }                // jump to address if ax is not zero
        else if (op == JSR) { *--sp = (int)(pc + 1); pc = (int*)*pc; }       // jump to subroutine
        else if (op == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }   // enter subroutine, make new stack frame
        else if (op == ADJ) sp = sp + *pc++;                                 // adjust stack
        else if (op == LEV) { sp = bp; bp = (int*)*sp++; pc = (int*)*sp++; } // leave/return from subroutine

        // calculation: arithmetic/bit-wise/logical
        else if (op == ADD) ax = *sp++ + ax;
        else if (op == SUB) ax = *sp++ - ax;
        else if (op == MUL) ax = *sp++ * ax;
        else if (op == DIV) ax = *sp++ / ax;
        else if (op == MOD) ax = *sp++ % ax;
        else if (op == AND) ax = *sp++ & ax;
        else if (op == OR)  ax = *sp++ | ax;
        else if (op == XOR) ax = *sp++ ^ ax;
        else if (op == SHL) ax = *sp++ << ax;
        else if (op == SHR) ax = *sp++ >> ax;
        else if (op == EQ)  ax = *sp++ == ax;
        else if (op == NE)  ax = *sp++ != ax;
        else if (op == LT)  ax = *sp++ < ax;
        else if (op == LE)  ax = *sp++ <= ax;
        else if (op == GT)  ax = *sp++ > ax;
        else if (op == GE)  ax = *sp++ >= ax;

        // system calls, pass arguments through stack
        else if (op == OPEN) ax = open((char*)sp[1], *sp);          // (filename, openFlag)
        else if (op == READ) ax = read(sp[2], (char*)sp[1], *sp);   // (fileHandle, buffer, maxCount)
        else if (op == CLOS) ax = close(*sp);                       // (fileHandle)
        else if (op == WRIT) ax = write(sp[2], (char*)sp[1], *sp);  // (fileHandle, buffer, maxCount)
        else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char*)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }
        else if (op == MALC) ax = (int)malloc(*sp);                // (size)
        else if (op == FREE) free((void*)*sp);                     // (addr)
        else if (op == MSET) ax = (int)memset((char*)sp[2], sp[1], *sp);        // (dest, val, size)
        else if (op == MCMP) ax = (int)memcmp((char*)sp[2], (char*)sp[1], *sp); // (dest, val, size)
        else if (op == MCPY) ax = (int)memcpy((char*)sp[2], (char*)sp[1], *sp); // (dest, src, count)
        else if (op == EXIT) { printf("exit(%d), cycle = %d\n", *sp, cycle); return *sp; }
        else { printf("unkown instruction = %d! cycle = %d\n", op, cycle); return -1; }
    }
    return 0;
}

int main(int argc, char** argv)
{
    int tmp;
    int fd;     // file descriptor
    int *tmpp;
    argc--;
    argv++;
    if (argc > 0 && (*argv)[0] == '-' && (*argv)[1] == 'd')
    {
        debug = 1;
        argc--;
        argv++;
    }
    if (argc < 1)
    {
        printf("usage: jatccex [-d] xxx.c [args to main]\n");
        return -1;
    }

    poolsize = 256 * 1024; // 256KB, could be arbitrary size.

    // 为VM分配内存
    if (!(code = (int*)malloc(poolsize)))
    {
        printf("Could not malloc(%d) for code area\n", poolsize);
        return -1;
    }
    if (!(data = (char*)malloc(poolsize)))
    {
        printf("Could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = (int*)malloc(poolsize)))
    {
        printf("Could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    memset(code, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    
    // 为parser分配内存
    if (!(symbols = (struct Symbol_item*)malloc(poolsize)))
    {
        printf("Could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }
    if (!(break_list = (struct Bc_list_item*)malloc(sizeof(struct Bc_list_item) * 1024))) // 1024 unit
    {
        printf("Could not malloc(%d) for break list of parser\n", 8 * 1024);
        exit(-1);
    }
    if (!(continue_list = (struct Bc_list_item*)malloc(sizeof(struct Bc_list_item) * 1024))) // 1024 unit
    {
        printf("Could not malloc(%d) for continue list of parser\n", sizeof(struct Bc_list_item) * 1024);
        exit(-1);
    }
    if (!(label_list = (struct Label_list_item*)malloc(sizeof(struct Label_list_item) * 1024))) // 1024 unit
    {
        printf("Could not malloc(%d) for label list for goto of parser\n", sizeof(struct Label_list_item) * 1024);
        exit(-1);
    }
    if (!(func_list = (struct Func_call_item*)malloc(sizeof(struct Func_call_item) * 1024))) // 1024 unit
    {
        printf("Could not malloc(%d) for function call list of parser\n", sizeof(struct Func_call_item) * 1024);
        exit(-1);
    }
    if (!(struct_symbols_list = (struct us_domain*)malloc(sizeof(struct us_domain) * 1024))) // 1024 unit
    {
        printf("Could not malloc(%d) for struct symbol list of parser\n", sizeof(struct us_domain) * 1024);
        exit(-1);
    }
    if (!(union_symbols_list = (struct us_domain*)malloc(sizeof(struct us_domain) * 1024))) // 1024 unit
    {
        printf("Could not malloc(%d) for union symbol list of parser\n", sizeof(struct us_domain) * 1024);
        exit(-1);
    }
    if (!(us_domains_list = (struct us_domain*)malloc(sizeof(struct us_domain) * 4096))) // 4096 unit
    {
        printf("Could not malloc(%d) for union and struct domains symbol info list of parser\n", sizeof(struct us_domain) * 4096);
        exit(-1);
    }
    memset(symbols, 0, poolsize);
    memset(break_list, 0, sizeof(struct Bc_list_item) * 1024);
    memset(continue_list, 0, sizeof(struct Bc_list_item) * 1024);
    memset(label_list, 0, sizeof(struct Label_list_item) * 1024);
    memset(func_list, 0, sizeof(struct Func_call_item) * 1024);
    memset(struct_symbols_list, 0, sizeof(struct us_domain) * 1024);
    memset(union_symbols_list, 0, sizeof(struct us_domain) * 1024);
    memset(us_domains_list, 0, sizeof(struct us_domain) * 4096);

    src = (char*)"break char continue do else enum for goto if int return sizeof struct union while "
        "open read close write printf malloc free memset memcmp memcpy exit void main";

    // 将关键字提前添加到符号表，在词法分析时关键字走标识符的识别流程，由于已经在符号表中，所以直接返回符号表的结果
    tmp = Break;
    while (tmp <= While)
    {
        next();
        current_id->token = tmp++; // only need token
    }

    // 将库函数添加到符号表中，和关键字含义类似
    tmp = OPEN;
    while (tmp <= EXIT)
    {
        next();
        current_id->class = Sys;   // 标识符类型是系统调用
        current_id->type = INT;    // 返回值类型
        current_id->value = tmp++; // 指令
    }
    // void将被视为char处理，main被作为标识符添加到符号表，并使用idmain记录main函数的符号表项
    next();
    current_id->token = Char; // void type, regard void as char
    next();
    idmain = current_id; // keep track on main

    // 打开并读取源文件到src
    if ((fd = open(*argv, 0)) < 0)
    {
        printf("Could not open(%s)\n", *argv);
        return -1;
    }
    if (!(src = malloc(poolsize)))
    {
        printf("Could not malloc(%d) for source code area\n", poolsize);
        return -1;
    }
    memset(src, 0, poolsize);

    if ((tmp = read(fd, src, poolsize)) <= 0)
    {
        printf("read() returned %d\n", tmp);
        return -1;
    }
    close(fd);
    src[tmp] = 0; // add \0 to identify end of source code

    // 解析并生成虚拟机指令
    line = 1;
    last_code = code + 1;
    parse();

    // 从main开始执行
    if (!(pc = (int*)idmain->value))
    {
        printf("main() not defined\n");
        return -1;
    }

    // 初始化VM寄存器，传递参数给main，main结束后执行PUSH和EXIT两条指令
    bp = sp = (int*)((int)stack + poolsize); // stack top
    *--sp = EXIT;
    *--sp = PUSH;
    tmpp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmpp;  // 返回地址，然后开始执行PUSH和EXIT

    // run VM
    return run_vm();
}
