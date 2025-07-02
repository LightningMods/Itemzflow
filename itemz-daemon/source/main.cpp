#include "defines.h"
#include "log.h"
#include "net.hpp"
#include "utils.hpp"
#include <errno.h>
#include <ftp.h>
#include <ps4sdk.h>
#include <pthread.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <user_mem.h>

bool IS_ERROR(uint32_t a1) { return a1 & 0x80000000; }

bool is_home_redirect_enabled = false;
bool critical_suspend = false;

// bool critical_suspend2 = false;
// int pid = 0;
void PS_Button_loop() {
  OrbisUserServiceInitializeParams params;
  memset(&params, 0, sizeof(params));
  params.priority = 256;
  int userId = -1;
  log_info(" sceUserServiceInitialize %x",
           sceUserServiceInitialize((int *)&params));
  log_info(" sceUserServiceGetForegroundUser %x",
           sceUserServiceGetForegroundUser(&userId));

  int ret = scePadInit();
  if (ret < 0)
    log_info(" %s scePadInit return error 0x%8x", __FUNCTION__, ret);

  log_info(" scePadSetProcessPrivilege %x", scePadSetProcessPrivilege(1));

  int pad = scePadOpen(userId, 0, 0, NULL);
  if (pad < 0)
    log_info(" %s scePadOpen return error 0x%8x", __FUNCTION__, pad);
  else
    log_info(" Opened Pad");

  ScePadData data;

  log_info(" l1 %x", sceLncUtilInitialize());

  LncAppParam param;
  param.sz = sizeof(LncAppParam);
  param.user_id = userId;
  param.app_opt = 0;
  param.crash_report = 0;
  param.check_flag = SkipSystemUpdateCheck;
  log_info(" Home menu Boot Option SkipSystemUpdateCheck Applied");
  log_info(" Main Loop Started");

  while (pad > 0) {
    if (isRestMode()) {
      log_info(" Rest Mode Detected shutting down ****");
      notify("Rest Mode Detected, Shutting Down Daemon...");
      raise(SIGQUIT);
    } else if (is_home_redirect_enabled || critical_suspend) {
      // get sample size
      scePadReadState(pad, &data);

      if (data.buttons & PS_BUTTON &&
          is_home_redirect_enabled) { // PS_BUTTON from RE

        log_info(" PS BUTTON Was Pressed & intercepted");
        log_info(" Redirecting Home Menu to ItemzCore");

        uint32_t sys_res = sceLncUtilLaunchApp(HOME_TITLE_ID, 0, &param);
        if (IS_ERROR(sys_res)) {
          if (sys_res == SCE_LNC_UTIL_ERROR_ALREADY_RUNNING ||
              sys_res == SCE_LNC_UTIL_ERROR_ALREADY_RUNNING_SUSPEND_NEEDED ||
              sys_res == SCE_LNC_UTIL_ERROR_ALREADY_RUNNING_KILL_NEEDED) {
            log_info(" ITEMzFlow Already running, resuming App....");
            log_info(" Redirect Successful");
          } else
            log_info(" launch ret 0x%x", sys_res);
        } else
          log_info(" Redirect Successful");
      }

      if (critical_suspend) {
        int BigAppid = sceSystemServiceGetAppIdOfBigApp();
        if ((BigAppid & ~0xFFFFFF) == 0x60000000) {
          log_info(" FOUND Open Game: %x", BigAppid);
          log_info(" Suspending Open Game");
          sceLncUtilLaunchApp(HOME_TITLE_ID, 0, &param);
          critical_suspend = false;
        }
      }
    }

    usleep(15000);
  }
}

void *Game_Stats_thread(void *args) {

  char tid[16];
  int retries = 0;
  int seconds_past = 0;

  log_info(" Game_Stats Thread Started");
  log_info(" sceLncUtilInitialize %x", sceLncUtilInitialize());

failed_timeout:
  if (!if_exists(STAT_DB)) {
    log_info(" Creating Stat DB");
    if (create_stat_db(STAT_DB))
      log_info(" Stat DB Created");
    else {
      log_info("[FATAL] Stat DB Creation Failed");
      goto err;
    }
  } else
    log_info(" Stat DB Exists");

  if (open_stat_db(STAT_DB))
    log_info(" Stat DB Opened");
  else {
    log_info("[FATAL]  Stat DB Open Failed");
    goto err;
  }

  while (1) {

    if (retries > 4) {
      retries = 0;
      goto failed_timeout;
    }

    // make sure app still exists
    // if(!if_exists("/user/app/ITEM00001/")){
    //  notify("ItemzFlow Not Installed!, Daemon Exiting...");
    // raise(SIGQUIT);
    //}

    check_ftp_addr_change();

    // if no big app is running then get time for mini app IF its not itemzflow
    int BigAppid = sceSystemServiceGetAppIdOfBigApp();
    int MiniAppID = sceSystemServiceGetAppIdOfMiniApp();

     if ((MiniAppID & ~0xFFFFFF) == 0x60000000) {
        memset(tid, 0, 16);
        if (sceLncUtilGetAppTitleId(MiniAppID, &tid[0]) == 0){
            //mini apps are low priority so dont even check if it was successful
            update_game_time_played(&tid[0]);
        }
    }
    // IS Big App Running?
    if ((BigAppid & ~0xFFFFFF) == 0x60000000) {
      memset(tid, 0, 16);
      // Get TID of Big App
      if (sceLncUtilGetAppTitleId(BigAppid, &tid[0]) != 0)
        log_info(" failed to get tid");
      else {
        if (!update_game_time_played(&tid[0]))
          log_info(" failed to update game time playe retry: %i", retries++);
        else {
          log_info(" Game Time Played Updated for 0x%08x", BigAppid);
        }
      }
    }

    sleep(59);          
    seconds_past += 59; // sleep for 60 Secs if everything successful
    if (seconds_past >= 60 * 5) {
      seconds_past = 0;
      print_memory();
    }
  }

err:
  notify("Failed, Game Stats will be Unavailable");
  pthread_exit(NULL);
  return NULL;
}
int main(int argc, char *argv[]) {
  ScePthread thread, stats_thr; //, fuse_thread;
  // internals: net, user_service, system_service
  loadModulesVanilla();

  if (!jailbreak("/app0/Media/jb.prx"))
    goto exit;

  if (!full_init())
    goto exit;

  log_info(" Registering Daemon...");

  sceSystemServiceRegisterDaemon();

  log_info("Starting SQL Services...");
  log_info("SQL library Version: %s", sqlite3_libversion());
  log_info("Started SQL Services ");

  log_info(" Starting IPC loop Thread...");
  scePthreadCreate(&thread, NULL, IPC_loop, NULL, "IPC_loop_thread");

  log_info(" Starting Game Stats Thread...");
  scePthreadCreate(&stats_thr, NULL, Game_Stats_thread, NULL, "Game_Stats_Thread");

  // log_info(" Starting Resource Monitor Thread...");
  // scePthreadCreate(&up_thread, NULL, resource_monitor_thread, NULL,
  // "Resource_Monitor_Thread");
 // if(!if_exists("/dev/mira")){

     log_info(" Starting FTP & Infinix...");
     if (!StartFTP())
         log_error(" Failed to Start FTP & Infinix");
     else
         log_info(" FTP Started Successfully");
 //  }
  //
  log_info(" Starting main PS Button loop...");
  PS_Button_loop();

  log_info(" WTF the Loop eneded???");

exit:
  log_info(" Exiting");
  if (!rejail())
    log_info(" rejail failed");

  sceKernelIccSetBuzzer(2);
  return sceSystemServiceLoadExec("exit", 0);
}