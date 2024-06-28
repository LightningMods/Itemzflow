#include "utils.h"
#include "defines.h"
#include <sys/stat.h>
#include <sys/errno.h>
#if defined(__ORBIS__)
#include <installpkg.h>
#endif

bool file_options = false;

//
extern vec2 resolution,
    p1_pos;


extern vec3 p_off;
fs_res fs_ret;


std::vector<std::string> global_pkg_folder_right; 
std::vector<std::string> global_pkg_folder_left; 

/* normalized rgba colors */
extern vec4 sele, // default for selection (blue)
    grey,         // rect fg
    white,
    col; // text fg

extern texture_font_t *main_font, // small
    *sub_font,                    // left panel
    *titl_font;

/* a double side (L/R) panel filemanager */
layout_t fm_lp, fm_rp;

std::vector<std::string> get_active_pkg_folder(){
   if(fm_lp.fs_is_active)
      return global_pkg_folder_left;
   else
      return global_pkg_folder_right;
} 

void push_back_to_active_pkg_folder(const std::string& pkg) {
   if(fm_lp.fs_is_active)
      global_pkg_folder_left.push_back(pkg);
   else
      global_pkg_folder_right.push_back(pkg);
}

void clear_all_active_pkg_folder(){
   global_pkg_folder_left.clear();
   global_pkg_folder_right.clear();
}

void clear_active_pkg_folder(){
   if(fm_lp.fs_is_active)
      global_pkg_folder_left.clear();
   else
      global_pkg_folder_right.clear();
}

bool StartFileBrowser(view_t vt, layout_t & l,
 int (*callback)(std::string fn, std::string fp), FS_FILTER filter)
{

    if (fs_ret.status == FS_NOT_STARTED)
    {
        fs_ret.last_v = vt;
        v_curr = FILE_BROWSER_LEFT;
        fs_ret.fs_func = callback;
        fs_ret.last_layout = &l;
        fs_ret.filter = filter;
        fs_ret.status = FS_STARTED;
    }
    else{
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FS_SEL_ERROR1), getLangSTR(LANG_STR::FS_SEL_ERROR2));
        return false;
    }

    return true;
}

VertexBuffer test_v; // pixelshader vbo
extern std::string path;
// build an item_data array from folder list
void index_items_from_dir_v2(std::string dirpath, ThreadSafeVector<item_t> &out_vec)
{
    // grab, count and sort entry list by name
    log_info("Loading items with Filter: %i", fs_ret.filter);
    std::vector<std::string> cvEntries;

    if (!getEntries(dirpath, cvEntries, fs_ret.filter, true))
        log_info("failed to get dir");
    else
        fmt::print("Found {} in: {}", cvEntries.size(), dirpath);

    if(fs_ret.filter == FS_FILTER::FS_PKG){
       for (const std::string &entry : cvEntries) {
          if (entry.size() >= 4 && entry.substr(entry.size() - 4) == ".pkg") {
            push_back_to_active_pkg_folder( dirpath + "/" + entry);
            log_info("Found PKG: %s %i", entry.c_str(), get_active_pkg_folder().size());
          }
        }

        if(get_active_pkg_folder().size() > 1){ 
            log_info("Install ALL PKGs active");
            cvEntries.insert(cvEntries.begin(), getLangSTR(LANG_STR::INSTALL_ALL_PKGS));
           // log_info(cvEntries[0].c_str());
        }
    }

    out_vec.resize(cvEntries.size() + 1); // +1 for reserved_index
    for (int i = 1; i < cvEntries.size() + 1; i++)
    { 
        out_vec[i].count.token_c = 1;
        out_vec[i].info.id = cvEntries[i - 1];
        (get_active_pkg_folder().size() > 1) ? out_vec[1].flags.is_all_pkg_enabled = true : out_vec[1].flags.is_all_pkg_enabled = false;
    }
    // save path and counted items in first reserved_index
    out_vec[0].info.id = dirpath;
    out_vec[0].count.token_c = cvEntries.size(); // don't count the reserved_index!
}

// imported
extern ivec4 menu_pos;

static bool update_rela_path(std::string &rela_path, std::string &append)
{
    log_info("[FS_DEBUG] b4 rela_path: %s", rela_path.c_str());

    if (!append.empty()){

        if(rela_path.compare(append.c_str()) != 0)
          rela_path = rela_path + std::string((rela_path.size() > 2) ? "/" : "") + append;
        else
          rela_path = append;
    }
    else
    {
        const size_t last_slash_idx = rela_path.rfind('/');
        if (std::string::npos != last_slash_idx)
            rela_path = rela_path.substr(0, last_slash_idx);
    }

    if (rela_path.size() <= 0)
        rela_path = "/";

    log_info("[FS_DEBUG] after rela_path: %s", rela_path.c_str());

    return true;
}


