#include "defines.h"
#include "utils.h"
#include <cstdint>
#include <cstring>
#include <installpkg.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string>


extern std::string tmp;
extern bool update_is_supported;

int PKG_ERROR(const char *name, int ret) {
  msgok(MSG_DIALOG::WARNING,
        fmt::format("{0:}\nHEX: {1:#x} Int: {2:d}\n{3:}",
                    getLangSTR(LANG_STR::INSTALL_FAILED), ret, ret, name));
  log_error("%s error: %x", name, ret);

  return ret;
}

/* we use bgft heap menagement as init/fini as flatz already shown at
 * https://github.com/flatz/ps4_remote_pkg_installer/blob/master/installer.c
 */

#define BGFT_HEAP_SIZE (1 * 1024 * 1024)

extern bool sceAppInst_done;
static bool s_bgft_initialized = false;
static struct bgft_init_params s_bgft_init_params;

bool app_inst_util_init(void) {
  int ret;

  if (sceAppInst_done) {
    goto done;
  }

  ret = sceAppInstUtilInitialize();
  if (ret) {
    log_debug("sceAppInstUtilInitialize failed: 0x%08X", ret);
    goto err;
  }

  sceAppInst_done = true;

done:
  return true;

err:
  sceAppInst_done = false;

  return false;
}

void app_inst_util_fini(void) {
  int ret;

  if (!sceAppInst_done) {
    return;
  }

  ret = sceAppInstUtilTerminate();
  if (ret) {
    log_debug("sceAppInstUtilTerminate failed: 0x%08X", ret);
  }

  sceAppInst_done = false;
}

bool bgft_init(void) {
  int ret;

  if (s_bgft_initialized) {
    goto done;
  }

  memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));
  {
    s_bgft_init_params.heapSize = BGFT_HEAP_SIZE;
    s_bgft_init_params.heap = (uint8_t *)malloc(s_bgft_init_params.heapSize);
    if (!s_bgft_init_params.heap) {
      log_debug("No memory for BGFT heap.");
      goto err;
    }
    memset(s_bgft_init_params.heap, 0, s_bgft_init_params.heapSize);
  }

  ret = sceBgftServiceInit(&s_bgft_init_params);
  if (ret) {
    log_debug("sceBgftInitialize failed: 0x%08X", ret);
    goto err_bgft_heap_free;
  }

  s_bgft_initialized = true;

done:
  return true;

err_bgft_heap_free:
  if (s_bgft_init_params.heap) {
    free(s_bgft_init_params.heap);
    s_bgft_init_params.heap = NULL;
  }

  memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));

err:
  s_bgft_initialized = false;

  return false;
}

void bgft_fini(void) {
  int ret;

  if (!s_bgft_initialized) {
    return;
  }

  ret = sceBgftServiceTerm();
  if (ret) {
    log_debug("sceBgftServiceTerm failed: 0x%08X", ret);
  }

  if (s_bgft_init_params.heap) {
    free(s_bgft_init_params.heap);
    s_bgft_init_params.heap = NULL;
  }

  memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));

  s_bgft_initialized = false;
}

bool app_inst_util_is_exists(const char *title_id, bool *exists) {
  int flag;

  if (!title_id)
    return false;

  if (!sceAppInst_done) {
    log_debug("Starting app_inst_util_init..");
    if (!app_inst_util_init()) {
      log_error("app_inst_util_init has failed...");
      return false;
    }
  }

  int ret = sceAppInstUtilAppExists(title_id, &flag);
  if (ret) {
    log_error("sceAppInstUtilAppExists failed: 0x%08X\n", ret);
    return false;
  }

  if (exists)
    *exists = flag;

  return true;
}

/* sample ends */
/* install package wrapper:
   init, (install), then clean AppInstUtil and BGFT
   for next install */
extern bool Download_icons;

bool pkg_is_patch(const char *src_dest) {

  // Read PKG header
  struct pkg_header hdr;
  static const uint8_t magic[] = {'\x7F', 'C', 'N', 'T'};
  // Open path
  int pkg = sceKernelOpen(src_dest, O_RDONLY, 0);
  if (pkg < 0)
    return false;

  sceKernelLseek(pkg, 0, SEEK_SET);
  sceKernelRead(pkg, &hdr, sizeof(struct pkg_header));

  sceKernelClose(pkg);

  if (memcmp(hdr.magic, magic, sizeof(magic)) != 0) {
    log_error("PKG Format is wrong");
    return false;
  }

  if (!extract_sfo_from_pkg(src_dest, "/user/app/ITEM00001/tmp_param.sfo")) {
    log_error("Could not extract param.sfo");
    return false;
  }
  // TODO make more effecient
  std::vector<uint8_t> sfo_data = readFile("/user/app/ITEM00001/tmp_param.sfo");
  if (sfo_data.empty()) {
    log_error("Could not read param.sfo");
    return false;
  }

  unlink("/user/app/ITEM00001/tmp_param.sfo");

  SfoReader sfo(sfo_data);
  return (sfo.GetValueFor<std::string>("CATEGORY") == "gp");
}

