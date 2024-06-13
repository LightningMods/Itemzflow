#include "defines.h"
#include "log.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvisibility"

// NOTE that max_read will be ignored when mounting so if you want to change it
// then you also have to change the fuse mounting func
// DEBUG
// char* argv[] = { "dummy", "/host", "-o", "allow_other", "-o", "rw", "-o",
// "use_ino", "-o", "direct_io", "-o", "attr_timeout=0.0", "-o",
// "max_read=262144", "-f", "-d", NULL };
// NON-DEBUG
// char* argv[] = { "dummy", "/hostapp", "-o", "allow_other", "-o", "rw", "-o",
// "use_ino", "-o", "direct_io", "-o", "attr_timeout=0.0", "-o",
// "max_read=262144", "-f", NULL };

#include <sys/sysctl.h>

uint32_t sdkVersion = -1;

uint32_t ps4_fw_version(void) {
  // cache the FW Version
  if (0 < sdkVersion) {
    size_t len = 4;
    // sysKernelGetLowerLimitUpdVersion == machdep.lower_limit_upd_version
    // rewrite of sysKernelGetLowerLimitUpdVersion
    sysctlbyname("machdep.lower_limit_upd_version", &sdkVersion, &len, NULL, 0);
  }

  // FW Returned is in HEX
  return (sdkVersion >> 16);
}

int fuse_kernel_patches_505(struct thread *td) {

  void *kernel_base = &((uint8_t *)kernelRdmsr(0xC0000082))[-0x1C0];
  uint8_t *kernel_ptr = (uint8_t *)kernel_base;
  int *ksuser_enabled = (int *)(kernel_base + 0x2300B88);
  if (*ksuser_enabled == 1) // kernel already patched
    return 0;

  cpu_disable_wp();
  struct vfsconf *p = (struct vfsconf *)(kernel_base + 0x19FC380);
  // suser_enabled in priv_check_cred
  *ksuser_enabled = 1;
  // add jail friendly for fuse file system
  p->vfc_flags = 0x00400000 | 0x00080000;
  // avoid enforce_dev_perms checks default prison_priv_check to 0
  kernel_ptr[0x3B219E] = 0;
  kernel_ptr[0x49DFE4] = 0x0F;
  kernel_ptr[0x49DFE5] = 0x84;

  // skip devkit/testkit/dipsw check in fuse_loader
  kernel_ptr[0x49DDDE] = 0xEB;
  kernel_ptr[0x49DDDF] = 0x1B;
  // skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
  kernel_ptr[0x4A28EE] = 0xEB;
  kernel_ptr[0x4A28EF] = 0x0;
  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_close_device
  kernel_ptr[0x4A29E2] = 0xEB;
  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_poll_device
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
  kernel_ptr[0x4A27FC] = 0xB6;
  // patch kernel
  cpu_enable_wp();

  int (*fuse_loader)(void *m, int op, void *arg) =
      (void *)(kernel_base + 0x49DDB0);
  fuse_loader(NULL, 0, NULL);

  return 0;
}

int fuse_kernel_patches_900(struct thread *td) {

  void *kernel_base = &((uint8_t *)kernelRdmsr(0xC0000082))[-0x1C0];
  uint8_t *kernel_ptr = (uint8_t *)kernel_base;
  int *ksuser_enabled = (int *)(kernel_base + 0x01B94588); // 0x01B94588

  if (*ksuser_enabled == 1) // kernel already patched
    return 0;

  cpu_disable_wp();
  struct vfsconf *p = (struct vfsconf *)(kernel_base + 0x01A7F188); // 0x01A7F188
  // suser_enabled in priv_check_cred
  *ksuser_enabled = 1;
  // add jail friendly for fuse file system
  p->vfc_flags = 0x00400000 | 0x00080000;
  // avoid enforce_dev_perms checks
  // default prison_priv_check to 0
  kernel_ptr[0x043BCB6] = 0;
  kernel_ptr[0x0493875] = 0x84;
  // skip devkit/testkit/dipsw check in fuse_loader
  kernel_ptr[0x049074E] = 0xEB;
  kernel_ptr[0x049074F] = 0x1B;
  // skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
  kernel_ptr[0x490AC5] = 0xEB;
  kernel_ptr[0x490AC4] = 0x0;
  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_close_device
  kernel_ptr[0x0490BC2] = 0xEB;
  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_poll_device
  kernel_ptr[0x0491112] = 0x84;
  // skip sceSblACMgrIsSyscoreProcess check in fuse_vfsop_mount
  kernel_ptr[0x0492BD7] = 0x85;
  // skip sceSblACMgrIsMinisyscore/unknown check in fuse_vfsop_unmount
  kernel_ptr[0x049332A] = 0x84;
  // skip sceSblACMgrIsSystemUcred check in fuse_vfsop_statfs
  kernel_ptr[0x04936DD] = 0xEB;
  kernel_ptr[0x04936DE] = 0x04;
  kernel_ptr[0x04909CE] = 0xB6;
  // patch kernel
  cpu_enable_wp();

  int (*fuse_loader)(void *m, int op, void *arg) =
      (void *)(kernel_base + 0x0490720); // 0x0490720
  fuse_loader(NULL, 0, NULL);

  return 0;
}

