/*                        M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2016 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rtwizard/main.c
 *
 */

#include "common.h"
#include "string.h"

#include "vmath.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bu/opt.h"
#include "bu/str.h"

#define RTWIZARD_HAVE_GUI 0

struct rtwizard_settings {
    struct bu_ptbl *color;
    struct bu_ptbl *ghost;
    struct bu_ptbl *line;
    int use_gui;
    int no_gui;
    struct bu_vls *fb_dev;
    int port;
    size_t width;
    size_t height;
    size_t size; /* Assumes square - width and height - overridden by width and height */
    struct bu_color *bkg_color;
    struct bu_color *line_color;
    struct bu_color *non_line_color;
    double ghosting_intensity;
    int occlusion;
    int cpus;
    /* View model */
    double viewsize;
    quat_t orientation;
    vect_t eye_pt;
    /* View settings */
    double az, el, tw;
    double perspective;
    double zoom;
    vect_t center;
};

struct rtwizard_settings * rtwizard_settings_create() {
    struct rtwizard_settings *s;
    BU_GET(s, struct rtwizard_settings);
    BU_GET(s->color, struct bu_ptbl);
    BU_GET(s->ghost, struct bu_ptbl);
    BU_GET(s->line,  struct bu_ptbl);
    bu_ptbl_init(s->color, 8, "color init");
    bu_ptbl_init(s->ghost, 8, "ghost init");
    bu_ptbl_init(s->line, 8, "line init");
    BU_GET(s->fb_dev, struct bu_vls);
    bu_vls_init(s->fb_dev);
    BU_GET(s->bkg_color, struct bu_color);
    BU_GET(s->line_color, struct bu_color);
    BU_GET(s->non_line_color, struct bu_color);
    s->width = 0; s->height = 0; s->size = 0;
    s->use_gui = 0; s->no_gui = 0;
    return s;
}

void rtwizard_settings_destroy(struct rtwizard_settings *s) {
    bu_ptbl_free(s->color);
    bu_ptbl_free(s->ghost);
    bu_ptbl_free(s->line);
    /* TODO - loop over and free table contents */
    BU_PUT(s->color, struct bu_ptbl);
    BU_PUT(s->ghost, struct bu_ptbl);
    BU_PUT(s->line,  struct bu_ptbl);

    BU_PUT(s->bkg_color, struct bu_color);
    BU_PUT(s->line_color, struct bu_color);
    BU_PUT(s->non_line_color, struct bu_color);

    bu_vls_free(s->fb_dev);
    BU_PUT(s->fb_dev, struct bu_vls);
    BU_PUT(s, struct rtwizard_settings);
}

int
opt_objs(struct bu_vls *msg, int argc, const char **argv, void *obj_tbl)
{
    /* argv[0] should be either an object or a list. */
    int i = 0;
    char *objs = NULL;
    int acnum = 0;
    char **avnum;
    struct bu_ptbl *t = (struct bu_ptbl *)obj_tbl;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "opt_objs");

    objs = bu_strdup(argv[0]);

    while (objs[i]) {
	/* If we have a separator or a quote, replace with a space */
	if (objs[i] == ',' || objs[i] == ';' || objs[i] == '\'' || objs[i] == '\"' ) {
	    if (i == 0) objs[i] = ' ';
	    if (objs[i-1] != '\\') objs[i] = ' ';
	}
	i++;
    }

    avnum = (char **)bu_calloc(strlen(objs), sizeof(char *), "breakout array");
    acnum = bu_argv_from_string(avnum, strlen(objs), objs);

    /* TODO - use quote/unquote routines to scrub names... */

    for (i = 0; i < acnum; i++) {
	bu_ptbl_ins(t, (long *)bu_strdup(avnum[i]));
    }
    bu_free(objs, "string dup");
    bu_free(avnum, "array memory");

    return (acnum > 0) ? 1 : -1;
}

int
opt_letter(struct bu_vls *msg, int argc, const char **argv, void *l)
{
    char *letter = (char *)l;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_int");

    if (strlen(argv[0]) != 1) {
	if (msg) bu_vls_printf(msg, "Invalid letter specifier for rtwizard type: %s\n", argv[0]);
	return -1;
    }

    if (argv[0][0] != 'A' && argv[0][0] != 'B' && argv[0][0] != 'C' && argv[0][0] != 'D' && argv[0][0] != 'E') {
	if (msg) bu_vls_printf(msg, "Invalid letter specifier for rtwizard type: %c\n", argv[0][0]);
	return -1;
    }

    (*letter) = argv[0][0];

    return 1;
}


