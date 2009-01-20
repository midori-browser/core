/*
 Copyright (C) 2009 Matthias Kruk <mkruk@matthiaskruk.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MOUSE_GESTURES_H__
#define __MOUSE_GESTURES_H__

typedef struct _MouseGesture MouseGesture;
typedef enum _MouseButton MouseButton;

enum _MouseButton {
	MOUSE_BUTTON_LEFT = 1,
	MOUSE_BUTTON_RIGHT = 3,
	MOUSE_BUTTON_MIDDLE = 2,
	MOUSE_BUTTON_UNSET = 0
};

struct MouseGestureNode {
	double x;
	double y;
} MouseGestureNode_t;

struct _MouseGesture {
	struct MouseGestureNode start;
	struct MouseGestureNode middle;
	struct MouseGestureNode end;
	MouseButton last;
};

#endif