int fuse_kernel_patches_672(struct thread *td) {

  void *kernel_base = &((uint8_t *)kernelRdmsr(0xC0000082))[-0x1C0];
  uint8_t *kernel_ptr = (uint8_t *)kernel_base;
  int *ksuser_enabled = (int *)(kernel_base + 0x1be9498); // 0x1BE9498

  if (*ksuser_enabled == 1) // kernel already patched
    return 0;

  cpu_disable_wp();
  struct vfsconf *p = (struct vfsconf *)(kernel_base + 0x01aa0a68); // 0x01A7F188
  // suser_enabled in priv_check_cred
  *ksuser_enabled = 1;
  // add jail friendly for fuse file system
  p->vfc_flags = 0x00400000 | 0x00080000;
  // avoid enforce_dev_perms checks default prison_priv_check to 0
  kernel_ptr[0x270e8e] = 0;
  kernel_ptr[0x4b0c74] = 0x0F;
  kernel_ptr[0x4b0c75] = 0x84;
  // skip devkit/testkit/dipsw check in fuse_loader
  kernel_ptr[0x4af2fe] = 0xEB;
  kernel_ptr[0x4af2ff] = 0x1B;
  // skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
  kernel_ptr[0x4aeb14] = 0xEB;
  kernel_ptr[0x4aeb15] = 0x0;
  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_close_device
  kernel_ptr[0x4aec12] = 0xEB;
  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_poll_device
  kernel_ptr[0x4AF174] = 0xEB;
  // skip sceSblACMgrIsSyscoreProcess check in fuse_vfsop_mount
  kernel_ptr[0x4ac086] = 0xEB;
  kernel_ptr[0x4ac087] = 0x04;
  // skip sceSblACMgrIsMinisyscore/unknown check in fuse_vfsop_unmount
  kernel_ptr[0x4ac7dc] = 0xEB;
  kernel_ptr[0x4ac7dd] = 0x00;
  // skip sceSblACMgrIsSystemUcred check in fuse_vfsop_statfs
  kernel_ptr[0x4acb8d] = 0xEB;
  kernel_ptr[0x4acb8e] = 0x04;
  kernel_ptr[0x4aea2c] = 0xB6;
  // patch kernel
  cpu_enable_wp();

  int (*fuse_loader)(void *m, int op, void *arg) =
      (void *)(kernel_base + 0x4af2d0); // 0x0490720
  fuse_loader(NULL, 0, NULL);

  return 0;
}

int fuse_kernel_patches_1100(struct thread *td) {

  void *kernel_base = &((uint8_t *)kernelRdmsr(0xC0000082))[-0x1C0];
  uint8_t *kernel_ptr = (uint8_t *)kernel_base;
  int *ksuser_enabled = (int *)(kernel_base + 0x22A6828); // 0x01B94588

  if (*ksuser_enabled == 1) // kernel already patched
    return 0;

  cpu_disable_wp();
  struct vfsconf *p = (struct vfsconf *)(kernel_base + 0x01A7F188); // 0x01A7F188
  // suser_enabled in priv_check_cred
  *ksuser_enabled = 1;
  // add jail friendly for fuse file system
  p->vfc_flags = 0x00400000 | 0x00080000;
  // avoid enforce_dev_perms checks
  
// default prison_priv_check to 0
  kernel_ptr[0x354D86] = 0;     //sb
  kernel_ptr[0x48F9C5] = 0x84;  //sb

  // skip devkit/testkit/dipsw check in fuse_loader
  kernel_ptr[0x49499E] = 0xEB;  //sb
  kernel_ptr[0x49499F] = 0x1B;  //sb

  // skip sceSblACMgrIsSyscoreProcess check in fuse_open_device
  kernel_ptr[0x495EA4] = 0x0;   //sb
  kernel_ptr[0x495EA5] = 0xEB;  //sb

  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_close_device
  kernel_ptr[0x495FA2] = 0xEB;  //sb

  // skip sceSblACMgrIsDebuggerProcess/sceSblACMgrIsSyscoreProcess check in
  // fuse_poll_device
  kernel_ptr[0x4964F2] = 0x84;  //sb

  // skip sceSblACMgrIsSyscoreProcess check in fuse_vfsop_mount
  kernel_ptr[0x0492987] = 0x85; //sb

  // skip sceSblACMgrIsMinisyscore/unknown check in fuse_vfsop_unmount
  kernel_ptr[0x4930DA] = 0x84;  //sb

  // skip sceSblACMgrIsSystemUcred check in fuse_vfsop_statfs
  kernel_ptr[0x49348D] = 0xEB;  //sb
  kernel_ptr[0x49348E] = 0x04;  //sb

  kernel_ptr[0x495DAE] = 0xB6;  //sb

  // patch kernel
  cpu_enable_wp();

  int (*fuse_loader)(void *m, int op, void *arg) =
      (void *)(kernel_base + 0x494970); // 0x0490720
  fuse_loader(NULL, 0, NULL);

  return 0;
}

bool fuse_fw_supported() {
  switch (ps4_fw_version()) {
  case 0x507:
  case 0x505:
    syscall(11, fuse_kernel_patches_505);
    return true;
  case 0x672:
    syscall(11, fuse_kernel_patches_672);
    return true;
  case 0x900:
    syscall(11, fuse_kernel_patches_900);
    return true;
  case 0x1100:
    syscall(11, fuse_kernel_patches_1100);
	return true;
  default: {
    log_info("fuse: Unsupported firmware version, exiting ...");
    return false;
  }
  }
}