int
opt_quat(struct bu_vls *msg, int argc, const char **argv, void *inq)
{
    int i = 0;
    int acnum = 0;
    char *str1 = NULL;
    char *avnum[5] = {NULL, NULL, NULL, NULL, NULL};

    quat_t *q = (quat_t *)inq;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_int");


    /* First, see if the first string converts to a quat_t*/
    str1 = bu_strdup(argv[0]);
    while (str1[i]) {
	/* If we have a separator, replace with a space */
	if (str1[i] == ',' || str1[i] == '/') str1[i] = ' ';
	i++;
    }
    acnum = bu_argv_from_string(avnum, 5, str1);
    if (acnum == 4) {
	/* We might have four numbers - find out */
	fastf_t q1, q2, q3, q4;
	int have_four = 1;
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[0], &q1) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[0]);
	    have_four = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[1], &q2) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[1]);
	    have_four = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[2], &q3) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[2]);
	    have_four = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[3], &q4) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[3]);
	    have_four = 0;
	}
	bu_free(str1, "free tmp str");
	/* If we got here, we do have four numbers */
	if (have_four) {
	    (*q)[0] = q1;
	    (*q)[1] = q2;
	    (*q)[2] = q3;
	    (*q)[3] = q4;
	    return 1;
	}
    } else {
	/* Can't be just the first arg */
	bu_free(str1, "free tmp str");
    }
    /* First string didn't have three numbers - maybe we have 4 args ? */
    if (argc >= 4) {
	/* We might have four numbers - find out */
	fastf_t q1, q2, q3, q4;
	if (bu_opt_fastf_t(msg, 1, &argv[0], &q1) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", argv[0]);
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[1], &q2) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", argv[1]);
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[2], &q3) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", argv[2]);
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[3], &q4) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", argv[3]);
	    return -1;
	}
	(*q)[0] = q1;
	(*q)[1] = q2;
	(*q)[2] = q3;
	(*q)[3] = q4;
	return 1;
    } else {
	if (msg) bu_vls_sprintf(msg, "No valid quaternion found: %s\n", argv[0]);
	return -1;
    }
}

void print_rtwizard_state(struct rtwizard_settings *s) {
    size_t i = 0;
    struct bu_vls slog = BU_VLS_INIT_ZERO;
    bu_vls_printf(&slog, "color:");
    for (i = 0; i < BU_PTBL_LEN(s->color); i++) {
	bu_vls_printf(&slog, " %s", (const char *)BU_PTBL_GET(s->color, i));
    }
    bu_vls_printf(&slog, "\nghost:");
    for (i = 0; i < BU_PTBL_LEN(s->ghost); i++) {
	bu_vls_printf(&slog, " %s", (const char *)BU_PTBL_GET(s->ghost, i));
    }
    bu_vls_printf(&slog, "\nline:");
    for (i = 0; i < BU_PTBL_LEN(s->line); i++) {
	bu_vls_printf(&slog, " %s", (const char *)BU_PTBL_GET(s->line, i));
    }
    bu_vls_printf(&slog, "\n\n");
    bu_vls_printf(&slog, "use_gui: %d\n", s->use_gui);
    bu_vls_printf(&slog, "no_gui: %d\n", s->no_gui);
    bu_vls_printf(&slog, "fb_dev: %s\n", bu_vls_addr(s->fb_dev));
    bu_vls_printf(&slog, "port: %d\n", s->port);
    bu_vls_printf(&slog, "size: %d\n", s->size);
    bu_vls_printf(&slog, "width: %d\n", s->width);
    bu_vls_printf(&slog, "height: %d\n", s->height);
    bu_vls_printf(&slog, "bkg_color: %d,%d,%d\n", (int)s->bkg_color->buc_rgb[0], (int)s->bkg_color->buc_rgb[1], (int)s->bkg_color->buc_rgb[2]);
    bu_vls_printf(&slog, "line_color: %d,%d,%d\n", (int)s->line_color->buc_rgb[0], (int)s->line_color->buc_rgb[1], (int)s->line_color->buc_rgb[2]);
    bu_vls_printf(&slog, "non_line_color: %d,%d,%d\n", (int)s->non_line_color->buc_rgb[0], (int)s->non_line_color->buc_rgb[1], (int)s->non_line_color->buc_rgb[2]);
    bu_vls_printf(&slog, "ghosting intensity: %f\n", s->ghosting_intensity);
    bu_vls_printf(&slog, "occlusion: %d\n", s->occlusion);
    bu_vls_printf(&slog, "cpus: %d\n", s->cpus);
    bu_vls_printf(&slog, "viewsize: %f\n", s->viewsize);
    bu_vls_printf(&slog, "quat: %f,%f,%f,%f\n", s->orientation[0], s->orientation[1], s->orientation[2], s->orientation[3]);
    bu_vls_printf(&slog, "eye_pt: %f,%f,%f\n", s->eye_pt[0], s->eye_pt[1], s->eye_pt[2]);
    bu_vls_printf(&slog, "az,el,tw: %f,%f,%f\n", s->az, s->el, s->tw);
    bu_vls_printf(&slog, "perspective: %f\n", s->perspective);
    bu_vls_printf(&slog, "zoom: %f\n", s->zoom);
    bu_vls_printf(&slog, "center: %f,%f,%f\n", s->center[0], s->center[1], s->center[2]);

    bu_log("%s", bu_vls_addr(&slog));
    bu_vls_free(&slog);
}

