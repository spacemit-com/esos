dtc_script=$(shell command -v dtc || echo ./dtc)

SRCS = $(wildcard *.dts)
OBJS = $(patsubst %.dts, %.dtb, $(SRCS))

TARGET = mydtb

all: $(TARGET)

$(TARGET): $(OBJS)
	$(shell echo > $@)

#规则:
%.tmp_dts: %.dts
	cpp -nostdinc -I. -undef -x assembler-with-cpp $< > $@

# 规则: 从.dts 文件 生成 .dtb 文件
%.dtb: %.tmp_dts
	$(dtc_script) -I dts -O dtb -o $@ $<

# 伪目标：清理构建文件
.PHONY: clean
clean:
	rm -rf *.dtb $(TARGET)
