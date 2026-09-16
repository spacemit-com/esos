import os

# toolchains options
ARCH        ='risc-v'
CPU         = os.getenv('TARGET_CHIP')
BOARD       = os.getenv('TARGET_BOARD')
VENDOR      ='spacemit'
CROSS_TOOL  ='gcc'

if os.getenv('RTT_CC'):
    CROSS_TOOL = os.getenv('RTT_CC')

if  CROSS_TOOL == 'gcc':
    PLATFORM    = 'gcc'
else:
    print ('Please make sure your toolchains is GNU GCC!')
    exit(0)

if os.getenv('RTT_EXEC_PATH'):
    EXEC_PATH = os.getenv('RTT_EXEC_PATH')

TARGET_ENTRY=os.getenv('TARGET_ENTRY_POINT')

# BUILD = 'debug'
BUILD = 'release'

if CPU == 'n308':
    if os.getenv('RTT_EXEC_PATH') is None:
        EXEC_PATH   = os.getcwd() + '/../../tools/toolchain/gcc/bin'
    if PLATFORM == 'gcc':
        # toolchains
        PREFIX  = 'riscv-nuclei-elf-'
        CC      = PREFIX + 'gcc'
        CXX     = PREFIX + 'gcc'
        AS      = PREFIX + 'gcc'
        AR      = PREFIX + 'ar'
        LINK    = PREFIX + 'gcc'
        TARGET_EXT = 'elf'
        SIZE    = PREFIX + 'size'
        OBJDUMP = PREFIX + 'objdump'
        OBJCPY  = PREFIX + 'objcopy'
        STRIP   = PREFIX + 'strip'

        DEVICE = ' -march=rv32imafc -mabi=ilp32f -mcmodel=medlow'

        CFLAGS  = DEVICE + ' -c -g -ffunction-sections -fdata-sections -Wall'
        AFLAGS  = ' -c' + DEVICE + ' -x assembler-with-cpp'
        LFLAGS  = DEVICE + ' -nostartfiles -Wl,--no-whole-archive ' + ' -Xlinker --defsym=ENTRY_POINT=%s -T ./platform/%s/%s/gcc.ld -Wl,-gc-sections -Wl,-Map=rtt.map' % (TARGET_ENTRY,CPU,BOARD)
        # Allow injecting extra link flags (e.g. -L<path>) from environment.
        if os.getenv('EXTRA_LFLAGS'):
            LFLAGS = ' ' + os.getenv('EXTRA_LFLAGS') + ' ' + LFLAGS
        CPATH   = ''
        LPATH   = ''

        if BUILD == 'debug':
            CFLAGS += ' -O0 -gdwarf-2'
            AFLAGS += ' -gdwarf-2'
        else:
            CFLAGS += ' -O2 -g2'

        CXXFLAGS = CFLAGS

        DUMP_ACTION = OBJDUMP + ' -D -S $TARGET > rtt.asm\n'
        POST_ACTION = OBJCPY + ' -O binary $TARGET rtthread.bin\n' + SIZE + ' $TARGET \n' + STRIP + ' $TARGET\n'

else:
    if os.getenv('RTT_EXEC_PATH') is None:
        EXEC_PATH   = os.getcwd() + '/../../tools/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9/bin'
    if PLATFORM == 'gcc':
        # toolchains
        PREFIX  = 'riscv64-unknown-elf-'
        CC      = PREFIX + 'gcc'
        CXX     = PREFIX + 'gcc'
        AS      = PREFIX + 'gcc'
        AR      = PREFIX + 'ar'
        LINK    = PREFIX + 'gcc'
        SIZE    = PREFIX + 'size'
        OBJDUMP = PREFIX + 'objdump'
        OBJCPY  = PREFIX + 'objcopy'
        STRIP   = PREFIX + 'strip'
        TARGET_EXT = 'elf'

        DEVICE = ' -march=rv64imafdczifencei -mabi=lp64d -mcmodel=medany '

        CFLAGS  = DEVICE + '-ffreestanding -flax-vector-conversions -Wno-cpp -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -fdiagnostics-color=always'
        AFLAGS  = ' -c' + DEVICE + ' -x assembler-with-cpp -D__ASSEMBLY__ '
        LFLAGS  = DEVICE + ' -nostartfiles -Wl,--no-whole-archive ' + ' -Xlinker --defsym=ENTRY_POINT=%s -T ./platform/%s/%s/gcc.ld -Wl,-gc-sections -Wl,-Map=rtt.map' % (TARGET_ENTRY,CPU,BOARD)
        # Allow injecting extra link flags (e.g. -L<path>) from environment.
        if os.getenv('EXTRA_LFLAGS'):
            LFLAGS = ' ' + os.getenv('EXTRA_LFLAGS') + ' ' + LFLAGS
        CPATH   = ''
        LPATH   = ''

        if BUILD == 'debug':
            CFLAGS += ' -O0 -gdwarf-2'
            AFLAGS += ' -gdwarf-2'
        else:
            CFLAGS += ' -Os -g2'

        CXXFLAGS = CFLAGS

        DUMP_ACTION = OBJDUMP + ' -D -S $TARGET > rtt.asm\n'
        POST_ACTION = OBJCPY + ' -O binary $TARGET rtthread.bin\n' + SIZE + ' $TARGET \n' + STRIP + ' $TARGET\n'
