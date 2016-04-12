ARCH:=linux
#ARCH:=bcm15
#ARCH:=sh4

#PROFILE=-pg

ifeq ($(ARCH),linux)
CXXFLAGS:=-Wall $(PROFILE)
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

#CXXFLAGS+=-std=c99

BUILD:=build/$(IPSTB_ARCH)

TARGETS:=$(BUILD)/igmplisten $(BUILD)/igmpjoin

SRC:=src

C_FILES:=$(shell find $(SRC)/*.c)
OBJS:=$(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(C_FILES))

.PHONY: dirs

all: dirs $(TARGETS)

dirs: $(BUILD)

$(BUILD):
	mkdir -p $(BUILD)

dist: $(TARGET)
	cp $(TARGET) /var/www/files

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) -c $^ -o $@ $(CXXFLAGS)

$(BUILD)/igmplisten: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(BUILD)/igmpclient.o

$(BUILD)/igmpjoin: $(OBJS)
	$(CXX) $(LDFLAGS) $(PROFILE) -o $@ $(BUILD)/igmpjoin.o

#$(TARGETS): $(OBJS)
#	$(CXX) $(LDFLAGS) -o $@ $^  

clean:
	rm -rf build/*

info:
	@echo ARCH: $(ARCH)
	@echo DK_VERSION: $(DK_VERSION)
	@echo TOOLCHAIN_DIR: $(TOOLCHAIN_DIR) 
	@echo DK_DIR: /kreatel/$(DK_VERSION)/dist/$(ARCH)/toolchain_dir
	@echo C SRC: $(C_FILES)
	@echo OBJS: $(OBJS)
	@echo CC: $(CC)
	@echo CXX: $(CXX)

