[+ autogen5 template +]
[+INCLUDE (string-append "licenses/" (get "License") ".tpl") \+]
[+INCLUDE (string-append "indent.tpl") \+]
/* [+INVOKE EMACS-MODELINE MODE="C" \+] */
[+INVOKE START-INDENT\+]
/*
 * main.c
 * Copyright (C) [+(shell "date +%Y")+] [+Author+] <[+Email+]>
 * 
[+INVOKE LICENSE-DESCRIPTION PFX=" * " PROGRAM=(get "Name") OWNER=(get "Author") \+]
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <X11/X.h>
#include <X11/xpm.h>
#include <X11/Xlib.h>

#include "wmgeneral.h"
#include "pixmaps.h"

/* Prototypes */
static void print_usage(void);
static void ParseCMDLine(int argc, char *argv[]);

static void print_usage(void)
{
	printf("\nHello Dock App version: %s\n", VERSION);
	printf("\nTODO: Write This.\n\n");
}

void ParseCMDLine(int argc, char *argv[])
{
	int	i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-display")) {
			++i; /* -display is used in wmgeneral */
		} else {
			print_usage();
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	XEvent	event;

	ParseCMDLine(argc, argv);
	openXwindow(argc, argv, xpm_master, xpm_mask_bits, xpm_mask_width, xpm_mask_height);

	/* Loop Forever */
	while (1) {
		/* Process any pending X events. */
		while (XPending(display)) {
			XNextEvent(display, &event);
				switch (event.type) {
					case Expose:
						RedrawWindow();
						break;
					case ButtonPress:
						break;
					case ButtonRelease:
						break;
				}
		}
		usleep(10000);
	}

	/* we should never get here */
	return (0);
}
[+INVOKE END-INDENT\+]
