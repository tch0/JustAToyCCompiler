#include <stdio.h>

/*
multi-line
comment
test
*/
// single line comment test

int total_count, local_count, failed_count;
char *test_class;

void test(int actual, int expected)
{
    if (actual != expected)
        failed_count++;
    printf("test %3d %s (test for %s %2d), ",
        total_count++, (expected == actual ? "passed" : "failed"), test_class, local_count++);
    printf("expected: %d, actual: %d%s\n", expected, actual, expected != actual ? " ============> failed" : "");
}

void test_number()
{
    int a, b, c;

    test_class = (char*)"number";
    local_count = 1;

    a = 0x7FFF;
    b = 07563;
    c = 109951234;
    test(0x7FFF, a);
    test(07563, b);
    test(109951234, c);
    // corner case
    a = 0x7FFFFFFF;
    b = 0xffffffff;
    c = 0x70000000;
    test(0x7FFFFFFF, a);
    test(0xffffffff, b);
    test(0X70000000, c);
}

enum { First = 10, Second, Third = 1000, Fourth };
enum named { First1, Second1, Third1, Fourth1 };

void test_enum()
{
    test_class = (char*)"enum";
    local_count = 1;

    test(First, 10);
    test(Second, 11);
    test(Third, 1000);
    test(Fourth, 1001);

    test(First1, 0);
    test(Second1, 1);
    test(Third1, 2);
    test(Fourth1, 3);
}

// operator: = ?: || && | ^ & ! == != < > <= >= << >> + - * /  % ++ -- [] ()
void test_operator()
{
    int a, b, c, d;
    char* str;

    test_class = (char*)"operator";
    local_count = 1;

    // =
    test_class = (char*)"operator =";
    local_count = 1;
    a = 0;
    b = 1;
    c = 2;
    d = c;
    test(a, 0);
    test(a + b, 1);
    test(d, c);

    // ?:
    test_class = (char*)"operator ?:";
    local_count = 1;
    test(b ? c : 100, c);
    a = a ? 100 : 200;
    test(a, 200);

    // ||
    test_class = (char*)"operator ||";
    local_count = 1;
    a = 0;
    b = 1;
    c = -100;
    test(a || b, 1);
    test(a || a, 0);
    test(b || c, 1);
    test(c || c, 1); // fail, result: -100

    // &&
    test_class = (char*)"operator &&";
    local_count = 1;
    a = 0;
    b = 1;
    c = -100;
    d = 100;
    test(a && b, 0);
    test(a && c, 0);
    test(a && d, 0);
    test(b && c, 1); // fail, result: -100
    test(b && d, 1); // fail, result: 100
    test(c && d, 1); // fail, result: 100

    // |
    test_class = (char*)"operator |";
    local_count = 1;
    a = 0;
    b = 1;
    c = 0xFFFF;
    d = 0x00FF;
    test(a | b, 1);
    test(a | c, 0xFFFF);
    test(a | d, 0x00FF);
    test(b | c, 0xFFFF);
    test(c | d, 0xFFFF);

    // ^
    test_class = (char*)"operator ^";
    local_count = 1;
    a = 0;
    b = 1;
    c = 0xFFFF;
    d = 0x00FF;
    test(a ^ b, 1);
    test(a ^ c, 0xFFFF);
    test(a ^ d, 0x00FF);
    test(b ^ c, 0xFFFE);
    test(c ^ d, 0xFF00);

    // &
    test_class = (char*)"operator &";
    local_count = 1;
    a = 0;
    b = 1;
    c = 0xFFFF;
    d = 0x00FF;
    test(a & b, 0);
    test(a & c, 0);
    test(a & d, 0);
    test(b & c, 1);
    test(c & d, 0x00FF);

    // !
    test_class = (char*)"operator !";
    local_count = 1;
    a = 0;
    b = 1;
    c = -100;
    test(!a, 1);
    test(!b, 0);
    test(!c, 0);

    // ==
    test_class = (char*)"operator != ==";
    local_count = 1;
    a = 0;
    b = 0;
    c = -100;
    test(a == 0, 1);
    test(a != 0, 0);
    test(a == b, 1);
    test(a != b, 0);
    test(c == -100, 1);
    test(c != -100, 0);

    // < > <= >=
    test_class = (char*)"operator < > <= >=";
    local_count = 1;
    a = 1;
    b = 2;
    test(a > b, 0);
    test(a >= b, 0);
    test(a < b, 1);
    test(a <= b, 1);

    // << >>
    test_class = (char*)"operator << >>";
    local_count = 1;

    // + - * / %
    test_class = (char*)"operator + - * / %";
    local_count = 1;
    a = 1;
    b = 3;
    c = 5;
    test(a + b - c, -1);
    test(a - b - c, -7);
    test(a - b * 5 / 4, -2);
    test(100 + 200 + (300 * 10 - 2000 - 500 * 4 / (200 / 100)) * 10 - 100 * 2, 100);
    test((1 + 5 % 3 % 3 * 4 - 1) / 5 + 10, 11);

    // ++ --
    test_class = (char*)"operator ++ --";
    local_count = 1;
    a = 0;
    test(a++, 0);
    test(a++, 1);
    test(++a, 3);
    test(--a, 2);
    test(a--, 2);

    // [] & *
    test_class = (char*)"operator [] & *";
    local_count = 1;
    str = (char*)"hello";
    test(str[1], 'e');
    test(str[5], 0);
    test(*&str[4], 'o');
    test(str[1], *(str + 1));
    test((int)(&str[1]), (int)(str + 1));
}

