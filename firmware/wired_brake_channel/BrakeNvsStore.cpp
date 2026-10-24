#include "BrakeNvsStore.h"
#include <Arduino.h>
#include <nvs.h>
#include <esp_timer.h>
#include <algorithm>
using namespace wired_brake;
LoadResult BrakeNvsStore::loadSlots(PersistRecord&out){
 if(!enabled_)return LoadResult::Unavailable;
 nvs_handle_t h{};const auto opened=nvs_open("brake6p_v1",NVS_READONLY,&h);
 if(opened==ESP_ERR_NVS_NOT_FOUND)return LoadResult::Missing;
 if(opened!=ESP_OK)return LoadResult::Unavailable;
 uint8_t a[32]{},b[32]{};
 const auto readSlot=[h](const char*key,uint8_t*bytes){size_t n=0;auto result=nvs_get_blob(h,key,nullptr,&n);if(result!=ESP_OK)return result;if(n!=32)return esp_err_t(ESP_OK);return nvs_get_blob(h,key,bytes,&n);};
 const auto ra=readSlot("slot0",a),rb=readSlot("slot1",b);nvs_close(h);
 if((ra!=ESP_OK&&ra!=ESP_ERR_NVS_NOT_FOUND)||(rb!=ESP_OK&&rb!=ESP_ERR_NVS_NOT_FOUND))return LoadResult::Unavailable;
 bool recovered=false;const auto result=selectPersist(ra==ESP_OK?a:nullptr,rb==ESP_OK?b:nullptr,out,recovered);out.recovered=recovered;return result;
}
LoadResult BrakeNvsStore::load(PersistRecord&out){
 auto result=loadSlots(out);if(result!=LoadResult::Missing)return result;
 nvs_handle_t h{};const auto opened=nvs_open("brake12v",NVS_READONLY,&h);
 if(opened==ESP_ERR_NVS_NOT_FOUND)return LoadResult::Missing;
 if(opened!=ESP_OK)return LoadResult::Unavailable;
 int32_t alarm=600;uint8_t warning=1;const auto a=nvs_get_i32(h,"alarmC",&alarm),b=nvs_get_u8(h,"buzzer",&warning);nvs_close(h);
 if(a==ESP_ERR_NVS_NOT_FOUND&&b==ESP_ERR_NVS_NOT_FOUND)return LoadResult::Missing;
 if(a!=ESP_OK||b!=ESP_OK||alarm<100||alarm>900||warning>1||alarm%10)return LoadResult::Invalid;
 out={{uint16_t(alarm),warning!=0},0,2,true};Serial.println("Legacy brake12v read-only migration; touch confirmation required");return LoadResult::Valid;
}
StoreResult BrakeNvsStore::commit(const PersistRecord&r){
 uint8_t bytes[32];if(!enabled_||!encodePersist(r,bytes,32))return StoreResult::NotCommitted;
 const auto started=uint64_t(esp_timer_get_time());nvs_handle_t h{};
 if(nvs_open("brake6p_v1",NVS_READWRITE,&h)!=ESP_OK)return StoreResult::NotCommitted;
 const auto write=nvs_set_blob(h,(r.version&1)?"slot1":"slot0",bytes,32);const auto committed=write==ESP_OK?nvs_commit(h):write;nvs_close(h);
 const auto ended=uint64_t(esp_timer_get_time());if(ended>=started)maxPause_=std::max(maxPause_,ended-started);
 PersistRecord read{};const auto verified=loadSlots(read);const bool match=verified==LoadResult::Valid&&read.version==r.version&&read.config==r.config;
 Serial.printf("NVS save ver=%lu write=%d commit=%d verified=%u max_pause_us=%llu\n",static_cast<unsigned long>(r.version),write,committed,match,static_cast<unsigned long long>(maxPause_));
 return match?StoreResult::Committed:StoreResult::Unknown;
}
LoadResult BrakeNvsStore::readBack(uint32_t,PersistRecord&r){return loadSlots(r);}
