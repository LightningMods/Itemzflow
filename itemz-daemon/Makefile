ifndef ORBISDEV
$(error ORBISDEV, is not set)
endif

target ?= ps4_elf_sce
TargetFile=daemon.elf

include $(ORBISDEV)/make/ps4sdk.mk

LinkerFlags +=  -lfuse -lkernel_stub -lSceLibcInternal_stub -lSceSysmodule_stub -lSceSystemService_stub -lSceNet_stub -lSceUserService_stub -lScePigletv2VSH_stub -lSceVideoOut_stub  -lSceImeDialog_stub -lSceGnmDriver_stub  -lorbis -lorbisGl -lScePad_stub -lSceCommonDialog_stub -lSceMsgDialog_stub -lSceAudioOut_stub -lSceIme_stub -lSceAppInstUtil_stub -lSceBgft_stub -lSceSysUtil_stub -lSceHttp_stub -lSceSsl_stub -lSceNetCtl_stub -lfreetype-gl -lSceFreeTypeOl_stub -lkernel_sys_stub -lSceLncUtil_stub -lSQLite -lSceSysCore_stub

CompilerFlags += -Wno-implicit-function-declaration -Wno-int-to-pointer-cast -D_FILE_OFFSET_BITS=64 -D__Freebsd__ -D__PS4__ -D__ORBIS__ -fstack-protector-all 

IncludePath += -I$(ORBISDEV)/usr/include -I$(ORBISDEV)/usr/include/c++/v1 -I$(ORBISDEV)/usr/include/orbis
IncludePath += -I$(ORBISDEV)/usr/include/orbis/freetype-gl


oelf:
	orbis-elf-create bin/daemon.elf bin/daemon.oelf >/dev/null 2>&1


AUTH_INFO = 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

eboot:
	python2 $(ORBISDEV)/bin/make_fself.py --auth-info $(AUTH_INFO) bin/daemon.oelf bin/eboot.bin
pkg_build:
	cp bin/eboot.bin pkg/eboot.bin && cd pkg && pkgTool pkg_build Project.gp4 . && cp *.pkg ../bin/
