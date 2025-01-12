#include "tilling.h"
#include "frame.h"

void split_vertically()
{
    Frame *focused_frame = get_focus_frame();   

    unsigned int width_old_frame = focused_frame->w / 2;
    unsigned int width_new_frame = focused_frame->w - width_old_frame;
    unsigned int x_new_frame = focused_frame->x + width_old_frame + 1;

    focused_frame->w = width_old_frame;
    create_empty_frame(x_new_frame, focused_frame->y, width_new_frame, focused_frame->h);
    
    reload_frame(focused_frame);
}   

void split_horizontally()
{
    Frame *focused_frame = get_focus_frame();   

    unsigned int hight_old_frame = focused_frame->h / 2;
    unsigned int hight_new_frame = focused_frame->h - hight_old_frame;
    unsigned int y_new_frame = focused_frame->y + hight_old_frame + 1;

    focused_frame->h = hight_old_frame;
    create_empty_frame(focused_frame->x, y_new_frame, focused_frame->w, hight_new_frame);
    
    reload_frame(focused_frame);
}   