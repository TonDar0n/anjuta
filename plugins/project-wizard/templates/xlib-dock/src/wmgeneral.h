[+ autogen5 template +]
/*
 * wmgeneral.h
 * Copyright (C) [+Author+] [+(shell "date +%Y")+] <[+Email+]>
 *
[+CASE (get "License") +]
[+ == "BSD" +][+(bsd (get "Name") (get "Author") "\t")+]
[+ == "LGPL" +][+(lgpl (get "Name") (get "Author") "\t")+]
[+ == "GPL" +][+(gpl (get "Name")  "\t")+]
[+ESAC+] */

#ifndef WMGENERAL_H_INCLUDED
#define WMGENERAL_H_INCLUDED
/* Defines */
#define MAX_MOUSE_REGION (8)

/* Typedefs */
typedef struct _rckeys rckeys;

struct _rckeys {
	const char	*label;
	char		**var;
};

typedef struct {
	Pixmap		pixmap;
	Pixmap		mask;
	XpmAttributes	attributes;
} XpmIcon;

/* Global variable */
Display	*display;
Window          Root;
GC              NormalGC;
XpmIcon         wmgen;

/* Function Prototypes */
void AddMouseRegion(int idx, int left, int top, int right, int bottom);
int CheckMouseRegion(int x, int y);
void openXwindow(int argc, char *argv[], char **, char *, int, int);
void RedrawWindow(void);
void RedrawWindowXY(int x, int y);
void copyXPMArea(int, int, int, int, int, int);
void copyXBMArea(int, int, int, int, int, int);
void setMaskXY(int, int);
void parse_rcfile(const char *, rckeys *);

#endif
