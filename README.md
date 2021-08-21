# 能够自举的C编译器

一个能够自举的玩具C编译器，只支持其中一部分核心特性。完全就是[rswier/c4](https://github.com/rswier/c4)的重构和扩展。

## 特性

编译器特性：
- 前后端合一，无中间优化。
- 目标代码基于自定义的虚拟机（VM），非常简单但也足够完整。
- 单趟（One-Pass）解析过程：源代码经过解析和代码生成过程直接生成虚拟机指令，然后由虚拟机来执行。
- 按字节读取源码，支持兼容ASCII码的编码，比如GBK/UTF-8，不支持UTF-16/UTF-32。
- 可以使用GCC或者MSVC来编译，支持32位或者64位。

`jatcc.c`（仅仅是c4的重构）支持的C语言特性：
- 基本类型：`char`，`int`，指针，匿名`enum`
- 控制流：`if-else`，`while`
- 运算符：`, = ?: || && | ^ & == != < > <= >= << >> + - * / % ++ -- [] ()`
- 函数定义

由于实现带来的限制：
- 变量定义时不能初始化。
- 函数内局部变量的定义必须集中放在函数开头。
- `void`将被视为`char`。
- 执行类型转换但不做严格的类型检查。
- 不支持函数的前向声明，也就不能做到相互递归。
- 不检查函数调用参数个数与类型是否匹配。
- 类型系统实现得非常简单，支持数组下标操作但不支持数组定义。

`jatccex.c`添加的扩展：
- 控制流：`for` `do while`

正在支持：
- 浮点类型支持
- 函数前向声明
- 命名`enum`支持，简单等同于`int`
- `union` `struct`类型支持，相关操作符`. ->`支持
- `switch` `break` `continue` `goto`
- 使用了支持的特性替换部分实现细节
- 同样自举

## 构建与测试

```shell
jatcc [-d] xxx.c [args to main]
```

c4的重构：

```shell
gcc jatcc.c -o jatcc
./jatcc test.c
./jatcc -d test.c
./jatcc jatcc.c test.c
./jatcc jatcc.c jatcc.c test.c
```

扩展版本：
```shell
gcc jatccex.c -o jatccex
./jatccex testex.c
./jatccex jatccex.c testex.c
./jatccex jatcc.c test.c
```

## 实现细节

- [jatcc.c的实现细节](jatcc.md)
- [扩展的实现介绍](jatccex.md)

## License

同c4使用[GPL-2.0 License](LICENSE)。

## 参考

- 启发：[Bilibili-700行手写编译器](https://www.bilibili.com/video/BV1Kf4y1V783)
- 间接参考：[rswier/c4](https://github.com/rswier/c4)，一个500行的能够自举的简易C语言编译器。
- 源码基本上参考学习自：[手把手教你构建 C 语言编译器](https://lotabout.me/2015/write-a-C-interpreter-0/)系列，[lotabout/write-a-C-interpreter](https://github.com/lotabout/write-a-C-interpreter)
- 更多关于C4以及实现一个C编译器相关的细节：[知乎-RednaxelaFX关于C4的文章](https://www.zhihu.com/question/28249756/answer/84307453)
- c4改进的x86 JIT编译器：[EarlGray/c4](https://github.com/EarlGray/c4)

## TODO

- switch & break & continue & for ?
- struct & union & enum  ?
- typedef ?
- x86 JIT compiling ?

欢迎任何有意义的改进、扩展与建议。