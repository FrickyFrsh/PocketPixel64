#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

PSP_MODULE_INFO("PSP Pixel Studio",0,2,0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#define SW 480
#define SH 272
#define PITCH 512
#define MAXC 128
#define TRANSPARENT 0u
#define RGB(r,g,b) (0xff000000u | ((unsigned)(b)<<16) | ((unsigned)(g)<<8) | (unsigned)(r))
#define SAVE_DIR "ms0:/PSP/PIXELSTUDIO"

typedef struct { unsigned color; short x,y,z; } Vertex;
static unsigned int __attribute__((aligned(16))) display_list[262144];
static uint32_t artwork[MAXC*MAXC];
static uint32_t palette[64];
static int palette_id=0,palette_count=64,chosen=0;
static int size=64,cursor_x=0,cursor_y=0,zoom=3,view_x=0,view_y=0;
static int show_panels=0,grid_mode=1,theme=0,dirty=0;
static int menu=0,menu_item=0,option_item=0,new_item=2,confirm_new=0;
static int running=1,status_frames=0;
static char status[80]="";
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
static unsigned bg(void){return theme?RGB(13,16,31):RGB(24,65,84);}
static unsigned panel(void){return theme?RGB(33,30,65):RGB(40,113,132);}
static unsigned accent(void){return theme?RGB(255,90,188):RGB(123,236,225);}
static unsigned ink(void){return RGB(242,249,255);}
static void init_canvas(int n){size=n;memset(artwork,0,sizeof(artwork));cursor_x=cursor_y=view_x=view_y=0;zoom=n==16?12:n==32?6:n==64?3:2;dirty=0;}
static void adjust_view(void){
 int cols=SW/zoom,rows=SH/zoom;
 int mx=cols>6?2:0,my=rows>6?2:0;
 if(cursor_x<view_x+mx)view_x=cursor_x-mx;
 if(cursor_y<view_y+my)view_y=cursor_y-my;
 if(cursor_x>=view_x+cols-mx)view_x=cursor_x-cols+mx+1;
 if(cursor_y>=view_y+rows-my)view_y=cursor_y-rows+my+1;
 int maxx=size-cols,maxy=size-rows;if(maxx<0)maxx=0;if(maxy<0)maxy=0;
 if(view_x<0)view_x=0;if(view_y<0)view_y=0;
 if(view_x>maxx)view_x=maxx;if(view_y>maxy)view_y=maxy;
}
static void save_path(char *s,int n){snprintf(s,n,SAVE_DIR "/art_%d.pps",size);}
static void save_art(void){
 sceIoMkdir(SAVE_DIR,0777);
 char path[96];save_path(path,sizeof(path));SceUID f=sceIoOpen(path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
 if(f<0){msg("SAVE FAILED");return;}
 unsigned char hdr[8]={'P','P','S','2',(unsigned char)size,0,0,0};
 int a=sceIoWrite(f,hdr,8),b=sceIoWrite(f,artwork,size*size*4);sceIoClose(f);
 if(a==8&&b==size*size*4){dirty=0;msg("PROJECT SAVED");}else msg("SAVE FAILED");
}
static void load_art(void){
 char path[96];save_path(path,sizeof(path));SceUID f=sceIoOpen(path,PSP_O_RDONLY,0);
 if(f<0){msg("NO SAVE FOR THIS SIZE");return;}
 unsigned char hdr[8];int a=sceIoRead(f,hdr,8);
 if(a!=8||memcmp(hdr,"PPS2",4)||hdr[4]!=size){sceIoClose(f);msg("INVALID PROJECT");return;}
 static uint32_t temp[MAXC*MAXC];int b=sceIoRead(f,temp,size*size*4);sceIoClose(f);
 if(b==size*size*4){memcpy(artwork,temp,size*size*4);dirty=0;msg("PROJECT LOADED");}else msg("INVALID PROJECT");
}
static void canvas_frame(void){
 rect(0,0,SW,SH,bg());
 int cols=(SW+zoom-1)/zoom,rows=(SH+zoom-1)/zoom;
 for(int sy=0;sy<rows;sy++)for(int sx=0;sx<cols;sx++){
  int x=view_x+sx,y=view_y+sy,px=sx*zoom,py=sy*zoom;
  if(x>=size||y>=size){rect(px,py,zoom,zoom,bg());continue;}
  unsigned c=artwork[y*size+x];
  if(c==TRANSPARENT)c=((x/8+y/8)&1)?RGB(197,201,213):RGB(230,232,239);
  rect(px,py,zoom,zoom,c);
 }
 if(grid_mode){
  int sub=grid_mode==1?size/2:grid_mode==2?size/4: size/8;
  if(sub<2)sub=2;
  for(int i=0;i<=size;i++){
   int x=(i-view_x)*zoom,y=(i-view_y)*zoom;
   int thick=(i%sub==0)?2:1;
   unsigned c=(i%sub==0)?(theme?RGB(232,111,255):RGB(32,138,159)):RGB(165,171,184);
   if(zoom>=3||thick==2){if(x>=0&&x<SW)rect(x,0,thick,SH,c);if(y>=0&&y<SH)rect(0,y,SW,thick,c);}
  }
 }
 int px=(cursor_x-view_x)*zoom,py=(cursor_y-view_y)*zoom;
 unsigned mark=theme?RGB(255,233,99):RGB(255,58,104);
 rect(px,py,zoom,1,mark);rect(px,py+zoom-1,zoom,1,mark);
 rect(px,py,1,zoom,mark);rect(px+zoom-1,py,1,zoom,mark);
}
static void overlay_panels(void){
 rect(0,0,115,SH,panel());rect(0,0,115,3,accent());
 textat(8,10,"PSP PIXEL",ink(),2);textat(8,26,"STUDIO V2",ink(),2);
 rect(8,47,99,1,accent());
 textat(8,57,"DRAW X",ink(),2);textat(8,73,"ERASE SQ",ink(),2);
 textat(8,89,"MOVE DPAD",ink(),2);textat(8,105,"OR ANALOG",ink(),2);
 textat(8,128,"GRID TRI",ink(),2);textat(8,144,"PAL CIRCLE",ink(),2);
 textat(8,160,"COLOR L/R",ink(),2);
 textat(8,186,"START MENU",ink(),2);textat(8,202,"SELECT HIDE",ink(),2);
 rect(352,0,128,SH,panel());rect(352,0,128,3,accent());
 textat(361,10,"PALETTE",ink(),2);
 textat(361,27,palette_names[palette_id],accent(),2);
 for(int i=0;i<palette_count;i++){
  int x=361+(i%8)*14,y=52+(i/8)*15;
  if(i==chosen)rect(x-2,y-2,16,16,ink());
  rect(x,y,12,12,palette[i]);
 }
 rect(361,184,108,15,palette[chosen]);
 char buf[48];snprintf(buf,sizeof(buf),"%d X %d",size,size);textat(361,210,buf,ink(),2);
 snprintf(buf,sizeof(buf),"ZOOM %dX",zoom);textat(361,227,buf,ink(),2);
 textat(361,246,theme?"CYBER":"AERO",accent(),2);
}
static const char *menu_names[]={"NEW PROJECT","SAVE PROJECT","LOAD PROJECT","OPTIONS","EXIT TO PSP"};
static void menu_box(int x,int y,int w,int h){rect(x+3,y+3,w,h,RGB(8,10,20));rect(x,y,w,h,panel());rect(x,y,w,3,accent());}
static void menu_frame(void){
 if(menu==1){menu_box(124,45,232,185);textat(142,59,"PSP PIXEL STUDIO",ink(),2);
 for(int i=0;i<5;i++){int yy=91+i*25;if(i==menu_item)rect(136,yy-5,208,21,accent());textat(145,yy,menu_names[i],i==menu_item?bg():ink(),2);}}
 else if(menu==2){menu_box(136,56,208,166);textat(154,69,"NEW CANVAS",ink(),2);
 const int sizes[]={16,32,64,128};char buf[32];for(int i=0;i<4;i++){
  int yy=102+i*25;if(i==new_item)rect(147,yy-5,185,21,accent());
  snprintf(buf,sizeof(buf),"%d X %d",sizes[i],sizes[i]);textat(157,yy,buf,i==new_item?bg():ink(),2);}}
 else if(menu==3){menu_box(119,49,242,183);textat(137,62,"OPTIONS",ink(),2);
 char buf[48];for(int i=0;i<4;i++){
  int yy=95+i*29;if(i==option_item)rect(130,yy-5,218,22,accent());
  if(i==0)snprintf(buf,sizeof(buf),"THEME %s",theme?"CYBER":"AERO");
  if(i==1)snprintf(buf,sizeof(buf),"ZOOM %dX",zoom);
  if(i==2)snprintf(buf,sizeof(buf),"GRID %s",grid_mode==0?"OFF":grid_mode==1?"HALF":grid_mode==2?"QUARTER":"EIGHTH");
  if(i==3)snprintf(buf,sizeof(buf),"BACK");
  textat(140,yy,buf,i==option_item?bg():ink(),2);}}
 else if(menu==4){menu_box(91,79,298,116);textat(109,92,"DISCARD UNSAVED ART?",ink(),2);
 textat(109,121,"X YES",accent(),2);textat(109,147,"O CANCEL",ink(),2);}
}
static void frame(void){
 sceGuStart(GU_DIRECT,display_list);sceGuClearColor(bg());sceGuClear(GU_COLOR_BUFFER_BIT);
 canvas_frame();if(show_panels)overlay_panels();
 if(status_frames>0){rect(124,0,228,17,panel());textat(132,4,status,ink(),2);}
 if(menu)menu_frame();
 sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuSwapBuffers();
}
static void apply_new(void){static const int sizes[]={16,32,64,128};init_canvas(sizes[new_item]);menu=0;msg("NEW PROJECT READY");}
static void select_menu(void){
 if(menu_item==0){menu=2;new_item=size==16?0:size==32?1:size==64?2:3;}
 else if(menu_item==1){save_art();menu=0;}
 else if(menu_item==2){load_art();menu=0;}
 else if(menu_item==3){menu=3;option_item=0;}
 else if(menu_item==4){running=0;}
}
int main(int argc,char **argv){
 (void)argc;(void)argv;setup_callbacks();sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
 select_palette(0);init_canvas(64);
 sceGuInit();sceGuStart(GU_DIRECT,display_list);
 sceGuDrawBuffer(GU_PSM_8888,(void*)0,PITCH);sceGuDispBuffer(SW,SH,(void*)0x88000,PITCH);
 sceGuDepthBuffer((void*)0x110000,PITCH);sceGuOffset(2048-SW/2,2048-SH/2);
 sceGuViewport(2048,2048,SW,SH);sceGuScissor(0,0,SW,SH);sceGuEnable(GU_SCISSOR_TEST);
 sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_TEXTURE_2D);sceGuShadeModel(GU_FLAT);
 sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
 unsigned prev=0;int held=0;
 while(running){
  SceCtrlData pad;sceCtrlReadBufferPositive(&pad,1);
  unsigned now=pad.Buttons,pressed=now&~prev;
  int left=(now&PSP_CTRL_LEFT)||(!(now&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT))&&pad.Lx<88);
  int right=(now&PSP_CTRL_RIGHT)||(!(now&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT))&&pad.Lx>168);
  int up=(now&PSP_CTRL_UP)||(!(now&(PSP_CTRL_UP|PSP_CTRL_DOWN))&&pad.Ly<88);
  int down=(now&PSP_CTRL_DOWN)||(!(now&(PSP_CTRL_UP|PSP_CTRL_DOWN))&&pad.Ly>168);
  int dir=left||right||up||down;
  int move=dir&&(held==0||(held>12&&(held%3)==0));
  held=dir?held+1:0;
  if(menu){
   if(pressed&PSP_CTRL_START){menu=0;}
   else if(pressed&PSP_CTRL_CIRCLE){menu=(menu==4?2:menu==1?0:1);}
   else if(menu==4){if(pressed&PSP_CTRL_CROSS)apply_new();}
   else {
    int *index=menu==1?&menu_item:menu==2?&new_item:&option_item;
    int count=menu==1?5:4;
    if(move&&up)*index=(*index+count-1)%count;
    else if(move&&down)*index=(*index+1)%count;
    if(menu==3&&(pressed&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT|PSP_CTRL_CROSS))){
     if(option_item==0)theme=!theme;
     if(option_item==1){int step=(pressed&PSP_CTRL_LEFT)?-1:1;zoom+=step;if(zoom<1)zoom=1;if(zoom>16)zoom=16;adjust_view();}
     if(option_item==2)grid_mode=(grid_mode+1)%4;
     if(option_item==3&&(pressed&PSP_CTRL_CROSS))menu=1;
    }else if(pressed&PSP_CTRL_CROSS){
     if(menu==1)select_menu();else if(menu==2){if(dirty)menu=4;else apply_new();}
    }
   }
  }else{
   if(pressed&PSP_CTRL_START){menu=1;menu_item=0;}
   else if(pressed&PSP_CTRL_SELECT)show_panels=!show_panels;
   else{
    if(move){
     if(left&&cursor_x>0)cursor_x--;else if(right&&cursor_x<size-1)cursor_x++;
     if(up&&cursor_y>0)cursor_y--;else if(down&&cursor_y<size-1)cursor_y++;
     adjust_view();
    }
    if(pressed&PSP_CTRL_LTRIGGER)chosen=(chosen+palette_count-1)%palette_count;
    if(pressed&PSP_CTRL_RTRIGGER)chosen=(chosen+1)%palette_count;
    if(pressed&PSP_CTRL_CIRCLE){select_palette((palette_id+1)%9);msg(palette_names[palette_id]);}
    if(pressed&PSP_CTRL_TRIANGLE)grid_mode=(grid_mode+1)%4;
    if((now&PSP_CTRL_CROSS)&&((pressed&PSP_CTRL_CROSS)||move)){artwork[cursor_y*size+cursor_x]=palette[chosen];dirty=1;}
    if((now&PSP_CTRL_SQUARE)&&((pressed&PSP_CTRL_SQUARE)||move)){artwork[cursor_y*size+cursor_x]=TRANSPARENT;dirty=1;}
   }
  }
  if(status_frames>0)status_frames--;frame();prev=now;
 }
 sceGuTerm();sceKernelExitGame();return 0;
}
