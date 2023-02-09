#include "log.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <stddef.h>
#define FUSE_USE_VERSION 26
#include "fuse/fuse.h"
#include "defines.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>

int fuse_ret  = -1;
int sceKernelSetBesteffort(int besteffort, int priority);
// NOTE that max_read will be ignored when mounting so if you want to change it then you
// also have to change the fuse mounting func 
//DEBUG 
//char* argv[] = { "dummy", "/host", "-o", "allow_other", "-o", "rw", "-o", "use_ino", "-o", "direct_io", "-o", "attr_timeout=0.0", "-o", "max_read=262144", "-f", "-d", NULL };
// NON-DEBUG
//char* argv[] = { "dummy", "/hostapp", "-o", "allow_other", "-o", "rw", "-o", "use_ino", "-o", "direct_io", "-o", "attr_timeout=0.0", "-o", "max_read=262144", "-f", NULL };
int fuse_kernel_patches_505(struct thread *td)
{
   
	void* kernel_base = &((uint8_t*)kernelRdmsr(0xC0000082))[-0x1C0];
	uint8_t* kernel_ptr = (uint8_t*)kernel_base;
	int *ksuser_enabled=(int *)(kernel_base+0x2300B88);
	if(*ksuser_enabled == 1) // kernel already patched
	   return NULL;

    cpu_disable_wp();
	struct vfsconf *p=(struct vfsconf *)(kernel_base + 0x19FC380 );
	//suser_enabled in priv_check_cred
	*ksuser_enabled=1;
	//add jail friendly for fuse file system
	p->vfc_flags= 0x00400000 | 0x00080000;
	//avoid enforce_dev_perms checks default prison_priv_check to 0
	kernel_ptr[0x3B219E]=0;     
	kernel_ptr[0x49DFE4] = 0x0F;
	kernel_ptr[0x49DFE5] = 0x84;
	//skip devkit/testkit/dipsw check in fuse_loader
	kernel_ptr[0x49DDDE] = 0xEB;
	kernel_ptr[0x49DDDF] = 0x1B;
	//skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
	kernel_ptr[0x4A28EE] = 0xEB;
	kernel_ptr[0x4A28EF] = 0x0;
	//skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in fuse_close_device
	kernel_ptr[0x4A29E2] = 0xEB;
	//skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in fuse_poll_device
	kernel_ptr[0x4A2F34] = 0xEB;
	// skip sceSblACMgrIsSyscoreProcess check in fuse_vfsop_mount
	kernel_ptr[0x4A30F7] = 0xEB;
	kernel_ptr[0x4A30F8] = 0x04;
	// skip sceSblACMgrIsMinisyscore/unknown check in fuse_vfsop_unmount
	kernel_ptr[0x4A384C] = 0xEB;
	kernel_ptr[0x4A384D] = 0x00;
	// skip sceSblACMgrIsSystemUcred check in fuse_vfsop_statfs
	kernel_ptr[0x4A3BED] = 0xEB;
	kernel_ptr[0x4A3BEE] = 0x04;	
    kernel_ptr[0x4A27FC]=0xB6;
    // patch kernel
    cpu_enable_wp();

	int (*fuse_loader)(void* m, int op, void* arg)=(void *)(kernel_base + 0x49DDB0);
    fuse_loader(NULL, 0, NULL);


    return NULL;
}

int fuse_kernel_patches_900(struct thread *td)
{
   
	void* kernel_base = &((uint8_t*)kernelRdmsr(0xC0000082))[-0x1C0];
	uint8_t* kernel_ptr = (uint8_t*)kernel_base;
	int *ksuser_enabled=(int *)(kernel_base+0x01B94588); //0x01B94588

	if(*ksuser_enabled == 1) // kernel already patched
	    return NULL;

    cpu_disable_wp();
	struct vfsconf *p=(struct vfsconf *)(kernel_base + 0x01A7F188 ); //0x01A7F188
	//suser_enabled in priv_check_cred
	*ksuser_enabled=1;
	//add jail friendly for fuse file system
	p->vfc_flags = 0x00400000 | 0x00080000;
	//avoid enforce_dev_perms checks
	//default prison_priv_check to 0
	kernel_ptr[0x043BCB6] = 0;
	kernel_ptr[0x0493875] = 0x84; 
	//skip devkit/testkit/dipsw check in fuse_loader
	kernel_ptr[0x049074E] = 0xEB; 
    kernel_ptr[0x049074F] = 0x1B; 
	//skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
	kernel_ptr[0x490AC5] = 0xEB; 
    kernel_ptr[0x490AC4] = 0x0;
	//skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in fuse_close_device
	kernel_ptr[0x0490BC2] = 0xEB;
	//skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in fuse_poll_device
	kernel_ptr[0x0491112] = 0x84; 
	// skip sceSblACMgrIsSyscoreProcess check in fuse_vfsop_mount
	kernel_ptr[0x0492BD7] = 0x85;
	// skip sceSblACMgrIsMinisyscore/unknown check in fuse_vfsop_unmount
	kernel_ptr[0x049332A] = 0x84;
	// skip sceSblACMgrIsSystemUcred check in fuse_vfsop_statfs
	kernel_ptr[0x04936DD] = 0xEB; 
    kernel_ptr[0x04936DE] = 0x04;
    kernel_ptr[0x04909CE]= 0xB6; 
    // patch kernel
    cpu_enable_wp();

	int (*fuse_loader)(void* m, int op, void* arg)=(void *)(kernel_base + 0x0490720);  //0x0490720
    fuse_loader(NULL, 0, NULL);

    return NULL;
}

bool fuse_fw_supported()
{
	switch (ps4_fw_version())
	{
	case 0x505:
		syscall(11, fuse_kernel_patches_505);
        return true;
	case 0x900:
		syscall(11, fuse_kernel_patches_900);
		return true;
	default:{
		log_info("fuse: Unsupported firmware version, exiting ...");
		return false;
	  }
	}
}

int initialize_userland_fuse(const char* mp)
{
    log_info("Starting FUSE ...");
    sceKernelSetBesteffort(ULPMGR_THREAD_PRIO_BASE, 255); // set best cpu effort to highest for IO

    if(!if_exists("/dev/mira")){
	  log_info("fuse: Mira not found");
      if(!fuse_fw_supported())
		  return FUSE_FW_NOT_SUPPORTED;
    }
    else
      log_debug("fuse: Mira found, no patches will be applied");
	
	struct fuse_actions args = { args.argc = 16 };
	snprintf(args.path, sizeof(args.path)-1, "%s", mp);
	snprintf(args.ip, sizeof(args.ip)-1, "%s", fuse_session_ip);
	args.is_debug_mode = fuse_debug_flag;

	free(fuse_session_ip), fuse_session_ip = NULL;
    
	unlink("/data/itemzflow_daemon/libfuse.log");
	fuse_ret = fuse_nfs_main(args);
	log_info("fuse: fuse_main returned %d", fuse_ret);
	return fuse_ret;
}