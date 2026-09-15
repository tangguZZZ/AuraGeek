#pragma once
#include <lvgl.h>
#include "ui/UserSettings.h"
namespace aurageek { namespace ui {
class SettingsView {
 public:
 void attach(lv_obj_t* parent,UserSettings* value);
 void detach(){view_=nullptr;}
 void rotate(int steps);
 void click();
 bool back();
 void update(uint32_t now);
 bool takeCommit(){bool c=commit_;commit_=false;return c;}
 bool takePortalRequest(){bool c=portalRequest_;portalRequest_=false;return c;}
 void saveResult(bool ok){saveState_=ok?2:3;saveMessageUntil_=lv_tick_get()+2500;if(view_)lv_obj_invalidate(view_);}
 bool editing()const{return editing_;}
 int selected()const{return selected_;}
 private:
 static void draw(lv_event_t* e);
 lv_obj_t* view_=nullptr;
 UserSettings* value_=nullptr;
 UserSettings saved_{};
 int selected_=0;
 bool editing_=false,commit_=false;
 bool portalRequest_=false;
 unsigned saveState_=0;
 uint32_t saveMessageUntil_=0;
 float position_=0,velocity_=0,editor_=0,editorVelocity_=0,reading_=100,readingVelocity_=0;
 uint32_t tick_=0;
};
}}
