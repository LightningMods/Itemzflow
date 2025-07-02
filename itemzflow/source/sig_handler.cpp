
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h> // malloc, qsort, free
#include <string.h>
#include <sys/signal.h>
#include <unistd.h> // close
#include <utils.h>
// #include <sys/mount.h>
#include "feature_classes.hpp"
#include "lang.h"
#include "net.h"
#include "zip/zip.h"
#include <dumper.h>
#include <fmt/format.h>
#include <fstream>
#include <libkernel.h>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utils.h>

#define SAMPLER_MSG "Option: Sandbox Copy\n\n"\
"What is Sandbox Copy?\n\n"\
"Sandbox Copy is a feature that copies games directly from the sandbox folder instead of "\
"copying from the PFS (PlayStation File System) Image. This alternative method can help "\
"dump games that may have missing or incomplete files.\n\n"\
"Why is it important?\n\n"\
"Some games may not have all content installed (such as additional language packs, updates, "\
"or DLC), which can cause issues when copying from the PFS Image. Copying from the Sandbox bypasses these potential "\
"problems by working directly with the files that are actually present in the sandbox.\n\n"\
"Do I need Sandbox Copy enabled?\n\n"\
"For most complete game installations with all content and updates, Sandbox Copy isn't necessary. "\
"However, it's particularly useful for games that are missing additional language files, "\
"incomplete installations, or when you want to dump only the base game files that are "\
"currently available in the sandbox.\n\n"\
"When unsure, enabling Sandbox Copy is a safer choice to avoid parsing errors with incomplete game data."

extern bool is_remote_va_launched;
static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::string readZip(const std::string &filename) {
  std::ifstream file(filename);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string generate_random_log_filename(bool is_crash = false) {
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);

  // Generate the date folder in the format 00-00-0000
  char dateFolder[11];
  std::strftime(dateFolder, sizeof(dateFolder), "%m-%d-%Y", &tm);

  // Generate the log filename with timestamp and random number
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm);
  std::stringstream ss;
  std::string str = is_crash ? "_Crash" : "_User_Upload";
  ss << dateFolder << "/Itemzflow_" << buffer << str << "_" << rand() % 10000
     << ".zip";

  return ss.str();
}

std::string generate_timestamp() {
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm);
  return std::string(buffer);
}

char *XORencryptDecrypt(const char *toEncrypt, size_t len) {
  const char *key = XOR_KEY;
  size_t keyLen = strlen(key);

  char *output = (char *)malloc(len + 1);
  for (size_t i = 0; i < len; i++) {
    output[i] = toEncrypt[i] ^ key[i % keyLen];
  }

  output[len] = '\0';
  return output;
}

