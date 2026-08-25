# Makefile 入门与实用指南

Makefile 用来描述项目的构建规则：有哪些目标文件、每个目标依赖哪些文件、如何由依赖生成目标。它的核心价值在于 **增量构建**——只有发生变化的文件才重新编译，从而节省大量时间。

## 1. 规则的基本结构

Makefile 的基本单位是规则（rule）：

```makefile
target: dependencies
	recipe
```

- `target`：要生成的目标，通常是一个文件（如 `app`、`main.o`），也可以是伪目标（如 `clean`）。
- `dependencies`：生成目标所依赖的文件（依赖关系，可以有多个，用空格分隔）。
- `recipe`：生成目标要执行的命令（一行或多行）。

两条硬性规定，新手最容易踩坑：

1. **命令前必须用 Tab 缩进，不能用空格。** 用空格会报 `missing separator` 错误。
2. 每条命令独占一行，`target` 和依赖之间用冒号分隔，依赖之间用空格分隔。

```makefile
app: main.o foo.o
	gcc -o app main.o foo.o
```

## 2. Make 如何判断是否需要重新构建

Make 通过**时间戳**判断目标是否需要重建：

- 如果目标文件不存在 → 执行命令生成它。
- 如果目标文件存在，但某个依赖文件的修改时间比目标文件更新 → 执行命令重新生成。
- 如果目标比所有依赖都新 → 跳过，什么都不做。

这就是"增量编译"的原理：改一个 `.c` 文件，只有它对应的 `.o` 和最终链接会重做，其他不动。

```makefile
app: main.o foo.o
	gcc -o app main.o foo.o

main.o: main.c
	gcc -c main.c -o main.o

foo.o: foo.c
	gcc -c foo.c -o foo.o
```

第二次执行 `make` 且源码无变化时，输出类似 `make: 'app' is up to date.`，不会重复编译。

## 3. 一个最小可运行示例

目录下放 `main.c`、`foo.c`、`foo.h` 和一个 `Makefile`：

```makefile
CC := gcc
CFLAGS := -Wall -Wextra -O2
TARGET := app
OBJS := main.o foo.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)
```

执行 `make` 构建，`make clean` 清理。这个模板已经覆盖了绝大多数单目录 C 项目的需求，下面逐条拆解其中的语法。

## 4. GCC 编译流程回顾

Make 只负责调度，真正编译靠 GCC。GCC 的完整流程：

```text
.c ──预处理──> .i ──编译──> .s ──汇编──> .o ──链接──> 可执行文件
```

常用选项：

| 选项 | 含义 |
| --- | --- |
| `-E` | 只预处理，输出到 `.i`，不编译 |
| `-S` | 编译成汇编，输出 `.s`，不汇编 |
| `-c` | 编译/汇编生成 `.o`，不链接 |
| `-o file` | 指定输出文件名 |

```bash
gcc -E main.c -o main.i   # 预处理
gcc -S main.c -o main.s   # 生成汇编
gcc -c main.c -o main.o   # 生成目标文件，不链接
gcc main.o foo.o -o app   # 链接
```

注意区分几个 `-C`/`-c`：

- `gcc -c`：只编译不链接。
- `gcc -E -C`：预处理时**保留注释**（配合 `-E` 使用）。
- `make -C dir`：Make 的参数，进入 `dir` 目录执行其中的 Makefile，等价于 `cd dir && make`。

后面模板里反复出现的编译/预处理选项，提前认识：

| 选项 | 含义 |
| --- | --- |
| `-Wall` | 打开一批常见警告 |
| `-Wextra` | 在 `-Wall` 基础上打开更多警告 |
| `-O0` / `-O2` | 优化级别：`-O0` 不优化（调试用），`-O2` 常规优化 |
| `-g` | 生成调试信息（配合 gdb 使用） |
| `-I dir` | 头文件搜索路径（预处理阶段，可写多个） |
| `-D NAME` / `-D NAME=value` | 定义宏，等价于源码里 `#define NAME` |
| `-MMD` | 编译时生成 `.d` 依赖文件 |
| `-MP` | 为每个头文件生成空规则，防止头文件被删后报错 |
| `-std=c11` | 指定 C 语言标准 |

