# AI Coding 测试报告

## 文档摘要

本文基于 `SPU` 项目，对本地部署的 AI Coding 能力进行了阶段性测试与记录，主要从以下三个方向展开：

1. 代码生成
2. 代码检查
3. UT 用例生成

整个测试流程均基于现有工程进行，测试环境为 `Qwen Code CLI + Qwen 122B 4bit` 量化模型。

## 测试范围

本文重点关注以下问题：

1. AI 是否能够在已有工程上下文中生成可用代码
2. AI 是否能够在不同范围和约束下发现代码问题
3. AI 是否能够辅助生成、修复并迭代完善 UT 用例
4. AI 是否适合作为 CI 流程中的辅助分析工具

## 目录

1. [代码生成](#代码生成)
2. [代码检查](#代码检查)
3. [UT 用例生成](#ut-用例生成)
4. [CI 持续集成](#ci-持续集成)

---

## 代码生成

在代码生成时，可以给出参考代码，并且指定代码风格。

### 简单接口实现

让 AI 帮助实现串口 `shell` 功能。

事先将对应的代码删除如下。

```c
    /* Get ADC value */
    else if(0 == strcmp((char *)msgBuf, "getAdcvalue")  && NULL == argv[0])
    {
        get_adcValue();
    }
    /* Get power */
    else if(0 == strcmp((char *)msgBuf, "getPower")  && NULL == argv[0])
    {
        get_Power();
    }
    /* Get CPU information */
    else if(0 == strcmp((char *)msgBuf, "cpuInfo")  && NULL == argv[0])
    {
        CPU_Information_Show();
    }
```

提示词如下：

```text
  > 你是一个资深的嵌入式软件工程师，当前目录是一个基于stm32f103vgt6的座椅电源SPU的工程源码。@Core/lib/shell/ShellCmd.c
    是一个命令行工具，支持串口输入指定的命令并进行操作。我现在希望你能够为我添加几条命令，比如命令行键入getAdcvalue 获取ADC的值，
    getPower获取功率值，cpuInfo获取CPU信息。如果有可以使用的的接口函数，请直接调用，如果没有的话请实现。代码风格请参照c99.

  ✦ 需要先了解现有代码结构，同时读取 ShellCmd.c、查找 ADC/功率/CPU 相关的接口函数、了解项目结构，以便实现新命令。
  ...

  ✦ 现在我已经了解了所有必要的接口。让我修改 ShellCmd.c 文件，添加这三个命令：
  
  /* Get ADC value */
  else if ((0 == strcmp((char *)msgBuf, "getAdcValue")) && (NULL == argv[0]))
  {
      get_adcValue();
  }

  /* Get power */
  else if ((0 == strcmp((char *)msgBuf, "getPower")) && (NULL == argv[0]))
  {
      get_Power();
  }

  /* Get CPU information */
  else if ((0 == strcmp((char *)msgBuf, "cpuInfo")) && (NULL == argv[0]))
  {
      CPU_Information_Show();
  }

  /* Get current EEPROM log count */
  else if ((0 == strcmp((char *)msgBuf, "logCount")) && (NULL == argv[0]))
  {
      /* ... */
  }

  ✦ 需要查看文件开头的 include 部分，确认已有的头文件引用是否足够支持新添加的命令函数调用。

```

简单的接口调用函数，AI 能够从工程中查找、理解并编写。

### 复杂代码实现

还是同样的代码，进一步删除它具体的接口实现，看看AI是否能够生成。

将具体的接口函数删除如下。

```c
void get_adcValue(void)
{
    printf("DC_current[%d]_sample:  %d\r\n",0,adc_buf[ChannelDC_Cur1]);
    printf("DC_current[%d]_sample:  %d\r\n",1,adc_buf[ChannelDC_Cur2]);
    printf("DC_current[%d]_sample:  %d\r\n",2,adc_buf[ChannelDC_Cur3]);
    printf("DC_voltage[%d]_sample:  %d\r\n",0,adc_buf[ChannelDC_Vol1]);
    printf("DC_voltage[%d]_sample:  %d\r\n",1,adc_buf[ChannelDC_Vol2]);
    printf("DC_voltage[%d]_sample:  %d\r\n",2,adc_buf[ChannelDC_Vol3]);
    printf("DC_COM_voltage_sample:  %d\r\n",adc_buf[ChannelDC_COM_Vol]);

    if (MODE_DC_AC == g_power_mode)
    {
        printf("Ac_current[%d]_sample:  %d\r\n",0,adc_buf[ChannelAC_Cur1]);
        printf("Ac_current[%d]_sample:  %d\r\n",1,adc_buf[ChannelAC_Cur2]);
        printf("Ac_current[%d]_sample:  %d\r\n",2,adc_buf[ChannelAC_Cur3]);
        printf("Ac_voltage_sample:      %d\r\n",adc_buf[ChannelAC_COM_Vol]);
    }

    printf("\r\n");

    printf("DC_current[%d]:  %fA\r\n",0,dev.dc_i[0]);
    printf("DC_current[%d]:  %fA\r\n",1,dev.dc_i[1]);
    printf("DC_current[%d]:  %fA\r\n",2,dev.dc_i[2]);
    printf("DC_voltage[%d]:  %fV\r\n",0,dev.dc_u[0]);
    printf("DC_voltage[%d]:  %fV\r\n",1,dev.dc_u[1]);
    printf("DC_voltage[%d]:  %fV\r\n",2,dev.dc_u[2]);
    printf("DC_COM_voltage:  %fV\r\n",dev.dc_COM_u);

    if (MODE_DC_AC == g_power_mode)
    {
        printf("Ac_current[%d]:  %fA\r\n",0,dev.ac_i[0]);
        printf("Ac_current[%d]:  %fA\r\n",1,dev.ac_i[1]);
        printf("Ac_current[%d]:  %fA\r\n",2,dev.ac_i[2]);
        printf("Ac_voltage:      %fV\r\n",dev.ac_u);
    }
}

void get_Power(void)
{
    for(int i = 0;i<3;i++)
    {
        printf("DC_channel[%d] power : %0.4fW\r\n",i,dev.dc_power[i]);
    }
    printf("DC_channel total power : %0.4fW\r\n",dev.dc_total_power);

    if (MODE_DC_AC == g_power_mode)
    {
        for(int i = 0;i<3;i++)
        {
            printf("AC_channel[%d] power : %0.4fW\r\n",i,dev.ac_power[i]);
        }

        printf("AC_channel total power : %0.4fW\r\n",dev.ac_total_power);
    }
}

void CPU_Information_Show(void)
{
    static uint8_t CPU_RunInfo[1024];
    memset(CPU_RunInfo, 0, sizeof(CPU_RunInfo));

    vTaskList((char *)CPU_RunInfo);

    printf("---------------------------------------------\r\n");
    printf("TaskName            State   Prio   Stack   Num\r\n");
    printf("%s", CPU_RunInfo);
    printf("---------------------------------------------\r\n\n");
}
```

提示词如下：

```text
> 你是一个资深的嵌入式软件工程师，当前目录是一个基于stm32f103vgt6的座椅电源SPU的工程源码。@Core/lib/shell/ShellCmd.c
    是一个命令行工具，支持串口输入指定的命令并进行操作。我现在希望你能够为我添加几条命令，比如命令行键入getAdcvalue 获取ADC的值，
    getPower获取功率值，cpuInfo获取CPU信息。如果有可以使用的的接口函数，请直接调用，如果没有的话请实现。代码风格请参照c99.
  ✦ 需要先查看 ShellCmd.c 的现有实现和代码结构，同时搜索项目中已有的 ADC、功率和 CPU 信息相关的接口函数，以便确定哪些可以直接调用，哪些需要新实现。
  ✦ 我来帮你为 ShellCmd.c 添加这些命令。首先让我分析一下现有代码结构和相关接口。
  ...
  ✦ 现在我已经了解了足够的信息。让我创建一个任务清单并开始实现：
  任务清单：
  - 在 Board.c 中实现 get_adcValue 函数
  - 在 Board.c 中实现 get_Power 函数
  - 在 Board.c 中实现 CPU_Information_Show 函数
  - 在 ShellCmd.c 中添加 getAdcvalue 命令
  - 在 ShellCmd.c 中添加 getPower 命令
  - 在 ShellCmd.c 中添加 cpuInfo 命令
  ...
  ✦ 现在让我验证一下代码，确保没有编译错误：
  ✦ 编译成功！但有一些警告需要修复。让我修复这些警告：
  ...
  ✦ 编译成功，无警告和错误。现在让我总结一下为你添加的命令功能：

    已完成的工作

    我已经在 Shell 命令系统中添加了以下三条命令：

    1. getAdcvalue - 获取 ADC 值
    显示所有 ADC 通道的原始值及转换后的电气参数：
     - DC 通道电流（DC1/DC2/DC3）
     - DC 电压
     - AC 电压
     - AC 通道电流（AC1/AC2/AC3）

    2. getPower - 获取功率值
    显示当前各通道的功率信息：
     - 各 DC 通道功率及开关状态
     - DC 总功率
     - 各 AC 通道功率及开关状态
     - AC 总功率
     - 总功率

    3. cpuInfo - 获取 CPU 信息
    显示 MCU 相关信息：
     - MCU 类型（STM32F103VG）
     - 设备 ID
     - 版本号
     - 系统时钟（72 MHz）
     - RAM/Flash 大小
     - 运行时间
     - SPU 地址

    修改的文件
     1. Core/board/Board.c - 实现了 get_adcValue()、get_Power()、CPU_Information_Show() 三个函数
     2. Core/lib/shell/ShellCmd.c - 添加了三个命令的解析逻辑

    你可以在串口终端中输入这些命令进行测试：

     1 getAdcvalue
     2 getPower
     3 cpuInfo
```

生成的代码如下：

```c
void get_adcValue(void)
{
    float tmp_val;

    /* DC current conversion (ADC value -> actual current) */
    printf("=== DC Channels ===\r\n");
    for (uint8_t i = 0; i < 3; i++) {
        tmp_val = Convert_DC_Current(adc_buf[ChannelDC_Cur1 + i]);
        printf("DC%d Current: ADC=%4u, Value=%.2f A\r\n",
                i + 1, adc_buf[ChannelDC_Cur1 + i], tmp_val);
    }

    /* DC voltage conversion (ADC value -> actual voltage) */
    tmp_val = Convert_DC_Voltage(adc_buf[ChannelDC_COM_Vol]);
    printf("DC Voltage: ADC=%4u, Value=%.2f V\r\n",
            adc_buf[ChannelDC_COM_Vol], tmp_val);

    /* AC voltage conversion */
    tmp_val = Convert_AC_Voltage();
    printf("AC Voltage: %.2f V\r\n", tmp_val);

    /* AC current conversion (requires zero-crossing detection) */
    printf("=== AC Channels ===\r\n");
    for (uint8_t i = 0; i < 3; i++) {
        tmp_val = Convert_AC_Current(i);
        printf("AC%d Current: %.2f A\r\n", i + 1, tmp_val);
    }
}

void get_Power(void)
{
    printf("=== Power Information ===\r\n");

    /* DC power information */
    printf("=== DC Channels ===\r\n");
    for (uint8_t i = 0; i < 3; i++) {
        printf("DC%d: Power=%.2f W, Switch=%s\r\n",
                i + 1,
                dev.dc_power[i],
                dev.dc_switch[i] ? "ON" : "OFF");
    }
    printf("DC Total Power: %.2f W\r\n", dev.dc_total_power);

    /* AC power information */
    printf("=== AC Channels ===\r\n");
    for (uint8_t i = 0; i < 3; i++) {
        printf("AC%d: Power=%.2f W, Switch=%s\r\n",
                i + 1,
                dev.ac_power[i],
                dev.ac_switch[i] ? "ON" : "OFF");
    }
    printf("AC Total Power: %.2f W\r\n", dev.ac_total_power);

    /* Total power */
    printf("Total Power: %.2f W\r\n", dev.total_power);
}

void CPU_Information_Show(void)
{
    uint32_t uptime_ms = HAL_GetTick();
    uint32_t cpu_id_low = READ_REG(DBGMCU->IDCODE) & 0x0FFF;
    uint32_t revision = (READ_REG(DBGMCU->IDCODE) >> 16) & 0xFFF;

    printf("=== CPU Information ===\r\n");
    printf("MCU Type: STM32F103VG\r\n");
    printf("Device ID: 0x%04lX\r\n", cpu_id_low);
    printf("Revision: 0x%03lX\r\n", revision);
    printf("System Clock: 72 MHz\r\n");
    printf("RAM: 64 KB\r\n");
    printf("Flash: 1024 KB\r\n");
    printf("Uptime: %lu ms\r\n", uptime_ms);
    printf("SPU Address: %#x\r\n", getDevAddr());
}
```

AI 能够生成一个基本的 demo，且可以编译通过；但是由于需求不够清晰，仍会缺少部分功能。
> get_adcValue中，未输出DC电压，没有根据全局变量选择模式。也没有打印转换后的值
> get_Power中，功能基本实现
> CPU_Information_Show，由于需求不清晰，打印信息与原本的代码不太一样。

AI 能够读取工程源码的上下文并生成可用代码，不过在需求不够清晰的情况下，生成结果会出现偏差。

### 小结

给定需求，AI能够生成一些逻辑简单的接口函数，还可以依靠阅读源码寻找依赖。但是复杂的业务逻辑代码需要更加详细的需求描述和边界限定。

---

## 代码检查

代码检查主要分为静态检查和逻辑检查。静态检查可以直接调用 `cppcheck` 或 `pclint` 等工具，由 Agent 或人工执行差异不大；而逻辑检查则存在更多影响因素。
### 整体检查 | 目标不明确

在源码中注入了数组越界代码：

```c
power_detect.h
    bool ac_connected[3];    // 标记是否接入
power_detect.c
    dev.ac_connected[5] = plug1.connected;
```

首先是直接让Agent分析`Core/lib`下的所有源码，询问其是否存在逻辑问题：

```
> 请帮我检查一下@Core/lib/ ，该目录下的源码是否存在代码逻辑流程问题
✦ 需要先查看 Core/lib/ 目录下有哪些源文件，才能进行代码逻辑检查。
  ...
高优先级问题
    1. 空指针解引用风险
    2. 竞态条件/并发问题
中优先级问题
    3. 缓冲区溢出风险
    4. 资源未正确关闭
    5. 状态机逻辑错误
    6. 未初始化变量使用
低优先级问题
    7. 命名/逻辑混淆
```

```text
> 请帮我检查一下@Core/lib/ ，该目录下的源码是否存在代码逻辑流程问题
✦ 需要先查看 Core/lib/ 目录下有哪些源文件，才能进行代码逻辑检查。
✓ ListFiles Core/lib
  Listed 19 item(s)
✦ 目录包含 19 个子目录，需要搜索代码中的逻辑流程问题模式如死循环、条件判断和状态机，同时限制搜索范围以提高效率。
✓ Agent 检查 Core/lib 代码逻辑
  Explore completed
  Execution Summary: 35 tool uses · 1,723,497 tokens · 3m 23s
✦ 代理已完成代码分析并返回了详细报告，现在需要将这份报告整理后呈现给用户，总结主要问题并提供后续建议。
✦ 已完成对 Core/lib/ 目录下源码的静态代码分析。以下是发现的主要逻辑流程问题汇总：

高优先级问题
1. 空指针解引用风险
  - syslog/syslog.c: log_format_and_add() 中 fmt 参数未做 NULL 检查
  - onewire/onewire.c: DS2488_Read_Buffer() 中 result 参数未做 NULL 检查
  - power_detect.c: Remove_From_Order() 中 order 和 count 指针未做 NULL 检查
2. 竞态条件/并发问题
  - power_detect.c: 全局变量 dev 在多任务中访问但无互斥锁保护
  - gpio/gpio.c: g_pulseSignalCnt 在中断中递增、主循环读取，无原子操作保护
  - syslog/syslog.c: flush_log_batch() 使用 portMAX_DELAY 等待互斥锁，可能导致优先级反转

中优先级问题
3. 缓冲区溢出风险
  - shell/ShellCmd.c: addChar() 边界检查条件 len + 1 >= CMD_LEN_MAX 可能有误
  - syslog/syslog.c: level 参数可能越界访问 g_logLevelStr 数组
4. 资源未正确关闭
  - usart/usart.c: HAL_UART_ErrorCallback() 中错误处理可能导致忙等待死循环
  - eeprom/eeprom.c: eepromByteWrite() 中设备无响应时可能无限循环
5. 状态机逻辑错误
  - lin/lin_master.c: 状态机使用多个 if 而非 else if，可能导致状态跳跃过快
  - lin/lin_slave.c: SYNC 字节校验失败时静默丢弃，未通知上层
6. 未初始化变量使用
  - led/LED.c: CheckPlugState() 中 curr_state 逻辑分支覆盖不完整

低优先级问题
7. 命名/逻辑混淆
  - power_detect.c: AC_OVERVOLT_LOW 命名令人困惑（过压保护下限）

如需我提供具体文件的详细修复建议或生成修复代码，请告诉我具体文件。
```

**`Core/lib`下的代码量约为10k，在提示词无限定的情况下，AI并不能够发现注入的问题。多次询问下代码的风险项会有不一致的情况。简言之，AI不能一次性发现大代码量下的所有问题。**

### 整体检查 | 目标明确

同样的错误代码，也是询问整体目录，但是限定只查找数组越界问题：

```
> 请检查@Core/lib/ 下是否有数组越界的风险
✦ 用户要求专门检查数组越界风险 ...
  数组越界风险分析报告
高风险漏洞
1. Core/lib/shell/ShellCmd.c - DBG_LOG 格式错误
2. Core/lib/signal/signal_processing.c - channel 参数无边界检查
3. Core/lib/onewire/onewire.c - blen 值未限制
中风险漏洞
4. Core/lib/lin/lin_master.c - 边界检查依赖外部保证
5. Core/lib/syslog/syslog.c - 缓冲区大小未检查
6. Core/lib/message/Message.c - 长度计算错误
  ...	
```



```text
> 请检查@Core/lib/ 下是否有数组越界的风险
✦ 用户要求专门检查数组越界风险，需要启动一个专门的探索代理来全面扫描 Core/lib 目录下所有源文件中的数组访问和边界检查情况。
✓ Agent 检查数组越界风险
  Explore completed
  Execution Summary: 43 tool uses · 2,281,329 tokens · 4m 25s

	IMPORTANT: This conversation approached the input token limit for Qwen3.5-122B-A10B-Q4_K_M-00001-of-00003.gguf. A compressed context 	will be sent for future messages.

✦ 亚代理已返回完整的数组越界风险分析报告，我需要将这份详细报告整理呈现给用户，并按高/中/低风险分类，同时询问用户是否需要进一步行动。
✦ 根据全面的静态代码分析，我在 Core/lib/ 目录下发现了多个数组越界风险点，现分类呈现如下：

数组越界风险分析报告
高风险漏洞
1. Core/lib/shell/ShellCmd.c - DBG_LOG 格式错误
 - 行号: 719-726
 - 问题: 用 %s 打印非字符串指针
   1 DBG_LOG(LogError, "The command is: %s, %s, %s, %s, %s, %s",
   2         msgBuf, argv[0], argv[1], argv[2], argv[3], argv[4]);
 - 风险: 当 argv[i] 指向 parameter 数组时，%s 会将其值当作指针地址读取，导致非法内存访问
 - 触发条件: 任何命令执行失败时都会触发
2. Core/lib/signal/signal_processing.c - channel 参数无边界检查
 - 行号: 49, 162-168
 - 代码:
   1 bool Wait_AC_Current_ZeroCross(uint8_t channel)
   2 {
   3     while (1) {
   4         v = (int16_t)adc_buf[channel] - ADC_MID;  // channel 未验证
   5         // ...
   6     }
   7 }
 - 风险: adc_buf 大小为 11 (ChannelBottom)，但 channel 参数无任何范围检查
 - 触发条件: 调用者传入 channel >= 11 或 channel = 255
3. Core/lib/onewire/onewire.c - blen 值未限制
 - 行号: 215-222
 - 问题: DS2488_Write_Buffer 函数中 result->blen 由调用者设置，无上限检查
 - 风险: 若调用者设置 blen > 8，将导致 result->data[i] 越界访问

中风险漏洞
4. Core/lib/lin/lin_master.c - 边界检查依赖外部保证
 - 行号: 245-258
 - 代码:
   1 if (g_rx.idx < (uint8_t)(g_rx.len + 1)) {
   2     g_rx.buf[g_rx.idx++] = byte;
   3 }
 - 风险: 依赖 g_rx.len <= 8，若该值被篡改将越界
5. Core/lib/syslog/syslog.c - 缓冲区大小未检查
 - 行号: 288-308
 - 问题: log_read_latest 函数未检查调用者传入的 buf 缓冲区大小
 - 风险: buf + count * LOG_ITEM_SIZE 可能越界
6. Core/lib/message/Message.c - 长度计算错误
 - 行号: 80-100
 - 问题: getCheckSum 中 msgLen 减少 2 但按 16 位指针读取，奇数长度时会出问题

低风险问题
- adc/adc.c: adc_buf 多处访问无统一边界检查
- led/LED.c: IUL_gpio_map[3] 依赖循环条件保证
- power/power_detect.c: Remove_From_Order 的 order 数组大小依赖调用者保证

建议立即修复
1. signal_processing.c - 添加 channel 验证
   bool Wait_AC_Current_ZeroCross(uint8_t channel)
   {
       if (channel >= ChannelBottom) {
           return false;
       }
       // ...
   }

2. ShellCmd.c - 修复格式字符串
   DBG_LOG(LogError, "The command is: %s, %p, %p, %p, %p, %p",
           msgBuf, argv[0], argv[1], argv[2], argv[3], argv[4]);
```

**依然没有查找到具体的错误代码，并且在同一个对话中两次全量读取`/Core/lib`下的代码便达到了对话上下文的限制。**

### 单一模块 | 目标不明确

#### 简单错误

同样的错误代码，但是指定具体的文件吗，让其检查是否有逻辑错误：

```
 ✦ 经过详细分析 Core/lib/power/power_detect.c 代码，我发现了以下代码逻辑流程问题：
     🔴 严重逻辑问题
    1. Device_Init() - 插口检测逻辑错误 (行 138-172)
     1 // 问题：每次读取都覆盖 dev.ac_connected 数组，导致之前检测的结果被丢失
     2 dev.ac_connected[5] = plug1.connected;  // 索引 5 越界！数组只有 3 个元素
     3 dev.ac_connected[1] = plug2.connected;  // 应该是 ac_connected[0]
     4 dev.ac_connected[2] = plug3.connected;  // 应该是 ac_connected[1]
    问题：
     - dev.ac_connected[5] 数组越界访问（数组大小为 3）
     - 索引不连续：应该是 [0], [1], [2] 而不是 [5], [1], [2]
     ...
```



```text
  > 请帮我检查一下@Core/lib/power/power_detect.c ，该源码是否存在代码逻辑流程问题
  ✦ 用户要求检查 power_detect.c 的代码逻辑流程问题，需要先读取该文件的内容。
  │ ✓  ReadFile Core/lib/power/power_detect.c
  │    Read lines 1-717 of 1404 from Core/lib/power/power_detect.c (truncated)
  ✦ 文件有 1404 行但只显示了前 717 行，需要读取剩余部分以完整分析代码逻辑流程问题。
  │ ✓  ReadFile Core/lib/power/power_detect.c (lines 718-1417)
  │    Read lines 718-1404 of 1404 from Core/lib/power/power_detect.c
  ✦ 已读取完整的 power_detect.c 文件内容，现在需要分析代码逻辑流程问题并向用户总结发现的严重问题、潜在风险和代码建议。
  ✦ 经过详细分析 Core/lib/power/power_detect.c 代码，我发现了以下代码逻辑流程问题：
    🔴 严重逻辑问题
    1. Device_Init() - 插口检测逻辑错误 (行 138-172)
     1 // 问题：每次读取都覆盖 dev.ac_connected 数组，导致之前检测的结果被丢失
     2 dev.ac_connected[5] = plug1.connected;  // 索引 5 越界！数组只有 3 个元素
     3 dev.ac_connected[1] = plug2.connected;  // 应该是 ac_connected[0]
     4 dev.ac_connected[2] = plug3.connected;  // 应该是 ac_connected[1]
    问题：
     - dev.ac_connected[5] 数组越界访问（数组大小为 3）
     - 索引不连续：应该是 [0], [1], [2] 而不是 [5], [1], [2]
    ---
    2. Operation_status_LED() - 直流连接检测使用硬编码索引 (行 919-932)
     1 // 硬编码使用 adc_buf[4 + i]，应该使用 ChannelDC_Cur1
     2 dev.dc_i[i] = Convert_DC_Current(adc_buf[4 + i]);  // 应该是 adc_buf[ChannelDC_Cur1 + i]
    问题： 与 Read_And_Convert_Signals() 函数中使用的索引不一致，可能导致读取错误的 ADC 通道。
    ---
    3. Check_Fault_Recovery() - 直流欠压恢复逻辑注释但变量赋值错误 (行 1057-1078)
     1 // 被注释的代码中使用了错误的变量
     2 dev.dc_current_fault[i] = 0;  // 应该是 dev.dc_undervoltage_fault[i] = 0
    ---
    4. AC_Output_Control() - 逻辑编号跳跃 (行 817)
     1 // 1. 系统上电状态管理
     2 ...
     3 // 3. 遍历 3 路交流通道处理  // 应该是 2.
    ---
    5. temp_detect_protect() - 温度恢复逻辑存在竞态风险
     1 // 在恢复区间，g_recover_step 递增逻辑可能跳过某些通道
     2 // 如果某个通道的 ac_temp_cut_flag/dc_temp_cut_flag 为 0，会跳过但不会重置步骤
    问题： 当温度在恢复阈值附近波动时，可能导致部分通道无法恢复。
    ---
    🟡 潜在问题
    6. Calculate_Power() - 功率切断后未更新总功率 (行 650-671)
     1 if(dev.total_power > TOTAL_MAX) {
     2     if (DC_Power_Cut_Power() == 0){
     3         AC_Power_Cut_Power();
     4     }
     5     // 切断后没有重新计算 total_power，可能导致连续触发
     6 }
    ---
    7. IUL_Check_And_Control() - DC 模式下的 AC 连接检查
     1 if(g_power_mode == MODE_DC_AC) {
     2     need_activate = (dev.ac_connected[ch] || ...);
     3 } else {
     4     need_activate = (dev.dc_connected[ch] || ...);
     5 }
    问题： DC 模式下，dev.ac_connected[ch] 的值可能未初始化或无效。
    ---
    8. IUL_Fault_Blink() - 故障检测逻辑中 DC 模式下的 AC 故障检查 (行 1266-1280)
      1 else  // DC 模式
      2 {
      3     if (
      4         //HAL_GPIO_ReadPin(OC_IUL_gpio_map[ch].port, OC_IUL_gpio_map[ch].pin) == GPIO_PIN_RESET ||
      5         dev.dc_voltage_fault ||
      6         dev.dc_current_fault[ch] ||
      7         g_current_temp > TEMP_OVER_THRESHOLD
      8     )
      9     {
     10         current_fault = 1;
     11     }
     12 }
    问题： 注释掉的硬件检查可能导致漏检某些故障。
    ---
    建议修复清单
    - 🔴 高: Device_Init() 数组越界
      建议：将 [5] 改为 [0]，索引改为 [0], [1], [2]
    - 🔴 高: Operation_status_LED() 硬编码索引
      建议：改为使用 ChannelDC_Cur1 常量
    - 🟡 中: Calculate_Power() 未更新总功率
      建议：切断后调用 Calculate_DC_Total_Power() 更新
    - 🟡 中: temp_detect_protect() 恢复竞态
      建议：添加温度波动防抖逻辑
    - 🟢 低: 代码注释编号
      建议：修正逻辑编号

```

**显式的数组越界问题能够检查出来。**

#### 复杂错误

然后尝试注入更为复杂的越界问题：

```c
    port0_in = MAX7312_Read(IN_PORT0);
    // 检测插口1（IO0和IO1），更新状态
    plug1.connected = CheckPlugState(IO0_MASK, IO1_MASK, &plug1) ? 1 : 0;
    dev.ac_connected[port0_in] = plug1.connected;
```

为数组索引赋一个毫不相关的变量，并且只告诉AI能否发现代码逻辑问题：

```
✦ 我已经完整阅读了 power_detect.c 文件。现在为您分析存在的代码逻辑流程问题：

    代码逻辑流程问题分析
    1. `temp_detect_protect()` 函数：温度恢复逻辑存在死锁风险
    2. `Calculate_Power()` 函数：功率切断后未重新计算总功率
    3. `Check_Fault_Recovery()` 函数：过流恢复试探逻辑存在竞态条件
    4. `AC_Output_Control()` 函数：逻辑流程编号混乱
    5. `Operation_status_LED()` 函数：DC 连接状态检测被硬编码测试代码覆盖
    ...
    
```



```text
  > 请帮我检查一下@Core/lib/power/power_detect.c ，该源码是否存在代码逻辑流程问题
  ✦ 用户要求检查指定源码文件是否存在逻辑流程问题，我需要先读取该文件内容才能进行分析。
  │ ✓  ReadFile Core/lib/power/power_detect.c
  │    Read lines 1-717 of 1404 from Core/lib/power/power_detect.c (truncated)
  ✦ 文件有 1404 行但只返回了前 717 行，需要继续读取剩余部分才能完整分析代码逻辑问题。
  ✦ 让我继续读取文件的剩余部分：
  │ ✓  ReadFile Core/lib/power/power_detect.c (lines 718-1417)
  │    Read lines 718-1404 of 1404 from Core/lib/power/power_detect.c
  ✦ 文件已完整读取，现在需要分析代码逻辑流程问题并生成结构化的中文报告。
  ✦ 我已经完整阅读了 power_detect.c 文件。现在为您分析存在的代码逻辑流程问题：
    代码逻辑流程问题分析
    1. `temp_detect_protect()` 函数：温度恢复逻辑存在死锁风险
    问题位置：第 650-720 行
    问题分析：
     - 在恢复区间 (g_current_temp < TEMP_RECOVER_THRESHOLD)，代码使用 while 循环查找下一个需要恢复的通道
     - 但 recover_channel() 函数中，对于 AC 通道，检查的是 dev.ac_need_replug[ch] == 0，而非 dev.ac_temp_cut_flag[ch]
     - 这导致因温度切断的 AC 通道可能无法恢复（因为 ac_need_replug 可能为 1）
     1 // recover_channel 中
     2 case 0:
     3     if(dev.ac_need_replug[0] == 0){  // 问题：应检查 ac_temp_cut_flag
     4         dev.ac_switch[0] = 1;
     5         ...
     6     }
     7     break;
    ---
    2. `Calculate_Power()` 函数：功率切断后未重新计算总功率
    问题位置：第 570-590 行
    问题分析：
     - 在 MODE_ONLY_DC 模式下，切断直流功率后没有重新计算 dev.dc_total_power
     - 这可能导致连续的功率超限判断基于过时的数据
     1 else{
     2     if(dev.dc_total_power > DC_TOTAL_MAX_MODE_ONLY_DC) {
     3         DC_Power_Cut_Power();
     4         // 缺少：Calculate_DC_Total_Power();
     5     }
     6 }
    ---
    3. `Check_Fault_Recovery()` 函数：过流恢复试探逻辑存在竞态条件
    问题位置：第 1050-1100 行
    问题分析：
     - 在试探期间读取电流 test_current = Convert_DC_Current(adc_buf[ChannelDC_Cur1+i])
     - 但试探只持续 10ms，此时电流可能尚未稳定，导致误判
     - 另外，在打开输出后没有等待足够的稳定时间就立即关闭
    ---
    4. `AC_Output_Control()` 函数：逻辑流程编号混乱
    问题位置：第 830-900 行
    问题分析：
     - 注释中标注了 "1.", "1.1", "3."，缺少 "2."
     - 虽然不影响功能，但代码可读性较差
    ---
    5. `Operation_status_LED()` 函数：DC 连接状态检测被硬编码测试代码覆盖
    问题位置：第 950-970 行
    问题分析：
     - 存在测试代码块（////////////////////test//////////////），使用硬编码的 ADC 索引 4+i
     - 正常代码中使用的是 ChannelDC_Cur1+i
     - 测试代码会覆盖正常的 DC 连接状态检测逻辑
     1 ////////////////////test/////////////////////////////////////////////////////////
     2 for (uint8_t i = 0; i < 3; i++) {
     3     dev.dc_i[i] = Convert_DC_Current(adc_buf[4 +i]);  // 硬编码索引
     4     ...
     5 }
     6 ////////////////////test////////////////////////////////////////////////////////////
    ---
    6. `IUL_Fault_Blink()` 函数：故障恢复后闪烁状态未正确重置
    问题位置：第 1240-1260 行
    问题分析：
     - 当故障恢复时，fault_detected[ch] 被置为 0，但 fault_timeout_latched[ch] 也被置为 0
     - 这导致如果故障再次出现，5 分钟超时计数会重新开始，而非延续之前的状态
    ---
    7. `Device_Init()` 函数：AC 通道顺序数组索引错误
    问题位置：第 130-160 行
    问题分析：
      1 if (1 == plug1.connected) {
      2     dev.ac_order[dev.ac_count] = 0;
      3     dev.ac_count++;
      4 }
      5 // ...
      6 plug2.connected = ...;
      7 dev.ac_connected[1] = plug2.connected;  // 硬编码为 1，应使用 dev.ac_count 或其他逻辑
      8 if (plug2.connected) {
      9     dev.ac_order[dev.ac_count] = 1;
     10     dev.ac_count++;
     11 }
     - dev.ac_connected[] 的索引使用硬编码值，而非基于实际通道编号
```

**结果是AI甚至认为该变量是对的，其他的索引赋值有错误。** 复杂的逻辑错误甚至可能污染AI判断。

### 单一模块 | 目标明确

现在明确告诉AI检查具体模块的具体哪一类错误：

```
  ✦ 现在我已经完整了解了代码结构。下面是详细的数组越界风险分析报告：
    ---
    数组越界风险分析报告
    ...
    3. `Device_Init()` 函数：`dev.ac_connected[]` 索引使用错误
    位置：第 145-175 行
    问题：
      1 // 插口 1
      2 plug1.connected = CheckPlugState(IO0_MASK, IO1_MASK, &plug1) ? 1 : 0;
      3 dev.ac_connected[port0_in] = plug1.connected;  // port0_in 可能超出 [0,2] 范围！
      4 if (1 == plug1.connected) {
      5     dev.ac_order[dev.ac_count] = 0;
      6     dev.ac_count++;
      7 }
      8
      9 // 插口 2
     10 plug2.connected = CheckPlugState(IO2_MASK, IO3_MASK, &plug2) ? 1 : 0;
     11 dev.ac_connected[1] = plug2.connected;  // 硬编码为 1
    风险：
     - port0_in 是从 MAX7312_Read(IN_PORT0) 读取的值，范围可能是 0-255，直接用作数组索引可能越界
     - 逻辑不一致：插口 1 使用 port0_in，插口 2 硬编码为 1，插口 3 硬编码为 2
```



```text
  > 请帮我检查一下@Core/lib/power/power_detect.c ,改源码是否存在数组越界的风险
  ✦ 我已经阅读了完整的文件，现在需要系统地检查所有数组访问操作，识别潜在的越界风险点。
  │ ✓  ReadFile Core/lib/power/power_detect.c
  │    Read lines 1-717 of 1404 from Core/lib/power/power_detect.c (truncated)
  ✦ 继续读取文件剩余部分以完成数组越界风险的全面检查。
  │ ✓  ReadFile Core/lib/power/power_detect.c (lines 718-1417)
  │    Read lines 718-1404 of 1404 from Core/lib/power/power_detect.c
  ✦ 为了全面分析数组越界风险，需要查看 PowerDevice 结构体的定义以确认各数组字段的大小。
  ✦ 为了全面分析数组越界风险，我需要查看 PowerDevice 结构体的定义：
  │ ✓  Grep 'typedef.*PowerDevice|struct.*PowerDevice|PowerDevice.*\{' in path '/home/public/test/ai_test/master-F103VGT6/Core'
  │    No matches found
  ✦ 搜索 PowerDevice 结构体定义未在 Core 目录找到，需要尝试搜索 power_detect.h 头文件来获取结构体定义。
  │ ✓  Glob '**/power_detect.h'
  │    Found 1 matching file(s)
  ✦ 需要读取 power_detect.h 头文件以查看 PowerDevice 结构体定义，从而确认数组大小以完整分析越界风险。
  │ ✓  ReadFile Core/lib/power/power_detect.h
  ✦ 已获取 PowerDevice 结构体定义，现在可以全面分析代码中的数组越界风险，包括 DC/AC 切断函数、温度恢复函数、Device_Init 函数、Update_Device_Status 函数、AC_Output_Control 函数以及 IUL
    检测函数中的潜在问题。
  ✦ 现在我已经完整了解了代码结构。下面是详细的数组越界风险分析报告：
    ---
    数组越界风险分析报告
    🔴 高风险问题
    1. `AC_Power_Cut_Power()` 函数：缺少通道索引有效性检查
    位置：第 340-355 行
    问题：
     1 for (int i = dev.ac_count - 1; i >= 0; i--) {
     2     uint8_t ch = dev.ac_order[i];
     3     if (dev.ac_connected[ch] && dev.ac_switch[ch]) {  // 直接使用 ch 作为索引，未检查范围
     4         ...
     5     }
     6 }
    风险：如果 dev.ac_order[i] 的值超出 [0, 2] 范围（例如被初始化为 0xff 或未正确初始化），将导致数组越界访问 dev.ac_connected[ch] 和 dev.ac_switch[ch]。
    对比：DC_Power_Cut_Power() 和 DC_Power_Cut_Temp() 有检查 if (ch >= 3)，但 AC 版本缺失此检查。
    ---
    2. `AC_Power_Cut_Temp()` 函数：同样缺少通道索引有效性检查
    位置：第 362-377 行
    问题：与 AC_Power_Cut_Power() 相同，缺少 if (ch >= 3) 检查。
    ---
    3. `Device_Init()` 函数：`dev.ac_connected[]` 索引使用错误
    位置：第 145-175 行
    问题：
      1 // 插口 1
      2 plug1.connected = CheckPlugState(IO0_MASK, IO1_MASK, &plug1) ? 1 : 0;
      3 dev.ac_connected[port0_in] = plug1.connected;  // port0_in 可能超出 [0,2] 范围！
      4 if (1 == plug1.connected) {
      5     dev.ac_order[dev.ac_count] = 0;
      6     dev.ac_count++;
      7 }
      8
      9 // 插口 2
     10 plug2.connected = CheckPlugState(IO2_MASK, IO3_MASK, &plug2) ? 1 : 0;
     11 dev.ac_connected[1] = plug2.connected;  // 硬编码为 1
    风险：
     - port0_in 是从 MAX7312_Read(IN_PORT0) 读取的值，范围可能是 0-255，直接用作数组索引可能越界
     - 逻辑不一致：插口 1 使用 port0_in，插口 2 硬编码为 1，插口 3 硬编码为 2
    ---
    4. `Remove_From_Order()` 函数：循环边界可能导致越界写入
    位置：第 186-203 行
    问题：
     1 if (pos != 0xff) {
     2     for (int i = pos; i < *count - 1; i++) {
     3         order[i] = order[i + 1];  // 当 i = *count - 2 时，i+1 = *count - 1，未越界
     4     }
     5     order[*count - 1] = 0xff;  // 但如果 *count = 0，这里会越界！
     6     (*count)--;
     7 }
    风险：虽然前面有 if (*count == 0) return; 保护，但如果 *count 在函数执行过程中被其他线程修改（多线程环境），仍可能导致越界。
    ---
    🟡 中风险问题
    5. `temp_detect_protect()` 函数：`g_recover_step` 可能越界访问 `recover_channel()`
    位置：第 650-720 行
    问题：
      1 while (g_recover_step < 6) {
      2     switch (g_recover_step) {
      3         case 0: is_temp_cut = dev.ac_temp_cut_flag[0]; break;
      4         ...
      5         case 5: is_temp_cut = dev.dc_temp_cut_flag[2]; break;
      6     }
      7     if (!is_temp_cut) {
      8         g_recover_step++;  // 可能增加到 6
      9     } else {
     10         break;
     11     }
     12 }
     13
     14 if (g_recover_step < 6) {
     15     recover_channel(g_recover_step, g_current_temp);  // 安全
     16 }
    风险：虽然 recover_channel() 内部有 switch 处理 case 0-5 和 default，但如果 g_recover_step 因某种原因超过 6，会进入 default 分支不做任何操作，逻辑上没问题但不严谨。
    ---
    6. `Update_Device_Status()` 函数：`dev.dc_order[]` 写入前边界检查不足
    位置：第 740-765 行
    问题：
     1 if (dev.dc_connected[i] && !dev.dc_prev_connected[i]) {
     2     if (dev.dc_count < 3) {
     3         dev.dc_order[dev.dc_count] = i;  // i 范围是 0-2，安全
     4         dev.dc_count++;
     5     }
     6 }
    风险：虽然 dev.dc_count < 3 检查存在，但如果 i 的值意外超出 [0, 2]（例如从外部传入的错误数据），仍可能导致逻辑错误。
    ---
    🟢 低风险问题
    7. `recover_channel()` 函数：step 参数未做有效性验证
    位置：第 384-420 行
    问题：函数使用 switch(step) 处理 case 0-5，default 分支为空。如果外部传入的 step 值异常，不会有任何响应，但也不会导致数组越界。
    ---
    8. `AC_Output_Control()` 函数：`switch(ch)` 中 ch 范围受 for 循环限制
    位置：第 870-890 行
    问题：for (uint8_t ch = 0; ch < 3; ch++) 确保 ch 范围是 [0, 2]，switch(ch) 的 case 0/1/2 覆盖完整，相对安全。
    ---
    总结与建议
    1. AC_Power_Cut_Power() | 🔴 高
       问题：缺少 ch >= 3 边界检查
       建议：添加与 DC 版本相同的检查
    2. AC_Power_Cut_Temp() | 🔴 高
       问题：缺少 ch >= 3 边界检查
       建议：添加与 DC 版本相同的检查
    3. Device_Init() | 🔴 高
       问题：port0_in 直接用作数组索引
       建议：添加范围检查或使用固定索引
    4. Remove_From_Order() | 🟡 中
       问题：多线程环境下边界检查可能失效
       建议：考虑添加互斥保护
    5. temp_detect_protect() | 🟡 中
       问题：g_recover_step 未严格限制
       建议：添加 g_recover_step >= 6 的 early return

```

**在明确模块和指定问题后，AI能够发现问题。** 在使用AI辅助代码逻辑检查时，可能需要指定一定的规则，并且限制检查范围，才能够有比较好的效果。

### 小结

AI单词进行全局代码检查效果一般，并且存在单次多轮检查结果不一致的情况(其实就是没有一次性检查出所有问题，每一次检查结果都是子集)。并且对于复杂逻辑AI的判断能力较弱，甚至会污染判断。

在明确规则，限制范围的情况下，AI才能有一个比较好的表现。

---

## UT 用例生成

整体思路从： 全局生成UT用例 -> 单一模块生成UT用例 -> 全局先打桩再分模块生成UT -> 单一模块UT+可扩展测试架构，逐步生成多模块UT。

### 项目整体的 UT 生成

直接生成整个工程是不太现实的，需求和接口要求过多，目前的模型上下文不支持。

同时尝试了不生成UT用例，先为业务代码进行UT测试框架的搭建，划分好桩边界，让AI进行整个工程的桩函数生成。
生成效果不尽人意，HAL和FreeRTOS等外部库有大量函数定义，一次性全部进行打桩超出本地模型能力。

最后考虑进行单一模块的生成和评估。

### 单一模块的 UT 生成

#### 未进行任何规划的 UT 生成

**已有的测试架构下生成新模块的UT**

以一个已经有了基本测试框架的`SPU`源码，告诉AI基于现有的框架生成一个新模块(例如`i2c`)的UT测试。
(在此之前也是由AI辅助将原来的单一模块的测试框架重构为支持多模块的测试框架)。

```
> 你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。@UTest/
  下是一个UT用例框架，它可以在x86_64下编译运行，用来检查源码的逻辑。现在已经有了@Core/lib/eeprom的UT，我现在希望能够按照当前的UT测试架构，生成有关i2c模块的UT
  ✦ 需要先了解现有的 UTest 框架结构以及 Core/lib 目录下的代码，特别是 i2c 和 eeprom 模块的实现。
  ✦ 我来分析一下当前的 UTest 框架结构，然后为 i2c 模块生成 UT 用例。
  ...
  让我创建任务列表并开始生成 i2c 模块的 UT：
- 创建 UTest/lib/i2c 目录
- 创建 test_i2c.h 头文件
- 创建 test_i2c.c 测试用例文件
- 更新 UTest/board/main.c 注册 i2c 测试
- 更新 UTest/Makefile 添加 i2c 源文件
- 编译并运行测试验证
  ...
  ✦ 编译出现多个符号重复定义错误，需要从 test_i2c.c 中移除已在 eeprom 测试中定义的函数和变量。
  ✦ 有多个符号重复定义的问题。我需要修复：
  1. hi2c2 在 stub_hal_i2c.c 和 stub_i2c.c 中都有定义
  2. g_utContext 和 ut_run_test 在 test_eeprom.c 和 test_i2c.c 中重复定义
...
```



```text
> 你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。@UTest/
  下是一个UT用例框架，它可以在x86_64下编译运行，用来检查源码的逻辑。现在已经有了@Core/lib/eeprom的UT，我现在希望能够按照当前的UT测试架构，生成有关i2c模块的UT
✦ 需要先了解现有的 UTest 框架结构以及 Core/lib 目录下的代码，特别是 i2c 和 eeprom 模块的实现。
✦ 我来分析一下当前的 UTest 框架结构，然后为 i2c 模块生成 UT 用例。
...
✦ i2c 模块的源码主要是 HAL 库的初始化代码，没有额外的业务逻辑。让我查看一下是否有封装的 i2c 业务逻辑头文件：
...
✦ i2c 模块主要是 HAL 初始化代码，没有额外的业务逻辑封装。让我查看 eeprom 的完整实现，了解它如何使用 i2c，这样可以更好地理解需要测试的内容。同时我需要查看 Makefile 来了解如何添加新的测试模块：
...
✦ 现在我了解了整个架构。i2c 模块的源码主要是 STM32 HAL 库的初始化代码（MX_I2C1_Init、MX_I2C2_Init），没有额外的业务逻辑封装。

但是，针对 i2c 模块的 UT，我可以测试以下内容：
1. MX_I2C1_Init 和 MX_I2C2_Init 初始化函数的逻辑
2. HAL I2C 函数的调用验证

让我先查看一下 eeprom.h，了解接口定义：
...
✦ 现在我已经了解了整个 UTest 框架的结构。由于 i2c 模块主要是 HAL 初始化代码，没有额外的业务逻辑封装，我将创建一个针对 I2C 初始化函数的 UT 用例。

让我创建任务列表并开始生成 i2c 模块的 UT：
- 创建 UTest/lib/i2c 目录
- 创建 test_i2c.h 头文件
- 创建 test_i2c.c 测试用例文件
- 更新 UTest/board/main.c 注册 i2c 测试
- 更新 UTest/Makefile 添加 i2c 源文件
- 编译并运行测试验证
...
✦ i2c.c 文件依赖于 STM32 的硬件定义，无法直接在 x86 上编译。我需要创建一个桩文件来模拟 i2c 的初始化逻辑，或者重写测试策略。

由于 i2c.c 主要包含 STM32 HAL 库的初始化代码，依赖硬件定义，我需要创建一个桩版本的 i2c 源码用于测试。让我修改策略：创建一个简化的 i2c 桩源码文件用于测试。
...
✦ 编译出现多个符号重复定义错误，需要从 test_i2c.c 中移除已在 eeprom 测试中定义的函数和变量。
✦ 有多个符号重复定义的问题。我需要修复：
1. hi2c2 在 stub_hal_i2c.c 和 stub_i2c.c 中都有定义
2. g_utContext 和 ut_run_test 在 test_eeprom.c 和 test_i2c.c 中重复定义
...
```

**模块之间如果存在相互引用，UT生成时可能会出现冗余代码(重复定义的桩函数)。需要在一开始对其进行限制，告诉AI如果桩函数已经有了，考虑复用或者扩展而不是另起炉灶。或者在UT生成完毕并且编译通过后，启动新的agent对代码进行精简**

```text
  > 你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。@UTest/下是一个UT用例框架，它可以在x86_64下编译运行，用来检查源码的逻辑。现在已经有了@Core/lib/eeprom的UT，我现在希望能够按照当前的UT测试架构，生成有关i2c模块的UT，详情见@README.md。要求：如果模块间存在相互调用，请直接复用代码，不要生成新的冗余代码
  ✦ 需要先了解现有的 UTest 框架结构、README 中的 i2c 测试要求以及 eeprom 的现有 UT 实现方式，才能按照相同架构生成 i2c 模块的 UT。
  ✦ 我来分析现有的 UTest 框架结构，然后为 i2c 模块生成 UT 用例。
  ...
  ✦ 测试全部通过，现在需要总结完成的工作。
  ✦ 所有测试通过！总结一下完成的工作：
    完成总结
    成功为 STM32 I2C 模块生成了单元测试用例
    创建的 Files:
     1. UTest/lib/i2c/test_i2c.h - I2C 测试头文件
     2. UTest/lib/i2c/test_i2c.c - I2C 测试用例（28 个测试）
     3. UTest/board/ut_runner.c - 公共测试运行器（可复用）
     4. UTest/include/ut_runner.h - 测试运行器头文件
    修改的 Files:
     1. UTest/board/main.c - 注册 I2C 测试
     2. UTest/Makefile - 添加 I2C 源文件和编译规则
     3. UTest/include/stub_hal_types.h - 添加 I2C 宏定义
     4. UTest/include/ut_framework.h - 添加全局变量声明
     5. UTest/lib/eeprom/test_eeprom.c - 移除重复定义
    测试覆盖:
     - HAL_I2C_Master_Transmit (5 个测试)
     - HAL_I2C_Master_Receive (5 个测试)
     - HAL_I2C_Mem_Write (6 个测试)
     - HAL_I2C_Mem_Read (6 个测试)
     - HAL_I2C_Init/DeInit (2 个测试)
     - HAL 工具函数 (4 个测试)
    测试结果: EEPROM 28 个测试全部通过，I2C 28 个测试全部通过。
```


#### 给出通用性和扩展性要求的 UT 生成

生成UT测试的时候最好明确隔离层/桩函数的边界。比如这里我们规定`Core`中定义的函数不需要打桩，其余的都需要进行打桩。在重新组织目录结构后，再次进行UT用例的生成测试。

```text
  > 你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。@Core/ 中是主要的工程代码。而@Core/board/ 中的则是主要的业务逻辑代码。
    我现在想要为业务逻辑代码中的@Core/board/module/eeprom/ 模块构造UT用例 。我希望：1。打桩隔离@Core/ 目录以外定义的函数，@Core/ 内部的函数不做隔离。
    2.源码的架构师arm，我希望UT框架基于x86，能够在本机编译并且运行测试。3.UT的代码全部放在项目根目录的UTest目录下，UT代码的组织结构我希望能够尽量通用，之后我可以方便的编写其他模块的UT测试。4.保证UT测试框架
    的通用性和扩展性，后续增添业务代码时也能够轻松地完成，只需要添加新的桩函数，构造输入，调业务函数然后断言输出就能够写新的UT ​
  
  ✦ 我需要先了解项目的目录结构、依赖关系和现有的 eeprom 模块代码，然后才能设计一个可扩展的 UT 框架。

  ✦ 好的，我来为 EEPROM 模块设计并实现一个可扩展的单元测试框架。首先让我分析一下项目结构和 EEPROM 模块的代码。

  ✦ 现在我已经了解了项目结构和 EEPROM 模块的代码。让我来创建任务列表并开始实现 UT 框架。
  任务列表：
  - 设计并创建通用 UT 框架目录结构和基础文件
  - 创建桩函数层 - 隔离 Core/目录外的依赖函数（HAL/I2C/日志等）
  - 创建 eeprom 模块的 UT 测试用例
  - 创建 UT 编译 Makefile
  - 验证编译和测试运行
  ✦ 让我开始创建 UT 框架。首先创建目录结构和基础框架文件：
  ...

  ✦ 所有任务已完成，测试全部通过，现在需要向用户提供一份完整的总结，包括创建的目录结构、测试覆盖范围、框架特性和使用方式。
  ✦ 完成！我已经为 EEPROM 模块创建了一个完整的单元测试框架。以下是总结：

  完成的工作
  1. 创建了通用 UT 框架目录结构
     UTest/
     ├── include/      # 框架头文件（含 HAL 模拟）
     ├── stub/         # 桩函数实现
     ├── test/eeprom/  # EEPROM 测试用例
     ├── build/        # 编译输出
     ├── Makefile      # 编译脚本
     └── README.md     # 使用文档

  2. 实现了桩函数层
   - 隔离了 Core/ 目录外的依赖函数（HAL I2C、日志等）
   - 提供了 EEPROM 内存模拟
   - 支持配置桩函数的返回值来模拟各种场景

  3. 编写了 22 个测试用例
   - eepromByteWrite: 4 个测试（正常、空指针、非法地址、HAL 失败）
   - eepromPageWrite: 4 个测试（正常、空指针、零长度、超长）
   - eepromBufWrite: 6 个测试（单字节、页对齐、非页对齐、空指针、零长度、地址溢出）
   - eepromBufRead: 5 个测试（正常、空指针、零长度、地址溢出、HAL 失败）
   - eeprom_init: 1 个测试
   - 集成测试: 2 个测试（读写验证、跨页写入）

  4. 框架特性
   - x86 架构: 可在 PC 上编译运行
   - 通用可扩展: 添加新模块测试只需添加新的桩函数和测试用例
   - 简单易用: 只需要添加桩函数、构造输入、调用业务函数、断言输出

  使用方式
  1. cd UTest
  2. make test_eeprom
  3. make clean

  所有 22 个测试用例均已通过！

```

完成


### UT 覆盖率

#### AI 分析结果

AI生成的UT测试最初生成时可能会存在编译问题，但是他会自己进行多轮的修改和测试检查，最后能够得到一个可以运行的UT用例。

这是AI生成的两个模块的UT覆盖情况
```text
EEPROM 模块（28 个用例）
- eepromByteWrite: 6 个用例
  覆盖场景：正常写入、I2C 失败、超时、NULL 指针、地址越界、边界地址
- eepromPageWrite: 7 个用例
  覆盖场景：满页/部分页写入、长度超限、NULL 指针、零长度、I2C 失败、边界地址
- eepromBufRead: 8 个用例
  覆盖场景：正常读取、单字节、零长度、TX/RX 失败、大数据、地址越界、NULL 指针
- eepromBufWrite: 7 个用例
  覆盖场景：小数据、单页、多页、跨页、NULL 指针、零长度、地址越界

覆盖分析
- 正常路径覆盖充分
- 异常路径覆盖充分（HAL 返回 ERROR/TIMEOUT/BUSY）
- 边界条件覆盖（NULL 指针、零长度、地址越界）
- 缺少对 eeprom_init() 函数的测试

I2C 模块（28 个用例）
- HAL_I2C_Master_Transmit: 5 个用例
  覆盖场景：正常发送、超时、错误、忙状态、大数据
- HAL_I2C_Master_Receive: 5 个用例
  覆盖场景：正常接收、超时、错误、单字节、大数据
- HAL_I2C_Mem_Write: 6 个用例
  覆盖场景：正常写入、超时、错误、忙状态、单字节、页面大小
- HAL_I2C_Mem_Read: 6 个用例
  覆盖场景：正常读取、地址写入超时、接收超时、接收错误、单字节、大数据
- HAL_I2C_Init/DeInit: 2 个用例
  覆盖场景：初始化、反初始化
- HAL 工具函数: 4 个用例
  覆盖场景：GetState、GetTick、IncTick、Delay

覆盖分析
- 桩函数验证充分
```

AI分析结果显示：基本覆盖了模块的功能以及边界情况，不过还是会出现一些遗漏。还是需要人工/AI review。在后续的几轮对话任务中，AI能够进一步提升代码质量，统一代码风格，修复编译警告等。

#### 人工分析

重新让Agent生成了`eeprom`模块的UT用例。

以`eepromByteWrite()`为例，新生成的UT用例仅包含了 正常写，NULL指针，非法地址和HAL失败四种情况，可能没有考虑到写入超时或者边界地址等情况。
而`eepromPageWrite()`则是有 正常写，长度超限，空长度的情况，明显存在缺失。

**这种单轮的UT用例生成很难一次性覆盖业务代码的所有分支条件。需要后期的人工或AI review来完善。**

**另外还可以发现Agent在多次单轮执行相同任务时会存在一致性问题。每一次独立的任务中生成的UT用例覆盖情况各不相同。纯AI分析结果中的用例更全面，而人工分析时生成的用例覆盖率就差一些。**

**另外UTest/目录下的子目录结构也会由于未明确导致多次结果不一致,不过这个可以依靠prompt进行约束。**

### UT 正确率

需要继续分析UT覆盖率和正确率的情况。如果源码存在有逻辑错误导致UT不通过。AI是否能够分析是UT用例的错误还是源码的错误。

手动修改源码，将`eeprom.c`中一处`rtnVal = ERR`改为`OK`。这样会导致UT测试不通过：
```text
[RUN] test_eepromByteWrite_hal_failure... [FAIL] /home/public/test/ai_test/master-F103VGT6/UTest/test/eeprom/test_eeprom.c:140: Expected -1, got 0

```
让AI分析一下源码：

```text
  > 你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。@Core/ 中是主要的工程代码。而@Core/board/ 中的则是主要的业务逻辑代码。
     我的UT测试似乎失败了，可以帮我定位一下问题吗？

  ✦ 需要先了解 U 测试的失败情况，先查看测试目录结构、README 文档，并运行测试命令来获取错误信息。


  ✦ 我来帮你定位 U 测试失败的问题。首先让我查看一下测试相关的文件和运行结果。

  ✦ 测试结果显示有一个用例失败，需要查看失败用例的具体代码来分析问题原因。

  ✦ 找到问题了！测试失败在 test_eepromByteWrite_hal_failure 用例。让我查看具体的测试代码和被测函数：

  ✦ 测试用例期望 HAL 失败时返回 ERR，但实际返回了 0，需要查看 eeprom.c 的实际实现来找出问题所在。

  ✦ 现在让我查看 eeprom.c 的实现代码，看看当 HAL_I2C_Mem_Write 失败时返回值是什么：

  ✦ 发现问题：eepromByteWrite 在 HAL 写入失败时将 rtnVal 设置为 OK 而不是 ERR，这与 eepromPageWrite 的处理不一致，需要修复这个 bug。

  ✦ 找到问题了！在 eepromByteWrite 函数中，当 HAL_I2C_Mem_Write 失败时，代码将返回值设置为 OK 而不是 ERR：

     1 if (HAL_OK != rtnVal)
     2 {
     3     DBG_LOG(LogError, "Eeprom write(%#x) byte data failed, rtnVal = %d",
     4             writeAddr, rtnVal);
     5     rtnVal = OK;  /* 这里应该是 ERR，但写成了 OK */
     6 }

    这是一个 bug。当 HAL 写入失败时，函数应该返回错误码 ERR，但当前代码将其设置为 OK。

    让我修复这个问题：
```

在AI可以阅读源码时，简单的源码错误可以被识别并改正。

#### UT 生成是否能够检验代码正确

如果代码存在错误，生成UT时是按照错误逻辑检测，还是会发现问题？

我让AI在`eeprom`模块中注入一些函数错误，然后新开一个终端(保证AI上下午清空)，让其基于错误代码进行UT生成。注入错误如下：
```text
错误 1 | eepromByteWrite | 边界条件错误
影响：在函数末尾增加 writeAddr >= EEPROM_SIZE 判断，导致地址为 EEPROM_SIZE 时被误判为非法

错误 2 | eepromPageWrite | 返回值错误
影响：成功写入后将返回值从 OK 改为 ERR，导致 UT 检测写入返回值时失败

错误 3 | eepromBufWrite | 逻辑判断错误
影响：将 pageNum == 0 改为 pageNum != 0，导致单页写入时走错分支，调用错误的写入逻辑

错误 4 | eepromBufRead | 边界条件错误
影响：将 readAddr + tmpLen > EEPROM_SIZE 改为 >=，导致恰好填满 EEPROM 的读取被误判为非法

错误 5 | eeprom_init | 返回值错误
影响：将返回值强制设为 ERR，导致 UT 检测初始化返回值时失败
```

接下来用同样的prompt来为`eeprom`模块生成UT测试用例：

`eeprom`模块生成UT的prompt：

```text
  你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。
  @Core/ 中是主要的工程代码。而@Core/board/ 中的则是主要的业务逻辑代码。
  我现在想要为业务逻辑代码中的@Core/board/module/eeprom/ 模块构造UT用例 。
  我希望：
  1.打桩隔离@Core/ 目录以外定义的函数，@Core/ 内部的函数不做隔离。  
  2.源码的架构是arm，我希望UT框架基于x86，能够在本机编译并且运行测试。
  3.UT的代码全部放在项目根目录的UTest目录下，UT代码的组织结构我希望能够尽量通用，之后我可以方便的编写其他模块的UT测试。
  4.保证UT测试框架的通用性和扩展性，后续增添业务代码时也能够轻松地完成，只需要添加新的桩函数，构造输入，调业务函数然后断言输出就能够写新的UT
```

在完成UT生成后，Agent会编译运行UT用例，在发现UT失败后会去检查失败原因:
```text
  ...
  ✦ 测试运行成功但有 2 个测试失败，需要查看原始代码以理解测试用例预期与实际行为的差异，这是代码中故意存在的 bug。

  ✦ 测试运行成功！发现了 eeprom.c 中故意存在的 bug：
     1. eepromPageWrite 在成功时返回 ERR 而不是 OK（错误 2）
     2. eepromBufWrite 在单页写入时走错了分支（错误 3）
  ...
```

在单轮UT生成中，Agent发现了5个错误中的其中两个错误：错误2和错误3。分别是返回值错误和逻辑判断错误。但是其余三个错误并没有出现，甚至检查了UT源码，发现它直接基于错误的逻辑编写UT用例，导致无法检查出这种边界错误。

AI可以比较容易的检查出返回值和一些简单的逻辑判断错误，但是对于涉及具体参数的判断它无法在没有先验信息的情况下进行判断，只会根据代码逻辑来写UT测试。简单来说就是如果没有文档或输入提示AI有关这类边界条件，比如eeprom最大写入地址是0x8000,大于0x8000不可写入。那么AI就会默认使用代码逻辑进行UT用例的生成。比如这里我将`eepromByteWrite()`中的边界判断错误复原。编译运行UT用例结果变成：

```text
========================================
EEPROM Module Unit Tests
========================================

eepromByteWrite tests:
  Running test_byte_write_valid... PASSED
  Running test_byte_write_null_buffer... [DBG_LOG 3] The input parameter is illegal
FAILED
    Assertion failed: expected -1, got 0
  Running test_byte_write_addr_exceeds_size... [DBG_LOG 3] The input parameter is illegal
FAILED
    Assertion failed: expected -1, got 0
  Running test_byte_write_addr_equals_size... FAILED
    Assertion failed: expected -1, got 0
  Running test_byte_write_hal_error... [DBG_LOG 3] Eeprom write(0x100) byte data failed, rtnVal = 1
FAILED
    Assertion failed: expected -1, got 0
  Running test_byte_write_hal_timeout... [DBG_LOG 3] Eeprom write(0x100) byte data failed, rtnVal = 3
FAILED
    Assertion failed: expected -1, got 0
  Running test_byte_write_data_persists... PASSED
  ...
========================================
Test Summary:
  Passed: 23
  Failed: 7
  Total:  30
========================================
```
用例测试错误变得更多了。并且代码还有其他的一些错误。AI在发现UT测试未通过时会去修复错误。但是由边界判断导致的问题因为UT与源码匹配了导致不会去修复。

可以下一个基本结论了：
**AI生成UT用例时能够根据发现并修复基本的输出错误(OK/ERR错误)以及一些简单的逻辑判断问题，但是对于涉及硬件地址边界问题等一些缺乏信息的逻辑判断，AI不能够进行判断，甚至会基于错误代码编写UT用例。**

---

如果写进CI脚本中，可以让Agent根据UT输出的结果去查看对应源码来修改错误。Jenkins虽然脚本运行环境受限制，但是其运行目录就是`workspace`，可以直接分析从SVN检出的代码。


### 多轮独立任务提升 AI 代码质量

#### `eeprom` 模块代码 Agent 改进

尝试让AI进行多轮的UT用例检视和修改，看是否会出现结果收敛。然后再人工检视。
请注意，每一轮任务尽量打开一个新的窗口或者清空Agent上下文进行，确保每一次任务的独立性。避免AI被先前的任务污染上下文做出错误决策。

每一次独立任务的初始提示词如下：
```text
  > 你是一名资深的嵌入式软件工程师。当前目录下是一个基于stm32f103vgt6的座椅电源工程源码。
  @Core/ 中是主要的工程代码。而@Core/board/ 中的则是主要的业务逻辑。
  @UTest/test/ 下是分模块的UT用例，请你帮我检查一下eeprom的用例质量。
  当前的代码风格是c99，请你沿用这个代码风格。
  现在的检查是针对UT用例，工程源码可以认为是没有任何问题，有问题都是UT的问题。
```

1.在测试中Agent在遇到死循环代码时(例如`timeout`类的UT用例)可能不一定会发现错误，如果还编译通过运行，会导致进程卡住，Agent无法自己截断。需要人工干预。

2.多轮检查时，AI仅凭纯代码可能会无法理解一些业务逻辑，因此会出现误报的情况。需要人工进行检查AI的结论并给出修改建议。 AI甚至会比较固执，在多轮独立的检视中总是会以不同的角度报告同样的错误并提出修改源码的建议。

3.AI在边界条件上总是会分不清，对`EEPROM_SIZE`与`EEPROM_SIZE+1`和`*-1`以及大于小于等于这类判断会出现错误。比较好的方法是显式地告诉AI边界条件而不是靠他自己去判断。

4.单轮检视中，如果修改问题过多会触发输入token限制，压缩上下文。(模型能力问题)

5.同样是边界问题，我在限制了源码正确无误，问题只会出现在UT用例中时。AI仍然会对边界问题反复警告。

在经过4轮的独立任务后(每一次完成改进并且通过编译测试后，新开一个Agent，使用独立的上下文进行下一轮任务)。Agent在第5轮检视时最后只发现了误报类的问题以及不必要的UT(还有一点代码风格的问题)，此时Agent认为代码质量基本可靠了。

#### 人工检视

现在我对代码进行人工检视，对`eeprom`的代码逻辑进行检查。

检查发现对 `eepromPageWrite()`没有进行边界检查(源码本身只是对地址和长度的边界进行了检查，没有对写入最大地址进行检查)。这里不知道是源码逻辑有问题还是eeprom的硬件设计使然。





**总体来看，对UT生成结果的多轮检视问题依然是集中在对代码的边界条件和复杂逻辑认知模糊。这个与代码检查时遇到的问题类似。**

个人认为是两个方面限制了AI的表现：
1. 服务器的硬件条件，本地的AI能力终究有限
2. 在检视代码时如果有较为详细的需求文档，AI基于文档来检视、生成代码效果应该会更好。


### 小结

UT用例测试方面，受限于本地AI能力，嵌入式项目(具有大量库函数，总代码量远大于业务代码)无法一次性完成UT用例的生成。

然后考虑单一模块的UT用例生成，单轮任务中AI能够完成任务。
增量添加新模块的UT用例，AI能够完成任务，但是代码冗余量大。在任务提示词中添加限定词能够改善。

由此延申出是否可以生成工程级别的UT测试框架，将桩函数一次性生成完毕，后续的UT用例只用考虑测试。最后结果是桩函数过多，远超AI能力范围，无法实现。

采用好的提示词让AI生成通用可扩展的UT测试框架，然后单一模块增量化生成是目前效果最好的方式。

在UT覆盖率方面，单一模块单论任务生成的UT用例很难做到所有分支的全覆盖，总是会有遗漏的情况，需要人工介入或者多轮review。目前最好的办法还是UT生成完毕后人工或者AI进行检查。

在UT正确度方面，AI会在UT生成任务中进行编译和测试，这样会检查到一些明显的逻辑和判断错误，从而进行代码修复。不过针对一些复杂的业务逻辑或者涉及到硬件地址边界的问题，AI在没有信息(不显式告诉它)时并不会发现这类问题，甚至会根据错误逻辑编写UT。

---

# CI 持续集成

## 持续集成环境搭建

基本想法是要去实现一个局域网内的持续集成/持续部署的环境。当代码向远程仓库提交时，服务端会检测到提交动作，实现拦截/编译/测试，通过后允许上库。如果在某一个环节失败，则拒绝本次提交并通过输出log或者其他方式来记录反馈。
最开始想要利用Agent实现一个智能化的方案，调研后发现Agent虽然可以实现基本的CI/CD功能，但是存在安全隐患。Agent在全自动执行任务时，很难在权限和安全之间做取舍。黑名单机制可以限制Agent进行显式地恶意操作，但是无法避免Agent通过写权限修改关键文件。通过prompt能够在一定程度上限制Agent行为，但其安全程度难以量化。


经过调研，比较成熟可靠的方案为“HOOK+CI tools”，在server端利用SVN仓库自带的HOOK机制， 使用pre-commit或者post-commit来自定义提交申请发出后，入库前和入库后的操作。
目前比较流行的做法是在pre-commit时进行一个简单的UT测试。完成后上库，通过post-commit触发完整的检查流程。


需要注意，HOOK本身的运行环境是一个沙箱环境，指令和权限限制较强。因此一般不推荐直接在HOOK脚本中进行复杂的操作流程。

### systemd 守护进程+ Hook

可以通过离线安装openjdk-17-jdk openjdk-17-jre 然后是jenkins.jar来为离线虚拟机服务器部署jenkins服务。

不过jenkins功能很强大，在熟悉其具体流程前。最好先将svn HOOK与CI服务器的流程打通：
> svn服务器pre-commit进行简单的log校验，post-commit通过某种形式向CI服务器发送请求，CI服务器根据请求拉取对应代码进行编译测试，向svn服务器反馈结果。

### Jenkins 可视化界面

自己实现的简单CI 进程可以完成任务，但是用户不友好，另外后期维护也不方便。目前已经有了Jenkins这类开源的CI/CD服务器方案，还是采用Jenkins来实现。

SVN在版本控制和分支方面不如git轻便，通过`post-commit`进行CI流程后考虑通过/拦截的方式实现起来非常麻烦，遂转而考虑使用定时任务+可视化输出+日志归档的形式来实现CI。

Jenkins可以为SVN的每一个代码仓库创建独立任务，这里以`pipeline`为例。Jenkins支持使用`cron`语法+拓展来实现定时任务。并且通过pipeline 脚本的形式来管理和控制任务流。

目前已经实现了

```text
checkout -> cppcheck -> compile -> UTest
```
全流程的实现，并且可以将日志输出归档。

更进一步就是在流程中加入AI。Qwen Code支持使用文本输入输出的单轮任务

```text
qwen < input.txt > output.txt
```
在Jenkins服务器上部署离线的Qwen Code，并且将路径加入到`pipeline`脚本中就可以在任务中调用Qwen来辅助分析。目前是在静态检查后对检查结果进行分析，和编译/测试失败后对控制台输出进行分析。

需要对AI的分析能力进行测评，在相同的错误下期望Agent的分析结果能够基本一致，并且能够给出可行的解决方案。

## AI 分析能力测试

CI流程中的AI仅仅是作为分析工具，对cppcheck等静态检查工具的输出分析得到一个用户友好的报告，帮助开发者快速定位和修改问题。AI分析能够提出一些可行性的建议。


不过cppcheck仅仅是静态检查工具，一些代码的逻辑错误并不能检查出来。AI也是基于检测工具输出提出建议。

当然也可以单独设定一个`stage`，使用AI来分析整个项目代码。但是正如之前所说，本地部署的AI受限于模型理解能力和上下文限制(当然还可能有Agent编排的能力限制)，10K行的代码量完整解析就已经有些捉襟见肘。直接将工程级别的逻辑检查交给本地部署的AI，并且放进CI流程目前不具备可行性。

AI的代码检查能力在前面的章节也有进行测试分析，AI检查想要得到较好的效果最好是基于模块(小代码量)的规则明确的检查。
在粒度较细的情况下AI能够较好地进行工作。

### 小结

在 CI 流程中，AI 更适合作为分析与辅助定位工具，而不适合直接承担工程级代码审查或高风险自动决策任务。

结合本文前面的测试结果，可以得到一个较为稳定的结论：

1. 规则明确、范围受限时，AI 的表现明显更好
2. 在大代码量、弱约束、复杂边界条件场景下，AI 的稳定性和一致性不足
3. 将 AI 放入 CI 的合理方式，是让它分析已有检查结果并输出更易理解的报告，而不是直接替代传统检查工具

