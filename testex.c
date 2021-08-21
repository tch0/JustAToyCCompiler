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
    for (i = 0; i <= 100; i++)
    {
        result = result + i;
    }
    test(result, 5050);

    // no init and iter
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


int main()
{
    total_count = 0;
    failed_count = 0;

    test_for();
    test_do_while();

    printf("total: %d, passed: %d, failed: %d\n", total_count, total_count - failed_count, failed_count);
    return 0;
}