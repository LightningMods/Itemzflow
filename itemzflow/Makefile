ifndef ORBISDEV
$(error ORBISDEV, is not set)
endif

target ?= ps4_elf_sce
TargetFile=homebrew.elf
AUTH_INFO = 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

include $(ORBISDEV)/make/ps4sdk.mk

LinkerFlags +=  -ltag -lSceShellCoreUtil_stub  -ldumper -lc++ -lc++abi -lc++experimental -luser_mem_sys  -lkernel_sys_stub -lSceLibcInternal_stub -lSceSysmodule_stub -lSceSystemService_stub -lSceNet_stub -lSceUserService_stub -lScePigletv2VSH_stub -lSceVideoOut_stub  -lSceImeDialog_stub -lSceGnmDriver_stub  -lorbis -lorbisGl -lScePad_stub -lSceCommonDialog_stub -lSceMsgDialog_stub -lSceAudioOut_stub -lSceIme_stub -lSceAppInstUtil_stub -lSceBgft_stub -lSceSysUtil_stub -lSceHttp_stub -lSceSsl_stub -lSceNetCtl_stub -lfreetype-gl -lSceFreeTypeOl_stub -lkernel_sys_stub -lSceLncUtil_stub -lSQLite -lSceSaveData_stub  -lSceShellUIUtil_stub -lSceSysCore_stub
CompilerFlagsCpp += -fstack-protector-all -Wall -Werror  -Wno-excess-initializers -DFMT_HEADER_ONLY  # -DHAVE_SHADER_COMPILER 
CompilerFlagsCpp += -std=c++17 -D_FILE_OFFSET_BITS=64 -Wno-c++11-narrowing  -DHAVE_CONFIG_H -D_U_="__attribute__((unused))" -D__ORBIS__=1 -D_ORBIS_ -DNDEBUG -D_BSD_SOURCE -fno-exceptions -frtti
CompilerFlags += -D_FILE_OFFSET_BITS=64
IncludePath += -I$(ORBISDEV)/usr/include -I$(ORBISDEV)/usr/include/c++/v1 -I$(ORBISDEV)/usr/include/orbis
IncludePath += -I$(ORBISDEV)/usr/include/orbis/freetype-gl


install:
	@cp $(OutPath)/homebrew.self /hostapp/homebrew.self
	@echo "Installed!"
oelf:
	orbis-elf-create bin/homebrew.elf bin/homebrew.oelf >/dev/null 2>&1

eboot:
	python2 $(ORBISDEV)/bin/make_fself.py --auth-info $(AUTH_INFO) bin/homebrew.oelf bin/ItemzCore.self
pkg_build:
	pkgTool pkg_build ../App-Media-Assets/itemzflow.gp4 .
