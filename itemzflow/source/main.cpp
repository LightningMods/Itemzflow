#include "defines.h"
#include <utils.h>
#include <sys/signal.h>
#include <orbisGl.h>
#include <orbisPad.h>
#include <pthread.h>
#include <signal.h>
#include "net.h"
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <patcher.h>
#include <sys/mount.h>
#include "log.h"

double dt, u_t = 0;

int libcmi = -1;
OrbisGlConfig* GlConf;
OrbisPadConfig *confPad;
bool flag=true;
extern std::vector<item_t> all_apps;
std::atomic_bool is_idle = false;


int    selected_icon;
struct timeval  t1, t2;

#define DELTA  (123) // dpad delta movement

static int (*jailbreak_me)(void) = NULL;

extern std::atomic_bool found_usb;
extern std::atomic_int inserted_disc;
std::atomic_bool reset_idle_timer = false;
extern bool is_connected_app;

extern "C" void standalone_main(void);

extern void* __stack_chk_guard;
std::atomic_bool is_disc_inserted = false;
pthread_mutex_t notifcation_lock, disc_lock, usb_lock;
std::atomic_int mins_played = MIN_STATUS_RESET;
std::atomic_long started_epoch = MIN_STATUS_RESET;
std::atomic_int AppVis = -1;

typedef struct UtiltiyPadConfig
{
	ScePadData padDataCurrent;
	ScePadData padDataLast;

}UtiltiyPadConfig;

void* ItemzCore_Util_thread(void* args) {
    int ret = -1;
    char stack_tid[16];
    u32 userId = -1;
    bool usb_found = false;
    // IPC IS USING C
    char ipc_msg[100], name[100];

    UtiltiyPadConfig pad;

    log_info("Starting thread main loop");
    if(sceUserServiceGetForegroundUser(&userId) == ITEMZCORE_SUCCESS){
       memset(name, 0, sizeof(name));
	   sceUserServiceGetUserName(userId, name, sizeof(name));
       ani_notify(NOTIFI_PROFILE, fmt::format("{0:}: {1:}", getLangSTR(SIGN_IN_USER_NOTIF), name), "");
    }

    time_t idle_clock = time(NULL);

    while (true) 
    {
        if(reset_idle_timer.load()) {
            idle_clock = time(NULL);
            reset_idle_timer = false;
            is_idle = false;
            GLES2_refresh_sysinfo();
        }

        time_t fifteenMinutesAgo = idle_clock + 15 * 60;
        if (!is_idle.load() && time(NULL) > fifteenMinutesAgo) {
            log_info("15 mins have passed and its now idling %llx %llx",idle_clock, fifteenMinutesAgo);
            is_idle = true;
            GLES2_refresh_sysinfo();
        }

        if(is_idle.load()) {
            continue;
        }
        // re-get the userid incase it changed
        sceUserServiceGetForegroundUser(&userId);

        ret = scePadReadState(scePadGetHandle(userId,0,0), &pad.padDataCurrent);
        if (ret < 0 || !pad.padDataCurrent.connected) {
            // Function is thread-safe
            // Notfiy func has mutexs in place for threads
            ani_notify(NOTIFI_GAME, getLangSTR(CON_DIS), getLangSTR(CON_DIS_2));
            is_idle = true;
            GLES2_refresh_sysinfo();
            log_error("scePadReadState: %x, userid: %x, handle: %x", ret, userId, scePadGetHandle(userId,0,0));
        }

        //check if theres a disc
        pthread_mutex_lock(&disc_lock);
        memset(&stack_tid[0], 0, 16);
        if ((ret = sceAppInstUtilAppGetInsertedDiscTitleId(&stack_tid[0])) >= 0 && !is_disc_inserted.load()) {
            if (!all_apps.empty() && all_apps.size() > 1) {
                for (int i = 0; i < all_apps[0].token_c + 1; i++) {
                    // skip if its an FPKG
                    if(all_apps[i].is_fpkg)
                       continue;

                   // log_info("Disc inserted: %s : %i %s", stack_tid, i , all_apps[i].id.c_str());
                    if (all_apps[i].id.find(&stack_tid[0]) != std::string::npos) {
                        //Store the texture ID of the inserted disc
                        inserted_disc = i;
                        //inserted_disc: 1 CUSA00900 1  18
                        log_info("[UTIL] inserted_disc: %i %s %i %s %i", inserted_disc.load(), stack_tid, i, all_apps[i].id.c_str(), all_apps.size());
                        is_disc_inserted = true;
                        break;
                    }
                }
            }
        }
        else if (ret == SCE_APP_INSTALLER_ERROR_DISC_NOT_INSERTED) {
            // log_info("[UTIL] disc not inserted %x", ret);
            inserted_disc = -999;
            is_disc_inserted = false;
        }
       // else 
          //log_info("[UTIL] disc  %x %i", ret, inserted_disc.load());

        if(mins_played.load() == MIN_STATUS_RESET && //check if a game is open before asking the daemon
        !title_id.empty() && app_status != APP_NA_STATUS) { // check if an app is already selected too, TODO: Atmoic int for app_status
            
            if(is_vapp(title_id)){
                mins_played = MIN_STATUS_VAPP;
                continue;
            }
            memset(&ipc_msg[0], 0, sizeof ipc_msg);
            strcpy((char*)&ipc_msg[1], title_id.c_str());

            if(is_connected_app && IPCSendCommand(GAME_GET_MINS, (uint8_t*)&ipc_msg[0]) == NO_ERROR){
                mins_played =  atoi((const char*)&ipc_msg[0]); //convert to INT
            }
            else
                mins_played = MIN_STATUS_NA;
            
            memset(&ipc_msg[0], 0, sizeof ipc_msg);
            strcpy((char*)&ipc_msg[1], title_id.c_str());
            if(is_connected_app && IPCSendCommand(GAME_GET_START_TIME, (uint8_t*)ipc_msg) == NO_ERROR)
               started_epoch = atol((const char*)&ipc_msg[0]);
            else
               started_epoch = MIN_STATUS_NA;

        }

        pthread_mutex_unlock(&disc_lock);

        //check if theres a usb
        if(!usb_found && usbpath() != -1){
            usb_found = true;
            //USB Notify
            ani_notify(NOTIFI_INFO, getLangSTR(USB_NOTIFY), getLangSTR(USB_NOTIFY1));
            log_info("[UTIL] USB Inserted");
        }
        else if (usb_found && usbpath() == -1){
           usb_found = false;
           ani_notify(NOTIFI_INFO, getLangSTR(USB_DISCONNECTED), getLangSTR(USB_DISCONNECTED1));
           log_info("[UTIL] USB removed");
        }

       sleep(1);
    }
 
    log_error("Utility Thread failed to start");

    return NULL;
}
struct timeval last_tv;
int button_numb = -1;
int if_numb = -1;
bool check_if_held = false;

