#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

PSP_MODULE_INFO("PocketPixel64", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define W 480
#define H 272
#define PITCH 512
#define CX 104
#define CY 32
#define CELL 3
#define PW 64
#define SAVE_PATH "ms0:/PSP/GAME/POCKETPIXEL64/art.p64"
#define COLOR(r,g,b) (0xff000000u | ((unsigned)(b)<<16) | ((unsigned)(g)<<8) | (unsigned)(r))

typedef struct { unsigned color; short x,y,z; } Vertex;
static unsigned int __attribute__((aligned(16))) display_list[262144];
static unsigned char pixels[PW*PW];
static unsigned palette[64];
static int palette_id=0, palette_count=64;
static int cursor_x=0,cursor_y=0,chosen=0,show_grid=1;
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
static int hexval(char c) { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return 0; }
static void select_palette(int id) {
 palette_id=id;palette_count=0;
 if(id==0) { for(int i=0;i<64;i++)palette[i]=COLOR(((i>>4)&3)*85,((i>>2)&3)*85,(i&3)*85);palette_count=64; }
 else { const char *s=palette_hex[id];while(*s && palette_count<64) {
  while(*s==' ')s++;if(!*s)break;
  unsigned v=0;for(int j=0;j<6;j++)v=(v<<4)|hexval(*s++);
  palette[palette_count++]=COLOR((v>>16)&255,(v>>8)&255,v&255);
 } }
 if(chosen>=palette_count)chosen=0;
}

static int status_frames=0;
static const char *status_text="";
static int running=1;

static int exit_callback(int arg1,int arg2,void *common) { (void)arg1;(void)arg2;(void)common; running=0; return 0; }
static int callback_thread(SceSize args,void *argp) {
    (void)args;(void)argp;
    int cb=sceKernelCreateCallback("Exit Callback",exit_callback,NULL);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int th=sceKernelCreateThread("callback_thread",callback_thread,0x11,0xFA0,0,0);
    if(th>=0) sceKernelStartThread(th,0,0);
}
static void message(const char *s) { status_text=s; status_frames=180; }
static void save_art(void) {
    SceUID fd=sceIoOpen(SAVE_PATH,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
    if(fd<0) { message("SAVE FAILED - CHECK FOLDER"); return; }
    const char hdr[4]={'P','P','6','5'};
    int a=sceIoWrite(fd,hdr,4);
    int c=sceIoWrite(fd,&palette_id,1);
    int b=sceIoWrite(fd,pixels,sizeof(pixels));
    sceIoClose(fd);
    message(a==4 && c==1 && b==(int)sizeof(pixels) ? "SAVED: art.p64" : "SAVE FAILED");
}
static void load_art(void) {
    SceUID fd=sceIoOpen(SAVE_PATH,PSP_O_RDONLY,0);
    if(fd<0) { message("NO SAVED ART FOUND"); return; }
    char hdr[4]; unsigned char temp[sizeof(pixels)];
    int a=sceIoRead(fd,hdr,4);
    unsigned char pal=0;
    int is_new=(a==4 && memcmp(hdr,"PP65",4)==0);
    if(is_new)sceIoRead(fd,&pal,1);
    int b=sceIoRead(fd,temp,sizeof(temp));
    sceIoClose(fd);
    if(a==4 && (memcmp(hdr,"PP64",4)==0 || is_new) && b==(int)sizeof(temp)) {
        if(is_new && pal<9)select_palette(pal);
        memcpy(pixels,temp,sizeof(pixels)); message("ART LOADED");
    } else message("INVALID ART FILE");
}
static void rect(int x,int y,int w,int h,unsigned color) {
    Vertex *v=(Vertex*)sceGuGetMemory(2*sizeof(Vertex));
    v[0].color=color;v[0].x=x;v[0].y=y;v[0].z=0;
    v[1].color=color;v[1].x=x+w;v[1].y=y+h;v[1].z=0;
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,v);
}
static void frame(void) {
    sceGuStart(GU_DIRECT,display_list);
    sceGuClearColor(COLOR(17,20,30));
    sceGuClear(GU_COLOR_BUFFER_BIT);
    rect(CX-2,CY-2,196,196,COLOR(100,111,137));
    rect(CX,CY,192,192,COLOR(250,250,250));
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
        unsigned char c=pixels[y*64+x];
        if(c!=255 && c<palette_count) rect(CX+x*CELL,CY+y*CELL,CELL,CELL,palette[c]);
    }
    if(show_grid) {
        unsigned gc=COLOR(202,205,214);
        for(int i=0;i<=64;i++) {
            rect(CX+i*CELL,CY,1,192,gc);
            rect(CX,CY+i*CELL,192,1,gc);
        }
    }
    int px=CX+cursor_x*CELL,py=CY+cursor_y*CELL;
    unsigned outline=COLOR(255,80,110);
    rect(px-1,py-1,5,1,outline);rect(px-1,py+3,5,1,outline);
    rect(px-1,py,1,3,outline);rect(px+3,py,1,3,outline);
    for(int i=0;i<palette_count;i++) {
        int x=333+(i%8)*15,y=43+(i/8)*15;
        if(i==chosen) rect(x-2,y-2,16,16,COLOR(255,255,255));
        rect(x,y,12,12,palette[i]);
    }
    rect(333,174,117,3,palette[chosen]);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    select_palette(0);memset(pixels,255,sizeof(pixels));
    sceGuInit();
    sceGuStart(GU_DIRECT,display_list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0, PITCH);
    sceGuDispBuffer(W,H,(void*)0x88000,PITCH);
    sceGuDepthBuffer((void*)0x110000,PITCH);
    sceGuOffset(2048-(W/2),2048-(H/2));
    sceGuViewport(2048,2048,W,H);
    sceGuScissor(0,0,W,H);sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);
    sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
    unsigned prev=0;int held=0;
    while(running) {
        SceCtrlData pad;sceCtrlReadBufferPositive(&pad,1);
        unsigned now=pad.Buttons,pressed=now&~prev;
        // Analog stick is centered near 128. Deadzone prevents cursor drift.
        // D-pad takes priority on each axis when both are used.
        int left=(now&PSP_CTRL_LEFT) || (!(now&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT)) && pad.Lx<88);
        int right=(now&PSP_CTRL_RIGHT) || (!(now&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT)) && pad.Lx>168);
        int up=(now&PSP_CTRL_UP) || (!(now&(PSP_CTRL_UP|PSP_CTRL_DOWN)) && pad.Ly<88);
        int down=(now&PSP_CTRL_DOWN) || (!(now&(PSP_CTRL_UP|PSP_CTRL_DOWN)) && pad.Ly>168);
        int dir=left||right||up||down;
        int move=dir && (held==0 || (held>12 && (held%3)==0));
        if(move) {
            if(left) cursor_x=(cursor_x+63)%64;
            else if(right) cursor_x=(cursor_x+1)%64;
            if(up) cursor_y=(cursor_y+63)%64;
            else if(down) cursor_y=(cursor_y+1)%64;
        }
        held=dir?held+1:0;
        if(pressed&PSP_CTRL_LTRIGGER) chosen=(chosen+palette_count-1)%palette_count;
        if(pressed&PSP_CTRL_RTRIGGER) chosen=(chosen+1)%palette_count;
        if(pressed&PSP_CTRL_CIRCLE) { select_palette((palette_id+1)%9); message(palette_names[palette_id]); }
        if(pressed&PSP_CTRL_TRIANGLE) show_grid=!show_grid;
        if(pressed&PSP_CTRL_START) save_art();
        if(pressed&PSP_CTRL_SELECT) load_art();
        if((now&PSP_CTRL_CROSS) && ((pressed&PSP_CTRL_CROSS)||move)) pixels[cursor_y*64+cursor_x]=(unsigned char)chosen;
        if((now&PSP_CTRL_SQUARE) && ((pressed&PSP_CTRL_SQUARE)||move)) pixels[cursor_y*64+cursor_x]=255;
        if(status_frames>0) status_frames--;
        frame();prev=now;
    }
    sceGuTerm();sceKernelExitGame();return 0;
}
