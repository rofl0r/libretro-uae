#ifndef LIBRETRO_MAPPER_H
#define LIBRETRO_MAPPER_H

extern int SHOWKEY;
extern int vkey_pos_x;
extern int vkey_pos_y;

#define RETRO_DEVICE_UAE_JOYSTICK         RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)
#define RETRO_DEVICE_UAE_CD32PAD          RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)
#define RETRO_DEVICE_UAE_ANALOG           RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 2)
#define RETRO_DEVICE_UAE_KEYBOARD         RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)

#endif /* LIBRETRO_MAPPER_H */
