#include <stdio.h>

int total_count, local_count, failed_count;
char *test_class;

// function forward declaration
void test(int actual, int expected);
void test_for();
void test_do_while();
void test_break();
void test_continue();
void test_goto();
void test_enumeration();
void test_struct_union();
void composite_test_binary_tree();

// repeat declaration
void test(int actual, int expected);
void test_for();
void test_do_while();
void test_break();
void test_continue();
void test_goto();
void test_enumeration();
void test_struct_union();
void composite_test_binary_tree();

int main()
{
    total_count = 0;
    failed_count = 0;

    test_for();
    test_do_while();
    test_break();
    test_continue();
    test_goto();
    test_enumeration();
    test_struct_union();

    printf("total: %d, passed: %d, failed: %d\n", total_count, total_count - failed_count, failed_count);
    
    composite_test_binary_tree();
    return 0;
}


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

void test_goto()
{
    int i, result;
    i = result = 0;

    test_class = (char*)"conditionals goto & label";
    local_count = 1;

    // simulate while loop
start:
    if (i > 100)
        goto end;
    result = result + i;
    i++;
    goto start;
end:
    test(result, 5050);

    // simulate break
    i = result = 0;
    while (i < 1000)
    {
        if (i > 100)
            goto endloop;
        result = result + i;
        i++;
    }
endloop:
    test(result, 5050);
}

// test for enumeration
// definition, global declaration, return type, paramter, local declaration, cast, sizeof
enum TestStatusEnum { GoodStatus = 1, BadStatus = 100, NotSureStatus, AreYouSureStatus, AreYouSeriousStatus,  };
enum TestStatusEnum status;
enum TestStatusEnum getBossStatus(enum TestStatusEnum input)
{
    if (input == GoodStatus || input == BadStatus)
        return AreYouSureStatus;
    else if (input == NotSureStatus)
        return AreYouSeriousStatus;
    else
        return BadStatus;
}

void test_enumeration()
{
    enum TestStatusEnum myStatus;

    test_class = (char*)"conditionals enumeration";
    local_count = 1;

    myStatus = GoodStatus;
    test(myStatus, GoodStatus);
    test(getBossStatus(myStatus), AreYouSureStatus);
    test(getBossStatus(NotSureStatus), AreYouSeriousStatus);

    status = (enum TestStatusEnum)102;
    test(status, AreYouSureStatus);
    test(sizeof(enum TestStatusEnum), sizeof(int));
}

// test for struct and union

// forward declaration
union Variant;

struct Matrix
{
    // just test, no meaning
    int m11, m12, m21, m22;
    int flag;
    struct Matrix *ma;
    union Variant *p;
};

union Variant
{
    int i;
    char c;
    struct Matrix m;
    void *p;
};

// global
struct Matrix m;
union Variant v;

