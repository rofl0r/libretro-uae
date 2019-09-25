#include "libretro.h"
#include "libretro-glue.h"
#include "keyboard.h"
#include "libretro-keymap.h"
#include "graph.h"
#include "vkbd.h"
#include "libretro-mapper.h"

#include "uae_types.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "inputdevice.h"

#include "gui.h"
#include "xwin.h"
#include "disk.h"

#ifdef __CELLOS_LV2__
#include "sys/sys_time.h"
#include "sys/timer.h"
#include <sys/time.h>
#include <time.h>
#define usleep  sys_timer_usleep

void gettimeofday (struct timeval *tv, void *blah)
{
   int64_t time = sys_time_get_system_time();

   tv->tv_sec  = time / 1000000;
   tv->tv_usec = time - (tv->tv_sec * 1000000);  // implicit rounding will take care of this for us
}

#else
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif

unsigned short int bmp[1024*1024];
unsigned short int savebmp[1024*1024];

int NPAGE=-1,PAS=4;
int SHIFTON=-1,ALTON=-1;
int MOUSEMODE=-1,SHOWKEY=-1,SHOWKEYPOS=-1,SHOWKEYTRANS=-1,STATUSON=-1,LEDON=-1;

char RPATH[512];

int analog_left[2];
int analog_right[2];
extern int analog_deadzone;
extern unsigned int analog_sensitivity;
extern unsigned int opt_dpadmouse_speed;
int fmousex,fmousey; // emu mouse
int slowdown=0;
extern int pix_bytes;
extern bool fake_ntsc;
extern bool real_ntsc;

int vkflag[7]={0};
static int jflag[4][16]={0};
static int kjflag[2][16]={0};
static int mflag[16]={0};
static int jbt[24]={0};
static int kbt[16]={0};

extern void reset_drawing(void);
extern void retro_key_up(int);
extern void retro_key_down(int);
extern void retro_mouse(int, int);
extern void retro_mouse_but0(int);
extern void retro_mouse_but1(int);
extern unsigned int uae_devices[4];
extern int mapper_keys[31];
extern int video_config;
extern int video_config_aspect;
extern int zoom_mode_id;
extern bool request_update_av_info;
extern bool opt_enhanced_statusbar;
extern int opt_statusbar_position;
extern unsigned int opt_analogmouse;
extern unsigned int opt_keyrahkeypad;
int turbo_fire_button=-1;
unsigned int turbo_pulse=2;
unsigned int turbo_state[5]={0};
unsigned int turbo_toggle[5]={0};

enum EMU_FUNCTIONS {
   EMU_VKBD = 0,
   EMU_STATUSBAR,
   EMU_MOUSE_TOGGLE,
   EMU_MOUSE_SPEED_DOWN,
   EMU_MOUSE_SPEED_UP,
   EMU_RESET,
   EMU_ASPECT_RATIO_TOGGLE,
   EMU_ZOOM_MODE_TOGGLE,
   EMU_FUNCTION_COUNT
};

/* VKBD_MIN_HOLDING_TIME: Hold a direction longer than this and automatic movement sets in */
/* VKBD_MOVE_DELAY: Delay between automatic movement from button to button */
#define VKBD_MIN_HOLDING_TIME 200
#define VKBD_MOVE_DELAY 50
bool let_go_of_direction = true;
long last_move_time = 0;
long last_press_time = 0;

void emu_function(int function) {
   switch (function)
   {
      case EMU_VKBD:
         SHOWKEY=-SHOWKEY;
         Screen_SetFullUpdate();
         break;
      case EMU_STATUSBAR:
         STATUSON=-STATUSON;
         LEDON=-LEDON;
         Screen_SetFullUpdate();
         break;
      case EMU_MOUSE_TOGGLE:
         MOUSEMODE=-MOUSEMODE;
         break;
      case EMU_MOUSE_SPEED_DOWN:
         switch(PAS)
         {
            case 4:
               PAS=8;
               break;
            case 6:
               PAS=10;
               break;
            case 8:
               PAS=4;
               break;
            case 10:
               PAS=6;
               break;
         }
         break;
      case EMU_MOUSE_SPEED_UP:
         PAS=opt_dpadmouse_speed;
         break;
      case EMU_RESET:
         uae_reset(0, 1); /* hardreset, keyboardreset */
         fake_ntsc=false;
         break;
      case EMU_ASPECT_RATIO_TOGGLE:
         if(real_ntsc)
            break;
         if(video_config_aspect==0)
            video_config_aspect=(video_config & 0x02) ? 1 : 2;
         else if(video_config_aspect==1)
            video_config_aspect=2;
         else if(video_config_aspect==2)
            video_config_aspect=1;
         request_update_av_info=true;
         break;
      case EMU_ZOOM_MODE_TOGGLE:
         zoom_mode_id++;
         if(zoom_mode_id>5)zoom_mode_id=0;
         request_update_av_info=true;
         break;
   }
}

int STAT_BASEY;
int STAT_DECX=4;
int FONT_WIDTH=1;
int FONT_HEIGHT=1;
int BOX_PADDING=2;
int BOX_Y;
int BOX_WIDTH;
int BOX_HEIGHT=11;

extern char key_state[512];
extern char key_state2[512];

static retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

#define MDEBUG
#ifdef MDEBUG
#define mprintf printf
#else
#define mprintf(...) 
#endif

#ifdef WIIU
#include <features_cpu.h>
#endif

/* in milliseconds */
long GetTicks(void)
{
#ifdef _ANDROID_
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (now.tv_sec*1000000 + now.tv_nsec/1000)/1000;
#elif defined(WIIU)
   return (cpu_features_get_time_usec())/1000;
#else
   struct timeval tv;
   gettimeofday (&tv, NULL);
   return (tv.tv_sec*1000000 + tv.tv_usec)/1000;
#endif
} 

char* joystick_value_human(int val[16])
{
    static char str[4];
    sprintf(str, "%3s", "   ");

    if (val[RETRO_DEVICE_ID_JOYPAD_UP])
        str[1] = '^';

    if (val[RETRO_DEVICE_ID_JOYPAD_DOWN])
        str[1] = 'v';

    if (val[RETRO_DEVICE_ID_JOYPAD_LEFT])
        str[0] = '<';

    if (val[RETRO_DEVICE_ID_JOYPAD_RIGHT])
        str[2] = '>';

    if (val[RETRO_DEVICE_ID_JOYPAD_B])
        str[1] = '1';

    if (val[RETRO_DEVICE_ID_JOYPAD_A])
        str[1] = '2';

    if (val[RETRO_DEVICE_ID_JOYPAD_B] && val[RETRO_DEVICE_ID_JOYPAD_A])
        str[1] = '3';

    str[1] = (val[RETRO_DEVICE_ID_JOYPAD_B] || val[RETRO_DEVICE_ID_JOYPAD_A]) ? (str[1] | 0x80) : str[1];
    return str;
}

void Print_Status(void)
{
   if (!opt_enhanced_statusbar)
      return;

   // Statusbar location
   if (video_config & 0x04) // PUAE_VIDEO_HIRES
   {
      if (opt_statusbar_position < 0)
         if (opt_statusbar_position == -1)
             STAT_BASEY=2;
         else
             STAT_BASEY=-opt_statusbar_position+1+BOX_PADDING;
      else
         STAT_BASEY=gfxvidinfo.outheight-BOX_HEIGHT-opt_statusbar_position+2;

      BOX_WIDTH=retrow-146;
   }
   else // PUAE_VIDEO_LORES
   {
      if (opt_statusbar_position < 0)
         if (opt_statusbar_position == -1)
             STAT_BASEY=0;
         else
             STAT_BASEY=-opt_statusbar_position-BOX_HEIGHT+3;
      else
         STAT_BASEY=gfxvidinfo.outheight-opt_statusbar_position+2;

      BOX_WIDTH=retrow;
   }

   BOX_Y=STAT_BASEY-BOX_PADDING;

   // Joy port indicators
   char JOYPORT1[10];
   char JOYPORT2[10];
   char JOYPORT3[10];
   char JOYPORT4[10];

   // Regular joyflags
   sprintf(JOYPORT1, "J1%3s", joystick_value_human(jflag[0]));
   sprintf(JOYPORT2, "J2%3s", joystick_value_human(jflag[1]));
   sprintf(JOYPORT3, "J3%3s", joystick_value_human(jflag[2]));
   sprintf(JOYPORT4, "J4%3s", joystick_value_human(jflag[3]));

   // Mouse flag
   if (strcmp(JOYPORT2, "J2   ") == 0)
      sprintf(JOYPORT2, "J2%3s", joystick_value_human(mflag));

   // Keyrah joyflags
   if (opt_keyrahkeypad)
   {
      if (strcmp(JOYPORT1, "J1   ") == 0)
         sprintf(JOYPORT1, "J1%3s", joystick_value_human(kjflag[0]));
      if (strcmp(JOYPORT2, "J2   ") == 0)
         sprintf(JOYPORT2, "J2%3s", joystick_value_human(kjflag[1]));
   }

   // Emulated mouse speed
   char PASSTR[2];
   switch (PAS)
   {
      case 4:
         PASSTR[0]='S';
         break;
      case 6:
         PASSTR[0]='M';
         break;
      case 8:
         PASSTR[0]='F';
         break;
      case 10:
         PASSTR[0]='V';
         break;
   }

   // Zoom mode
   char ZOOM_MODE[10];
   switch (zoom_mode_id)
   {
      default:
      case 0:
         sprintf(ZOOM_MODE, "%s", "None");
         break;
      case 1:
         sprintf(ZOOM_MODE, "%s", "Small");
         break;
      case 2:
         sprintf(ZOOM_MODE, "%s", "Medium");
         break;
      case 3:
         sprintf(ZOOM_MODE, "%s", "Large");
         break;
      case 4:
         sprintf(ZOOM_MODE, "%s", "Larger");
         break;
      case 5:
         sprintf(ZOOM_MODE, "%s", "Maximum");
         break;
   }

   // Statusbar output
   if (pix_bytes == 4)
   {
      DrawFBoxBmp32((uint32_t *)bmp,0,BOX_Y,BOX_WIDTH,BOX_HEIGHT,RGB888(0,0,0));

      Draw_text32((uint32_t *)bmp,STAT_DECX+0,STAT_BASEY,0xffffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT1);
      Draw_text32((uint32_t *)bmp,STAT_DECX+40,STAT_BASEY,0xffffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT2);
      Draw_text32((uint32_t *)bmp,STAT_DECX+80,STAT_BASEY,0xffffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT3);
      Draw_text32((uint32_t *)bmp,STAT_DECX+120,STAT_BASEY,0xffffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT4);

      Draw_text32((uint32_t *)bmp,STAT_DECX+160,STAT_BASEY,0xffffff,0x0000,FONT_WIDTH,FONT_HEIGHT,20,((MOUSEMODE==-1) ? "Joystick" : "Mouse:%s "), PASSTR);
      Draw_text32((uint32_t *)bmp,STAT_DECX+240,STAT_BASEY,0xffffff,0x0000,FONT_WIDTH,FONT_HEIGHT,20,"Zoom:%s", ZOOM_MODE);
   }
   else
   {
      DrawFBoxBmp(bmp,0,BOX_Y,BOX_WIDTH,BOX_HEIGHT,RGB565(0,0,0));

      Draw_text(bmp,STAT_DECX+0,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT1);
      Draw_text(bmp,STAT_DECX+40,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT2);
      Draw_text(bmp,STAT_DECX+80,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT3);
      Draw_text(bmp,STAT_DECX+120,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,JOYPORT4);

      Draw_text(bmp,STAT_DECX+160,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,20,((MOUSEMODE==-1) ? "Joystick" : "Mouse:%s "), PASSTR);
      Draw_text(bmp,STAT_DECX+240,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,20,"Zoom:%s", ZOOM_MODE);
   }
}

