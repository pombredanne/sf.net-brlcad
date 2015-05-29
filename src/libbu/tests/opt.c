/*                       O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"
#include <ctype.h>
#include "bu.h"
#include "bn.h"
#include "string.h"

int
d1_verbosity(struct bu_vls *msg, struct bu_opt_data *data)
{
    int verb;
    if (!data) return -1;
    if (msg) bu_vls_sprintf(msg, "d1");
    sscanf((const char *)BU_PTBL_GET(data->args, 0), "%d", &verb);
    if (verb < 0 || verb > 3) data->valid = 0;
    return 0;
}

int
isnum(const char *str) {
    int i, sl;
    if (!str) return 0;
    sl = strlen(str);
    for (i = 0; i < sl; i++) if (!isdigit(str[i])) return 0;
    return 1;
}

int
d2_color(struct bu_vls *msg, struct bu_opt_data *data)
{
    unsigned int *rgb;
    int color_arg_cnt;
    if (!data) return 0;
    if (!data->args) {
	data->valid = 0;
	return 0;
    }
    if (msg) bu_vls_sprintf(msg, "d2");

    color_arg_cnt = BU_PTBL_LEN(data->args);
    rgb = (unsigned int *)bu_calloc(3, sizeof(unsigned int), "fastf_t array");

    /* First, see if the first string converts to rgb */
    if (!bu_str_to_rgb((char *)BU_PTBL_GET(data->args, 0), (unsigned char *)&rgb)) {
	/* nope - maybe we have 3 args? */
	if (BU_PTBL_LEN(data->args) == 3) {
	    int rn, gn, bn = 0;
	    if (isnum((const char *)BU_PTBL_GET(data->args, 0)))
		rn = sscanf((const char *)BU_PTBL_GET(data->args, 0), "%02x", &rgb[0]);
	    if (isnum((const char *)BU_PTBL_GET(data->args, 1)))
		gn = sscanf((const char *)BU_PTBL_GET(data->args, 1), "%02x", &rgb[1]);
	    if (isnum((const char *)BU_PTBL_GET(data->args, 2)))
		bn = sscanf((const char *)BU_PTBL_GET(data->args, 2), "%02x", &rgb[2]);
	    if (rn != 1 || gn != 1 || bn != 1) {
		data->valid = 0;
		bu_free(rgb, "free rgb");
	    } else {
		data->user_data = (void *)rgb;
	    }
	} else {
	    data->valid = 0;
	    bu_free(rgb, "free rgb");
	}
    } else {
	/* yep - if we've got more args, tell the option parser we don't need them */
	data->user_data = (void *)rgb;
	if (BU_PTBL_LEN(data->args) > 1) return 1 - BU_PTBL_LEN(data->args);
    }

    return 0;
}

void
print_results(struct bu_ptbl *results)
{
    size_t i = 0;
    struct bu_opt_data *data;
    for (i = 0; i < BU_PTBL_LEN(results); i++) {
	data = (struct bu_opt_data *)BU_PTBL_GET(results, i);
	bu_log("option name: %s\n", data->name);
	if (data->args) {
	    size_t j = 0;
	    bu_log("option args: ");
	    for (j = 0; j < BU_PTBL_LEN(data->args); j++) {
		char *arg = (char *)BU_PTBL_GET(data->args, j);
		bu_log("%s ", arg);
	    }
	    bu_log("\n");
	}
	bu_log("option is valid: %d\n\n", data->valid);
    }
}

#define help_str "Print help string and exit."

int
main(int argc, const char **argv)
{
    int function_num;
    struct bu_ptbl *results = NULL;

    enum d1_opt_ind {D1_HELP, D1_VERBOSITY};
    struct bu_opt_desc d1[4] = {
	{D1_HELP,     0, 0, "h", "help",             NULL,          "-h",          "--help", help_str},
	{D1_HELP, 0, 0, "?", "", NULL, "-?", "", help_str},
	{D1_VERBOSITY, 0, 1, "v", "verbosity", &(d1_verbosity), "-v #", "--verbosity #", "Set verbosity (range is 0 to 3)"},
	BU_OPT_DESC_NULL
    };

    enum d2_opt_ind {D2_HELP, D2_COLOR};
    struct bu_opt_desc d2[4] = {
	{D2_HELP, 0, 0, "h", "help", NULL, "-h", "--help", help_str},
	{D2_COLOR, 1, 3, "C", "color", &(d2_color), "-c r/g/b", "--color r/g/b", "Set color"},
	BU_OPT_DESC_NULL
    };


    if (argc < 2)
	bu_exit(1, "ERROR: wrong number of parameters");

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 0:
	    (void)bu_opt_parse(&results, NULL, 0, NULL, NULL);
	    return (results == NULL) ? 0 : 1;
	    break;
	case 1:
	    (void)bu_opt_parse(&results, NULL, argc-2, argv+2, d1);
	    break;
	case 2:
	    (void)bu_opt_parse(&results, NULL, argc-2, argv+2, d2);
	    break;
    }

    if (results) {
	bu_opt_compact(results);
	print_results(results);
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