## 5. 变量

### 5.1 定义与使用

```makefile
CC := gcc
CFLAGS := -Wall -O2
TARGET := app

$(TARGET): main.o
	$(CC) $(CFLAGS) -o $@ $^
```

用 `$(变量名)` 或 `${变量名}` 引用。变量名一般大写。

### 5.2 四种赋值方式

| 写法 | 含义 | 展开时机 |
| --- | --- | --- |
| `=` | 递归展开（延迟求值） | 使用该变量时才展开 |
| `:=` | 简单展开（立即求值） | 定义时立即展开 |
| `?=` | 仅当变量未定义时赋值 | — |
| `+=` | 追加 | 取决于原变量类型 |

`=` 与 `:=` 的区别是关键：

```makefile
A = $(B)      # 延迟展开
B = hello
# 最终 A = hello

C := $(B)     # 立即展开，此时 B 还没定义
B = hello
# 最终 C 为空
```

- `=` 是**延迟展开**：定义时只记录表达式，用到时才展开。适合"自动跟踪后续赋值"的场景，但有两点风险：一是真正使用时值可能已被改写；二是自我引用会导致无限循环（如 `A = $(A) x`）。
- `:=` 是**立即展开**：定义那一刻就把右边算出来固定住。绝大多数情况下推荐用 `:=`，行为可预测、无递归风险。

`?=` 常用于提供默认值，且允许命令行覆盖：

```makefile
CC ?= gcc
```

```bash
make CC=clang   # 外部指定后，?= 不会覆盖
```

`+=` 追加内容，并且**继承原变量的展开方式**：

```makefile
A = x          # A 是递归展开变量
A += y         # 等价于 A = x y，仍是递归展开

B := x         # B 是立即展开变量
B += y         # 等价于 B := x y，仍是立即展开
```

所以一开始用 `=` 定义的变量，后续 `+=` 追加也是延迟展开；用 `:=` 定义的则追加也是立即展开。两者不要混用，否则展开时机会变得难以预料。

### 5.3 命令行覆盖变量

命令行传入的变量优先级最高，会覆盖 Makefile 里的赋值（`:=` 和 `=` 都会被覆盖）。大型项目常用 `V=1` 这类开关控制输出：

```makefile
ifeq ($(V),1)
Q :=
else
Q := @
endif

$(TARGET): $(OBJS)
	$(Q)$(CC) -o $@ $^
```

> 这里的 @ 是一个特殊 Makefile 的特殊前缀，能够让对应命令在控制台打印出来。

如果某个变量**必须**以 Makefile 里的值为准、不允许命令行覆盖，用 `override`：

```makefile
override CFLAGS := -Wall -O2
```

此时即使执行 `make CFLAGS=-g`，`CFLAGS` 仍是 `-Wall -O2`。

## 6. 自动变量

自动变量是命令里最常用的简写，它们的值由当前规则的上下文决定，规则不同值就不同。

| 自动变量 | 含义 |
| --- | --- |
| `$@` | 当前目标名（含路径） |
| `$^` | 所有依赖（去重，不含 order-only 依赖） |
| `$<` | 第一个依赖 |
| `$?` | 比目标更新的所有依赖 |
| `$*` | 模式规则中 `%` 匹配到的部分（stem） |
| `$|` | order-only 依赖（见 6.4） |

对于 `app: main.o foo.o`：

```text
$@ = app
$^ = main.o foo.o
$< = main.o
```

典型用法：链接用 `$^`（所有目标文件），编译用 `$<`（第一个依赖，即当前 `.c`）：

```makefile
$(TARGET): $(OBJS)
	$(CC) -o $@ $^               # 链接

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@  # 编译
```

### 6.1 取目录或文件名：`$(@D)` / `$(@F)`

每个自动变量都能加 `D`（目录部分）或 `F`（文件名部分）后缀，写成 `$(@D)`、`$(@F)`、`$(<D)`、`$(<F)` 等形式：

