/* Freestanding support; no firmware ROM dependencies. */
#include <stddef.h>
#include <stdint.h>
/* Word-at-a-time when both sides share 4-byte alignment; bytes otherwise.
   The pattern snapshots (2 x 30710 B) dominate generation time. */
void *memset(void *p,int v,size_t n){
 unsigned char *b=p;uint32_t w=(unsigned char)v;w|=w<<8;w|=w<<16;
 while(n&&((uintptr_t)b&3)){*b++=(unsigned char)v;n--;}
 {uint32_t *d=(uint32_t *)(void *)b;while(n>=16){d[0]=w;d[1]=w;d[2]=w;d[3]=w;d+=4;n-=16;}while(n>=4){*d++=w;n-=4;}b=(unsigned char *)d;}
 while(n--)*b++=(unsigned char)v;
 return p;
}
void *memcpy(void *p,const void *q,size_t n){
 unsigned char *b=p;const unsigned char *a=q;
 if((((uintptr_t)b^(uintptr_t)a)&3)==0){
  while(n&&((uintptr_t)b&3)){*b++=*a++;n--;}
  {uint32_t *d=(uint32_t *)(void *)b;const uint32_t *s=(const uint32_t *)(const void *)a;
   while(n>=16){d[0]=s[0];d[1]=s[1];d[2]=s[2];d[3]=s[3];d+=4;s+=4;n-=16;}
   while(n>=4){*d++=*s++;n-=4;}
   b=(unsigned char *)d;a=(const unsigned char *)s;}
 }
 while(n--)*b++=*a++;
 return p;
}