void Screen_SetFullUpdate(void)
{
   reset_drawing();
}

void ProcessKeyrah()
{
   /*** Port 2 ***/

   /* Up / Down */
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP8)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP2))
   {
      setjoystickstate(0, 1, -1, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_UP]=1;
   }
   else
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP2)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP8))
   {
      setjoystickstate(0, 1, 1, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_DOWN]=1;
   }

   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP8)
   && kjflag[0][RETRO_DEVICE_ID_JOYPAD_UP]==1)
   {
      setjoystickstate(0, 1, 0, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_UP]=0;
   }
   else
   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP2)
   && kjflag[0][RETRO_DEVICE_ID_JOYPAD_DOWN]==1)
   {
      setjoystickstate(0, 1, 0, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_DOWN]=0;
   }

   /* Left / Right */
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP4)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP6))
   {
      setjoystickstate(0, 0, -1, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_LEFT]=1;
   }
   else
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP6)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP4))
   {
      setjoystickstate(0, 0, 1, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_RIGHT]=1;
   }

   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP4)
   && kjflag[0][RETRO_DEVICE_ID_JOYPAD_LEFT]==1)
   {
      setjoystickstate(0, 0, 0, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_LEFT]=0;
   }
   else
   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP6)
   && kjflag[0][RETRO_DEVICE_ID_JOYPAD_RIGHT]==1)
   {
      setjoystickstate(0, 1, 0, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_RIGHT]=0;
   }

   /* Fire */
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP5)
   && kjflag[0][RETRO_DEVICE_ID_JOYPAD_B]==0)
   {
      setjoybuttonstate(0, 0, 1);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_B]=1;
   }
   else
   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP5)
   && kjflag[0][RETRO_DEVICE_ID_JOYPAD_B]==1)
   {
      setjoybuttonstate(0, 0, 0);
      kjflag[0][RETRO_DEVICE_ID_JOYPAD_B]=0;
   }


   /*** Port 1 ***/
   /* Up / Down */
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP9)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP3))
   {
      setjoystickstate(1, 1, -1, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_UP]=1;
   }
   else
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP3)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP9))
   {
      setjoystickstate(1, 1, 1, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_DOWN]=1;
   }

   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP9)
   && kjflag[1][RETRO_DEVICE_ID_JOYPAD_UP]==1)
   {
      setjoystickstate(1, 1, 0, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_UP]=0;
   }
   else
   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP3)
   && kjflag[1][RETRO_DEVICE_ID_JOYPAD_DOWN]==1)
   {
      setjoystickstate(1, 1, 0, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_DOWN]=0;
   }

   /* Left / Right */
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP7)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP1))
   {
      setjoystickstate(1, 0, -1, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_LEFT]=1;
   }
   else
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP1)
   && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP7))
   {
      setjoystickstate(1, 0, 1, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_RIGHT]=1;
   }

   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP7)
   && kjflag[1][RETRO_DEVICE_ID_JOYPAD_LEFT]==1)
   {
      setjoystickstate(1, 0, 0, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_LEFT]=0;
   }
   else
   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP1)
   && kjflag[1][RETRO_DEVICE_ID_JOYPAD_RIGHT]==1)
   {
      setjoystickstate(1, 0, 0, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_RIGHT]=0;
   }

   /* Fire */
   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP0)
   && kjflag[1][RETRO_DEVICE_ID_JOYPAD_B]==0)
   {
      setjoybuttonstate(1, 0, 1);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_B]=1;
   }
   else
   if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP0)
   && kjflag[1][RETRO_DEVICE_ID_JOYPAD_B]==1)
   {
      setjoybuttonstate(1, 0, 0);
      kjflag[1][RETRO_DEVICE_ID_JOYPAD_B]=0;
   }
}

