# ============================================================
#  Makefile for CodeObfuscator
#  单文件 Windows C 代码混淆工具
#
#  用法:
#    make            编译生成 wvocexample.exe
#    make clean      清理生成的可执行文件
#
#  说明:
#    可执行文件生成在本目录 (CodeObfuscator/) 下,
#    运行时通过 ..\LIB\ 和 ..\LIBNEWNAME0\ 相对路径访问上级目录。
# ============================================================

CC      = gcc
CFLAGS  = -Wall -O2 -g
LDFLAGS =

TARGET  = wvocexample.exe
SRC     = wvocexample.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
