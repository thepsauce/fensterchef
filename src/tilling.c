#include "fensterchef.h"
#include "frame.h"
#include "tilling.h"

#include <inttypes.h>
#include <stdint.h>

void split_vertically(void)
{       
    uint32_t width_old_frame = g_frames[g_cur_frame].w / 2;
    uint32_t width_new_frame = g_frames[g_cur_frame].w - width_old_frame;
    int32_t x_new_frame = g_frames[g_cur_frame].x + width_old_frame + 1;

    g_frames[g_cur_frame].w = width_old_frame;
    create_frame(NULL, x_new_frame, g_frames[ g_cur_frame].y, width_new_frame,
            g_frames[g_cur_frame].h);
    
    reload_frame(g_cur_frame);

    LOG(stderr, "frame %" PRId32 " was split vertically\n", g_cur_frame);
}   

void split_horizontally(void)
{
    uint32_t hight_old_frame = g_frames[g_cur_frame].h / 2;
    uint32_t hight_new_frame = g_frames[g_cur_frame].h - hight_old_frame;
    int32_t y_new_frame = g_frames[g_cur_frame].y + hight_old_frame + 1;

    g_frames[g_cur_frame].h = hight_old_frame;
    create_frame(NULL, g_frames[g_cur_frame].x, y_new_frame, g_frames[g_cur_frame].w,
            hight_new_frame);
    
    reload_frame(g_cur_frame);

    LOG(stderr, "frame %" PRId32 " was split horizontally\n", g_cur_frame);
}   