```makefile
build/main.o: src/main.c
	@echo $@       # build/main.o
	@echo $(@D)    # build   目标所在目录
	@echo $(@F)    # main.o  目标文件名
	@echo $(<D)    # src     第一个依赖所在目录
	@echo $(<F)    # main.c  第一个依赖文件名
```

### 6.2 `$?`：比目标新的依赖

`$?` 只列出那些"比目标更新、需要重新处理"的依赖，常用于归档类命令，只更新改动的部分：

```makefile
libfoo.a: foo.o bar.o
	ar r $@ $?    # 只把发生变化的 .o 加进静态库
```

### 6.3 `$*`：模式匹配到的 stem

只在模式规则或静态模式规则里有意义。对 `%.o: %.c`，构建 `main.o` 时 `$*` 就是 `main`：

```makefile
%.o: %.c
	$(CC) -c $< -o $@ -MMD -MF $*.d   # 用 stem 给依赖文件命名
```

### 6.4 order-only 依赖（`|` 分隔）

普通依赖既参与"是否需要重建"的时间戳比较，也会进入 `$^`；order-only 依赖只保证"先被构建"，不参与时间戳比较。用 `|` 分隔，`|` 右边的都是 order-only 依赖，可用 `$|` 引用。

```makefile
obj/%.o: %.c | obj
	$(CC) -c $< -o $@

obj:
	mkdir -p obj
```

这里 `obj` 是 order-only 依赖：只要 `obj` 目录存在，即使目录的修改时间比 `main.o` 新，也不会触发 `main.o` 重建，且 `$^` 里不会包含 `obj`。

## 7. 模式规则与静态模式规则

多个 `.c` 文件如果逐个写规则会非常啰嗦：

```makefile
main.o: main.c
	gcc -c main.c -o main.o
foo.o: foo.c
	gcc -c foo.c -o foo.o
```

### 7.1 模式规则

用 `%` 做通配符，一条规则覆盖所有匹配文件：

```makefile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

它表示：任意 `xxx.o` 可由 `xxx.c` 生成。

关于 `%` 的两条规则：

1. **目标里只能出现一个 `%`**，它匹配任意长度的字符串（含空串），匹配到的部分叫 stem。
2. 依赖里的 `%` 会替换成目标中 `%` 匹配到的同一内容，保证目标和依赖的 `%` 一致。

例如构建 `foo.o` 时：目标 `%.o` 的 `%` 匹配 `foo`，依赖 `%.c` 就变成 `foo.c`，`$*` 等于 `foo`。

### 7.2 静态模式规则

模式规则是**全局**的，会作用于所有匹配的文件。如果只想让**指定的某些目标**套用该规则，用静态模式规则，格式为：

```makefile
目标列表: 目标模式: 依赖模式
	recipe
```

| 部分 | 含义 |
| --- | --- |
| `目标列表` | 明确指定哪些目标适用此规则（通常是一个变量） |
| `目标模式` | 与目标列表里的文件名匹配的模式（含 `%`） |
| `依赖模式` | 由目标模式推导出依赖文件的模式（含 `%`） |

```makefile
OBJS := main.o foo.o bar.o

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

这样规则只作用于 `$(OBJS)` 里的三个文件，其他 `.o` 不会被意外命中。适用场景：目标文件只在某个固定列表里、且想精确控制规则作用范围时。

## 8. 内置函数

函数调用格式：`$(函数名 参数...)`，参数之间用逗号分隔，函数名与第一个参数之间用空格分隔。参数里可以嵌套其他函数调用。

### 8.1 `$(wildcard pattern)`

按 Shell 通配符列出**实际存在**的文件，返回空格分隔的文件名列表。

| 参数 | 含义 |
| --- | --- |
| `pattern` | 通配符表达式，支持 `*`、`?`、`[]`，规则与 Shell 通配一致 |

```makefile
SRCS := $(wildcard *.c)
# 展开结果：当前目录下所有 .c 文件，如 main.c foo.c bar.c
```

注意：`wildcard` 只匹配真实存在的文件。如果模式没有匹配到任何文件，返回**空字符串**，而不是把 `*.c` 原样保留——这一点和 Shell 不同，也是新手常踩的坑。