void test_address_dereference()
{
    int a, *p, **pp, ***ppp;

    test_class = (char*)"address and dereference";
    local_count = 1;
    a = 0;
    p = &a;
    pp = &p;
    ppp = &pp;
    test((int)p, (int)&a);
    test((int)pp, (int)&p);
    test((int)ppp, (int)&pp);
    test(*p, a);
    test((int)*pp, (int)p);
    test((int)*ppp, (int)pp);
    test((int)*&p, (int)p);
    test(**&p, a);
    test(***ppp, a);
}

// test for precedence and associativity
// precedence up            associativity (default is left-to-right)
// =                        right-to-left
// ?:                       right-to-left
// ||
// &&
// |
// ^
// & bit-wise and
// == != 
// < > <= >=
// << >>
// + - 
// * /  %
/*                          right-to-left
    pre++/--
    unary+/-
    (cast)
    !
    ~
    *(address-of)
    &(dereference)
    sizeof
*/
// post++/-- () []
void test_expression()
{
    int a, b, c, d;
    int *p;

    test_class = (char*)"expression";
    local_count = 1;

    // = and ? :
    a = 0;
    b = (a > 0) ? 10 : 100;
    test(b, 100);
    b = (a > 0 ? 10 : 100);
    test(b, 100);

    // ?: and ||
    a = 1;
    b = 0;
    c = a && b ? 10 : 100;
    test(c, 100);

    // || and &&
    a = 1;
    b = 0;
    c = 1;
    test(a && b || c, c || a && b);

    // && and |
    a = 1;
    b = 0xFFFF;
    c = 0;
    test(a && b | c, b | c && a); // failed because of the result of &&
    test(a && b | c, a && (b | c));
    test(b | c && a, (b | c) && a);

    // | and ^
    a = 0x49BE;
    b = 0xFDE7;
    c = 0x73F3;
    test(a ^ b | c, (a ^ b) | c);
    test(a | b ^ c, a | (b ^ c));
    test(a ^ b | c, c | a ^ b);

    // ^ and &
    a = 0x49BE;
    b = 0xFDE7;
    c = 0x73F3;
    test(a & b ^ c, (a & b) ^ c);
    test(a & b ^ c, c ^ a & b);
    test(a ^ b & c, a ^ (b & c));

    // & and ==/!=
    a = 0xFF0;
    test(a & a == 0xFF0, a & (a == 0xFF0));

    // etc.
}

int fibonacci(int i)
{
    if (i <= 0)
        return 0;
    if (i == 1)
        return 1;
    return fibonacci(i - 1) + fibonacci(i - 2);
}

void test_recursive()
{
    test_class = (char*)"recursive";
    local_count = 1;

    test(fibonacci(0), 0);
    test(fibonacci(10), 55);
}

// test of functions
int func(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)
{
    return a + b + c + d + e + f + g + h + i + j;
}

void swap(int *a, int *b)
{
    int tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

void swapp(int **a, int **b)
{
    int *tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

void test_function()
{
    int a, b;
    int *pa, *pb;
    a = 0;
    b = 100;

    test_class = (char*)"function";
    local_count = 1;
    
    test(func(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), 55);
    swap(&a, &b);
    test(a, 100);
    test(b, 0);

    pa = &a;
    pb = &b;
    swapp(&pa, &pb);
    test((int)pa, (int)&b);
    test((int)pb, (int)&a);
}

void test_control_flows()
{
    int i;
    int result;

    test_class = (char*)"conditionals";
    local_count = 1;

    // while
    test_class = (char*)"conditionals while";
    local_count = 1;
    i = 0;
    result = 0;
    while (i <= 100)
    {
        result = result + i++;
    }
    test(result, 5050);

    // if-else
    test_class = (char*)"conditionals if-else";
    local_count = 1;
    i = 0;
    result = 0;
    while (i <= 100)
    {
        if (i % 4 == 0)
        {
            result = result + i;
        }
        else if (i % 4 == 1)
        {
            result = result - i;
        }
        else if (i % 4 == 2)
        {
            result = result + 2*i;
        }
        else
            ;
        i++;
    }
    test(result, 2575);
}

int main()
{
    total_count = 0;
    failed_count = 0;

    test_number();
    test_enum();
    test_operator();
    test_expression();
    test_address_dereference();
    test_recursive();
    test_function();
    test_control_flows();

    printf("total: %d, passed: %d, failed: %d\n", total_count, total_count - failed_count, failed_count);
    return 0;
}