int retro_button_to_uae_button(int i)
{
   int uae_button = -1;
   switch(i)
   {
      case RETRO_DEVICE_ID_JOYPAD_B:
         uae_button = 0;
         break;
      case RETRO_DEVICE_ID_JOYPAD_A:
         uae_button = 1;
         break;
      case RETRO_DEVICE_ID_JOYPAD_Y:
         uae_button = 2;
         break;
      case RETRO_DEVICE_ID_JOYPAD_X:
         uae_button = 3;
         break;
      case RETRO_DEVICE_ID_JOYPAD_L:
         uae_button = 4;
         break;
      case RETRO_DEVICE_ID_JOYPAD_R:
         uae_button = 5;
         break;
      case RETRO_DEVICE_ID_JOYPAD_START:
         uae_button = 6;
         break;
   }
   return uae_button;
}

void ProcessController(int retro_port, int i)
{
   int uae_button = -1;

   if(i>3 && i<8) // Directions, need to fight around presses on the same axis
   {
      if(i==RETRO_DEVICE_ID_JOYPAD_UP || i==RETRO_DEVICE_ID_JOYPAD_DOWN)
      {
         if(i==RETRO_DEVICE_ID_JOYPAD_UP && SHOWKEY==-1)
         {
            if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i)
            && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
            {
               setjoystickstate(retro_port, 1, -1, 1);
               jflag[retro_port][i]=1;
            }
         }
         else if(i==RETRO_DEVICE_ID_JOYPAD_DOWN && SHOWKEY==-1)
         {
            if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i)
            && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
            {
               setjoystickstate(retro_port, 1, 1, 1);
               jflag[retro_port][i]=1;
            }
         }

         if(!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)
         && jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_UP]==1)
         {
            setjoystickstate(retro_port, 1, 0, 1);
            jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_UP]=0;
         }
         else if(!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)
         && jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_DOWN]==1)
         {
            setjoystickstate(retro_port, 1, 0, 1);
            jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_DOWN]=0;
         }
      }

      if(i==RETRO_DEVICE_ID_JOYPAD_LEFT || i==RETRO_DEVICE_ID_JOYPAD_RIGHT)
      {
         if(i==RETRO_DEVICE_ID_JOYPAD_LEFT && SHOWKEY==-1)
         {
            if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i)
            && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
            {
               setjoystickstate(retro_port, 0, -1, 1);
               jflag[retro_port][i]=1;
            }
         }
         else if(i==RETRO_DEVICE_ID_JOYPAD_RIGHT && SHOWKEY==-1)
         {
            if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i)
            && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
            {
               setjoystickstate(retro_port, 0, 1, 1);
               jflag[retro_port][i]=1;
            }
         }

         if(!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)
         && jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_LEFT]==1)
         {
            setjoystickstate(retro_port, 0, 0, 1);
            jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_LEFT]=0;
         }
         if(!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
         && jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_RIGHT]==1)
         {
            setjoystickstate(retro_port, 0, 0, 1);
            jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_RIGHT]=0;
         }
      }
   }
   else if(i != turbo_fire_button) // Buttons
   {
      uae_button = retro_button_to_uae_button(i);
      if(uae_button != -1)
      {
         if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && jflag[retro_port][i]==0 && SHOWKEY==-1)
         {
            setjoybuttonstate(retro_port, uae_button, 1);
            jflag[retro_port][i]=1;
         }
         else
         if(!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && jflag[retro_port][i]==1)
         {
            setjoybuttonstate(retro_port, uae_button, 0);
            jflag[retro_port][i]=0;
         }
      }
   }
}

void ProcessTurbofire(int retro_port, int i)
{
   if(turbo_fire_button != -1 && i == turbo_fire_button)
   {
      if(input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, turbo_fire_button))
      {
         if(turbo_state[retro_port])
         {
            if((turbo_toggle[retro_port]) == (turbo_pulse))
               turbo_toggle[retro_port] = 1;
            else
               turbo_toggle[retro_port]++;

            if(turbo_toggle[retro_port] > (turbo_pulse / 2))
            {
               setjoybuttonstate(retro_port, 0, 0);
               jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_B]=0;
            }
            else
            {
               setjoybuttonstate(retro_port, 0, 1);
               jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_B]=1;
            }
         }
         else
         {
            turbo_state[retro_port] = 1;
            setjoybuttonstate(retro_port, 0, 1);
            jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_B]=1;
         }
      }
      else if(!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, turbo_fire_button) && turbo_state[retro_port]==1)
      {
         turbo_state[retro_port] = 0;
         turbo_toggle[retro_port] = 1;
         setjoybuttonstate(retro_port, 0, 0);
         jflag[retro_port][RETRO_DEVICE_ID_JOYPAD_B]=0;
      }
   }
}


