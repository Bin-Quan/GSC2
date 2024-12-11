# 编译器设置
ifdef MSVC     # 避免 MingW/Cygwin 环境
    uname_S := Windows
else
    uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
endif

# 默认安装位置
prefix      = /usr/local
exec_prefix = $(prefix)

# 目录设置
SRC_DIR := src
INC_DIR := include
LIB_DIR := lib
OBJ_DIR := obj
BIN_DIR := bin

# 编译器和编译选项
CXX := g++
# CXXFLAGS := -Wall -O3 -g -m64 -std=c++14 -pthread -MMD -MP -I $(INC_DIR) -I $(SRC_DIR)
CXXFLAGS := -std=c++17 -Wall -O0 -g -m64 -pthread -MMD -MP -I $(INC_DIR) -I $(SRC_DIR)


# 平台特定设置
ifeq ($(uname_S),Linux)
    # 检查CPU是否支持SSE4.2
    HAVE_SSE4 := $(filter-out 0,$(shell grep sse4.2 /proc/cpuinfo | wc -l))
endif
ifeq ($(uname_S),Darwin)
    CXX := clang++
    # 检查CPU是否支持SSE4.2
    HAVE_SSE4 := $(filter-out 0,$(shell sysctl machdep.cpu.features | grep SSE4.2 - | wc -l))
endif

# 根据SSE支持情况设置编译选项
CXXFLAGS += $(if $(HAVE_SSE4),-msse4.2,-msse2)

# 链接选项
LDFLAGS := -L$(LIB_DIR) -Wl,-rpath,$(LIB_DIR)
LDLIBS := -lm -lpthread -mpopcnt -lz -lbz2 -llzma -lhts -lsdsl -lbsc -lzstd -lmmf

# 源文件、目标文件和可执行文件
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
TARGET := $(BIN_DIR)/gsc

# 默认目标
all: $(TARGET)

# 编译规则
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 链接规则
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# 包含依赖文件
-include $(DEPS)

# 清理规则
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# 安装和卸载规则
install:
	mkdir -p -m 755 $(exec_prefix)/bin
	cp $(TARGET) $(exec_prefix)/bin/

uninstall:
	rm -f $(exec_prefix)/bin/$(notdir $(TARGET))

# 防止与同名文件冲突
.PHONY: all clean install uninstall