void test_struct_union()
{
    // local
    union Variant v;
    struct Matrix *m_list;
    struct Matrix *tmpm;

    test_class = (char*)"union and struct";
    local_count = 1;

    v.p = &v;
    test((int)v.p, (int)&v);
    v.i = 0x798D145F;
    test(v.i, 0x798D145F);
    v.c = 'c';
    test(v.c, 'c');

    v.m.flag = 100;
    test(v.m.flag, 100);
    v.m.ma = &v.m;
    test((int)v.m.ma, (int)&v.m);

    test_class = (char*)"union and struct operator . ->";
    local_count = 1;

    // operator . ->
    v.m.m11 = 1;
    v.m.m12 = 2;
    v.m.m21 = 3;
    v.m.m22 = 4;
    test(v.m.m11, 1);
    test(v.m.m12, 2);
    test(v.m.m21, 3);
    test(v.m.m22, 4);

    (&v)->m.m11 = 100;
    (&v.m)->m12 = 200;
    (&(&v)->m)->m21 = 300;
    *&v.m.m22 = 400;
    test(v.m.m11, 100);
    test(v.m.m12, 200);
    test(v.m.m21, 300);
    test(v.m.m22, 400);

    (*&v).m.m11 = 1000;
    (&*&v.m)->m12 = 2000;
    (&(*&v).m)->m21 = 3000;
    (*&(v.m)).m22 = 4000;
    test(v.m.m11, 1000);
    test(v.m.m12, 2000);
    test(v.m.m21, 3000);
    test(v.m.m22, 4000);

    // ++ -- + - []
    test_class = (char*)"union and struct pointer operator ++/--/+/-/[]";
    local_count = 1;

    m_list = 0;
    m_list = (struct Matrix*)malloc(sizeof(struct Matrix) * 100);
    memset(m_list, 0, sizeof(struct Matrix) * 100);

    test(m_list, (int)&m_list[0]);
    test(&m_list[1], m_list + 1);
    test(&m_list[10], m_list + 10);

    tmpm = m_list + 10;
    test(&m_list[10], tmpm++);
    test(&m_list[11], tmpm);
    tmpm = tmpm + 10;
    test(&m_list[21], tmpm);
    test(&m_list[11], tmpm = tmpm - 10);
    test(&m_list[11], tmpm--);
    test(&m_list[10], tmpm);
    test(&m_list[9], --tmpm);
    test(&m_list[10], ++tmpm);
    tmpm = m_list + 10;
    test(tmpm - m_list, 10);

    // sizeof, the result of sizeof depend on implementation
    test_class = (char*)"union and struct operator sizeof";
    local_count = 1;
    test(sizeof(union Variant), sizeof(struct Matrix));
}


// composite test: binary tree
struct TreeNode
{
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
};

void reverse_tree(struct TreeNode* root)
{
    struct TreeNode *tmp;
    if (!root)
        return;
    tmp = root->left;
    root->left = root->right;
    root->right = tmp;
    reverse_tree(root->left);
    reverse_tree(root->right);
}

struct TreeNode* malloc_a_node(int val)
{
    struct TreeNode* node;
    node = 0;
    node = malloc(sizeof(struct TreeNode));
    memset(node, 0, sizeof(struct TreeNode));
    node->data = val;
    return node;
}

void insert_node_to_bst(struct TreeNode *node, int val)
{
    if (!node)
        return;
    if (val >= node->data)
    {
        if (!node->right)
            node->right = malloc_a_node(val);
        else
            insert_node_to_bst(node->right, val);
    }
    else
    {
        if (!node->left)
            node->left = malloc_a_node(val);
        else
            insert_node_to_bst(node->left, val);
    }
}

void pre_traverse_tree(struct TreeNode *root)
{
    if (!root)
        return;
    pre_traverse_tree(root->left);
    printf("%d ", root->data);
    pre_traverse_tree(root->right);
}

// LCG: generate simple pseudo random number
// Nj+1 = (A*Nj + B) Mod M, A = 1103515245, B=12345, M = 2^31
int cur_rand;
void srand(int seed)
{
    cur_rand = seed;
}

int rand()
{
    return cur_rand = (1103515245 * cur_rand + 12345) % 0x7FFFFFFF;
}

int min_rand, max_rand;
int rand_range()
{
    return rand() % (max_rand - min_rand) + min_rand;
}

void composite_test_binary_tree()
{
    int i;
    struct TreeNode *root;
    
    srand(0);
    min_rand = 0;
    max_rand = 1000;

    // create a bst
    root = malloc_a_node(rand_range());
    for (i = 1; i < 100; i++)
    {
        insert_node_to_bst(root, rand_range());
    }

    printf("\na simple binary tree test:\n");
    // pre order traverse
    printf("before reverse:\n");
    pre_traverse_tree(root);
    printf("\n");

    // reverse
    reverse_tree(root);

    // pre order traverse again
    printf("after reverse:\n");
    pre_traverse_tree(root);
    printf("\n");
}