void ProcessKey(int disable_physical_cursor_keys)
{
   int i;
   for(i=0;i<320;i++)
   {
      key_state[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i)?0x80:0;

      /* CapsLock */
      if(keyboard_translation[i]==AK_CAPSLOCK)
      {
         if(key_state[i] && key_state2[i]==0)
         {
            retro_key_down(keyboard_translation[i]);
            retro_key_up(keyboard_translation[i]);
            SHIFTON=-SHIFTON;
            Screen_SetFullUpdate();
            key_state2[i]=1;
         }
         else if(!key_state[i] && key_state2[i]==1)
            key_state2[i]=0;
      }
      /* Special key (Right Alt) for overriding RetroPad cursor override */ 
      else if(keyboard_translation[i]==AK_RALT)
      {
         if(key_state[i] && key_state2[i]==0)
         {
            ALTON=1;
            retro_key_down(keyboard_translation[i]);
            key_state2[i]=1;
         }
         else if(!key_state[i] && key_state2[i]==1)
         {
            ALTON=-1;
            retro_key_up(keyboard_translation[i]);
            key_state2[i]=0;
         }
      }
      else
      {
         /* Override cursor keys if used as a RetroPad */
         if(disable_physical_cursor_keys && (i == RETROK_DOWN || i == RETROK_UP || i == RETROK_LEFT || i == RETROK_RIGHT))
            continue;

         /* Skip numpad if Keyrah is active */
         if(opt_keyrahkeypad)
         {
            switch(i) {
               case RETROK_KP1:
               case RETROK_KP2:
               case RETROK_KP3:
               case RETROK_KP4:
               case RETROK_KP5:
               case RETROK_KP6:
               case RETROK_KP7:
               case RETROK_KP8:
               case RETROK_KP9:
               case RETROK_KP0:
                  continue;
            }
         }

         /* Skip keys if VKBD is active */
         if(SHOWKEY==1)
            continue;

         if(key_state[i] && keyboard_translation[i]!=-1 && key_state2[i] == 0)
         {
            if(SHIFTON==1)
               retro_key_down(keyboard_translation[RETROK_LSHIFT]);

            retro_key_down(keyboard_translation[i]);
            key_state2[i]=1;
         }
         else if(!key_state[i] && keyboard_translation[i]!=-1 && key_state2[i]==1)
         {
            retro_key_up(keyboard_translation[i]);
            key_state2[i]=0;

            if(SHIFTON==1)
               retro_key_up(keyboard_translation[RETROK_LSHIFT]);
         }
      }
   }
}

