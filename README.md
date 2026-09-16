# Spacemit #

## 1. 简介

ESOS(Energy Service OS) 是由进迭时空科技有限公司开发的能效及实时管理系统，为系统提供辅助管理功能

### 1.1 K1 平台

包括如下硬件特性：

| 硬件 | 描述 |
| -- | -- |
|芯片型号| k1 |
|CPU| N308 |
|主频| 245Mhz |
|DDR | 共享AP DDR |
|SRAM | 256KB |

### 1.2 K3 平台 (RT24)

K3 芯片集成了两个 RT24 RISC-V 小核（os0_rcpu 和 os1_rcpu），分别运行独立的 ESOS 实例，负责系统能效及实时任务管理。

| 硬件 | 描述 |
| -- | -- |
|芯片型号| k3 |
|CPU| RT24 (RISC-V) |
|主频| 614.4MHz |
|RAM | 5MB（每个核独立） |
|实例数| 2（os0_rcpu / os1_rcpu）|

支持的板型：evb、evb2_1、evb2_2、com260、com260_ifx、com260_kit_v02、deb1、gemini_c0、gemini_c1、pico-itx、dc_board、BS01DCMA

## 2. 编译说明

### 2.1 K1 平台编译

| 环境 | 说明 |
| --- | --- |
|PC操作系统|Linux|
|编译器|riscv-nuclei-elf-gcc version 10.2.0|
|构建工具|scons|

1) 下载源码

```
随SDK一起发布
```
2) 配置工程并准备env
```
    cd esos
    ./build.sh config

    INFO: prepare to config sdk ...
    All valid soc chips:
            0: n308
    Please select a chip:0
    All valid boards:
            0: k1-x
    Please select a board:0

```
3) 编译
```
    ./build.sh
```
4) 清除编译临时文件
```
    ./build.sh clean
```
5) 配置
```
    cd bsp/spacemit/
    scons --meuconfig
```
如果编译正确无误，会产生rtthread.bin、rtthread-n308.elf文件。其中rtthread-n308.elf需要copy到主系统的ramfs中供linux加载并启动运行。

### 2.2 K3 平台编译 (RT24)

| 环境 | 说明 |
| --- | --- |
|PC操作系统|Linux|
|编译器|riscv64-unknown-elf-gcc version 14.2.1 (spacemit-toolchain-elf-newlib-x86_64-v1.0.9)|
|构建工具|buildroot / scons / deb |

#### 2.2.1 Bianbu 环境

在 Bianbu 系统中，esos 以 deb 包形式分发，可直接从源码构建并安装。

1) 下载源码
```
git clone https://github.com/spacemit-com/esos.git
cd esos/components
git clone https://github.com/spacemit-com/esos-lite.git
git checkout -b k3-release remotes/origin/k3-release
cd esos
git checkout -b k3-release remotes/origin/k3-release
```

2) 安装编译依赖
```
apt-get build-dep .
```

3) 构建 deb 包
```
dpkg-buildpackage -uc -us -b
```

4) 安装
```
dpkg -i ../esos-spacemit_*.deb
```

#### 2.2.2 Buildroot 环境

ESOS 作为 buildroot 的一个 package 进行编译

1) 编译（默认编译 rt24 all core）
```
# 在 buildroot-k3 根目录下执行
make esos
```

2) 清理
```
make esos-dirclean
```

3) 重新编译
```
# 注意：不是 make esos-rebuild
make esos-reconfigure
```

##### 2.2.2.1 修改/配置 esos

esos 的修改或 menuconfig 配置需要进入 esos 源码目录，并选择要修改的 core：

```
$ cd buildroot-k3/package-src/esos
$ ./build.sh config   # 选择要配置 core0 或 core1
INFO: prepare to config esos sdk ...
All valid soc chips:
        0: n308
        1: rt24
Please select a chip:1
All valid boards:
        0: os0_rcpu
        1: os1_rcpu
Please select a board:0

INFO: target configuration is as follows:
INFO: -------------------------------------------------------------------------
export TARGET_CHIP=rt24
export TARGET_BOARD=os0_rcpu
export TARGET_DEFCONFIG=rt24_os0_rcpu_defconfig
export TARGET_ENTRY_POINT=0x100200000
INFO: -------------------------------------------------------------------------
INFO: prepare to toolchain ...
```

选完 core 后可以用 menuconfig 进行图形化配置，配置结果保存在
`bsp/spacemit/platform/rt24/osX_rcpu/rt24_osX_rcpu_defconfig`：

```
./build.sh menuconfig
```

修改完成后，运行以下指令可生成该 core 的 .elf 文件：

```
./build.sh
```

> **注意**：如果要生成最终的 `esos.itb` 镜像，不要直接运行 `./build.sh itb`，
> 需要回到 buildroot 根目录重新编译 esos 才能生效（修改 esos 代码同理）。
> 生成的 `esos.itb` 镜像位于 `./output/k3/images/` 目录下：
>
> ```
> $ cd buildroot-k3
> $ make esos-reconfigure
> ```

### 3 运行结果

如果编译 & 烧写无误，会在RUART0上看到RT-Thread的启动logo信息：

```
\ | /
- RT -     Thread Operating System
 / | \     4.0.4 build Oct 22 2025 14:57:47
 2006 - 2021 Copyright by rt-thread team

```

## 4. 驱动支持情况及计划

### 4.1 K1 平台驱动支持

| 驱动 | 支持情况  |  备注  |
| ------ | :----:  | :------:  |
| uart | 支持 | uart0-1 |
| gpio | 支持 | / |
| pinctrl | 支持 | / |
| clk | 支持 | / |
| i2c | 支持 | i2c0 |
| spi | 支持 | spi0 |
| mailbox | 支持 | / |
| remoteproc | 支持 | / |
| pwm | 支持 | pwm0~9 |
| dma | 支持 | / |
| mmu | 不支持 | / |
| adma | 半支持 | 仅支持中断投送功能 |
| can | 半支持 | 仅支持中断投送功能 |
| ir | 半支持 | 仅支持中断投送功能 |

### 4.2 K3 平台驱动支持 (RT24)

| 驱动 | os0_rcpu | os1_rcpu | 备注 |
| ------ | :----: | :----: | :------: |
| uart | 支持 | 支持 | / |
| gpio | 支持 | 支持 | / |
| pinctrl | 支持 | 支持 | / |
| i2c | 支持 | 支持 | / |
| spi | 支持 | 支持 | / |
| mailbox | 支持 | 支持 | / |
| dma | 支持 | 支持 | / |
| adma | 不支持 | 支持 | / |
| pm | 支持 | 支持 | / |
| regulator | 支持 | 支持 | / |
| clk | 支持 | 支持 | / |
| pwm | 支持 | 支持 | / |
| can | 支持 | 支持 | / |
| remoteproc | 支持 | 支持 | / |


## 5. 联系人信息

维护人:
[zhuxianbin][4] < [xianbin.zhu@spacemit.com][5] >
