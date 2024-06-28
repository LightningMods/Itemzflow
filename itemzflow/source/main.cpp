#include "defines.h"
#include <future>
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
#include "feature_classes.hpp"
#include "log.h"

#include "installpkg.h"

double dt, u_t = 0;

int libcmi = -1;
OrbisGlConfig* GlConf;
OrbisPadConfig *confPad;
bool flag=true;
extern ThreadSafeVector<item_t> all_apps;
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
std::atomic_int mins_played = MIN_STATUS_RESET;
std::atomic_long started_epoch = MIN_STATUS_RESET;
std::atomic_int AppVis = -1;

typedef struct UtiltiyPadConfig
{
	ScePadData padDataCurrent;
	ScePadData padDataLast;

}UtiltiyPadConfig;
#include <stdatomic.h>
atomic_bool is_inilized = false;

void* ItemzCore_PlayTime_thread(void* args) {
    log_info("Starting playtime main loop");
   // std::string checked_tid;
    while (true) 
    {
        int secs = 0;
        if(is_idle.load()) {
            continue;
        }

        IPC_Client& ipc = IPC_Client::getInstance();

        if(title_id.get().empty()){
           sleep(1);
           continue;
        }

       std::string ipc_msg = title_id.get();
       if (is_connected_app && ipc.IPCSendCommand(IPC_Commands::GAME_GET_MINS, ipc_msg, true) == IPC_Ret::NO_ERROR) {
            int result;
            sscanf(ipc_msg.c_str(), "%d", &result);
            mins_played = result;
           // checked_tid = title_id.get();
        } else {
            mins_played = MIN_STATUS_NA;
        }

        do{
            sleep(1);
            secs++;
            if(secs > 50){
                log_info("50 sec check started");
                break;
            }
        }while (mins_played != MIN_STATUS_RESET);
    }

    return nullptr;
}
void* ItemzCore_Util_thread(void* args) {
    int ret = -1;
    u32 userId = -1;
    bool usb_found = false;
    char name[100];

    UtiltiyPadConfig pad;

    log_info("Starting thread main loop");
    if(sceUserServiceGetForegroundUser(&userId) == ITEMZCORE_SUCCESS){
       memset(name, 0, sizeof(name));
	   sceUserServiceGetUserName(userId, name, sizeof(name));
       ani_notify(NOTIFI::PROFILE, fmt::format("{0:}: {1:}", getLangSTR(LANG_STR::SIGN_IN_USER_NOTIF), name), "");
    }

    while(!is_inilized){
        sceKernelUsleep(500);
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
            ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::CON_DIS), getLangSTR(LANG_STR::CON_DIS_2));
            is_idle = true;
            GLES2_refresh_sysinfo();
            log_error("scePadReadState: %x, userid: %x, handle: %x", ret, userId, scePadGetHandle(userId,0,0));
        }
        scan_for_disc();
        //check if theres a usb
        if(!usb_found && usbpath() != -1){
            usb_found = true;
            //USB Notify
            ani_notify(NOTIFI::INFO, getLangSTR(LANG_STR::USB_NOTIFY), getLangSTR(LANG_STR::USB_NOTIFY1));
            log_info("[UTIL] USB Inserted");
        }
        else if (usb_found && usbpath() == -1){
           usb_found = false;
           ani_notify(NOTIFI::INFO, getLangSTR(LANG_STR::USB_DISCONNECTED), getLangSTR(LANG_STR::USB_DISCONNECTED1));
           log_info("[UTIL] USB removed");
        }

        // Replace the use of ipc_msg with vecto
        if(title_id.get().empty()){
           sleep(1);
           continue;
        }


        if(app_status.load() == NO_APP_OPENED){
           sleep(1);
           continue;
        }

        int BigAppid = sceSystemServiceGetAppIdOfBigApp();
        // IS Big App Running?
        if ((BigAppid & ~0xFFFFFF) == 0x60000000){
            memset(name, 0, 100);
            // Get TID of Big App
            if(sceLncUtilGetAppTitleId(BigAppid, &name[0]) == 0){
                // Check if its the same as the current app
                if(title_id.get().compare(name) == 0){
                   // log_info("[TEST] Big App is running");
                }
                else{
                  //  log_info("[TEST] Big App is not running");
                    continue;
                }
            }
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

void (*crash)() = NULL;

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
            if(v_curr != FILE_BROWSER_LEFT && v_curr != FILE_BROWSER_RIGHT)
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
            log_info("R2 pressed");
            // crash();
            /// throw std::runtime_error("Crash");
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



float toMilliSeconds(uint64_t t)
{
	return (float)t / 1000.0f ;
}

/// for timing, fps
#define WEN  (2048)
unsigned int frame = 1;
float time_ms = 0.f;
int main(int argc, char* argv[])
{

    int ret = -1;
    bool speed1 = true,speed2 = false,speed3 = false;
    vec2 pen = (vec2){ 26, 990 };
    vec4 col = (vec4){ .8164, .8164, .8125, 1. };
		int      fpsCount = 0;
		uint64_t fpsTime1 = 0;
		uint64_t fpsTime2 = 0;
    VertexBuffer fps_vbo;
    std::chrono::time_point<std::chrono::system_clock> now, prev;
    std::chrono::steady_clock::time_point lastUpdate;
   // vec4 c = col * .75f;

    pthread_t utility_thread = NULL, playtime_thr = NULL;
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

    if (Start_IF_internal_Services(argv[0]) == INIT_FAILED) goto error;

    log_info("Starting GL ...");
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

   log_info("Starting the ItemzCore Playtime Thread ...");
   pthread_create(&playtime_thr, NULL, ItemzCore_PlayTime_thread, NULL);
   log_info("ItemzCore Playtime Thread Started");


    /// reset timers
    now = std::chrono::system_clock::now();

    time_ms = get_time_ms(now);
    lastUpdate = std::chrono::steady_clock::now();
    //close any still running dialogs
    sceMsgDialogTerminate();

    // Initialize FPS counter
    fpsTime1 = sceKernelGetProcessTime();

    prev = std::chrono::system_clock::now();

    /// enter main render loop
    while (flag)
    {
        //skip the first frame 
        //so theres no race between 
        //the countroller update and the GLES UI

         if (frame != 1){
            updateController();
            if(!is_inilized)
                is_inilized = true;
         }

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

            fpsCount++;

			if (fpsCount == 120 ) {
                fpsTime2 = sceKernelGetProcessTime();
                float time =  get_time_ms(prev)  * 1000.0f;
                float avgFrameTime = time / 60.0f;
                fps_vbo = VertexBuffer( "vertex:3f,tex_coord:2f,color:4f" );
				float s_fps = 60.0f / toMilliSeconds(fpsTime2 - fpsTime1) * 1000.0f;
                std::string str = fmt::format("DEBUG FPS: {0:.2f}", s_fps);
                vec2 bk = pen;
                texture_font_load_glyphs(sub_font, str.c_str());
                fps_vbo.add_text(sub_font, str, col, bk);
                str = fmt::format("DEBUG ms: {0:.2f}", avgFrameTime);
               // log_info("%.f ", avgFrameTime);
                texture_font_load_glyphs(sub_font, str.c_str());
                bk.x = pen.x;
                bk.y -= 30;
                fps_vbo.add_text(sub_font, str, col, bk);

				fpsTime1 = fpsTime2;
				fpsCount = 0;
			}
            //fps_vbo.render_vbo(NULL);


            /// get timing, fps
            if (!(frame % WEN))
            {
                float ti = get_time_ms(now);
                double framerate = 1000.f / ti;
                log_info("--------------------------------------------------");
                log_info("[Render] AVG_time: %.3lf ms AVG_FPS: %.3lf", ti, framerate);
                print_memory();
                //reset Frame so we dont overflow later in release
                frame = 1;
                // increase MIN status reset every so often (most of the time 15 secs)
                mins_played = MIN_STATUS_RESET;
                log_info("---------------------------------------------------");
                //time_ms = now;
            }
            
            if (1) // update for the GLES uniform time
            {
                auto now = std::chrono::steady_clock::now();
                dt = std::chrono::duration_cast<std::chrono::microseconds>(now - lastUpdate).count() / 1000000.0f;
                lastUpdate = now; 
                u_t += dt; 
            }

            orbisGlSwapBuffers();  /// flip frame
            frame++;

            MP3_Player(get->setting_strings[MP3_PATH]);
            dump_frame();
        
    }

error:
    sceKernelIccSetBuzzer(2);
    msgok(MSG_DIALOG::FATAL, fmt::format("App has Died: ({0:#x})", ret));

    // destructors
    ORBIS_RenderFillRects_fini();
    on_GLES2_Final();
    pixelshader_fini();
 
    finishOrbisLinkApp();
    raise(SIGQUIT);
    return -1;
}