### 8.2 `$(patsubst pattern,replacement,text)`

把 `text` 中所有匹配 `pattern` 的单词，替换成 `replacement`。

| 参数 | 含义 |
| --- | --- |
| `pattern` | 匹配模式，可含 `%` 通配符 |
| `replacement` | 替换结果，其中的 `%` 代表 pattern 里 `%` 匹配到的内容 |
| `text` | 待处理文本，按空格分词 |

```makefile
SRCS := main.c foo.c
OBJS := $(patsubst %.c,%.o,$(SRCS))
# 结果：main.o foo.o
```

`%` 是连接 pattern 和 replacement 的桥梁：`main.c` 里 `%` 匹配到 `main`，replacement 里的 `%.o` 就变成 `main.o`。`text` 中不匹配的单词原样保留。

### 8.3 替换引用（patsubst 的简写）

`$(var:a=b)` 等价于 `$(patsubst %a,%b,$(var))`，只把**结尾**的 `a` 换成 `b`：

```makefile
OBJS := $(SRCS:.c=.o)
# 等价于 $(patsubst %.c,%.o,$(SRCS))
```

### 8.4 `$(notdir names)` / `$(dir names)`

| 函数 | 作用 | 参数 |
| --- | --- | --- |
| `$(notdir names)` | 去掉路径，只保留文件名 | `names`：带路径的文件名列表 |
| `$(dir names)` | 只保留目录部分（含末尾 `/`） | `names`：带路径的文件名列表 |

```makefile
$(notdir src/main.c)   # main.c
$(dir src/main.c)      # src/
```

`notdir` 常用于"在任何目录编译、产物都落到当前目录"；`dir` 常用于提取目录前缀。

### 8.5 `$(addprefix prefix,names)` / `$(addsuffix suffix,names)`

| 函数 | 作用 | 参数 |
| --- | --- | --- |
| `$(addprefix prefix,names)` | 给每个单词前面加前缀 | `prefix`：前缀；`names`：单词列表 |
| `$(addsuffix suffix,names)` | 给每个单词后面加后缀 | `suffix`：后缀；`names`：单词列表 |

```makefile
$(addprefix obj/,$(OBJS))   # obj/main.o obj/foo.o
$(addsuffix .o,main foo)    # main.o foo.o
```

### 8.6 `$(filter pattern...,text)` / `$(filter-out pattern...,text)`

| 函数 | 作用 |
| --- | --- |
| `$(filter ...)` | 保留 `text` 中匹配任一 `pattern` 的单词 |
| `$(filter-out ...)` | 删除 `text` 中匹配任一 `pattern` 的单词 |

两者都支持**多个模式**，模式之间用空格分隔。

```makefile
SRCS := main.c foo.s bar.c
CFILES := $(filter %.c,$(SRCS))            # main.c bar.c
NONC   := $(filter-out %.c,$(SRCS))        # foo.s
```

### 8.7 `$(subst from,to,text)`

纯文本替换，把 `text` 中每个 `from` 都换成 `to`。与 `patsubst` 的区别：`subst` 不支持 `%`，是**精确字符串匹配**。

| 参数 | 含义 |
| --- | --- |
| `from` | 要被替换的字符串（精确匹配） |
| `to` | 替换后的字符串 |
| `text` | 待处理文本 |

```makefile
$(subst .c,.o,main.c foo.c)   # main.o foo.o
```

### 8.8 `$(shell command)`

执行一条 Shell 命令，把命令的**标准输出**（去掉末尾换行）作为函数返回值。

| 参数 | 含义 |
| --- | --- |
| `command` | 要执行的 Shell 命令 |

```makefile
SUBDIR := $(shell find . -maxdepth 1 -type d)
```

注意：`$(shell ...)` 在 Makefile **解析阶段**执行，与规则命令里直接写 Shell 是两回事。不要滥用——每次调用都有性能开销，且执行结果会缓存到该次 make 运行的解析期。

### 8.9 `$(foreach var,list,text)`