void upload_crash_log(bool is_crash) {
  CURL *curl = nullptr;
  CURLcode res;
  std::string tk, tmp, response;
  loadmsg("Uploading Logs ....");
  const char *zip_path = APP_PATH("/Itemz_logs.zip");
  if (!if_exists(ITEMZ_LOG)) {
    log_debug("Log file does not exist");
    msgok(MSG_DIALOG::NORMAL, "Log does not exist!");
    return;
  }
  if (!create_log_zip(zip_path)) {
    log_debug("Log file failed to be created");
    msgok(MSG_DIALOG::NORMAL, "Failed to create log zip!");
    return;
  }
  std::string logContent = readZip(zip_path);
  std::string encodedContent = Base64::Encode(logContent);
  std::string path = generate_random_log_filename(is_crash);
  std::string commitMessage = is_crash
                                  ? "Itemzflow (" + completeVersion + ") " +
                                        "has crashed on " + generate_timestamp()
                                  : "User uploaded (" + completeVersion + ") " +
                                        "logs on" + generate_timestamp();
  std::string apiUrl =
      "https://api.github.com/repos/ps4-logger/itemzflow-logs/contents/" + path;

  curl = curl_easy_init();
  if (curl) {
    // tmp);
    tk = XORencryptDecrypt(
        "\x04\x0e\x40\x58\x1f\x2f\x07\x33\x04\x06\x6e\x66\x76\x04\x03\x3f\x63\x1a\x11\x7b\x51\x47\x47\x0d\x2f\x38\x02\x19\x23\x58\x0b\x47\x13\x30\x32\x30\x34\x3e\x06\x16\x7f\x60\x02\x7e\x2f\x52\x44\x42\x25\x01\x10\x00\x08\x04\x00\x39\x09\x2d\x4e\x0e\x20\x22\x23\x5d\x27\x21\x7c\x5a\x2d\x20\x33\x3c\x30\x55\x18\x3c\x0a\x5a\x25\x29\x03\x60\x36\x23\x7f\x14\x39\x57\x0f\x12\x60\x54\x39",
        93);
    std::string json = "{\"message\": \"" + commitMessage +
                       "\", \"content\": \"" + encodedContent + "\"}";
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
    log_info("url %s", apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    Base64::Decode("QXV0aG9yaXphdGlvbjogdG9rZW4=", tmp);
    tmp = tmp + " " + tk;
   // log_info("tmp: %s", tmp.c_str());
    headers = curl_slist_append(headers, tmp.c_str());

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      msgok(MSG_DIALOG::NORMAL,
            fmt::format("Failed to Upload logs\n\n{}",
                        getLangSTR(LANG_STR::PATCH_CON_FAILED)));
      log_debug("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    } else {
      msgok(MSG_DIALOG::NORMAL, "Logs uploaded successfully");
      log_debug("Log uploaded to the Server");
    }

    // Cleanup
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
  }

  curl_global_cleanup();
}

extern "C" int pthread_getthreadid_np();
#define OBRIS_BACKTRACE 1
#define SYS_mount 21
using namespace multi_select_options;

extern "C" int sys_mount(const char *type, const char *dir, int flags,
                         void *data) {
  return syscall(SYS_mount, type, dir, flags, data);
}

extern ItemzSettings set, *get;

extern char *gm_p_text[20];
extern "C" int backtrace(void **buffer, int size);
Dump_Multi_Sels dump = SEL_DUMP_ALL;
// IPC IS USING C
extern std::string ipc_msg;

int wait_for_game(char *title_id) {
  int res = 0;

  DIR *dir;
  struct dirent *dp;

  dir = opendir("/mnt/sandbox/pfsmnt");
  if (!dir)
    return 0;

  while ((dp = readdir(dir)) != NULL) {

    log_info("%s", dp->d_name);

    if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
      // do nothing (straight logic)
    } else {
      log_info("%s", dp->d_name);

      if (strstr(dp->d_name, "-app0") != NULL) {
        res = 1;
        break;
      }
    }
  }
  closedir(dir);
  log_info("wait_for_game finish");

  return res;
}
/*

typedef struct {
    const char* dump_path;
    const char* title_id;
    Dump_Options opt;
    const char* title;
} Dumper_Options;

*/

