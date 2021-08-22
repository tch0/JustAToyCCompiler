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


// VM
char *data;     // 数据段
int *code,      // 代码段
    *stack;     // 运行栈
int *pc, *sp, *bp, ax, cycle; // 寄存器
int poolsize;   // 各个段分配内存大小

// tokenizer
int token;      // 当前token
int token_val;  // 当前token是常量或字符串字面值时用来记录值
char *src;      // 源码
int line;       // 行号
int last_token;         // 支持记录和回溯token流的功能
int last_token_val;     // 支持记录和回溯token流的功能
char* last_src;         // 支持记录和回溯token流的功能
int last_line;          // 支持记录和回溯token流的功能

// parser
int *symbols;   // 符号表，动态数组模拟结构体
int *idmain;    // main函数的符号表记录
int *current_id;// 当前正在操作的符号表记录
int basetype;   // 变量、函数和类型定义的基本类型，是指针类型时使用
int expr_type;  // 表达式类型
int index_of_bp;// 函数调用时第一个参数相对bp的位置，函数的参数数量+1
int *break_list;    // break语句跳转地址的列表
int *continue_list; // continue语句跳转地址列表
int *cur_loop;      // 保存当前正在解析的循环的地址，用来唯一标识一个循环
int *label_list;    // goto语句跳转地址列表

// debug
int debug;      // 调试模式
int *last_code; // 上一次打印至的code段指针


/*
变量类型，如果是char*则表示为CHAR + PTR，char**则表示为CHAR + PTR + PTR。
枚举最终解释为整型常量，不支持命名枚举。
用最终类型模PTR得到基本类型，整除PTR则是指针层数，只存在指针这一种复合类型情况下很取巧的做法。
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
    Brak            // [
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

/*
符号表号中的记录用struct表示，在不支持struct特性的情况下，用整型动态数组来模拟一个struct，用枚举表示下标来取成员。
符号表记录各个域含义：
Token:  标记，值应该是Token_type类型的。
Hash:   根据名称计算出的一个哈希值，加速查找，不需要每次都去遍历名字比较。
Name:   名称，计算出哈希值之后就不需要用它了，指向源文件某位置的char*指针。
Class:  标识符的类型，Id类型才需要，值为Identifier_type中枚举。
Type:   标识符的变量类型或者函数返回值类型，值为Var_type枚举中普通类型与PTR组合得到的值。
Value:  标识符的值。如果标识符是函数，则是函数地址，如果是变量或者字符串常量就是地址，如果是字面量则是具体的值。
GClass/GType/GVlaue: 同Class/Type/Value，处理全局作用域对函数作用域的覆盖。
IdSize: struct长度。
*/
enum Symbol_domain { Token = 0, Hash, Name, Class, Type, Value, GClass, GType, GValue, IdSize };


// break和continue列表的域，同样模拟结构体
enum Break_continue_list_domain { Loop = 0, BCAddress, BCListSize };

// goto跳转列表的域，第一个是标号的哈希，第二个是地址
enum Goto_list_domain { LabelHash = 0, JmpCodeAddress, GotoListSize };

// 指令操作码，最多一个操作数
enum Instruction
{
    LEA = 100, IMM, JMP, JSR, JZ, JNZ, ENT, ADJ, // 1个操作数，剩余的都没有操作数
    LEV, LI, LC, SI, SC, PUSH,
    OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
    OPEN, READ, CLOS, WRIT, PRTF, MALC, FREE, MSET, MCMP, EXIT
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
                        "OPEN,READ,CLOS,WRIT,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[(op - LEA) * 5]);
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
            while (current_id[Token])
            {
                if (current_id[Hash] == hash && !memcmp((char*)current_id[Name], last_pos, src - last_pos))
                {
                    // 找到了为已有的标识符
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }
            // 在符号表中保存新的标识符
            token = current_id[Token] = Id;
            current_id[Hash] = hash;
            current_id[Name] = (int)last_pos;
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
        else if (token == '-') { if (*src == '-') { src++; token = Dec; } else token = Sub; return; } // - --
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
            "Brak    ";
        if (tk >= Num && tk <= Brak)
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
    
    if (token != Int && token != Char && token != Enum)
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
        if (token == Id && current_id[Class] == EnumType)
        {
            type = INT; // 视为int
            match(Id);
        }
        else
        {
            printf("%d: invalid enum type\n", line);
            exit(-1);
        }
    }

    return type;
}

