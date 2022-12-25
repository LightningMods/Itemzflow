ifndef ORBISDEV
$(error ORBISDEV, is not set)
endif

target := ps4_lib
OutPath := lib
TargetFile := libfuse
AllTarget = $(OutPath)/$(TargetFile).a 

include $(ORBISDEV)/make/ps4sdklib.mk
CompilerFlags +=  -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -DHAVE_CONFIG_H -D_U_="__attribute__((unused))" -D__ORBIS__=1 -D_ORBIS_ -D__FreeBSD__=9  -DNDEBUG -D_BSD_SOURCE -fexceptions -frtti
IncludePath += -I$(ORBISDEV)/usr/include -I$(ORBISDEV)/usr/include/c++/v1 -Iinclude  


$(OutPath)/$(TargetFile).a: $(ObjectFiles)
	$(dirp)
	$(archive)

install:
	@cp $(OutPath)/$(TargetFile).a $(DESTDIR)$(ORBISDEV)/usr/lib
	@echo "$(TargetFile) Installed!"