int left_page_pos = 0, right_page_pos = 0;
bool is_all_pkg_enabled(int panel){
    return panel == 0 ? global_pkg_folder_right.size() > 1 : global_pkg_folder_left.size() > 1;
}

void fw_action_to_fm(int button)
{
    int ret = 0;
    ivec2 posi = (0);
    // we address this pointer to the active panel

update_idx:
    layout_t & l =  (fm_lp.fs_is_active ? fm_lp : fm_rp);
    // get item index
    // compute some indexes, first one of layout:
    // which icon is selected in field X*Y ?
    // which item is selected over all ?
    int idx = (l.item_sel.x * l.fieldsize.y + l.item_sel.y) + (l.fieldsize.y * l.fieldsize.x * l.page_sel.x); /// vertically!!!!
    fm_lp.fs_is_active ? left_page_pos = idx  : right_page_pos = idx;

    // update path on selected panel
    path = l.item_d[0].info.id;

    switch (button)
    { // movement
    case UP:
        posi.y--;
        break;
    case DOW:
        posi.y++;
        break;
    case L1:
        posi.x--;
        break;
    case R1:
        posi.x++;
        break;
    // actions
    case L2:{
        //(&path[0]
        char tmp[255], folder_name[255];
#if defined(__ORBIS__)
        if(!Keyboard(getLangSTR(LANG_STR::CREATE_CUSTOM_DIR).c_str(), NULL, &folder_name[0]))
            return;

        snprintf(&tmp[0], 254, "%s/%s", &path[0], &folder_name[0]);
#endif
        if (mkdir(&tmp[0], 0777) == 0){
            std::string tmp = fmt::format("{0:.20}: {1:.20}",  &path[0], &folder_name[0]);
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::FOLDER_MADE_SUCCESS), tmp);
        }
        else 
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FOLDER_MAKE_FAIL), strerror(errno));

        goto update_path;

        break;
    }
    case SQU:
    { // select item

        if (fs_ret.filter == FS_PKG) {
            get->setting_bools[BACKGROUND_INSTALL] ^= 1;
            SaveOptions(get);
            ani_notify(NOTIFI::INFO, get->setting_bools[BACKGROUND_INSTALL] ? "Background installing is now active" : "Background installing is now deactived", get->setting_bools[BACKGROUND_INSTALL] ? "PKGs will no longer show install progress" : "PKGs will now show install progress");
            return;
        }
        
        update_rela_path(path, l.item_d[idx].info.id);

        if (fs_ret.filter == FS_NONE && !file_options)
        {
            file_options = true;
            break;
        }
        else if (fs_ret.filter == FS_NONE && file_options)
        {
            file_options = false;
            break;
        }

        if (check_stat(path.c_str()) != S_IFREG && fs_ret.filter != FS_FOLDER && fs_ret.filter != FS_MP3)
            return;

        fs_ret.selected_full_path = path;
        fs_ret.selected_file = l.item_d[idx].info.id;

        fs_ret.status = FS_DONE;
        return;
    }
    break;
    case 57:
    {
        p_off.x = (!p_off.x) ? 200. : 0.;
        // refresh VBOs
        GLES2_UpdateVboForLayout(fm_lp);
        GLES2_UpdateVboForLayout(fm_rp);
    }
    break;
    case 58:
    {
    }
    break;
    case CRO:
    {
        //            sprintf(path, "%s", l.item_d[0].token_d[0].off.c_str());
        if (idx < 1)
            break; // on title

        if(fs_ret.filter ==  FS_FILTER::FS_PKG && 
        idx == 1 && get_active_pkg_folder().size() > 1){

            log_info("[FS_DEBUG] Installing %i PKGs", get_active_pkg_folder().size() );
            int i = 1;
            for (const std::string &entry : get_active_pkg_folder()) {
                log_info("[FS_DEBUG] Install PKG: %s", entry.c_str());
                #if defined(__ORBIS__)
                std::size_t pos = entry.find_last_of("/\\");  // Find last occurrence of directory separator character
                std::string filename = (pos == std::string::npos) ? entry : entry.substr(pos + 1);  // Extract substring after directory separator character
                pkginstall(entry.c_str(), filename.c_str(), !get->setting_bools[BACKGROUND_INSTALL], false, get_active_pkg_folder().size(), i);
                sleep(1);
                i++;
                #endif
            }

            break;
        }

        // update filepath, append item_data filename
        update_rela_path(path, l.item_d[idx].info.id);
        //          sprintf(path, "%s/%s", l.item_d[  0  ].token_d[0].off.c_str(),
        //                                 l.item_d[ idx ].token_d[0].off.c_str());
        if (check_stat(path.c_str()) == S_IFREG && fs_ret.filter != FS_FOLDER)
        {
            if(fs_ret.filter != FS_FILTER::FS_PKG)
              fw_action_to_fm(SQU);
            else{
#ifdef __ORBIS__
                if ((ret = pkginstall(path.c_str(), l.item_d[idx].info.id.c_str(), !get->setting_bools[BACKGROUND_INSTALL], false)) != ITEMZCORE_SUCCESS){
                    if (ret == IS_DISC_GAME)
                         ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OPTION_DOESNT_SUPPORT_DISC), fmt::format("{0:#x}",  ret));
                }
#endif
            }

        }
        else /* if a folder go to path */
            if (check_stat(path.c_str()) == S_IFDIR)
            {
            update_path:
                // recreate current item_data array
                clear_active_pkg_folder();
                l.item_d.clear();
                // use updated path
                index_items_from_dir_v2(path, l.item_d);
                // item count is know now, copy it
                l.item_c = l.item_d[0].count.token_c;
                l.vbo_s = ASK_REFRESH;
                // flag to refresh VBO, reset to first item
                ret = 1, idx = 0;
                goto update_pos;
            }
    }
    break;
    case CIR:
    {
        //            sprintf(path, "%s", l.item_d[0].token_d[0].off.c_str());
        // update filepath, step folder back
        std::string tmp = "";
        bool x1 = update_rela_path(path, tmp);
        goto update_path;
        // XXX not reached!
        // flag to refresh VBO, reset to first item
        ret = 1, idx = 0;
        // jump there
        if (x1)
            goto update_path;
        else
            goto update_pos;
    }
    break;
    case TRI:
    {
        fm_lp.item_d.clear();
        fm_rp.item_d.clear();
        clear_all_active_pkg_folder();
        fs_ret.status = FS_CANCELLED;

        if(fs_ret.filter != FS_NONE)
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), getLangSTR(LANG_STR::OP_CANNED_1));
    }
        return;
    }
    if (posi.x)
    { // we switch the active panel
        if (fm_lp.fs_is_active)
            fm_lp.fs_is_active = false, fm_rp.fs_is_active = true;
        else // !!!
            if (fm_rp.fs_is_active)
                fm_lp.fs_is_active = true, fm_rp.fs_is_active = false;
        // repeat with updated item index
        button = posi.x = 0;
        goto update_idx;
    }
    else if (posi.y)
    { // move
        idx += posi.y;

    update_pos:
        l.vbo_s = ASK_REFRESH;
        // check for bounds, flag to refresh VBO!
        if (idx > l.item_c){
            idx = 0, ret = 1;
        }
        else if (idx < 0)
            idx = l.item_c, ret = 1;

        ivec2 x = (ivec2){idx % l.fieldsize.x, idx / l.fieldsize.x};
        //log_info("x (%d, %d), %d %d\n", x.x, x.y,x.y % l.fieldsize.y, x.y / l.fieldsize.y);

        ivec3 X = (ivec3){x.x,
                          x.y % l.fieldsize.y,
                          x.y / l.fieldsize.y};
        //    printf("(x:%d, y:%d, z:%d)\n", X.x, X.y, X.z);

        // update selector pos
        l.item_sel = X.xy;
        // on page change flag to refresh VBO!
        if (l.page_sel.x != X.z)
            ret = 1;
        // update current page
        l.page_sel.x = X.z;
        fm_lp.fs_is_active ? left_page_pos = idx  : right_page_pos = idx;

        // all done
       // log_info("item_d[%d]: y:%d p:%d", idx, l.item_sel.y, l.page_sel.x);
    }

    // trigger VBO refresh
    if (ret)
        GLES2_UpdateVboForLayout(l);

    // feedback
#if 0
    if (l.item_c > 0)
        log_info("Selected: %s", l.item_d[idx].info.id.c_str());
    else
        log_info("no items");
#endif
}