void *install_prog(void *argument) {
  struct install_args *args = (install_args *)argument;
  SceBgftTaskProgress progress_info;
  bool is_threaded = args->is_thread;

  tmp = fmt::format("{0:.60}\n\n{1:}  {2:d}/{3:d}", args->fname,
                    getLangSTR(LANG_STR::INSTALLING), args->curr_pkg,
                    args->max_pkgs);

  if (!is_threaded)
    progstart(tmp.c_str());
  else
    log_info("Starting PKG Install");

  int prog = 0;
  while (prog < 100) {
    memset(&progress_info, 0, sizeof(progress_info));

    int ret = sceBgftServiceDownloadGetProgress(args->task_id, &progress_info);
    if (ret)
      return (void *)(intptr_t)PKG_ERROR("sceBgftDownloadGetProgress", ret);

    if (progress_info.error_result != 0) {

      if (!is_threaded)
        return (void *)(intptr_t)PKG_ERROR("BGFT_ERROR",
                                           progress_info.error_result);
      else {
        log_error("BGFT_ERROR error: %x", progress_info.error_result);
        return NULL;
      }
    }

    prog =
        (uint32_t)(((float)progress_info.transferred / progress_info.length) *
                   100.f);

    if (!is_threaded)
      ProgSetMessagewText(prog, tmp.c_str());

    if (progress_info.transferred % (4096 * 128) == 0) {
      log_debug("%s, Install_Thread: reading data, %lub / %lub (%%%i) ERR: %i",
                __FUNCTION__, progress_info.transferred, progress_info.length,
                prog, progress_info.error_result);
      // log_debug("ERR: %llu %llu %llu %llu", progress_info.transferred,
      // progress_info.transferredTotal, progress_info.length,
      // progress_info.lengthTotal);
    }
  }

  if (progress_info.error_result == 0) {
    if (!is_threaded) {
      if (args->curr_pkg >= args->max_pkgs)
        sceMsgDialogTerminate();

      ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::PKG_INST_SUCCESS),
                 fmt::format("{0:}", args->title_id));
    }
    if (args->delete_pkg)
      unlink(args->path);
  } else {
    if (!is_threaded) {
      sceMsgDialogTerminate();
      ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::PKG_INST_FAILED),
                 fmt::format("{0:#x}", progress_info.error_result));
      msgok(MSG_DIALOG::WARNING,
            fmt::format("{0:} {1:} {2:} {3:#X}",
                        getLangSTR(LANG_STR::INSTALL_OF), args->title_id,
                        getLangSTR(LANG_STR::INSTALL_FAILED),
                        progress_info.error_result));
    } else
      log_error("Installation of %s has failed with code 0x%x", args->title_id,
                progress_info.error_result);
  }

  log_info("Finalizing Memory...");
  free(args->title_id);
  free(args->path);
  free(args->fname);
  free(args);
  bgft_fini();

  if (is_threaded)
    pthread_exit(NULL);

  return NULL;
}

