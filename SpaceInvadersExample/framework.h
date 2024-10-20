#pragma once

#include <Windows.h>
#include "glew.h"
#include "wglew.h"
#include "log.h"

extern void allegro_message(const char *title, const char *message);
extern HWND win_get_window();
void swap_buffers();
extern void ViewOrtho(int width, int height);
extern int SCREEN_W;
extern int SCREEN_H;