void update_input(int disable_physical_cursor_keys)
{
// RETRO    B   Y   SLT STA UP  DWN LFT RGT A   X   L   R   L2  R2  L3  R3  LR  LL  LD  LU  RR  RL  RD  RU
// INDEX    0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19  20  21  22  23

   int i, mk;

   static int oldi=-1;
   static int vkx=0,vky=0;

   int LX, LY, RX, RY;
   int threshold=20000;

   /* Keyup only after button is up */
   if(oldi!=-1 && vkflag[4]!=1)
   {
      retro_key_up(oldi);

      if(SHIFTON==1)
         retro_key_up(keyboard_translation[RETROK_LSHIFT]);

      oldi=-1;
   }

   input_poll_cb();

   /* Keyboard hotkeys */
   for(i = 0; i < EMU_FUNCTION_COUNT; i++) {
      mk = i + 24;
      /* Key down */
      if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[mk]) && kbt[i]==0 && mapper_keys[mk]!=0)
      {
         //printf("KEYDN: %d\n", mk);
         kbt[i]=1;
         switch(mk) {
            case 24:
               emu_function(EMU_VKBD);
               break;
            case 25:
               emu_function(EMU_STATUSBAR);
               break;
            case 26:
               emu_function(EMU_MOUSE_TOGGLE);
               break;
            case 27:
               emu_function(EMU_MOUSE_SPEED_DOWN);
               break;
            case 28:
               emu_function(EMU_RESET);
               break;
            case 29:
               emu_function(EMU_ASPECT_RATIO_TOGGLE);
               break;
            case 30:
               emu_function(EMU_ZOOM_MODE_TOGGLE);
               break;
         }
      }
      /* Key up */
      else if (kbt[i]==1 && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[mk]) && mapper_keys[mk]!=0)
      {
         //printf("KEYUP: %d\n", mk);
         kbt[i]=0;
         switch(mk) {
            case 26:
               /* simulate button press to activate mouse properly */
               if(MOUSEMODE==1) {
                  retro_mouse_but0(1);
                  retro_mouse_but0(0);
               }
               break;
            case 27:
               emu_function(EMU_MOUSE_SPEED_UP);
               break;
         }
      }
   }

   

   /* The check for kbt[i] here prevents the hotkey from generating key events */
   /* SHOWKEY check is now in ProcessKey to allow certain keys while SHOWKEY */
   int processkey=1;
   for(i = 0; i < (sizeof(kbt)/sizeof(kbt[0])); i++) {
      if(kbt[i] == 1)
      {
         processkey=0;
         break;
      }
   }

   if (processkey && disable_physical_cursor_keys != 2)
      ProcessKey(disable_physical_cursor_keys);

   if (opt_keyrahkeypad)
      ProcessKeyrah();

   /* RetroPad hotkeys */
   if (uae_devices[0] == RETRO_DEVICE_JOYPAD) {

        LX = input_state_cb(0, RETRO_DEVICE_ANALOG, 0, 0);
        LY = input_state_cb(0, RETRO_DEVICE_ANALOG, 0, 1);
        RX = input_state_cb(0, RETRO_DEVICE_ANALOG, 1, 0);
        RY = input_state_cb(0, RETRO_DEVICE_ANALOG, 1, 1);

        /* shortcut for joy mode only */
        for(i = 0; i < 24; i++)
        {
            int just_pressed = 0;
            int just_released = 0;
            if(i > 0 && (i<4 || i>7) && i < 16) /* remappable retropad buttons (all apart from DPAD and B) */
            {
                /* Skip the vkbd extra buttons if vkbd is visible */
                if(SHOWKEY==1 && (i==RETRO_DEVICE_ID_JOYPAD_A || i==RETRO_DEVICE_ID_JOYPAD_X))
                    continue;

                if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && jbt[i]==0 && i!=turbo_fire_button)
                    just_pressed = 1;
                else if (jbt[i]==1 && !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
                    just_released = 1;
            }
            else if (i >= 16) /* remappable retropad joystick directions */
            {
                switch (i)
                {
                    case 16: /* LR */
                        if (LX > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LX < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 17: /* LL */
                        if (LX < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LX > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 18: /* LD */
                        if (LY > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LY < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 19: /* LU */
                        if (LY < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LY > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 20: /* RR */
                        if (RX > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RX < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 21: /* RL */
                        if (RX < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RX > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 22: /* RD */
                        if (RY > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RY < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 23: /* RU */
                        if (RY < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RY > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    default:
                        break;
                }
            }

            if (just_pressed)
            {
                jbt[i] = 1;
                if(mapper_keys[i] == 0) /* unmapped, e.g. set to "---" in core options */
                    continue;

                if(mapper_keys[i] == mapper_keys[24]) /* Virtual keyboard */
                    emu_function(EMU_VKBD);
                else if(mapper_keys[i] == mapper_keys[25]) /* Statusbar */
                    emu_function(EMU_STATUSBAR);
                else if(mapper_keys[i] == mapper_keys[26]) /* Toggle mouse control */
                    emu_function(EMU_MOUSE_TOGGLE);
                else if(mapper_keys[i] == mapper_keys[27]) /* Alter mouse speed */
                    emu_function(EMU_MOUSE_SPEED_DOWN);
                else if(mapper_keys[i] == mapper_keys[28]) /* Reset */
                    emu_function(EMU_RESET);
                else if(mapper_keys[i] == mapper_keys[29]) /* Toggle aspect ratio */
                    emu_function(EMU_ASPECT_RATIO_TOGGLE);
                else if(mapper_keys[i] == mapper_keys[30]) /* Toggle zoom mode */
                    emu_function(EMU_ZOOM_MODE_TOGGLE);
                else if(mapper_keys[i] == -2) /* Mouse left */
                {
                    setmousebuttonstate (0, 0, 1);
                    mflag[RETRO_DEVICE_ID_JOYPAD_B]=1;
                }
                else if(mapper_keys[i] == -3) /* Mouse right */
                {
                    setmousebuttonstate (0, 1, 1);
                    mflag[RETRO_DEVICE_ID_JOYPAD_A]=1;
                }
                else if(mapper_keys[i] == -4) /* Mouse middle */
                    setmousebuttonstate (0, 2, 1);
                else
                    retro_key_down(keyboard_translation[mapper_keys[i]]);
            }
            else if (just_released)
            {
                jbt[i] = 0;
                if(mapper_keys[i] == 0) /* unmapped, e.g. set to "---" in core options */
                    continue;

                if(mapper_keys[i] == mapper_keys[24])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[25])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[26])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[27])
                    emu_function(EMU_MOUSE_SPEED_UP);
                else if(mapper_keys[i] == mapper_keys[28])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[29])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[30])
                    ; /* nop */
                else if(mapper_keys[i] == -2)
                {
                    setmousebuttonstate (0, 0, 0);
                    mflag[RETRO_DEVICE_ID_JOYPAD_B]=0;
                }
                else if(mapper_keys[i] == -3)
                {
                    setmousebuttonstate (0, 1, 0);
                    mflag[RETRO_DEVICE_ID_JOYPAD_A]=0;
                }
                else if(mapper_keys[i] == -4)
                    setmousebuttonstate (0, 2, 0);
                else
                    retro_key_up(keyboard_translation[mapper_keys[i]]);
            }
        } /* for i */
    } /* if uae_devices[0]==joypad */
 
   /* Virtual keyboard */
   if(SHOWKEY==1)
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && vkflag[0]==0)
         vkflag[0]=1;
      else if (vkflag[0]==1 && !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
         vkflag[0]=0;

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && vkflag[1]==0)
         vkflag[1]=1;
      else if (vkflag[1]==1 && !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
         vkflag[1]=0;

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && vkflag[2]==0)
         vkflag[2]=1;
      else if (vkflag[2]==1 && !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
         vkflag[2]=0;

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && vkflag[3]==0)
         vkflag[3]=1;
      else if (vkflag[3]==1 && !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
         vkflag[3]=0;

      if (vkflag[0] || vkflag[1] || vkflag[2] || vkflag[3])
      {
         /* Screen needs to be refreshed in transparent mode */
         if(SHOWKEYTRANS==1)
            Screen_SetFullUpdate();

         long now = GetTicks();
         if (let_go_of_direction)
            /* just pressing down */
            last_press_time = now;

         if ( (now - last_press_time > VKBD_MIN_HOLDING_TIME
            && now - last_move_time > VKBD_MOVE_DELAY)
           || let_go_of_direction)
         {
            last_move_time = now;

            if (vkflag[0])
               vky -= 1;
            else if (vkflag[1])
               vky += 1;

            if (vkflag[2])
               vkx -= 1;
            else if (vkflag[3])
               vkx += 1;
         }
         let_go_of_direction = false;
      }
      else
         let_go_of_direction = true;

      if(vkx < 0)
         vkx=NPLGN-1;
      if(vkx > NPLGN-1)
         vkx=0;
      if(vky < 0)
         vky=NLIGN-1;
      if(vky > NLIGN-1)
         vky=0;

      virtual_kbd(bmp,vkx,vky);

      /* Position toggle */
      i=RETRO_DEVICE_ID_JOYPAD_X;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && vkflag[6]==0)
      {
         vkflag[6]=1;
         SHOWKEYPOS=-SHOWKEYPOS;
         Screen_SetFullUpdate();
      }
      else if(!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && vkflag[6]==1)
      {
         vkflag[6]=0;
      }

      /* Transparency toggle */
      i=RETRO_DEVICE_ID_JOYPAD_A;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && vkflag[5]==0)
      {
         vkflag[5]=1;
         SHOWKEYTRANS=-SHOWKEYTRANS;
         Screen_SetFullUpdate();
      }
      else if(!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && vkflag[5]==1)
      {
         vkflag[5]=0;
      }

      /* Key press */
      i=RETRO_DEVICE_ID_JOYPAD_B;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && vkflag[4]==0)
      {
         vkflag[4]=1;
         i=check_vkey2(vkx,vky);

         if(i==-1)
            oldi=-1;
         else if(i==-2)
         {
            oldi=-1;
            NPAGE=-NPAGE;
            Screen_SetFullUpdate();
         }
         else
         {
            if(i==AK_CAPSLOCK)
            {
               retro_key_down(i);
               retro_key_up(i);
               SHIFTON=-SHIFTON;
               Screen_SetFullUpdate();
               oldi=-1;
            }
            else
            {
               oldi=i;
               if(SHIFTON==1)
                  retro_key_down(keyboard_translation[RETROK_LSHIFT]);

               retro_key_down(i);
            }
         }
      }
      else if(!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && vkflag[4]==1)
      {
         vkflag[4]=0;
      }
   }
}