遍历 `list` 中的每个单词，依次赋值给 `var`，展开 `text`，最后把所有结果用空格拼接返回。

| 参数 | 含义 |
| --- | --- |
| `var` | 循环变量名（不带 `$`，直接写名字） |
| `list` | 单词列表 |
| `text` | 每次循环要展开的内容，可用 `$(var)` 引用当前项 |

```makefile
FILES := a.c b.c c.c
OBJS  := $(foreach f,$(FILES),$(f:.c=.o))
# 结果：a.o b.o c.o
```

注意：普通 C 项目优先用依赖和模式规则表达，不要用 `foreach` + Shell 循环重写构建逻辑。

## 9. 伪目标 .PHONY

`clean`、`all`、`install` 这类目标不是真实文件，只是命令名称。用 `.PHONY` 显式声明：

```makefile
.PHONY: all clean

all: app

clean:
	rm -f app *.o
```

为什么要声明：如果当前目录恰好存在一个名为 `clean` 的文件，Make 会认为该目标已经"最新"而拒绝执行 `rm`。声明 `.PHONY` 后，无论是否有同名文件都会执行命令。

```makefile
.PHONY: all clean install
```

## 10. 条件判断

```makefile
ifeq ($(DEBUG),1)
CFLAGS += -g -O0
else
CFLAGS += -O2
endif
```

```bash
make DEBUG=1   # 走 ifeq 分支
make           # 走 else 分支
```

### 10.1 四种判断形式

| 形式 | 含义 |
| --- | --- |
| `ifeq (a,b)` | `a` 与 `b` 相等时为真 |
| `ifneq (a,b)` | `a` 与 `b` 不等时为真 |
| `ifdef VAR` | 变量 `VAR` 已定义**且值非空**时为真 |
| `ifndef VAR` | 变量 `VAR` 未定义或值为空时为真 |

`ifeq`/`ifneq` 的参数可以用圆括号、花括号或引号包裹，效果相同：

```makefile
ifeq ($(DEBUG),1)      # 推荐：圆括号
ifeq '$(DEBUG)' '1'    # 单引号
ifeq "$(DEBUG)" "1"    # 双引号
```

注意 `ifdef` 只接受变量名（不带 `$` 和括号），且它判断的是"值是否非空"——`VAR =` 这种赋了空值的变量，`ifdef VAR` 也为假。

### 10.2 结构与限制

- 支持 `else` 分支，可嵌套，每个 `ifxxx` 必须有对应的 `endif`。
- 条件判断在 Makefile **解析阶段**执行（不是执行命令时），所以**不能**写在由 Tab 缩进的规则命令里。
- 可用于条件赋值、条件追加、选择依赖等场景：

```makefile
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O2
endif
```

多个条件可组合使用，例如 `ifneq ($(A),)` 判断 `A` 非空，`ifeq ($(A)$(B),)` 判断 `A` 和 `B` 都为空。

## 11. 命令前缀 @ - +

命令默认会先打印出来再执行（除非加了 `-s` 或声明 `.SILENT`）。前缀放在命令行的**最开头**，用来改变这一行为：

| 前缀 | 作用 |
| --- | --- |
| `@` | 不显示命令本身，只显示命令的输出 |
| `-` | 命令返回非零（失败）时忽略错误，继续执行后续命令 |
| `+` | 即使使用 `make -n`（只打印）、`make -t`（只 touch）、`make -q`（只查询）也强制执行 |

```makefile
target:
	@echo "building..."           # 不打印 echo 本身
	-rm -f maybe-missing-file     # 文件不存在也不报错，make 继续
```

说明：

- `@` 最常用，让输出干净，只显示真正需要的信息。
- `-` 用于"失败也无所谓"的命令，比如清理一个可能不存在的文件。但不要滥用，否则可能掩盖真正的错误。
- `+` 常用于递归 `$(MAKE)`，保证 `make -n` 预览时递归调用依然会被展开。

前缀可以组合，顺序任意，如 `-@rm -f x`。

## 12. `$$` 转义

Make 自己使用 `$`（变量、自动变量）。如果想在命令里把 `$` 原样传给 Shell 或其他程序（如 awk、bash 脚本），要写成 `$$`：