void updateController()
{
    unsigned int buttons=0;
    int ret = orbisPadUpdate();
    if(ret==0)
    {
        if(orbisPadGetButtonPressed(ORBISPAD_L2|ORBISPAD_R2) || orbisPadGetButtonHold(ORBISPAD_L2|ORBISPAD_R2))
        {
            log_info("Combo L2R2 pressed");
            buttons=orbisPadGetCurrentButtonsPressed();
            buttons&= ~(ORBISPAD_L2|ORBISPAD_R2);
            orbisPadSetCurrentButtonsPressed(buttons);
        }
        if(orbisPadGetButtonPressed(ORBISPAD_TOUCH_PAD))
        {
            reset_idle_timer = true;
        }
        if(orbisPadGetButtonPressed(ORBISPAD_L1|ORBISPAD_R1) )
        {
            log_info("Combo L1R1 pressed");
            buttons=orbisPadGetCurrentButtonsPressed();
            buttons&= ~(ORBISPAD_L1|ORBISPAD_R1);
            orbisPadSetCurrentButtonsPressed(buttons);
        }
        if(orbisPadGetButtonPressed(ORBISPAD_L1|ORBISPAD_R2) || orbisPadGetButtonHold(ORBISPAD_L1|ORBISPAD_R2))
        {
            log_info("Combo L1R2 pressed");
            buttons=orbisPadGetCurrentButtonsPressed();
            buttons&= ~(ORBISPAD_L1|ORBISPAD_R2);
            orbisPadSetCurrentButtonsPressed(buttons);
        }
        if(orbisPadGetButtonPressed(ORBISPAD_L2|ORBISPAD_R1) || orbisPadGetButtonHold(ORBISPAD_L2|ORBISPAD_R1) )
        {
            log_info("Combo L2R1 pressed");
            buttons=orbisPadGetCurrentButtonsPressed();
            buttons&= ~(ORBISPAD_L2|ORBISPAD_R1);
            orbisPadSetCurrentButtonsPressed(buttons);
        }

        if(orbisPadGetButtonPressed(ORBISPAD_UP) || confPad->padDataCurrent->ly < (127 - DELTA)){
            fw_action_to_cf(UP);
            if(!(confPad->padDataCurrent->ly < (127 - DELTA))){
               check_if_held = true;
               button_numb = ORBISPAD_UP;
               if_numb = UP;
            }
            reset_idle_timer = true;
            gettimeofday(&last_tv, NULL);
        }
        else
        if(orbisPadGetButtonPressed(ORBISPAD_DOWN)
        || confPad->padDataCurrent->ly > (127 + DELTA))
        {
            fw_action_to_cf(DOW);
            if(!(confPad->padDataCurrent->ly > (127 + DELTA))){
               check_if_held = true;
               button_numb = ORBISPAD_DOWN;
               if_numb = DOW;
            }
            reset_idle_timer = true;
            gettimeofday(&last_tv, NULL);
        }
        else
        if(orbisPadGetButtonPressed(ORBISPAD_RIGHT) 
        || confPad->padDataCurrent->lx > (127 + DELTA))
        {
            fw_action_to_cf(RIG);
            if(!(confPad->padDataCurrent->lx > (127 + DELTA))){
              check_if_held = true;
              button_numb = ORBISPAD_RIGHT;
              if_numb = RIG;
            }
            reset_idle_timer = true;
            gettimeofday(&last_tv, NULL);
        }
        else
        if(orbisPadGetButtonPressed(ORBISPAD_LEFT) 
        || confPad->padDataCurrent->lx < (127 - DELTA))
        {
            fw_action_to_cf(LEF);
            if(!(confPad->padDataCurrent->lx < (127 - DELTA))){
               check_if_held = true;
               button_numb = ORBISPAD_LEFT;
               if_numb = LEF;
            }
            reset_idle_timer = true;
            gettimeofday(&last_tv, NULL);
        }
        else
        if(orbisPadGetButtonPressed(ORBISPAD_TRIANGLE))
        {
            log_info( "Triangle pressed exit");
            fw_action_to_cf(TRI);
            reset_idle_timer = true;
        }
        else
        if(orbisPadGetButtonPressed(ORBISPAD_CIRCLE))
        {
            log_info( "Circle pressed");
            fw_action_to_cf(CIR);
            reset_idle_timer = true;

        }
        else
        if(orbisPadGetButtonPressed(ORBISPAD_CROSS))
        {
            log_info( "Cross pressed");
            fw_action_to_cf(CRO);
            reset_idle_timer = true;
        }
        if(orbisPadGetButtonPressed(ORBISPAD_SQUARE))
        {
            log_info( "Square pressed"); 
            fw_action_to_cf(SQU);
            reset_idle_timer = true;
        }
        if(orbisPadGetButtonPressed(ORBISPAD_L1))
        {
            log_info( "L1 pressed");
            if(v_curr != FILE_BROWSER)
               trigger_dump_frame();

            fw_action_to_cf(L1);
            reset_idle_timer = true;
        }
        if(orbisPadGetButtonPressed(ORBISPAD_L2))
        {
            log_info( "L2 pressed");
            fw_action_to_cf(L2);

            reset_idle_timer = true;
        }
        if(orbisPadGetButtonPressed(ORBISPAD_R1))
        {
            fw_action_to_cf(R1);
            reset_idle_timer = true;
        }
        if(orbisPadGetButtonPressed(ORBISPAD_R2))
        {
            reset_idle_timer = true;
        }
        if (orbisPadGetButtonPressed(ORBISPAD_OPTIONS))
        {
            fw_action_to_cf(OPT);
            reset_idle_timer = true;
            //patch_lnc_debug();

        }
    }
    
}

