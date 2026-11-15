#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

PSP_MODULE_INFO("PSP Pixel Studio",0,2,7);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#define SW 480
#define SH 272
#define PITCH 512
#define MAXC 128
#define TRANSPARENT 0u
#define RGB(r,g,b) (0xff000000u | ((unsigned)(b)<<16) | ((unsigned)(g)<<8) | (unsigned)(r))
#define SAVE_DIR "ms0:/PSP/PIXELSTUDIO"
#define TITLE_RAW SAVE_DIR "/PIXELSTUDIO_TITLE.RAW"

typedef struct { unsigned color; short x,y,z; } Vertex;
typedef struct { short u,v,x,y,z; } TexVertex;
static uint16_t __attribute__((aligned(16))) title_pixels[512*512];
static int title_ready=0,title_active=0;
static unsigned int __attribute__((aligned(16))) display_list[262144];
static uint32_t artwork[MAXC*MAXC];
static uint32_t palette[64];
static int palette_id=0,palette_count=64,chosen=0;
static int size=64,cursor_x=0,cursor_y=0,zoom=3,view_x=0,view_y=0;
static int show_panels=0,grid_mode=1,theme=0,dirty=0;
static int menu=0,menu_item=0,option_item=0,new_item=2,confirm_new=0;
static int running=1,status_frames=0;
static int slot=0, slot_action=0, pending=0, panel_focus=0, palette_cursor=0;
static int show_grid=1, clean_preview=0;
static int offset_x=0,offset_y=0;
#define HIST 64
typedef struct {uint16_t x,y;uint32_t before,after;} Change;
static Change history[HIST];static int hcount=0,hpos=0;
static char status[80]="";
static int circle_active=0,circle_x=0,circle_y=0,circle_radius=4,circle_undo=0,circle_redo=0;
static uint32_t circle_before[MAXC*MAXC],circle_after[MAXC*MAXC];
static const char *palette_names[]={"RGB64","AMMO-8","CL8UDS","LAVA-GB","MULFOK32","NYX8","BLESSING","SWEETIE16","TWILIGHT5"};
static const char *palette_hex[]={
"", "040c06 112318 1e3a29 305d42 4d8061 89a257 bedc7f eeffcc",
"fcb08c ef9d7f d6938a b48d92 a597a1 8fa0bf 9aabc9 a5b7d4",
"051f39 4a2480 c53a9d ff8e80",
"5ba675 6bc96c abdd64 fcef8d ffb879 ea6262 cc425e a32858 751756 390947 611851 873555 a6555f c97373 f2ae99 ffc3f2 ee8fcb d46eb3 873e84 1f102a 4a3052 7b5480 a6859f d9bdc8 ffffff aee2ff 8db7ff 6d80fa 8465ec 834dc4 7d2da0 4e187c",
"08141e 0f2a3f 20394f f6d6bd c3a38a 997577 816271 4e495f",
"74569b 96fbc7 f7ffae ffb3cb d8bfd8",
"1a1c2c 5d275d b13e53 ef7d57 ffcd75 a7f070 38b764 257179 29366f 3b5dc9 41a6f6 73eff7 f4f4f4 94b0c2 566c86 333c57",
"fbbbad ee8695 4a7a96 333f58 292831"};
static int hexval(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return 0;}
static void select_palette(int id){
 palette_id=id;palette_count=0;
 if(id==0){for(int i=0;i<64;i++)palette[i]=RGB(((i>>4)&3)*85,((i>>2)&3)*85,(i&3)*85);palette_count=64;}
 else {const char *s=palette_hex[id];while(*s && palette_count<64){while(*s==' ')s++;if(!*s)break;unsigned v=0;for(int j=0;j<6;j++)v=(v<<4)|hexval(*s++);palette[palette_count++]=RGB((v>>16)&255,(v>>8)&255,v&255);}}
 if(chosen>=palette_count)chosen=0;
}
static void msg(const char *s){snprintf(status,sizeof(status),"%s",s);status_frames=180;}
static int exit_callback(int a,int b,void *c){(void)a;(void)b;(void)c;running=0;return 0;}
static int callback_thread(SceSize a,void *b){(void)a;(void)b;int cb=sceKernelCreateCallback("exit",exit_callback,NULL);sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
static void setup_callbacks(void){int t=sceKernelCreateThread("callbacks",callback_thread,0x11,0xFA0,0,0);if(t>=0)sceKernelStartThread(t,0,0);}
static void rect(int x,int y,int w,int h,unsigned color){
 if(w<=0||h<=0||x>=SW||y>=SH||x+w<=0||y+h<=0)return;
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>SW)w=SW-x;if(y+h>SH)h=SH-y;
 Vertex *v=(Vertex*)sceGuGetMemory(2*sizeof(Vertex));
 v[0].color=color;v[0].x=x;v[0].y=y;v[0].z=0;
 v[1].color=color;v[1].x=x+w;v[1].y=y+h;v[1].z=0;
 sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,v);
}
/* Compact built-in 3x5 bitmap font, scaled 2x. Unsupported chars are spaces. */
static const char *glyph(char c){
 switch(c){
 case 'A':return "010101111101101";case 'B':return "110101110101110";
 case 'C':return "011100100100011";case 'D':return "110101101101110";
 case 'E':return "111100110100111";case 'F':return "111100110100100";
 case 'G':return "011100101101011";case 'H':return "101101111101101";
 case 'I':return "111010010010111";case 'J':return "001001001101010";
 case 'K':return "101101110101101";case 'L':return "100100100100111";
 case 'M':return "101111111101101";case 'N':return "101111111111101";
 case 'O':return "010101101101010";case 'P':return "110101110100100";
 case 'Q':return "010101101111011";case 'R':return "110101110101101";
 case 'S':return "011100010001110";case 'T':return "111010010010010";
 case 'U':return "101101101101111";case 'V':return "101101101101010";
 case 'W':return "101101111111101";case 'X':return "101101010101101";
 case 'Y':return "101101010010010";case 'Z':return "111001010100111";
 case '0':return "111101101101111";case '1':return "010110010010111";
 case '2':return "111001111100111";case '3':return "111001111001111";
 case '4':return "101101111001001";case '5':return "111100111001111";
 case '6':return "111100111101111";case '7':return "111001001001001";
 case '8':return "111101111101111";case '9':return "111101111001111";
 case ':':return "000010000010000";case '.':return "000000000000010";
 case '-':return "000000111000000";case '/':return "001001010100100";
 case '+':return "000010111010000";case '>':return "100010001010100";
 case '<':return "001010100010001";case '?':return "111001010000010";
 case '!':return "010010010000010";case 'x':return "000101010101000";
 default:return "000000000000000";
 }
}
static void textat(int x,int y,const char *s,unsigned color,int scale){
 for(;*s;s++,x+=4*scale){const char *g=glyph(*s);for(int i=0;i<15;i++)if(g[i]=='1')rect(x+(i%3)*scale,y+(i/3)*scale,scale,scale,color);}
}
/* Frutiger Aero: luminous aqua glass, no extra texture allocations. */
static unsigned bg(void){return RGB(191,235,244);}
static unsigned panel(void){return RGB(43,151,193);}
static unsigned accent(void){return RGB(151,241,247);}
static unsigned ink(void){return RGB(255,255,255);}
static unsigned deep(void){return RGB(18,79,107);}
/* Small rounded-rectangle compositor. Alpha is applied by GU blending. */
static unsigned rgba(int r,int g,int b,int a){
 return ((unsigned)a<<24)|((unsigned)b<<16)|((unsigned)g<<8)|(unsigned)r;
}
static void round_rect(int x,int y,int w,int h,int radius,unsigned color){
 if(w<=0||h<=0)return;
 if(radius<0)radius=0;if(radius>w/2)radius=w/2;if(radius>h/2)radius=h/2;
 for(int row=0;row<h;row++){
  int edge=row<h/2?row:h-1-row;
  int inset=0;
  if(edge<radius){
   int dy=radius-1-edge;
   while(inset<radius&&(radius-inset)*(radius-inset)+dy*dy>radius*radius)inset++;
  }
  rect(x+inset,y+row,w-2*inset,1,color);
 }
}
static void aero_glass(int x,int y,int w,int h){
 round_rect(x+2,y+3,w,h,10,rgba(11,60,96,90));
 round_rect(x,y,w,h,10,rgba(12,111,178,192));
 round_rect(x+2,y+2,w-4,h-4,9,rgba(54,181,228,80));
 round_rect(x+3,y+3,w-6,h/4,8,rgba(244,255,255,105));
 round_rect(x+5,y+4,w-10,2,1,rgba(255,255,255,195));
 round_rect(x+4,y+h-7,w-8,3,1,rgba(4,89,151,90));
}
static void aero_choice(int x,int y,int w,int h){
 round_rect(x,y,w,h,7,rgba(99,204,55,235));
 round_rect(x+2,y+1,w-4,h/2,6,rgba(236,255,189,205));
 round_rect(x+2,y+h-4,w-4,2,1,rgba(31,140,61,130));
}
static void fit_view(void){
 int z=SH/size;if(SW/size<z)z=SW/size;if(z<1)z=1;if(z>16)z=16;
 zoom=z;view_x=view_y=0;
}
static void init_canvas(int n){circle_active=circle_undo=circle_redo=0;size=n;memset(artwork,0,sizeof(artwork));cursor_x=cursor_y=0;dirty=0;hcount=hpos=0;fit_view();}
static void adjust_view(void){
 int cols=SW/zoom,rows=SH/zoom;
 if(cols<size){int margin=cols>8?3:1;if(cursor_x<view_x+margin)view_x=cursor_x-margin;if(cursor_x>=view_x+cols-margin)view_x=cursor_x-cols+margin+1;}
 if(rows<size){int margin=rows>8?3:1;if(cursor_y<view_y+margin)view_y=cursor_y-margin;if(cursor_y>=view_y+rows-margin)view_y=cursor_y-rows+margin+1;}
 int mx=size-cols,my=size-rows;if(mx<0)mx=0;if(my<0)my=0;
 if(view_x<0)view_x=0;if(view_y<0)view_y=0;if(view_x>mx)view_x=mx;if(view_y>my)view_y=my;
}
static void placement(void){
 int w=size*zoom,h=size*zoom;
 offset_x=w<SW?(SW-w)/2:0;offset_y=h<SH?(SH-h)/2:0;
}
static void set_zoom(int z){zoom=z;adjust_view();placement();}
static void next_zoom(void){
 static const int steps[]={1,2,3,4,6,8,12,16};
 int next=0;for(int i=0;i<8;i++)if(steps[i]>zoom){next=steps[i];break;}
 if(next)set_zoom(next);else {fit_view();adjust_view();placement();msg("FIT CANVAS");}
}
static void edit_pixel(uint32_t value){circle_undo=circle_redo=0;
 int idx=cursor_y*size+cursor_x;uint32_t old=artwork[idx];if(old==value)return;
 if(hpos<hcount)hcount=hpos;
 if(hcount==HIST){memmove(history,history+1,(HIST-1)*sizeof(Change));hcount--;hpos--;}
 history[hpos].x=cursor_x;history[hpos].y=cursor_y;history[hpos].before=old;history[hpos].after=value;
 hpos++;hcount=hpos;artwork[idx]=value;dirty=1;
}
static void undo(void){if(circle_undo){memcpy(artwork,circle_before,sizeof(artwork));circle_undo=0;circle_redo=1;dirty=1;msg("CIRCLE UNDO");return;}if(hpos<=0){msg("NOTHING TO UNDO");return;}Change *c=&history[--hpos];artwork[c->y*size+c->x]=c->before;dirty=1;msg("UNDO");}
static void redo(void){if(circle_redo){memcpy(artwork,circle_after,sizeof(artwork));circle_redo=0;circle_undo=1;dirty=1;msg("CIRCLE REDO");return;}if(hpos>=hcount){msg("NOTHING TO REDO");return;}Change *c=&history[hpos++];artwork[c->y*size+c->x]=c->after;dirty=1;msg("REDO");}
static void sample(void){
 uint32_t v=artwork[cursor_y*size+cursor_x];if(!v){msg("TRANSPARENT PIXEL");return;}
 for(int i=0;i<palette_count;i++)if(palette[i]==v){chosen=i;msg("COLOR PICKED");return;}
 if(palette_count<64){palette[palette_count]=v;chosen=palette_count++;msg("COLOR PICKED");}
 else{palette[chosen]=v;msg("COLOR PICKED");}
}
static void slot_path(char *s,int n,int index){snprintf(s,n,SAVE_DIR "/project_%02d.pps",index+1);}
static void save_art(void){
 sceIoMkdir(SAVE_DIR,0777);char path[96];slot_path(path,sizeof(path),slot);
 SceUID f=sceIoOpen(path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);if(f<0){msg("SAVE FAILED");return;}
 unsigned char hdr[8]={'P','P','S','3',(unsigned char)size,0,0,0};
 int a=sceIoWrite(f,hdr,8),b=sceIoWrite(f,artwork,size*size*4);sceIoClose(f);
 if(a==8&&b==size*size*4){dirty=0;msg("PROJECT SAVED");}else msg("SAVE FAILED");
}
static void load_art(void){
 char path[96];slot_path(path,sizeof(path),slot);SceUID f=sceIoOpen(path,PSP_O_RDONLY,0);if(f<0){msg("EMPTY SLOT");return;}
 unsigned char hdr[8];int a=sceIoRead(f,hdr,8);
 if(a!=8||memcmp(hdr,"PPS3",4)||!(hdr[4]==16||hdr[4]==32||hdr[4]==64||hdr[4]==128)){sceIoClose(f);msg("INVALID PROJECT");return;}
 int n=hdr[4];static uint32_t tmp[MAXC*MAXC];int b=sceIoRead(f,tmp,n*n*4);sceIoClose(f);
 if(b!=n*n*4){msg("INVALID PROJECT");return;}
 init_canvas(n);memcpy(artwork,tmp,n*n*4);dirty=0;msg("PROJECT LOADED");
}
/* Minimal PNG encoder: RGBA8, zlib stored DEFLATE blocks; no external PNG library. */
static uint32_t crc_table[256];
static void crc_init(void){for(unsigned n=0;n<256;n++){uint32_t c=n;for(int k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);crc_table[n]=c;}}
static uint32_t crc_step(uint32_t c,const unsigned char *p,unsigned n){while(n--)c=crc_table[(c^*p++)&255]^(c>>8);return c;}
static void put32(unsigned char *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static int write_all(SceUID f,const void *p,int n){return sceIoWrite(f,p,n)==n;}
static int chunk(SceUID f,const char *tag,const unsigned char *data,uint32_t n){
 unsigned char b[4];put32(b,n);if(!write_all(f,b,4)||!write_all(f,tag,4))return 0;
 if(n&&!write_all(f,data,n))return 0;
 uint32_t crc=crc_step(0xffffffffu,(const unsigned char*)tag,4);crc=crc_step(crc,data,n)^0xffffffffu;
 put32(b,crc);return write_all(f,b,4);
}
static unsigned char raw[128*(1+128*4)];static unsigned char compressed[128*(1+128*4)+64];
static void export_png(void){
 sceIoMkdir(SAVE_DIR,0777);sceIoMkdir(SAVE_DIR "/EXPORTS",0777);
 char path[120];int found=-1;
 for(int i=1;i<=999;i++){snprintf(path,sizeof(path),SAVE_DIR "/EXPORTS/ART_%03d.PNG",i);
  SceUID check=sceIoOpen(path,PSP_O_RDONLY,0);if(check<0){found=i;break;}sceIoClose(check);}
 if(found<0){msg("EXPORT FOLDER FULL");return;}
 int k=0;for(int y=0;y<size;y++){raw[k++]=0;for(int x=0;x<size;x++){
  uint32_t c=artwork[y*size+x];raw[k++]=(unsigned char)c;raw[k++]=(unsigned char)(c>>8);raw[k++]=(unsigned char)(c>>16);raw[k++]=c?255:0;
 }}
 int pos=0;compressed[pos++]=0x78;compressed[pos++]=0x01;
 int rem=k,at=0;while(rem){int n=rem>65535?65535:rem;compressed[pos++]=rem<=65535?1:0;
  compressed[pos++]=n&255;compressed[pos++]=n>>8;compressed[pos++]=(~n)&255;compressed[pos++]=((~n)>>8)&255;
  memcpy(compressed+pos,raw+at,n);pos+=n;at+=n;rem-=n;}
 uint32_t a=1,b=0;for(int i=0;i<k;i++){a=(a+raw[i])%65521;b=(b+a)%65521;}
 put32(compressed+pos,(b<<16)|a);pos+=4;
 SceUID f=sceIoOpen(path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);if(f<0){msg("EXPORT FAILED");return;}
 const unsigned char signature[8]={137,80,78,71,13,10,26,10};unsigned char ihdr[13]={0};
 put32(ihdr,size);put32(ihdr+4,size);ihdr[8]=8;ihdr[9]=6;
 int ok=write_all(f,signature,8)&&chunk(f,"IHDR",ihdr,13)&&chunk(f,"IDAT",compressed,pos)&&chunk(f,"IEND",NULL,0);
 sceIoClose(f);if(ok){char buf[60];snprintf(buf,sizeof(buf),"EXPORTED ART %03d",found);msg(buf);}else msg("EXPORT FAILED");
}
static void circle_dot(int x,int y,int commit){
 if(x<0||y<0||x>=size||y>=size)return;
 if(commit){artwork[y*size+x]=palette[chosen];return;}
 int px=offset_x+(x-view_x)*zoom,py=offset_y+(y-view_y)*zoom;
 if(px<SW&&py<SH&&px+zoom>0&&py+zoom>0)rect(px,py,zoom,zoom,RGB(255,230,72));
}
static void circle_outline(int cx,int cy,int r,int commit){
 int x=r,y=0,d=1-r;
 while(x>=y){
  circle_dot(cx+x,cy+y,commit);circle_dot(cx+y,cy+x,commit);
  circle_dot(cx-y,cy+x,commit);circle_dot(cx-x,cy+y,commit);
  circle_dot(cx-x,cy-y,commit);circle_dot(cx-y,cy-x,commit);
  circle_dot(cx+y,cy-x,commit);circle_dot(cx+x,cy-y,commit);
  y++;if(d<0)d+=2*y+1;else{x--;d+=2*(y-x)+1;}
 }
}
static void circle_commit(void){
 memcpy(circle_before,artwork,sizeof(artwork));
 circle_outline(circle_x,circle_y,circle_radius,1);
 memcpy(circle_after,artwork,sizeof(artwork));
 circle_undo=1;circle_redo=0;hcount=hpos=0;dirty=1;circle_active=0;msg("CIRCLE PLACED");
}
static void canvas_frame(void){
 rect(0,0,SW,SH,bg());placement();
 int cols=(SW-offset_x+zoom-1)/zoom,rows=(SH-offset_y+zoom-1)/zoom;
 if(cols>size)cols=size;if(rows>size)rows=size;
 for(int sy=0;sy<rows;sy++)for(int sx=0;sx<cols;sx++){
  int x=view_x+sx,y=view_y+sy;if(x>=size||y>=size)continue;
  int px=offset_x+sx*zoom,py=offset_y+sy*zoom;
  uint32_t c=artwork[y*size+x];if(!c)c=((x/8+y/8)&1)?RGB(160,166,179):RGB(215,220,228);
  rect(px,py,zoom,zoom,c);
 }
 int right=offset_x+(size-view_x)*zoom,bottom=offset_y+(size-view_y)*zoom;
 if(right>SW)right=SW;if(bottom>SH)bottom=SH;
 if(show_grid&&!clean_preview){
  int sub=grid_mode==1?size/2:grid_mode==2?size/4:size/8;if(sub<2)sub=2;
  for(int i=view_x;i<=size;i++){
   int x=offset_x+(i-view_x)*zoom;if(x>right)break;
   int thick=(i%sub==0)?2:1;
   if(zoom>=4||thick==2)rect(x,offset_y,thick,bottom-offset_y,(i%sub==0)?accent():RGB(113,124,143));
  }
  for(int i=view_y;i<=size;i++){
   int y=offset_y+(i-view_y)*zoom;if(y>bottom)break;
   int thick=(i%sub==0)?2:1;
   if(zoom>=4||thick==2)rect(offset_x,y,right-offset_x,thick,(i%sub==0)?accent():RGB(113,124,143));
  }
 }
 if(circle_active)circle_outline(circle_x,circle_y,circle_radius,0);
 if(!clean_preview){
  int x=offset_x+(cursor_x-view_x)*zoom,y=offset_y+(cursor_y-view_y)*zoom;
  unsigned mark=theme?RGB(255,227,91):RGB(255,72,123);
  rect(x,y,zoom,1,mark);rect(x,y+zoom-1,zoom,1,mark);rect(x,y,1,zoom,mark);rect(x+zoom-1,y,1,zoom,mark);
 }
}
static void frame_panel(int x,int y,int w,int h){aero_glass(x,y,w,h);}
static void overlay_panels(void){
 frame_panel(5,18,106,165);textat(16,27,"TOOLS",ink(),2);
 const char *items[]={"UNDO","REDO","PICK COLOR","GRID","PREVIEW","CIRCLE"};
 for(int i=0;i<6;i++){int yy=52+i*20;if(panel_focus==0&&option_item==i)aero_choice(9,yy-3,97,18);
  textat(14,yy,items[i],panel_focus==0&&option_item==i?deep():ink(),2);}
 frame_panel(346,18,129,204);textat(358,27,"PALETTES",ink(),2);
 for(int i=0;i<9;i++){int yy=48+i*17;if(panel_focus==1&&palette_cursor==i)aero_choice(351,yy-2,119,16);
  textat(356,yy,palette_names[i],panel_focus==1&&palette_cursor==i?deep():ink(),2);}
 for(int i=0;i<palette_count&&i<8;i++){round_rect(355+i*14,202,14,13,3,rgba(255,255,255,235));rect(357+i*14,204,10,9,palette[i]);}
}
static void mini_hud(void){
 if(clean_preview)return;
 char b[60];aero_glass(145,0,190,15);snprintf(b,sizeof(b),"%dX%d Z%d %d,%d",size,size,zoom,cursor_x,cursor_y);
 textat(151,3,b,ink(),2);
 rect(337,1,12,12,palette[chosen]);
}
static const char *main_items[]={"NEW PROJECT","SAVE PROJECT","LOAD PROJECT","EXPORT PNG","CONTROLS","OPTIONS","EXIT"};
static void menu_box(int x,int y,int w,int h){aero_glass(x,y,w,h);}
static void menu_frame(void){
 char b[80];int x=102,y=29,w=276,h=214;
 menu_box(x,y,w,h);
 if(menu==1){textat(119,40,"PSP PIXEL STUDIO",ink(),2);
  for(int i=0;i<7;i++){int yy=69+i*23;if(i==menu_item)aero_choice(111,yy-4,258,19);textat(121,yy,main_items[i],i==menu_item?deep():ink(),2);}}
 else if(menu==2){textat(120,42,"NEW CANVAS",ink(),2);int sizes[]={16,32,64,128};
  for(int i=0;i<4;i++){int yy=82+i*30;if(i==new_item)aero_choice(115,yy-4,247,21);snprintf(b,sizeof(b),"%d X %d",sizes[i],sizes[i]);textat(125,yy,b,i==new_item?deep():ink(),2);}}
 else if(menu==3){textat(120,42,"OPTIONS",ink(),2);
  const char *it[]={"AERO STYLE","GRID GUIDE","GRID LINES","BACK"};
  for(int i=0;i<4;i++){int yy=80+i*30;if(i==option_item)aero_choice(115,yy-4,247,21);
   if(i==0)snprintf(b,sizeof(b),"FRUTIGER AERO");
   if(i==1)snprintf(b,sizeof(b),"GRID %s",grid_mode==1?"HALF":grid_mode==2?"QUARTER":"EIGHTH");
   if(i==2)snprintf(b,sizeof(b),"LINES %s",show_grid?"ON":"OFF");
   if(i==3)snprintf(b,sizeof(b),"BACK");textat(125,yy,b,i==option_item?deep():ink(),2);}}
 else if(menu==4){textat(120,42,"UNSAVED CHANGES",ink(),2);textat(120,92,"X DISCARD",accent(),2);textat(120,124,"O CANCEL",ink(),2);}
 else if(menu==5){textat(120,42,"CONTROLS",ink(),2);
  const char *it[]={"DPAD STICK MOVE","X DRAW SQ ERASE","L R COLOR","O ZOOM","SELECT PANELS","START MENU","TRI GRID"};
  for(int i=0;i<7;i++)textat(119,69+i*22,it[i],ink(),2);}
 else if(menu==6){textat(120,42,slot_action?"LOAD SLOT":"SAVE SLOT",ink(),2);
  snprintf(b,sizeof(b),"SLOT %02d",slot+1);textat(133,102,b,accent(),3);
  textat(120,158,"LEFT RIGHT SLOT",ink(),2);textat(120,188,"X CONFIRM",ink(),2);}
 else if(menu==8){textat(120,42,"DISCARD CHANGES?",ink(),2);textat(120,94,"X LOAD",accent(),2);textat(120,126,"O CANCEL",ink(),2);}
 else if(menu==7){textat(120,42,"EXIT TO PSP?",ink(),2);textat(120,94,"X YES",accent(),2);textat(120,126,"O CANCEL",ink(),2);}
}
/* 512x512 RGB565 source, 480x272 visible image. Kept outside EBOOT for easy artwork swaps. */
static void title_load(void){
 SceUID fd=sceIoOpen(TITLE_RAW,PSP_O_RDONLY,0777);
 if(fd<0)return;
 int got=sceIoRead(fd,title_pixels,sizeof(title_pixels));sceIoClose(fd);
 if(got!=(int)sizeof(title_pixels))return;
 // RAW was packed as conventional RGB565; PSP GU_PSM_5650 expects reversed R/B bitfields.
 // Swap only the title texture's red and blue channels, preserving green and the editor palette.
 for(int i=0;i<512*512;i++){
  uint16_t p=title_pixels[i];
  title_pixels[i]=(uint16_t)((((p&0x001F)<<11)|(p&0x07E0)|((p&0xF800)>>11)));
 }
 sceKernelDcacheWritebackAll();title_ready=1;title_active=1;
}
static void title_frame(void){
 sceGuStart(GU_DIRECT,display_list);
 // The RGB565 title texture has no alpha channel; draw it without glass-panel blending.
 sceGuDisable(GU_BLEND);
 sceGuColor(0xFFFFFFFF);
 sceGuClearColor(RGB(19,143,206));sceGuClear(GU_COLOR_BUFFER_BIT);
 if(title_ready){
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexMode(GU_PSM_5650,0,0,0);
  sceGuTexImage(0,512,512,512,title_pixels);
  sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);
  sceGuTexFilter(GU_LINEAR,GU_LINEAR);
  sceGuTexWrap(GU_CLAMP,GU_CLAMP);
  TexVertex *v=(TexVertex*)sceGuGetMemory(2*sizeof(TexVertex));
  v[0].u=0;v[0].v=0;v[0].x=0;v[0].y=0;v[0].z=0;
  v[1].u=480;v[1].v=272;v[1].x=480;v[1].y=272;v[1].z=0;
  sceGuDrawArray(GU_SPRITES,GU_TEXTURE_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,v);
  sceGuDisable(GU_TEXTURE_2D);
 }
 // Restore alpha blending for the editor's translucent Aero panels.
 sceGuEnable(GU_BLEND);
 sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuSwapBuffers();
}
static void frame(void){
 sceGuStart(GU_DIRECT,display_list);sceGuClearColor(bg());sceGuClear(GU_COLOR_BUFFER_BIT);
 canvas_frame();if(show_panels&&!clean_preview&&!menu)overlay_panels();
 if(!menu&&!show_panels)mini_hud();
 if(status_frames>0){aero_glass(128,SH-17,230,17);textat(135,SH-14,status,ink(),2);}
 if(menu)menu_frame();sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuSwapBuffers();
}
static void apply_new(void){static const int sizes[]={16,32,64,128};init_canvas(sizes[new_item]);menu=0;msg("NEW PROJECT READY");}
static void open_menu_choice(void){
 switch(menu_item){
 case 0:menu=2;new_item=size==16?0:size==32?1:size==64?2:3;break;
 case 1:slot_action=0;menu=6;break;
 case 2:slot_action=1;menu=6;break;
 case 3:export_png();menu=0;break;
 case 4:menu=5;break;
 case 5:menu=3;option_item=0;break;
 case 6:menu=7;break;
 }
}
static void tool_activate(void){
 if(option_item==0)undo();else if(option_item==1)redo();else if(option_item==2)sample();
 else if(option_item==3){grid_mode=grid_mode%3+1;msg("GRID CHANGED");}
 else if(option_item==4){clean_preview=!clean_preview;show_panels=0;msg("PREVIEW TOGGLE");}
 else if(option_item==5){circle_active=1;circle_x=cursor_x;circle_y=cursor_y;circle_radius=4;show_panels=0;msg("DPAD SIZE ANALOG MOVE");}
}
int main(int argc,char **argv){
 (void)argc;(void)argv;setup_callbacks();sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
 crc_init();select_palette(0);init_canvas(64);
 sceGuInit();sceGuStart(GU_DIRECT,display_list);
 sceGuDrawBuffer(GU_PSM_8888,(void*)0,PITCH);sceGuDispBuffer(SW,SH,(void*)0x88000,PITCH);
 sceGuDepthBuffer((void*)0x110000,PITCH);sceGuOffset(2048-SW/2,2048-SH/2);
 sceGuViewport(2048,2048,SW,SH);sceGuScissor(0,0,SW,SH);sceGuEnable(GU_SCISSOR_TEST);
 sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_TEXTURE_2D);sceGuShadeModel(GU_FLAT);
 sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
 sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
 title_load();
 unsigned prev=0;int held=0;
 while(running){
  SceCtrlData pad;sceCtrlReadBufferPositive(&pad,1);unsigned now=pad.Buttons,pressed=now&~prev;
  if(title_active){
   if(pressed&PSP_CTRL_CROSS){title_active=0;prev=now;continue;}
   title_frame();prev=now;continue;
  }
  int left=(now&PSP_CTRL_LEFT)||(!(now&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT))&&pad.Lx<88);
  int right=(now&PSP_CTRL_RIGHT)||(!(now&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT))&&pad.Lx>168);
  int up=(now&PSP_CTRL_UP)||(!(now&(PSP_CTRL_UP|PSP_CTRL_DOWN))&&pad.Ly<88);
  int down=(now&PSP_CTRL_DOWN)||(!(now&(PSP_CTRL_UP|PSP_CTRL_DOWN))&&pad.Ly>168);
  int dir=left||right||up||down;int move=dir&&(held==0||(held>12&&(held%3)==0));held=dir?held+1:0;
  if(menu){
   if(pressed&PSP_CTRL_START){menu=0;}
   else if(pressed&PSP_CTRL_CIRCLE){menu=(menu==4?2:menu==1?0:1);}
   else if(menu==4){if(pressed&PSP_CTRL_CROSS)apply_new();}
   else if(menu==7){if(pressed&PSP_CTRL_CROSS)running=0;}
   else if(menu==6){if(move&&left)slot=(slot+9)%10;if(move&&right)slot=(slot+1)%10;
    if(pressed&PSP_CTRL_CROSS){if(slot_action){if(dirty){pending=1;menu=8;}else{load_art();menu=0;}}else{save_art();menu=0;}}}
   else if(menu==8){if(pressed&PSP_CTRL_CROSS){load_art();menu=0;}}
   else if(menu==5){if(pressed&PSP_CTRL_CROSS)menu=1;}
   else {int *index=menu==1?&menu_item:menu==2?&new_item:&option_item;int count=menu==1?7:4;
    if(move&&up)*index=(*index+count-1)%count;else if(move&&down)*index=(*index+1)%count;
    if(menu==3&&(pressed&(PSP_CTRL_CROSS|PSP_CTRL_LEFT|PSP_CTRL_RIGHT))){
     if(option_item==0)msg("AERO STYLE ACTIVE");
     if(option_item==1)grid_mode=grid_mode%3+1;
     if(option_item==2)show_grid=!show_grid;
     if(option_item==3)menu=1;
    }else if(pressed&PSP_CTRL_CROSS){if(menu==1)open_menu_choice();else if(menu==2){if(dirty)menu=4;else apply_new();}}
   }
  }else if(pressed&PSP_CTRL_START){menu=1;menu_item=0;clean_preview=0;}
  else if(pressed&PSP_CTRL_SELECT){show_panels=!show_panels;clean_preview=0;}
  else if(circle_active){
   if(pressed&PSP_CTRL_CROSS)circle_commit();
   else if(pressed&PSP_CTRL_SQUARE){circle_active=0;msg("CIRCLE CANCELLED");}
   else{
    if(move&&left&&circle_radius>1)circle_radius--;
    if(move&&right&&circle_radius<size-1)circle_radius++;
    if(move&&up&&circle_y>0)circle_y--;
    if(move&&down&&circle_y<size-1)circle_y++;
    if(pad.Lx<88&&circle_x>0)circle_x--;else if(pad.Lx>168&&circle_x<size-1)circle_x++;
    if(pad.Ly<88&&circle_y>0)circle_y--;else if(pad.Ly>168&&circle_y<size-1)circle_y++;
    cursor_x=circle_x;cursor_y=circle_y;adjust_view();
   }
  }else if(show_panels){
   if(pressed&PSP_CTRL_LTRIGGER)panel_focus=0;if(pressed&PSP_CTRL_RTRIGGER)panel_focus=1;
   if(move&&up){if(panel_focus==0)option_item=(option_item+5)%6;else palette_cursor=(palette_cursor+8)%9;}
   if(move&&down){if(panel_focus==0)option_item=(option_item+1)%6;else palette_cursor=(palette_cursor+1)%9;}
   if(pressed&PSP_CTRL_CROSS){if(panel_focus==0)tool_activate();else{select_palette(palette_cursor);msg(palette_names[palette_id]);}}
   if(pressed&PSP_CTRL_CIRCLE){show_panels=0;next_zoom();}
  }else{
   if(move){if(left&&cursor_x>0)cursor_x--;else if(right&&cursor_x<size-1)cursor_x++;
    if(up&&cursor_y>0)cursor_y--;else if(down&&cursor_y<size-1)cursor_y++;adjust_view();}
   if(pressed&PSP_CTRL_LTRIGGER)chosen=(chosen+palette_count-1)%palette_count;
   if(pressed&PSP_CTRL_RTRIGGER)chosen=(chosen+1)%palette_count;
   if(pressed&PSP_CTRL_CIRCLE)next_zoom();
   if(pressed&PSP_CTRL_TRIANGLE){show_grid=!show_grid;msg(show_grid?"GRID ON":"GRID OFF");}
   if((now&PSP_CTRL_CROSS)&&((pressed&PSP_CTRL_CROSS)||move))edit_pixel(palette[chosen]);
   if((now&PSP_CTRL_SQUARE)&&((pressed&PSP_CTRL_SQUARE)||move))edit_pixel(TRANSPARENT);
  }
  if(status_frames>0)status_frames--;frame();prev=now;
 }
 sceGuTerm();sceKernelExitGame();return 0;
}