```makefile
show:
	awk '{print $$9}'
```

Make 处理后会传给 Shell：

```bash
awk '{print $9}'
```

`$9` 才是 awk 理解的"第 9 个字段"。

## 13. 换行、注释与其他语法细节

- 注释用 `#`，从 `#` 到行尾都是注释。
- 长命令可以用反斜杠 `\` 换行：

```makefile
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ \
		$(LDLIBS)
```

- 变量名在 `$()` 中区分大小写，`$(CC)` 与 `$(cc)` 是不同变量。
- 命令之间默认每条都单独启动一个 Shell，变量赋值不跨行共享；需要跨行共享状态时，用 `\` 连接或用 `;` 写在一行。

## 14. 递归调用子目录 Makefile

大型项目分模块，每个模块有独立 Makefile，顶层用 `$(MAKE)` 递归调用：

```makefile
all:
	$(MAKE) -C kernel
	$(MAKE) -C driver
	$(MAKE) -C app
```

`$(MAKE)` 是当前 Make 程序的引用，等价于 `make`，但它能正确传递参数（如 `-j`、变量定义）。**递归调用一律用 `$(MAKE)`，不要直接写 `make`。**

## 15. 头文件依赖

如果 `.c` 里 `#include "foo.h"`，当 `foo.h` 修改时也必须重新编译。手写方式：

```makefile
main.o: main.c foo.h
	$(CC) $(CFLAGS) -c main.c -o main.o
```

但大型项目手工维护头文件依赖不现实，用 GCC 自动生成依赖文件：

```makefile
CFLAGS += -MMD -MP
-include $(OBJS:.o=.d)
```

- `-MMD`：编译时自动生成 `.d` 依赖文件（记录该源文件 include 的头文件）。
- `-MP`：为每个头文件生成空规则，防止头文件被删除时报错。
- `-include`：若 `.d` 文件不存在也不报错（首次构建时没有 `.d`）。

这是理解 Linux 内核、U-Boot 等大型 Makefile 的必备知识。

## 16. 多目录组织与 VPATH

源码放在子目录时，可以用 `VPATH` 或 `vpath` 指定搜索路径：

```makefile
VPATH = src:include
```

Make 在找不到依赖文件时，会到 `VPATH` 指定的目录里查找。更精细的按扩展名搜索用 `vpath`：

```makefile
vpath %.c src
vpath %.h include
```

## 17. 生产级完整模板

下面是一个可直接使用的单目录 C 项目模板，涵盖了自动收集源文件、头文件依赖、调试开关：

```makefile
CC := gcc

TARGET := app
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS := -Iinclude
CFLAGS := -Wall -Wextra -O2 -MMD -MP
LDFLAGS :=
LDLIBS :=

ifeq ($(DEBUG),1)
CFLAGS += -g -O0
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)
```

关键点：

- `CPPFLAGS` 放预处理选项（`-I`、`-D`），`CFLAGS` 放编译选项，两者在编译命令里都要出现。
- 链接时 `LDLIBS`（库）放在命令**末尾**，因为链接器按从左到右解析符号。
- `-MMD -MP` 配合 `-include` 自动处理头文件依赖。

## 18. GCC 常用变量与库链接

大型项目约定俗成的变量名：

| 变量 | 用途 |
| --- | --- |
| `CC` | C 编译器（gcc） |
| `CXX` | C++ 编译器（g++） |
| `AR` | 静态库打包工具（ar） |
| `LD` | 链接器（ld） |
| `CFLAGS` | C 编译选项 |
| `CPPFLAGS` | 预处理器选项（`-I`、`-D`） |
| `LDFLAGS` | 链接器选项（`-L`、`-s` 等） |
| `LDLIBS` | 要链接的库（`-lpthread` 等） |

标准编译命令：

```makefile
$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
```

链接动态库：

```makefile
LDLIBS += -pthread

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)
```

链接静态库：

```bash
gcc main.o -L./lib -lfoo -o app
```