/*
解析表达式：

对于每个运算符，递归地向后处理高于当前运算符优先级的运算符后再回来处理当前的运算。
一元运算符优先级总是高于二元运算符，所以总是先处理一元运算符。

代码生成的逻辑：
1.一元运算符操作ax，二元运算符操作栈顶和ax，然后将结果保存到ax。
2.每次计算完一个子表达式后运算结果都将保存在ax中，然后计算外层表达式。
3.遇到二元运算符则会将保存在ax的左侧子表达式结果压栈，然后计算右侧子表达式结果。
*/
void expression(int level)
{
    int *id;
    int tmp;
    int *addr;

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
    // sizeof运算符：一元运算符，仅支持sizeof(int),sizeof(char),sizeof( (int|char){*} )，并且结果类型是int
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
        *++code = (expr_type == CHAR) ? sizeof(char) : sizeof(int);
        expr_type = INT;
    }
    // 变量与函数调用：可能是函数调用、enum值、全局/局部变量
    else if (token == Id)
    {
        match(Id);
        // 记录函数或者变量的id
        id = current_id;

        // 函数调用
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
            if (id[Class] == Sys)
            {
                *++code = id[Value];
            }
            // 自定义函数调用
            else if (id[Class] == Fun)
            {
                *++code = JSR;
                *++code = id[Value];
            }
            else
            {
                printf("%d: invalid function call\n", line);
                exit(-1);
            }

            // 函数调用返回时，清理为参数分配的栈空间
            if (tmp > 0)
            {
                *++code = ADJ;
                *++code = tmp;
            }
            expr_type = id[Type]; // 返回值类型
        }
        // 枚举值
        else if (id[Class] == EnumVal)
        {
            *++code = IMM;
            *++code = id[Value];
            expr_type = INT;
        }
        // 全局或局部变量
        else
        {
            // 函数内定义的局部变量或者函数参数，加载与bp的相对地址
            if (id[Class] == Loc)
            {
                *++code = LEA;
                *++code = index_of_bp - id[Value];
            }
            // 全局变量则加载绝对地址
            else if (id[Class] == Glo)
            {
                *++code = IMM;
                *++code = id[Value];
            }
            else
            {
                printf("%d: undefined variable\n", line);
                exit(-1);
            }

            // 加载变量值到ax，地址已经由上面LEA或者IMM加载到了ax中
            expr_type = id[Type];
            *++code = (expr_type == CHAR) ? LC : LI;
        }
    }
    // 强制类型转换、括号运算符
    else if (token == '(')
    {
        match('(');
        // 强制类型转换，获取转换类型，并直接修改expr_type中保存的类型
        if (token == Int || token == Char || token == Enum)
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
            match(')');

            expression(Inc); // 强制类型转换优先级同++
            expr_type = tmp;
        }
        // 普通的括号运算符，而不是强制类型转换
        else
        {
            expression(Comma);
            match(')');
        }
    }
    // 指针解引用
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
    // 取地址操作
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
    // 逻辑非
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
    // 按位取反
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
    // 正号，什么都不做
    else if (token == Add)
    {
        match(Add);
        expression(Inc);
        expr_type = INT;
    }
    // 负号
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
    // 前缀++/--，右结合，后缀++/--优先级高于前缀并且是左结合
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
        *++code = (expr_type > PTR) ? sizeof(int) : sizeof(char); // 需要处理是指针的情况
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
            if (expr_type > PTR) // pointer and not char*
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = sizeof(int);
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
                *++code = sizeof(int);
                *++code = DIV;
                expr_type = INT;
            }
            else if (tmp > PTR) // 指针偏移
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = sizeof(int);
                *++code = MUL;
                *++code = SUB;
                expr_type = tmp;
            }
            else // 整数减法
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
        // 后缀 ++ --
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
                printf("%d: invlaid value int post ++/--\n", line);
                exit(-1);
            }

            *++code = PUSH;
            *++code = IMM;
            *++code = (expr_type > PTR) ? sizeof(int) : sizeof(char); // pointer but not char*
            *++code = (token == Inc) ? ADD : SUB;
            *++code = (expr_type == CHAR) ? SC : SI; // 先保存了++/--的结果
            *++code = PUSH;
            *++code = IMM;
            *++code = (expr_type > PTR) ? sizeof(int) : sizeof(char);
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
                *++code = sizeof(int);
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
    int *list_pos;
    int *tmp_loop;  // 考虑循环嵌套，暂存当前循环，以便结束内层循环后恢复cur_loop，为了实现break和continue
    int *id;

    a = b = c = end = 0;
    tmp_loop = 0;
    list_pos = 0;

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
        for (list_pos = break_list; *list_pos; list_pos = list_pos + BCListSize)
        {
            if (list_pos[Loop] == (int)cur_loop)
            {
                *(int*)list_pos[BCAddress] = (int)(code + 1);
                list_pos[Loop] = 0;
                list_pos[BCAddress] = 0;
            }
        }
        for (list_pos = continue_list; *list_pos; list_pos++)
        {
            if (list_pos[Loop] == (int)cur_loop)
            {
                *(int*)list_pos[BCAddress] = (int)a;
                list_pos[Loop] = 0;
                list_pos[BCAddress] = 0;
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
        for (list_pos = break_list; *list_pos; list_pos = list_pos + BCListSize)
        {
            if (list_pos[Loop] == (int)cur_loop)
            {
                *(int*)list_pos[BCAddress] = (int)(code + 1);
                list_pos[Loop] = 0;
                list_pos[BCAddress] = 0;
            }
        }
        for (list_pos = continue_list; *list_pos; list_pos = list_pos + BCListSize)
        {
            if (list_pos[Loop] == (int)cur_loop)
            {
                *(int*)list_pos[BCAddress] = (int)b; // continue will goto iter statement
                list_pos[Loop] = 0;
                list_pos[BCAddress] = 0;
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
        for (list_pos = break_list; *list_pos; list_pos = list_pos + BCListSize)
        {
            if (list_pos[Loop] == (int)cur_loop)
            {
                *(int*)list_pos[BCAddress] = (int)(code + 1);
                list_pos[Loop] = 0;
                list_pos[BCAddress] = 0;
            }
        }
        for (list_pos = continue_list; *list_pos; list_pos = list_pos + BCListSize)
        {
            if (list_pos[Loop] == (int)cur_loop)
            {
                *(int*)list_pos[BCAddress] = (int)b; // continue will goto condition
                list_pos[Loop] = 0;
                list_pos[BCAddress] = 0;
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
        for (list_pos = break_list; *list_pos; list_pos = list_pos + BCAddress) ;
        list_pos[Loop] = (int)cur_loop;
        list_pos[BCAddress] = (int)++code;
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
        for (list_pos = continue_list; *list_pos; list_pos = list_pos + BCAddress) ;
        list_pos[Loop] = (int)cur_loop;
        list_pos[BCAddress] = (int)++code;
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
        for (list_pos = label_list; *list_pos; list_pos = list_pos + GotoListSize) ;
        list_pos[LabelHash] = current_id[Hash];
        list_pos[JmpCodeAddress] = (int)++code;

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
                if (id[Class] >= EnumVal && id[Class] < Label)
                {
                    printf("%d: label can not have a same name with types, global vars, local vars, functions, system calls, and enum values\n", line);
                    exit(-1);
                }
                else if (id[Class] == Label)
                {
                    printf("%d: labels can not have same name\n", line);
                    exit(-1);
                }

                // 定义一个新的标号
                id[Class] = Label;
                id[Value] = (int)(code + 1);

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

        // 参数名称
        if (token != Id)
        {
            printf("%d: invalid parameter declaration\n", line);
            exit(-1);
        }
        // 已经定义同名局部变量
        else if (current_id[Class] == Loc)
        {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }
        match(Id);

        // 有同名全局变量的话先保存同名全局变量，再在函数内覆盖局部变量定义
        if (current_id[Class] == Glo)
        {
            current_id[GClass] = current_id[Class];
            current_id[GType] = current_id[Type];
            current_id[GValue] = current_id[Value];
        }
        current_id[Class] = Loc;
        current_id[Type] = type;
        current_id[Value] = params++; // 参数的下标，从0开始，最后用index_of_bp减去这个值得到相对bp偏移，为了统一局部变量和参数的处理

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
*/
void function_body()
{
    int type;
    int local_pos; // 局部变量计数
    local_pos = 0;

    // 局部变量声明，内置类型或者自定义类型，类型不能省略
    while (token == Char || token == Int || token == Enum)
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
            if (current_id[Class] == Loc)
            {
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);

            // 局部变量允许和全局变量、函数、枚举值同名，应该覆盖其定义
            if (current_id[Class] >= EnumVal && current_id[Class] <= Glo)
            {
                current_id[GClass] = current_id[Class];
                current_id[GType] = current_id[Type];
                current_id[GValue] = current_id[Value];
            }
            current_id[Class] = Loc;
            current_id[Type] = type;
            current_id[Value] = ++local_pos + index_of_bp; // 用index_of_bp减这个值得到相对bp偏移，为了统一局部变量和参数的处理

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
*/
void function_declaration()
{
    int* list_pos;
    int find_label;

    cur_loop = 0;

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('}'); // 不消耗}，留到global_declaration中用于标识函数解析过程的结束

    // 填充goto的标号地址
    for (list_pos = label_list; *list_pos; list_pos = list_pos + GotoListSize)
    {
        find_label = 0;
        for (current_id = symbols; current_id[Token]; current_id = current_id + IdSize)
        {
            if (current_id[Token] == Id && current_id[Class] == Label && current_id[Hash] == list_pos[LabelHash] && current_id[Value])
            {
                *(int*)list_pos[JmpCodeAddress] = current_id[Value];
                find_label = 1;
            }
        }
        if (!find_label)
        {
            printf("%d: invalid label for goto in function, hash: %d\n", line, list_pos[LabelHash]);
            exit(-1);
        }
        list_pos[LabelHash] = list_pos[JmpCodeAddress] = 0;
    }

    // 遍历符号表，恢复全局变量定义，如果没有同名全局变量，则删除后符号表中还有该项，但是Class/Type/Value都会被置空
    current_id = symbols;
    while (current_id[Token])
    {
        if (current_id[Class] == Loc || current_id[Class] == Label)  // 同时清空标号定义
        {
            current_id[Class] = current_id[GClass];
            current_id[Type] = current_id[GType];
            current_id[Value] = current_id[GValue];
        }
        current_id = current_id + IdSize;
    }
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
        current_id[Class] = EnumVal;
        current_id[Type] = INT;
        current_id[Value] = enum_val++;

        if (token != '}')
        {
            match(Comma);
        }
    }
}

/*
处理全局的变量定义、类型定义、以及函数定义：
global_decl = enum_decl | func_decl | var_decl;
enum_decl = enum, [id], "{", enum_body ,"}";
func_decl = ret_type, id, "(", param_decl, ")", "{", func_body, "}";
var_decl = type {"*"}, id, {",", {"*"}, id}, ";";
ret_type = void | type, {"*"};
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;
*/
void global_declaration()
{
    int type;   // 变量的数据类型
    int *id;

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
            if (id[Class] == 0)
            {
                id[Class] = EnumType;
                id[Type] = Enum;        // 这个域意义不大，这里不区分枚举类型，都解释为int
                match('{');
                enum_body();
                match('}');
            }
            // 已知标识符，但不是enum类型
            else if (id[Class] != EnumType)
            {
                printf("%d: known identifier can not be new enum name in definition\n", line);
                exit(-1);
            }
            // 已定义的enum类型
            else
            {
                basetype = INT;
                goto define_globals;
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

define_globals:
    // 变量或者函数定义，直到变量结束;，函数结束}
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
        // 这个标识符已经有了类型，重复的定义
        if (current_id[Class])
        {
            printf("%d: duplicate golbal declaration\n", line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        // 函数定义
        if (token == '(')
        {
            current_id[Class] = Fun;
            current_id[Value] = (int)(code + 1); // 函数的内存地址
            function_declaration();
        }
        // 全局变量定义，在data区分配内存
        else
        {
            current_id[Class] = Glo;
            current_id[Value] = (int)data;
            data = data + sizeof(int); // 每种变量
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
global_decl = enum_decl | func_decl | var_decl;
enum_decl = enum, [id], "{", id, ["=", number], {",", id, ["=", number]} ,"}";
func_decl = ret_type, id, "(", param_decl, ")", "{", func_body, "}";
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
    next();
    while (token > 0)
    {
        global_declaration();
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
                "OPEN,READ,CLOS,WRIT,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[(op - LEA) * 5]);
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
    if (!(symbols = (int*)malloc(poolsize)))
    {
        printf("Could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }
    if (!(break_list = (int*)malloc(8 * 1024))) // 8KB
    {
        printf("Could not malloc(%d) for break list of parser\n", 8 * 1024);
        exit(-1);
    }
    if (!(continue_list = (int*)malloc(8 * 1024))) // 8KB
    {
        printf("Could not malloc(%d) for continue list of parser\n", 8 * 1024);
        exit(-1);
    }
    if (!(label_list = (int*)malloc(8 * 1024))) // 8KB
    {
        printf("Could not malloc(%d) for label list for goto of parser\n", 8 * 1024);
        exit(-1);
    }
    memset(symbols, 0, poolsize);
    memset(break_list, 0, 8 * 1024);
    memset(continue_list, 0, 8 * 1024);
    memset(label_list, 0, 8 * 1024);

    src = (char*)"break char continue do else enum for goto if int return sizeof while "
        "open read close write printf malloc free memset memcmp exit void main";

    // 将关键字提前添加到符号表，在词法分析时关键字走标识符的识别流程，由于已经在符号表中，所以直接返回符号表的结果
    tmp = Break;
    while (tmp <= While)
    {
        next();
        current_id[Token] = tmp++; // only need token
    }

    // 将库函数添加到符号表中，和关键字含义类似
    tmp = OPEN;
    while (tmp <= EXIT)
    {
        next();
        current_id[Class] = Sys;   // 标识符类型是系统调用
        current_id[Type] = INT;    // 返回值类型
        current_id[Value] = tmp++; // 指令
    }
    // void将被视为char处理，main被作为标识符添加到符号表，并使用idmain记录main函数的符号表项
    next();
    current_id[Token] = Char; // void type, regard void as char
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
    if (!(pc = (int*)idmain[Value]))
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