bool Dump_w_opts(std::string tid, std::string d_p, std::string gtitle,
                 Dump_Multi_Sels dump) {
  int vaild_dlcs = 0, avail_dlcs = 0;

  Dumper_Options options;
  options.title_id = tid.c_str();
  options.dump_path = d_p.c_str();
  options.title = gtitle.c_str();
  options.opt = (Dump_Options)dump;
  options.sb_copy = false;


  if(Confirmation_Msg(SAMPLER_MSG) == YES){
       options.sb_copy = true;
  }

  progstart((char *)getLangSTR(LANG_STR::STARTING_DUMPER).c_str());

  std::string tmp =
      fmt::format("/mnt/sandbox/pfsmnt/{}-app0-nest/pfs_image.dat", tid);
  if (!if_exists(tmp.c_str())) {
    log_error("Dump_w_opts: %s doesnt exist", tmp.c_str());
    msgok(MSG_DIALOG::WARNING, "Game  was not launched successfully, do you own this game?\nNote: games with a pad lock icon are NOT able to be dumped (I.e games without a license)");
    return false;
  }

  if (dump == SEL_DUMP_ALL) {
    if (count_dlc(tid, vaild_dlcs, avail_dlcs, false)) {
      log_info("Found DLC, trying to dump");
      options.opt = ADDITIONAL_CONTENT_DATA;
      if (!Dumper(options)) {
        log_error("Dumping DLC failed");
      }
    }

    options.opt = BASE_GAME;
    if (Dumper(options)) {
      options.opt = GAME_PATCH;
      if (!does_patch_exist(title_id.get(), tmp)) {
        return true;
      }
      print_memory();
      return Dumper(options);
    } else {
      log_debug("Dumping Base game failed, trying to dump as Remaster...");
      options.opt = REMASTER;
      if (Dumper(options)) {
        options.opt = GAME_PATCH;
        if (!does_patch_exist(title_id.get(), tmp)) {
          return true;
        }
        print_memory();
        return Dumper(options);
      }
    }

  } else if (dump == SEL_BASE_GAME) {
    options.opt = BASE_GAME;
    if (Dumper(options)) {
      tmp = fmt::format("{}/{}.complete", d_p, tid);
      touch_file(tmp.c_str());
      tmp = fmt::format("{}/{}.dumping", d_p, tid);
      unlink(tmp.c_str());
      return true;
    }
  } else if (dump == SEL_GAME_PATCH) {
    options.opt = GAME_PATCH;
    return Dumper(options);
  } else if (dump == SEL_REMASTER) {
    options.opt = REMASTER;
    return Dumper(options);
  } else if (dump == SEL_DLC) {
    options.opt = ADDITIONAL_CONTENT_DATA;
    return Dumper(options);
  }

  return false;
}

int (*rejail_multi)(void) = NULL;

void Exit_Success(const char *text) {
  log_info("%s", text);

  if (rejail_multi) {
    log_debug("Rejailing App >>>");
    rejail_multi();
    log_debug("App ReJailed");
  }

  if (reboot_app)
    sceSystemServiceLoadExec("/app0/ItemzCore.self", 0);

  if(get->setting_bools[STOP_DAEMON_ON_CLOSE]){
    ItemzLocalKillApp(get->ItemzDaemon_AppId);
  }

  log_debug("Calling SystemService Exit");
  sceSystemServiceLoadExec("exit", 0);
}

void Exit_App_With_Message(int sig_numb) {
  if (sig_numb != SIGQUIT && sig_numb != SIGKILL &&
      sig_numb != 17 /*SIGSTOP*/) {
    if (Confirmation_Msg(
            fmt::format("############# This software has encountered a fatal "
                        "error (Signal: {}). ##########\n\n Would you like to "
                        "upload the crash log and exit?\n\n",
                        sig_numb)) == YES) {
      log_info("The end user has chosen to upload the crash log ");
      loadmsg("Uploading Crash Logs...");
      upload_crash_log(true);
      sceMsgDialogTerminate();
    } else {
      log_info("The end user has chosen violence, why???");
      // not everyone has internet unlike the store
      // upload_crash_log();
    }
  }

  Exit_Success("Exit_App_With_Message ->");
}

#if OBRIS_BACKTRACE == 1

void OrbisKernelBacktrace(const char *reason) {
  char addr2line[MAX_STACK_FRAMES * 20];
  callframe_t frames[MAX_STACK_FRAMES];
  OrbisKernelVirtualQueryInfo info;
  char buf[MAX_MESSAGE_SIZE + 3];
  unsigned int nb_frames = 0;
  char temp[80];

  memset(addr2line, 0, sizeof addr2line);
  memset(frames, 0, sizeof frames);
  memset(buf, 0, sizeof buf);

  snprintf(buf, sizeof buf, "<118>[Crashlog]: %s\n", reason);

  strncat(buf, "<118>[Crashlog]: Backtrace:\n", MAX_MESSAGE_SIZE);
  sceKernelBacktraceSelf(frames, sizeof frames, &nb_frames, 0);
  for (unsigned int i = 0; i < nb_frames; i++) {
    memset(&info, 0, sizeof info);
    sceKernelVirtualQuery(frames[i].pc, 0, &info, sizeof info);

    snprintf(temp, sizeof temp, "<118>[Crashlog]:   #%02d %32s: 0x%lx\n", i + 1,
             info.name, frames[i].pc - info.unk01 - 1);
    strncat(buf, temp, MAX_MESSAGE_SIZE);

    snprintf(temp, sizeof temp, "0x%lx ", frames[i].pc - info.unk01 - 1);
    strncat(addr2line, temp, sizeof addr2line - 1);
  }

  strncat(buf, "<118>[Crashlog]: addr2line: ", MAX_MESSAGE_SIZE);
  strncat(buf, addr2line, MAX_MESSAGE_SIZE);
  strncat(buf, "\n", MAX_MESSAGE_SIZE);

  buf[MAX_MESSAGE_SIZE + 1] = '\n';
  buf[MAX_MESSAGE_SIZE + 2] = '\0';

  log_error("%s", buf);
}
#endif
bool is_dumper_running = false;