void finishApp()
{
    orbisPadFinish();
    orbisNfsFinish();
}

static bool initAppGl()
{
    int ret = orbisGlInit(ATTR_ORBISGL_WIDTH, ATTR_ORBISGL_HEIGHT);
    if (ret > 0)
    {
        glViewport(0, 0, ATTR_ORBISGL_WIDTH, ATTR_ORBISGL_HEIGHT);
        ret = glGetError();
        if (ret)
        {
            log_info("[%s] glViewport failed: 0x%08X", __FUNCTION__, ret);
            return false;
        }
        
        //      glClearColor(0.f, 0.f, 1.f, 1.f);          // blue RGBA
        glClearColor(0.1211, 0.1211, 0.1211, 1.); // background color

        ret = glGetError();
        if (ret)
        {
            log_info("[%s] glClearColor failed: 0x%08X", __FUNCTION__, ret);
            return false;
        }
        return true;
    }
    return false;
}

bool initApp()
{
#if defined (USE_NFS)
    orbisNfsInit(NFSEXPORT);
    orbisFileInit();
    int ret = initOrbisLinkApp();
#endif

    sceSystemServiceHideSplashScreen();

    confPad = orbisPadGetConf();

    if (!initAppGl()) return false;

    return true;
}


/// for timing, fps
#define WEN  (2048)
unsigned int frame = 1,
time_ms = 0;
int main(int argc, char* argv[])
{

    int ret = -1;
    bool speed1 = true,speed2 = false,speed3 = false;
    pthread_t utility_thread = NULL;
    /* load custom .prx, resolve and call a function */
    rejail_multi = (int (*)(void))prx_func_loader("/app0/Media/jb.prx", "rejail_multi");
    jailbreak_me = (int (*)(void))prx_func_loader("/app0/Media/jb.prx", "jailbreak_me");
    if (jailbreak_me)
    {
        log_info("jailbreak_me resolved from PRX");
        if ((ret = jailbreak_me() != 0)) goto error;
    }
    else
        goto error;

    if (Start_IF_internal_Services(atoi(argv[0])) == INIT_FAILED) goto error;

    if (pthread_mutex_init(&notifcation_lock, NULL) != 0 ||
        pthread_mutex_init(&disc_lock, NULL) != 0 ||
        pthread_mutex_init(&usb_lock, NULL) != 0) {
        log_error("mutex init has failed");
        goto error;
    }

    // init some libraries
    flag = initApp();

    // feedback
    log_info("Here we are!");

    /* init GLES2 stuff */
    // demo-font.c init
    es2init_text(ATTR_ORBISGL_WIDTH, ATTR_ORBISGL_HEIGHT);
    // ES_UI
    log_info("Starting ItemzCore ....");
    init_ItemzCore(ATTR_ORBISGL_WIDTH, ATTR_ORBISGL_HEIGHT);
    log_info("ItemzCore started.");

    log_info("Starting the ItemzCore Utility Thread ...");
    pthread_create(&utility_thread, NULL, ItemzCore_Util_thread, NULL);
    log_info("ItemzCore Utility Thread Started");


    /// reset timers
    time_ms = get_time_ms();
    gettimeofday(&t1, NULL);
    t2 = t1;

    //close any still running dialogs
    sceMsgDialogTerminate();
    gettimeofday(&last_tv, NULL);

    /// enter main render loop
    while (flag)
    {
        //skip the first frame 
        //so theres no race between 
        //the countroller update and the GLES UI
         if (frame != 1)
            updateController();

    if(check_if_held && !orbisPadGetButtonReleased(button_numb)) {
       struct timeval tv;
       gettimeofday(&tv, NULL);
       //log_info("Holding UP %i %i", tv.tv_usec, last_tv.tv_usec);
       int elapsed_time = (tv.tv_sec - last_tv.tv_sec) * 1000000 + (tv.tv_usec - last_tv.tv_usec);
       if(speed1 && elapsed_time > 500000) {
          fw_action_to_cf(if_numb);
          gettimeofday(&last_tv, NULL);
          speed1 = false;
          speed2 = true;
       }
       else if(speed2 && elapsed_time > 300000) {
           fw_action_to_cf(if_numb);
           gettimeofday(&last_tv, NULL);
           speed2 = false;
           speed3 = true;
       }
       else if(speed3 && elapsed_time > 50000) {
            fw_action_to_cf(if_numb);
            gettimeofday(&last_tv, NULL);
       }
    }
    else {
      check_if_held = false;
      speed1 = true;
      speed2 = false;
      speed3 = false;
    }

         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

         ret = glGetError();
         if (ret) {
             log_info("[ORBIS_GL] glClear failed: 0x%08X", ret);
                //goto err;
            }

            // Render ItemzCore
            Render_ItemzCore();

            /// get timing, fps
            if (!(frame % WEN))
            {
                //ItemzCore funcs
                double const_1000 = 1000.0f;
                uint64_t now = get_time_ms();
                uint64_t frame_time = now - time_ms;
                double time_to_draw = frame_time / const_1000;
                double framerate = const_1000 / time_to_draw;
                log_info("--------------------------------------------------");
                log_info("[Render] time: %lums FPS: %.3lf", frame_time, framerate);
                print_memory();
                //reset Frame so we dont overflow later in release
                frame = 1;
                // MIN_STATUS_RESET wait for 120 frames to reset the status
                mins_played = MIN_STATUS_RESET;
                log_info("---------------------------------------------------");
                time_ms = now;
            }
            frame++;
            
            if (1) // update for the GLES uniform time
            {
                gettimeofday(&t2, NULL);
                // calculate delta time
                dt = t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6;

                t1 = t2;
                // update total time
                u_t += dt;
            }
            orbisGlSwapBuffers();  /// flip frame


            MP3_Player(get->setting_strings[MP3_PATH]);
            dump_frame();
        
    }

error:
    sceKernelIccSetBuzzer(2);
    msgok(FATAL, fmt::format("App has Died: ({0:#x})", ret));

    // destructors
    ORBIS_RenderFillRects_fini();
    on_GLES2_Final();
    pixelshader_fini();
 
    finishOrbisLinkApp();
    raise(SIGQUIT);
    return -1;
}