uint32_t pkginstall(const char *fullpath, const char *filename,
                    bool show_install_prog, bool delete_pkg_after, int max_pkgs,
                    int curr_pkg) {
  char title_id[16];
  int is_app, ret = -1;
  int task_id = -1;
  bool is_app_exists = false;
  std::string tmp;

  if (if_exists(fullpath)) {
    if (!sceAppInst_done) {
      log_info("Initializing AppInstUtil...");

      if (!app_inst_util_init())
        return PKG_ERROR("AppInstUtil", ret);
    }

    log_info("Initializing BGFT...");
    if (!bgft_init()) {
      return PKG_ERROR("BGFT_initialization", ret);
    }

    ret = sceAppInstUtilGetTitleIdFromPkg(fullpath, title_id, &is_app);
    if (ret)
      return PKG_ERROR("sceAppInstUtilGetTitleIdFromPkg", ret);

    if (pkg_is_patch(fullpath) &&
        app_inst_util_is_exists(title_id, &is_app_exists) && !is_app_exists)
      return PKG_ERROR("BASE_GAME_NOT_INSTALLED", ret);

    tmp = fmt::format("ItemzFlow: {}", title_id);
    struct bgft_download_param_ex download_params;
    memset(&download_params, 0, sizeof(download_params));
    download_params.param.entitlement_type = 5;
    download_params.param.id = "";
    download_params.param.content_url = fullpath;
    download_params.param.content_name = tmp.c_str();
    download_params.param.icon_path = "/update/fakepic.png";
    download_params.param.playgo_scenario_id = "0";
    if (show_install_prog)
      download_params.param.option = BGFT_TASK_OPTION_INVISIBLE;
    else
      download_params.param.option = BGFT_TASK_OPTION_DISABLE_CDN_QUERY_PARAM;

    download_params.slot = 0;

  retry:
    log_info("%s 1", __FUNCTION__);
    ret = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&download_params,
                                                           &task_id);
    if (ret == 0x80990088 || ret == 0x80990015) {
      if (Confirmation_Msg(getLangSTR(LANG_STR::REINSTALL_APP_CONFIRMATION)) ==
          YES) {
        ret = sceAppInstUtilAppUnInstall(&title_id[0]);
        if (ret != 0)
          return PKG_ERROR("sceAppInstUtilAppUnInstall", ret);

        goto retry;
      } else
        return PKG_ERROR("APP_ALREADY_EXISTS", ret);
    } 
    else if(ret == 0x80990086) {
        msgok(MSG_DIALOG::WARNING, "this game is already installing via the notifcations, please cancel it to continue.");
    return 0x80990086;
   }
   else if (ret)
      return PKG_ERROR("sceBgftServiceIntDownloadRegisterTaskByStorageEx", ret);

    log_info("Task ID(s): 0x%08X", task_id);

    ret = sceBgftServiceDownloadStartTask(task_id);
    if (ret)
      return PKG_ERROR("sceBgftDownloadStartTask", ret);
  } else // bgft_download_get_task_progress
    return PKG_ERROR("no file at", ret);

  if (!show_install_prog) {
      log_info("Install progress is disabled, sent to the PS4...");
      return 0;
  }

  struct install_args *args =
      (struct install_args *)malloc(sizeof(struct install_args));
  args->title_id = strdup(title_id);
  args->task_id = task_id;
  // args->size = size;
  args->path = strdup(fullpath);
  args->fname = strdup(filename);
  args->is_thread = !show_install_prog;
  args->delete_pkg = delete_pkg_after;
  args->max_pkgs = max_pkgs;
  args->curr_pkg = curr_pkg;

  install_prog((void *)args);

  log_info("%s(%s) done.", __FUNCTION__, fullpath);

  return 0;
}

uint32_t installPatchPKG(const char *url, const char *title_id, const char* icon_path) {
  int ret = -1;
  int task_id = -1;

  if(!update_is_supported){
    if(custom_Confirmation_Msg("This Update is not compatible with your current FW, would you like to install it anyways? ", "Install anyways", "DO NOT INSTALL") == NO){
      log_info("User chose not to install the update");
      return -1;
    }
  }

  log_info("Initializing BGFT...");
  if (!bgft_init()) {
    return PKG_ERROR("BGFT_initialization", ret);
  }

  std::string tmp = fmt::format("ItemzFlow Patch Installer | {}", title_id);
  struct bgft_download_param_ex download_params;
  memset(&download_params, 0, sizeof(download_params));
  download_params.param.entitlement_type = 5;
  download_params.param.id = "";
  download_params.param.content_url = url;
  download_params.param.content_name = tmp.c_str();
  download_params.param.icon_path = icon_path;
  download_params.param.playgo_scenario_id = "0";
  download_params.param.option = BGFT_TASK_OPTION_DISABLE_CDN_QUERY_PARAM;

  download_params.slot = 0;

  ret = sceBgftServiceIntDebugDownloadRegisterPkg(&download_params, &task_id);
  if(ret == 0x80990015) {
    msgok(MSG_DIALOG::WARNING, "A Patch for this game is already installing please cancel it to continue.");
    return 0x80990015;
  }
  else if(ret == 0x804101E8 || ret == 0x804101E1 || ret == 0x80990045 || ret  == 0x8041013D){
    msgok(MSG_DIALOG::WARNING, "Your Network is blocking the connection to Sonys Server/CDN, Try unblocking it or removing your DNS before trying again");
    return 0xDEADBABE;

  }
  else if (ret) return PKG_ERROR( "sceBgftServiceIntDebugDownloadRegisterPkg", ret);

  log_info("Task ID(s): 0x%08X", task_id);

  ret = sceBgftServiceDownloadStartTask(task_id);
  if (ret)
    return PKG_ERROR("sceBgftDownloadStartTask", ret);

  return 0;
}
