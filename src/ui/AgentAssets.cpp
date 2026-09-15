#include "ui/AgentAssets.h"
// Keep protocol emotion IDs stable while sharing six representative animations.
static constexpr unsigned faceAlias[]={0,1,2,2,3,4,3,5,5,1,1,5,2};
#ifdef ARDUINO
#include <model_path.h>
#include <atomic>
#include <cstdio>
#include <cstring>
namespace aurageek::ui {
static lv_image_dsc_t faces[6]{};
static std::atomic<bool> loaded{false};
bool loadAgentAssets(){
 auto* models=get_static_srmodels();if(!models)return false;
 for(int i=0;i<models->num;++i){
  if(strcmp(models->model_name[i],"ag_faces"))continue;
  auto* group=models->model_data[i];
  for(unsigned n=0;n<6;++n){
   char name[20];snprintf(name,sizeof(name),"face%u.gif",n);bool found=false;
   for(int f=0;f<group->num;++f){
    if(strcmp(group->files[f],name))continue;
    if(group->sizes[f]<10 || memcmp(group->data[f],"GIF8",4))return false;
    faces[n].header.magic=LV_IMAGE_HEADER_MAGIC;faces[n].header.cf=LV_COLOR_FORMAT_RAW;
    faces[n].data=reinterpret_cast<const uint8_t*>(group->data[f]);faces[n].data_size=group->sizes[f];found=true;break;
   }
   if(!found)return false;
  }
  loaded.store(true,std::memory_order_release);return true;
 }
 return false;
}
const lv_image_dsc_t* agentAsset(unsigned index){return loaded.load(std::memory_order_acquire)&&index<13?&faces[faceAlias[index]]:nullptr;}
}
#else
extern "C" {
LV_IMAGE_DECLARE(ag_face_listen);LV_IMAGE_DECLARE(ag_face_think);LV_IMAGE_DECLARE(ag_face_happy);
LV_IMAGE_DECLARE(ag_face_sad);LV_IMAGE_DECLARE(ag_face_angry);LV_IMAGE_DECLARE(ag_face_love);
}
namespace aurageek::ui {
const lv_image_dsc_t* agentAsset(unsigned index){
 static const lv_image_dsc_t* faces[]={&ag_face_listen,&ag_face_think,&ag_face_happy,&ag_face_sad,&ag_face_angry,&ag_face_love};
 return index<13?faces[faceAlias[index]]:nullptr;
}
}
#endif