void SIG_Handler(int sig_numb) {
  std::string profpath;
  IPC_Client &ipc = IPC_Client::getInstance();
  // rejail_multi =
  //    (int (*)(void))prx_func_loader("/app0/Media/jb.prx", "rejail_multi");
  int fw = (ps4_fw_version() >> 16);
  // void* bt_array[255];
  //
  switch (sig_numb) {
  case SIGABRT:
    log_debug("ABORT called.");
    goto SIGSEGV_handler;
    break;
  case SIGTERM:
    log_debug("TERM called");
    break;
  case SIGINT:
    log_debug("SIGINT called");
    break;
  case SIGQUIT:
    unmount_atexit();
    Exit_Success("SIGQUIT Called");
    break;
  case SIGKILL:
    Exit_Success("SIGKILL Called");
    break;
  case SIGSTOP:
    log_debug("App has been suspeneded");
    goto App_suspended;
    break;
  case SIGCONT:
    log_debug("App has resumed");
    goto App_Resumed;
    break;
  case SIGUSR2:
    log_debug("SIGUSER2 called, Prob from libFMT");
    goto App_Resumed;
    break;
  case SIGSEGV: {
  SIGSEGV_handler:
    log_debug("App has crashed");
    if (dump != SEL_RESET) {
      msgok(MSG_DIALOG::WARNING, getLangSTR(LANG_STR::FATAL_DUMP_ERROR));
      sceSystemServiceLoadExec("/app0/ItemzCore.self", 0);
    }
    unlink("/user/app/if_update.pkg");
    goto backtrace_and_exit;
  }
  default:

    // Proc info and backtrace
    goto backtrace_and_exit;

    break;
  }

backtrace_and_exit:
  unmount_atexit();
  log_debug("backtrace_and_exit called");
  ipc.IPCSendCommand(IPC_Commands::DISABLE_HOME_REDIRECT, ipc_msg);

  if (get->setting_bools[INTERNAL_UPDATE])
    mkdir("/user/recovery.flag", 0777);

  profpath = fmt::format("/mnt/proc/{0:d}/", getpid());
  if (getuid() == 0 && !if_exists(profpath.c_str())) {
    int result = mkdir("/mnt/proc", 0777);
    if (result < 0 && errno != 17)
      log_debug("Failed to create /mnt/proc: %s", strerror(errno));

    result = sys_mount("procfs", "/mnt/proc", 0, NULL);
    if (result < 0)
      log_debug("Failed to mount procfs: %s", strerror(errno));
  }
  //
  log_debug("############# Program has gotten a FATAL Signal: %i ##########",
            sig_numb);
  log_debug("# Thread ID: %i", pthread_getthreadid_np());
  log_debug("# PID: %i", getpid());

  if (getuid() == 0)
    log_debug("# mounted ProcFS to %s", profpath.c_str());

  log_debug("# UID: %i", getuid());

  char buff[255];

  if (getuid() == 0) {
    FILE *fp = fopen("/mnt/proc/curproc/status", "r");
    if (fp) {
      fscanf(fp, "%s", &buff[0]);
      log_debug("# Reading curproc...");
      log_debug("# Proc Name: %s", &buff[0]);
      fclose(fp);
    }
  }
  log_debug("###################  Backtrace  ########################");

#if OBRIS_BACKTRACE == 1
#if __cpplusplus
  extern "C" {
#endif
  OrbisKernelBacktrace("Fatal Signal");
#if __cpplusplus
  }
#endif
#endif
  void *bt_array[255];

  if (fw <= 0x505)
    backtrace(bt_array, 255);

  Exit_App_With_Message(sig_numb);

App_suspended:
  log_debug("App Suspended Checking for opened games...");
App_Resumed:
  log_debug("App Resumed Checking for opened games...");

  // lock thread mutex so it doesnt constantly continue running
  // while we are dumping or checking if a game is open
  //  only unlcok at the end of the func
  if (!is_remote_va_launched && app_status.load() != APP_NA_STATUS) {
    int BigAppid = sceSystemServiceGetAppIdOfBigApp();
    if ((BigAppid & ~0xFFFFFF) == 0x60000000) {
      log_info("[SIGNAL] FOUND Open Game: %x : Bool: %d", BigAppid,
               (BigAppid & ~0xFFFFFF) == 0x60000000);
      if (sceLncUtilGetAppId(title_id.get().c_str()) ==
          sceSystemServiceGetAppIdOfBigApp())
        app_status = RESUMABLE;
      else
        app_status = OTHER_APP_OPEN;
    } else
      log_info("[SIGNAL] No Open Games detected");
  }

  //  char* title_id.get() = "CUSA02290";
  if (dump == SEL_RESET || is_dumper_running) {
    log_info("[SIGNAL] Dumper not active OR is already active");
    return;
  }
  is_dumper_running = true;
  std::string tmp;
  std::string tmp2;

  progstart((char *)getLangSTR(LANG_STR::STARTING_DUMPER).c_str());

  fmt::print("title_id.get(): {},  DUMPER_PATH: {}", title_id.get(),
             get->setting_strings[DUMPER_PATH]);
  tmp = fmt::format("{}/{}", get->setting_strings[DUMPER_PATH], title_id.get());

  if ((dump == SEL_BASE_GAME || dump == SEL_DUMP_ALL) &&
      if_exists(tmp.c_str())) {
    ProgSetMessagewText(5, getLangSTR(LANG_STR::DELETING_GAME_FOLDER).c_str());
    rmtree(tmp.c_str());
    tmp = fmt::format("{}/{}.complete", get->setting_strings[DUMPER_PATH],
                      title_id.get());
    unlink(tmp.c_str());
  }

  tmp = fmt::format("{}/{}-patch/", get->setting_strings[DUMPER_PATH],
                    title_id.get());
  if ((dump == SEL_GAME_PATCH || dump == SEL_DUMP_ALL) &&
      if_exists(tmp.c_str())) {
    ProgSetMessagewText(5, getLangSTR(LANG_STR::DELETING_GAME_FOLDER).c_str());
    rmtree(tmp.c_str());
  }

  Timer timer;
  // dump_game has all the dialoge for copied proc
  if (Dump_w_opts(title_id.get(), get->setting_strings[DUMPER_PATH],
                  g_idx > all_apps.size() ? "WTF" : all_apps[g_idx].info.name,
                  dump)) {
    msgok(MSG_DIALOG::NORMAL,
          fmt::format("{0:} {1:} {2:}\nTime elapsed: {3:2.0f}.{4:2.0f} minutes",
                      getLangSTR(LANG_STR::DUMP_OF), title_id.get(),
                      getLangSTR(LANG_STR::COMPLETE_WO_ERRORS),
                      timer.GetTotalMinutes(), timer.GetFractionalMinutes()));
  } else {
    msgok(MSG_DIALOG::WARNING,
          fmt::format("{0:} {1:} {2:}", getLangSTR(LANG_STR::DUMP_OF),
                      title_id.get(), getLangSTR(LANG_STR::DUMP_FAILED)));
  }

  print_memory();

  log_info("finished");
  is_dumper_running = false;
  dump = SEL_RESET;
}
