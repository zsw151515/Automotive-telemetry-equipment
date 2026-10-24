#include "Gt911TouchCodec.h"
namespace wired_brake {
bool decodeGt911Report(uint8_t status,const uint8_t*b,size_t n,TouchSample&out){
 if(!(status&0x80)||(status&0x70)||(status&15)>1)return false;
 TouchSample s{};s.valid=true;s.count=status&15;s.pressed=s.count==1;
 if(s.pressed){if(!b||n!=8)return false;s.x=uint16_t(b[1]|uint16_t(b[2])<<8);s.y=uint16_t(b[3]|uint16_t(b[4])<<8);if(s.x>=1024||s.y>=600)return false;}
 else if(n)return false;
 out=s;return true;
}
}