- `-L./lib`：在 `./lib` 目录搜索库。
- `-lfoo`：搜索 `libfoo.so`（动态）或 `libfoo.a`（静态）。

## 19. 常用 make 命令

```bash
make              # 构建默认目标（Makefile 第一个目标）
make all          # 构建指定目标 all
make clean        # 执行 clean
make -j8          # 并行构建（最多 8 个任务）
make -C kernel    # 进入 kernel 目录执行
make CC=clang     # 命令行覆盖变量
make -n           # 只打印命令不执行（dry-run，调试用）
make -p           # 打印所有规则和变量（调试用）
make -d           # 输出详细调试信息
make V=1          # 显示完整编译命令（取决于项目约定）
```

常用选项详解：

| 选项 | 参数 | 含义 |
| --- | --- | --- |
| `-f file` | 文件名 | 指定 Makefile（默认依次找 `GNUmakefile`、`makefile`、`Makefile`） |
| `-C dir` | 目录 | 切换到 `dir` 再执行，等价 `cd dir && make` |
| `-j [N]` | 数字（可省略） | 并行执行，`N` 为最大并发任务数；`-j` 不带数字表示不限制 |
| `-n` | — | 只打印将要执行的命令，不真正执行 |
| `-B` | — | 强制全部重建，忽略时间戳（`--always-make`） |
| `-k` | — | 某个目标失败后继续构建其他无关目标（`--keep-going`） |
| `-s` | — | 静默模式，不打印命令（等价于所有命令都加 `@`） |
| `-p` | — | 打印所有规则和变量（调试用，信息量大） |
| `-d` | — | 输出 Make 决策过程（调试用，信息量极大） |
| `-I dir` | 目录 | 指定 `include` 指令的搜索目录 |

默认目标是 Makefile 中**第一个**目标，因此通常把 `all` 放在最前面：

```makefile
.PHONY: all
all: $(TARGET)
```

注意 `-C` 是 Make 的参数，与 GCC 的 `-c`（只编译不链接）无关，别混淆。

## 20. 常见问题与误区

1. **命令前用了空格** → 报 `*** missing separator. Stop.`，改成 Tab。
2. **`=` 和 `:=` 混淆** → 需要"定义时就确定值"的场景用 `:=`，避免递归展开导致意外结果或无限循环。
3. **依赖没写全** → 改了头文件不重新编译。用 `-MMD -MP` 自动依赖解决。
4. **库写错位置** → `-lfoo` 必须放在源文件/目标文件之后，否则链接器可能报 undefined reference。
5. **伪目标没声明 `.PHONY`** → 目录下出现同名文件时 `clean` 失效。
6. **递归调用写 `make` 而非 `$(MAKE)`** → 并行 `-j` 等参数无法正确传递。
7. **在命令里用 `$9` 而非 `$$9`** → Make 会尝试展开 `$9`（空），awk 收不到正确参数。

## 21. 核心知识速查

```text
规则        target: dependency
                	recipe            # Tab 缩进

变量        CC := gcc
            CFLAGS := -Wall

赋值        = 延迟    := 立即    ?= 未定义才赋值    += 追加

自动变量    $@ 目标    $< 第一个依赖    $^ 所有依赖

模式规则    %.o: %.c
                	$(CC) -c $< -o $@

函数        wildcard  patsubst  notdir  dir  addprefix  addsuffix
            filter  filter-out  subst  foreach  shell

伪目标      .PHONY: all clean

条件判断    ifeq / ifneq / ifdef / ifndef ... endif

命令前缀    @ 不显示    - 忽略错误    + 强制执行

转义        $$ → 传给 Shell 的字面量 $

递归        $(MAKE) -C dir

头文件依赖  CFLAGS += -MMD -MP ; -include $(OBJS:.o=.d)
```

## 22. 整体理解

一句话概括分工：

- **Make** 负责判断"什么时候执行、按什么依赖顺序执行"。
- **GCC** 负责"真正把源码变成目标文件和可执行文件"。

掌握了规则、变量、自动变量、模式规则和伪目标这五块，就能读懂和编写绝大多数 Makefile；其余函数和高级特性在遇到时再查即可。
