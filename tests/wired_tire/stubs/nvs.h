#pragma once
#include "esp_err.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
using nvs_handle_t=int;
constexpr int NVS_READONLY=0,NVS_READWRITE=1;
namespace nvs_mock{inline std::map<std::string,std::vector<uint8_t>> values;inline bool failRead=false,failAfterCommit=false;inline unsigned writes=0;}
inline int nvs_open(const char*n,int mode,nvs_handle_t*h){*h=std::string(n)=="brake6p_v1"?1:2;if(!mode){const std::string p=std::to_string(*h)+"/";bool found=false;for(auto&v:nvs_mock::values)if(v.first.find(p)==0)found=true;if(!found)return ESP_ERR_NVS_NOT_FOUND;}return ESP_OK;}
inline void nvs_close(nvs_handle_t){}
inline int nvs_get_blob(nvs_handle_t h,const char*k,void*out,size_t*n){if(nvs_mock::failRead)return ESP_FAIL;auto i=nvs_mock::values.find(std::to_string(h)+"/"+k);if(i==nvs_mock::values.end())return ESP_ERR_NVS_NOT_FOUND;if(out&&*n<i->second.size())return ESP_FAIL;*n=i->second.size();if(out)std::memcpy(out,i->second.data(),*n);return ESP_OK;}
inline int nvs_set_blob(nvs_handle_t h,const char*k,const void*b,size_t n){++nvs_mock::writes;const auto*p=static_cast<const uint8_t*>(b);nvs_mock::values[std::to_string(h)+"/"+k]={p,p+n};return ESP_OK;}
inline int nvs_commit(nvs_handle_t){if(nvs_mock::failAfterCommit)nvs_mock::failRead=true;return ESP_OK;}
inline int nvs_get_i32(nvs_handle_t,const char*,int32_t*){return ESP_ERR_NVS_NOT_FOUND;}
inline int nvs_get_u8(nvs_handle_t,const char*,uint8_t*){return ESP_ERR_NVS_NOT_FOUND;}