int
main(int argc, char **argv)
{
    int verbose = 0;
    int need_help = 0;
    int benchmark = 0;
    int uac = 0;
    int i = 0;
    char type = '\0';
    struct bu_vls in_fname = BU_VLS_INIT_ZERO;
    struct bu_vls out_fname = BU_VLS_INIT_ZERO;
    struct bu_vls logfile = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct rtwizard_settings *s = rtwizard_settings_create();
    struct bu_opt_desc d[33];
    BU_OPT(d[0], "h", "help",          "",          NULL,            &need_help,    "Print help and exit");
    BU_OPT(d[1], "",  "gui",           "",          &bu_opt_int,     &s->use_gui,   "Force use of GUI.");
    BU_OPT(d[2], "",  "no-gui",        "",          &bu_opt_vls,     &s->no_gui,    "Do not use GUI, even if information is insufficient.");
    BU_OPT(d[3], "i", "input-file",    "filename",  &bu_opt_vls,     &in_fname,     "Input .g database file");
    BU_OPT(d[4], "o", "output-file",   "filename",  &bu_opt_vls,     &out_fname,    "Image output file name");
    BU_OPT(d[5], "d", "fbserv-device", "/dev/*",    &bu_opt_vls,      s->fb_dev,    "Device for framebuffer viewing");
    BU_OPT(d[6], "p", "fbserv-port",   "#",         &bu_opt_int,     &s->port,      "Port # for framebuffer");
    BU_OPT(d[7], "w", "width",         "#",         &bu_opt_int,     &s->width,     "Output image width");
    BU_OPT(d[8], "n", "height",        "#",         &bu_opt_int,     &s->height,    "Output image height");
    BU_OPT(d[9], "s", "size",          "#",         &bu_opt_int,     &s->size,      "Output width & height (for square image)");
    BU_OPT(d[10], "c", "color-objects", "obj1,...",  &opt_objs,        s->color,     "List of color objects to render");
    BU_OPT(d[11], "g", "ghost-objects", "obj1,...",  &opt_objs,        s->ghost,     "List of ghost objects to render");
    BU_OPT(d[12], "l", "line-objects",  "obj1,...",  &opt_objs,        s->line,      "List of line objects to render");
    BU_OPT(d[13], "C", "background-color", "R/G/B",  &bu_opt_color,    s->bkg_color, "Background image color");
    BU_OPT(d[14], "",  "line-color",    "R/G/B",     &bu_opt_color,    s->line_color, "Color used for line rendering");
    BU_OPT(d[15], "",  "non-line-color", "R/G/B",    &bu_opt_color,    s->non_line_color, "Color used for non-line rendering ??");
    BU_OPT(d[16], "G", "ghosting-intensity", "#[.#]", &bu_opt_fastf_t, &s->ghosting_intensity,    "Intensity of ghost objects");
    BU_OPT(d[17], "O", "occlusion",     "#",         &bu_opt_int,     &s->occlusion, "Occlusion mode");
    BU_OPT(d[18], "",  "benchmark",     "",          NULL,            &benchmark,    "Benchmark mode");
    BU_OPT(d[19], "",  "cpu-count",     "#",         &bu_opt_int,     &s->cpus,      "Specify the number of CPUs to use");
    BU_OPT(d[20], "a", "azimuth",       "#[.#]",     &bu_opt_fastf_t, &s->az,        "Set azimuth");
    BU_OPT(d[21], "e", "elevation",     "#[.#]",     &bu_opt_fastf_t, &s->el,        "Set elevation");
    BU_OPT(d[22], " ", "twist",         "#[.#]",     &bu_opt_fastf_t, &s->tw,        "Set twist");
    BU_OPT(d[23], "P",  "perspective",  "#[.#]",     &bu_opt_fastf_t, &s->perspective, "Set perspective");
    BU_OPT(d[24], "t", "type",          "A|B|C|D|E", &opt_letter,     &type,         "Specify RtWizard picture type");
    BU_OPT(d[25], "z", "zoom",          "#[.#] ",    &bu_opt_fastf_t, &s->zoom,      "Set zoom");
    BU_OPT(d[26], "",  "center",        "x,y,z",     &bu_opt_vect_t,  &s->center,    "Set view center");
    BU_OPT(d[27], "",  "viewsize",      "#[.#}",     &bu_opt_fastf_t, &s->viewsize,  "Set view size");
    BU_OPT(d[28], "",  "orientation",   "#[.#]/#[.#]/#[.#]/#[.#]", &opt_quat, &s->orientation,    "Set view orientation");
    BU_OPT(d[29], "",  "eye_pt",        "x,y,z",     &bu_opt_vect_t,  &s->eye_pt,    "set eye point");
    BU_OPT(d[30], "v", "verbose",       "#",         &bu_opt_int,     &verbose,      "Verbosity");
    BU_OPT(d[31], "",  "log-file",      "filename",  &bu_opt_vls,     &logfile,      "Log debugging output to this file");
    BU_OPT_NULL(d[32]);

    /* Skip first arg */
    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);

    if (uac == -1) {
	bu_exit(1, bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    if (type != '\0') {
	bu_log("Image type: %c\n", type);
    }

    print_rtwizard_state(s);

    for (i = 0; i < uac; i++) {
	bu_log("av[%d]: %s\n", i, argv[i]);
    }

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