void retro_poll_event()
{
   /* If RetroPad is controlled with keyboard keys, then prevent up/down/left/right/fire/fire2 from generating keyboard key presses */
   if (uae_devices[0] == RETRO_DEVICE_JOYPAD && ALTON==-1 && 
      (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)
       )
   )
      update_input(2); /* Skip all keyboard input when fire/fire2 is pressed */
   else if (
      (uae_devices[0] == RETRO_DEVICE_JOYPAD) && ALTON==-1 &&
      (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
       )
   )
      update_input(1); /* Process all inputs but disable cursor keys */
   else
      update_input(0); /* Process all inputs */

   if (ALTON==-1) /* retro joypad take control over keyboard joy */
   /* override keydown, but allow keyup, to prevent key sticking during keyboard use, if held down on opening keyboard */
   /* keyup allowing most likely not needed on actual keyboard presses even though they get stuck also */
   {
      static int mbL=0,mbR=0;
      static int mouse_l=0,mouse_r=0;
      static int16_t mouse_x=0,mouse_y=0;
      static int i=0;

      int retro_port;
      for (retro_port = 0; retro_port <= 3; retro_port++)
      {
         switch (uae_devices[retro_port])
         {
            case RETRO_DEVICE_JOYPAD:
               // RetroPad control (user 0 disabled if MOUSEMODE is on)
               if (MOUSEMODE==1 && retro_port==0)
                  break;

               for (i=0;i<16;i++) // All buttons
               {
                  if (i==0 || (i>3 && i<9)) // DPAD + B + A
                  {
                     // Skip 2nd fire if keymapped
                     if(retro_port==0 && i==RETRO_DEVICE_ID_JOYPAD_A && mapper_keys[RETRO_DEVICE_ID_JOYPAD_A]!=0)
                        continue;

                     ProcessController(retro_port, i);
                  }
                  ProcessTurbofire(retro_port, i);
               }
               break;

            case RETRO_DEVICE_UAE_CD32PAD:
               for (i=0;i<16;i++) // All buttons
               {
                  if (i<2 || (i>2 && i<12)) // Only skip Select (2)
                  {
                     ProcessController(retro_port, i);
                  }
                  ProcessTurbofire(retro_port, i);
               }
               break;

            case RETRO_DEVICE_UAE_JOYSTICK:
               for (i=0;i<9;i++) // All buttons up to A
               {
                  if (i==0 || (i>3 && i<9)) // DPAD + B + A
                  {
                     ProcessController(retro_port, i);
                  }
               }
               break;
         }
      }
   
      // Mouse control
      if (SHOWKEY==-1)
      {
         mouse_l=mouse_r=0;
         fmousex=fmousey=0;

         // Joypad buttons
         if (MOUSEMODE==1 && uae_devices[0] == RETRO_DEVICE_JOYPAD)
         {
            mouse_l = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
            mouse_r = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
         }

         // Real mouse buttons
         if (!mouse_l && !mouse_r)
         {
            mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
            mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
         }

         // Joypad movement
         if (MOUSEMODE==1 && uae_devices[0] == RETRO_DEVICE_JOYPAD)
         {
            if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
               fmousex += PAS;
            if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
               fmousex -= PAS;
            if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
               fmousey += PAS;
            if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
               fmousey -= PAS;
         }

         // Left analog movement
         if (opt_analogmouse == 1 || opt_analogmouse == 3) 
            // No keymappings and mousing at the same time
            if (!fmousex && !fmousey && (!mapper_keys[16] && !mapper_keys[17] && !mapper_keys[18] && !mapper_keys[19]))
            {
               analog_left[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X));
               analog_left[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y));

               if (analog_left[0]<=-analog_deadzone)
                  fmousex-=(-analog_left[0])/analog_sensitivity;
               if (analog_left[0]>= analog_deadzone)
                  fmousex+=( analog_left[0])/analog_sensitivity;
               if (analog_left[1]<=-analog_deadzone)
                  fmousey-=(-analog_left[1])/analog_sensitivity;
               if (analog_left[1]>= analog_deadzone)
                  fmousey+=( analog_left[1])/analog_sensitivity;
            }

         // Right analog movement
         if (opt_analogmouse == 2 || opt_analogmouse == 3)
            // No keymappings and mousing at the same time
            if (!fmousex && !fmousey && (!mapper_keys[20] && !mapper_keys[21] && !mapper_keys[22] && !mapper_keys[23]))
            {
               analog_right[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X));
               analog_right[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y));

               if (analog_right[0]<=-analog_deadzone)
                  fmousex-=(-analog_right[0])/analog_sensitivity;
               if (analog_right[0]>= analog_deadzone)
                  fmousex+=( analog_right[0])/analog_sensitivity;
               if (analog_right[1]<=-analog_deadzone)
                  fmousey-=(-analog_right[1])/analog_sensitivity;
               if (analog_right[1]>= analog_deadzone)
                  fmousey+=( analog_right[1])/analog_sensitivity;
            }

         // Real mouse movement
         if (!fmousex && !fmousey)
         {
            mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
            mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

            if (mouse_x || mouse_y)
            {
               fmousex = mouse_x;
               fmousey = mouse_y;
            }
         }

         // Mouse buttons to UAE
         if (mbL==0 && mouse_l)
         {
            mbL=1;
            mflag[RETRO_DEVICE_ID_JOYPAD_B]=1;
            retro_mouse_but0(1);
         }
         else if (mbL==1 && !mouse_l)
         {
            mbL=0;
            mflag[RETRO_DEVICE_ID_JOYPAD_B]=0;
            retro_mouse_but0(0);
         }

         if (mbR==0 && mouse_r)
         {
            mbR=1;
            mflag[RETRO_DEVICE_ID_JOYPAD_A]=1;
            retro_mouse_but1(1);
         }
         else if (mbR==1 && !mouse_r)
         {
            mbR=0;
            mflag[RETRO_DEVICE_ID_JOYPAD_A]=0;
            retro_mouse_but1(0);
         }

         // Mouse movement to UAE
         if (fmousex || fmousey)
            if (fmousey<0 && mflag[RETRO_DEVICE_ID_JOYPAD_UP]==0)
               mflag[RETRO_DEVICE_ID_JOYPAD_UP]=1;
            if (fmousey>-1 && mflag[RETRO_DEVICE_ID_JOYPAD_UP]==1)
               mflag[RETRO_DEVICE_ID_JOYPAD_UP]=0;

            if (fmousey>0 && mflag[RETRO_DEVICE_ID_JOYPAD_DOWN]==0)
               mflag[RETRO_DEVICE_ID_JOYPAD_DOWN]=1;
            if (fmousey<1 && mflag[RETRO_DEVICE_ID_JOYPAD_DOWN]==1)
               mflag[RETRO_DEVICE_ID_JOYPAD_DOWN]=0;

            if (fmousex<0 && mflag[RETRO_DEVICE_ID_JOYPAD_LEFT]==0)
               mflag[RETRO_DEVICE_ID_JOYPAD_LEFT]=1;
            if (fmousex>-1 && mflag[RETRO_DEVICE_ID_JOYPAD_LEFT]==1)
               mflag[RETRO_DEVICE_ID_JOYPAD_LEFT]=0;

            if (fmousex>0 && mflag[RETRO_DEVICE_ID_JOYPAD_RIGHT]==0)
               mflag[RETRO_DEVICE_ID_JOYPAD_RIGHT]=1;
            if (fmousex<1 && mflag[RETRO_DEVICE_ID_JOYPAD_RIGHT]==1)
               mflag[RETRO_DEVICE_ID_JOYPAD_RIGHT]=0;

            retro_mouse(fmousex, fmousey);
      }
   }
}
