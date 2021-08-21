#include <stdio.h>

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

void test_for()
{
    int i, result;

    test_class = (char*)"conditionals for";
    local_count = 1;

    // normal for
    for (i = result = 0; i <= 100; i++)
    {
        result = result + i;
    }
    test(result, 5050);

    // no init or iter
    result = 0;
    i = 0;
    for (; i <= 100;)
    {
        result = result + i++;
    }
    test(result, 5050);

    // , operator
    for (i = 0, result = 0; i > 0, i <= 100; i = i + 2, i--)
    {
        result = result + i;
    }
    test(result, 5050);
}

void test_do_while()
{
    int i, result;

    test_class = (char*)"conditionals do while";
    local_count = 1;

    i = 0;
    result = 0;
    do
    {
        result = result + i;
        i++;
    } while (i <= 100);
    test(result, 5050);

    i = 0;
    result = 0;
    do 
    {
        result = result + i;
    } while (++i, i <= 100);
    test(result, 5050);
}

void test_break()
{
    int i, j, result;

    test_class = (char*)"conditionals break";
    local_count = 1;

    // in while
    i = result = 0;
    while (i < 200)
    {
        if (i > 100)
            break;
        result = result + i++;
    }
    test(result, 5050);

    // in for
    for (i = result = 0; i < 200; i++)
    {
        result = result + i;
        if (i == 100)
            break;
    }
    test(result, 5050);

    // in do while
    i = result = 0;
    do 
    {
        result = result + ++i;
        if (i == 100)
            break;
    } while (i < 200);
    test(result, 5050);

    // in nested loops
    i = result = 0;
    for (i = result = 0; i < 100; i++)
    {
        if (i >= 10) // only iterate 10 times
            break;
        for (j = 0; j <= 100; j++) // add 55 every time
        {
            result = result + j;
            if (j == 10) // only add to 10
                break;
        }
    }
    test(result, 550);
}

void test_continue()
{
    int i, j, result;

    test_class = (char*)"conditionals continue";
    local_count = 1;

    // in while
    i = result = 0;
    while (i <= 100)
    {
        i++;
        if (i > 10)
            continue;
        result = result + i;
    }
    test(result, 55);

    // in for
    for (i = result = 0; i < 100; i++)
    {
        if (i > 10)
            continue;
        result = result + i;
    }
    test(result, 55);

    // in do while
    i = result = 0;
    do 
    {
        if (i > 10)
            continue;
        result = result + i;
    } while (i++ < 100);
    test(result, 55);

    // in nested loops
    i = result = 0;
    for (i = result = 0; i < 100; i++)
    {
        if (i % 10 != 0) // only iterate 10 times
            continue;
        j = 0;
        while (j++ < 100)
        {
            if (j > 10)
                continue;
            result = result + j;
        }
    }
    test(result, 550);
}

int main()
{
    total_count = 0;
    failed_count = 0;

    test_for();
    test_do_while();
    test_break();
    test_continue();

    printf("total: %d, passed: %d, failed: %d\n", total_count, total_count - failed_count, failed_count);
    return 0;
}