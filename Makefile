ARCH:=linux
#ARCH:=bcm15
#ARCH:=sh4

ifeq ($(ARCH),linux)
CXXFLAGS:=-Wall
IPSTB_ARCH:=linux
DK_VERSION:=
TOOLCHAIN_DIR:=
endif


ifeq ($(ARCH),sh4)
CXXFLAGS:=-Wall -fPIC -Os -finline-limit=50 -m4-300 -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D__STDC_FORMAT_MACROS
IPSTB_ARCH:=sh4-kreatv-linux-uclibc
DK_VERSION:=4.7.telia.7
TOOLCHAIN_DIR:=/usr/local/kreatv/toolchain/st40/4.0.0
endif

ifeq ($(ARCH),bcm15)
CXXFLAGS:=-Wall -fPIC -Os -finline-limit=50 -DSDK5 -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D__STDC_FORMAT_MACROS -std=c++11
IPSTB_ARCH:=arm-kreatv-linux-gnueabihf
DK_VERSION:=FREJA-release_522228
TOOLCHAIN_DIR:=$(shell cat /kreatel/$(DK_VERSION)/dist/$(ARCH)/toolchain_dir)
endif

EMPTY:=

ifneq ($(TOOLCHAIN_DIR),$(EMPTY))
CXX:=$(TOOLCHAIN_DIR)/bin/$(IPSTB_ARCH)-c++
CC:=$(TOOLCHAIN_DIR)/bin/$(IPSTB_ARCH)-gcc
endif

BUILD:=build/$(IPSTB_ARCH)

TARGET:=$(BUILD)/igmpclient

SRC:=src

C_FILES:=$(shell find $(SRC)/*.c)
OBJS:=$(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(C_FILES))

.PHONY: dirs

all: dirs $(TARGET)

dirs: $(BUILD)

$(BUILD):
	mkdir -p $(BUILD)

dist: $(TARGET)
	cp $(TARGET) /var/www/files

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) -c $^ -o $@

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^  

info:
	@echo ARCH: $(ARCH)
	@echo DK_VERSION: $(DK_VERSION)
	@echo TOOLCHAIN_DIR: $(TOOLCHAIN_DIR) 
	@echo DK_DIR: /kreatel/$(DK_VERSION)/dist/$(ARCH)/toolchain_dir
	@echo C SRC: $(C_FILES)
	@echo OBJS: $(OBJS)
	@echo CC: $(CC)
	@echo CXX: $(CXX)
