
# Library metadata.
TARGET := curl.prx
# Libraries linked into the ELF.
LIBS 		 := -lcurl -lpolarssl -lSceNet -lc -lc++ -lkernel

# Additional compile flags.
#EXTRAFLAGS  :=

# You likely won't need to touch anything below this point.

# Root vars
TOOLCHAIN 	:= $(OO_PS4_TOOLCHAIN)
PROJDIR   	:= src
INTDIR    	:= build

# Define objects to build
CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
COMMONFILES := $(wildcard $(COMMONDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.c.obj, $(CFILES)) $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.c.obj, $(CPPFILES)) $(patsubst $(COMMONDIR)/%.cpp, $(INTDIR)/%.c.obj, $(COMMONFILES))
STUBOBJS    := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.c.obj, $(CFILES)) $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.c.obj.stub, $(CPPFILES)) $(patsubst $(COMMONDIR)/%.cpp, $(INTDIR)/%.c.obj.stub, $(COMMONFILES))

# Define final C/C++ flags
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fdeclspec -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -Iinclude
CXXFLAGS    := $(CFLAGS) -DBUILDING_LIBCURL=1 -D__PS4__ -DUSE_MBEDTLS=1 -DUSE_UNIX_SOCKETS=0 -DENABLE_THREADED_RESOLVER=0 -DENABLE_IPV6=0 -D__OPENORBIS__ -D__ORBIS__ -isystem $(TOOLCHAIN)/include/c++/v1 -Iinclude
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crtlib.o

# Create the intermediate directory incase it doesn't already exist.
_unused     := $(shell mkdir -p $(INTDIR))

# Check for linux vs macOS and account for clang/ld path
UNAME_S     := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
		CC      := clang
		CCX     := clang++
		LD      := ld.lld
		CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
		CC      := /usr/local/opt/llvm/bin/clang
		CCX     := /usr/local/opt/llvm/bin/clang++
		LD      := /usr/local/opt/llvm/bin/ld.lld
		CDIR    := macos
endif

$(TARGET): $(INTDIR) $(OBJS)
	$(LD) $(INTDIR)/*.c.obj -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf --lib=$(TARGET) --paid 0x3800000000000011

$(INTDIR)/%.c.obj: $(PROJDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.c.obj: $(PROJDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/%.c.obj.stub: $(PROJDIR)/%.c
	$(CC) -target x86_64-pc-linux-gnu -ffreestanding -nostdlib -fno-builtin -fPIC -c $(TOOLCHAIN)/include -o $@ $<

$(INTDIR)/%.c.obj.stub: $(PROJDIR)/%.cpp
	$(CCX) -target x86_64-pc-linux-gnu -ffreestanding -nostdlib -fno-builtin -fPIC -c $(TOOLCHAIN)/include -o $@ $<

.PHONY: clean
.DEFAULT_GOAL := all

all: $(TARGET) $(TARGETSTUB)

clean:
	rm -f $(TARGET)  $(INTDIR)/$(PROJDIR).elf $(INTDIR)/$(PROJDIR)/oelf $(OBJS)
