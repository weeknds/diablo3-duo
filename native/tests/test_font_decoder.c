// SPDX-License-Identifier: GPL-3.0-or-later
#include "duo_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#x);exit(1);} }while(0)
typedef struct Captured { unsigned calls; uint32_t line,first,count; EdenDsmodFontGlyph glyphs[95]; } Captured;
static void sink(void *p,uint32_t line,uint32_t first,const EdenDsmodFontGlyph *glyphs,uint32_t count) {
 Captured*c=p;CHECK(c&&glyphs&&count==95);c->calls++;c->line=line;c->first=first;c->count=count;memcpy(c->glyphs,glyphs,count*sizeof *glyphs);
}
static void u16(uint8_t *b,unsigned off,uint16_t n){b[off]=(uint8_t)n;b[off+1]=(uint8_t)(n>>8);}
static void valid_blob(uint8_t b[DUO_FONT_SIZE]) {
 memset(b,0,DUO_FONT_SIZE);memcpy(b,"DUOSANS1",8);
 u16(b,8,1);u16(b,10,32);u16(b,12,1024);u16(b,14,512);u16(b,16,32);u16(b,18,32);u16(b,20,95);u16(b,22,14);
 u16(b,24,95*14);
 for(unsigned i=0;i<95;i++){unsigned o=32+14*i;u16(b,o,(i%16)*64+2);u16(b,o+2,(i/16)*80+2);u16(b,o+4,i?20:0);u16(b,o+6,i?32:0);u16(b,o+8,0);u16(b,o+10,i?32:0);u16(b,o+12,i?24:10);}
}
static void accepted_and_borrowed(void) {
 uint8_t storage[DUO_FONT_SIZE+1],*b=storage+1;valid_blob(b);Captured c={0};
 CHECK(duo_font_decode(NULL,b,DUO_FONT_SIZE,&c,sink));CHECK(c.calls==1&&c.line==32&&c.first==32&&c.count==95);
 CHECK(c.glyphs[0].w==0&&c.glyphs[0].advance==10&&c.glyphs[94].w==20);
 memset(b,0,DUO_FONT_SIZE);CHECK(c.glyphs[94].w==20); // sink owns its deep copy
}
static void rejected_lengths_headers(void) {
 uint8_t b[DUO_FONT_SIZE+1];valid_blob(b);Captured c={0};
 CHECK(!duo_font_decode(NULL,NULL,DUO_FONT_SIZE,&c,sink));CHECK(!duo_font_decode(NULL,b,DUO_FONT_SIZE,&c,NULL));
 for(size_t n=0;n<DUO_FONT_SIZE;n++){CHECK(!duo_font_decode(NULL,b,n,&c,sink));CHECK(!c.calls);}
 CHECK(!duo_font_decode(NULL,b,DUO_FONT_SIZE+1,&c,sink));CHECK(!duo_font_decode(NULL,b,SIZE_MAX,&c,sink));
 for(unsigned off=0;off<32;off++){b[off]^=1;CHECK(!duo_font_decode(NULL,b,DUO_FONT_SIZE,&c,sink));CHECK(!c.calls);b[off]^=1;}
}
static void rejected_glyph_bounds(void) {
 const struct{unsigned offset;uint16_t value;} bad[]={{0,65535},{2,65535},{4,61},{6,77},{8,65},{8,(uint16_t)-65},{10,65},{10,(uint16_t)-65},{12,0},{12,65}};
 uint8_t b[DUO_FONT_SIZE];Captured c={0};
 for(unsigned glyph=0;glyph<95;glyph++)for(unsigned j=0;j<sizeof bad/sizeof bad[0];j++) {
  valid_blob(b);u16(b,32+14*glyph+bad[j].offset,bad[j].value);CHECK(!duo_font_decode(NULL,b,DUO_FONT_SIZE,&c,sink));CHECK(!c.calls);
 }
 for(unsigned glyph=1;glyph<95;glyph++)for(unsigned field=4;field<=6;field+=2) {
  valid_blob(b);u16(b,32+14*glyph+field,0);CHECK(!duo_font_decode(NULL,b,DUO_FONT_SIZE,&c,sink));CHECK(!c.calls);
 }
 valid_blob(b);u16(b,36,1);CHECK(!duo_font_decode(NULL,b,DUO_FONT_SIZE,&c,sink));CHECK(!c.calls);
}
static void file_roundtrip(const char *path) {
 FILE *f=fopen(path,"rb");CHECK(f);uint8_t b[DUO_FONT_SIZE+1];size_t n=fread(b,1,sizeof b,f);CHECK(!ferror(f)&&fclose(f)==0&&n==DUO_FONT_SIZE);
 Captured c={0};CHECK(duo_font_decode(NULL,b,n,&c,sink));CHECK(c.calls==1&&c.glyphs['H'-32].h==32&&c.glyphs['g'-32].bearing_y<c.glyphs['g'-32].h);
}
int main(int argc,char **argv) {
 const EdenDsmodFontExtensions *e=eden_dsmod_get_font_extensions(EDEN_DSMOD_FONT_EXT_VERSION,EDEN_DSMOD_FONT_EXT_HASH);
 CHECK(e&&e->version==1&&e->struct_size==24&&e->abi_hash==UINT64_C(0x2f8a1c6d94b7e053)&&e->decode_font==duo_font_decode);
 CHECK(!eden_dsmod_get_font_extensions(0,EDEN_DSMOD_FONT_EXT_HASH));CHECK(!eden_dsmod_get_font_extensions(2,EDEN_DSMOD_FONT_EXT_HASH));CHECK(!eden_dsmod_get_font_extensions(1,0));
 accepted_and_borrowed();rejected_lengths_headers();rejected_glyph_bounds();if(argc==2)file_roundtrip(argv[1]);
 puts("PASS font decoder: bounded ASCII95, unaligned source bytes, all truncations/header mismatches, glyph bounds, synchronous sink only");return 0;
}
