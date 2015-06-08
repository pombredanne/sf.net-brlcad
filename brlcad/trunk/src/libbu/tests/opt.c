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
#include <limits.h>
#include <ctype.h>
#include "bu.h"
#include "bn.h"
#include "string.h"


#define help_str "Print help string and exit."


int
d1_verb(struct bu_vls *msg, int argc, const char **argv, void *set_v)
{
    int val = INT_MAX;
    int *int_set = (int *)set_v;
    int int_ret;
    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc == 0) {
	/* Have verbosity flag but no valid arg - go with just the flag */
	if (int_set) (*int_set) = 1;
	return 0;
    }

    int_ret = bu_opt_int(msg, argc, argv, (void *)&val);
    if (int_ret == -1) return -1;

    if (val < 0 || val > 3) return -1;

    if (int_set) (*int_set) = val;

    return 1;
}


int desc_1(int test_num)
{
    static int print_help = 0;
    static int verbosity = 0;

    /* Option descriptions */
    struct bu_opt_desc d[4] = {
	{"h", "help",      0, 0, NULL,     (void *)&print_help, "",  help_str},
	{"?", "",          0, 0, NULL,     (void *)&print_help, "",  help_str},
	{"v", "verbosity", 0, 1, &d1_verb, (void *)&verbosity,  "#", "Set verbosity (range is 0 to 3)"},
	BU_OPT_DESC_NULL
    };

    int ac = 0;
    int val_ok = 1;
    int ret = 0;
    int containers = 6;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case 1:
	    ac = 1;
	    av[0] = "-v";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-v\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 2:
	    ac = 1;
	    av[0] = "-v1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-v1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 3:
	    ac = 2;
	    av[0] = "-v";
	    av[1] = "1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-v 1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 4:
	    ac = 1;
	    av[0] = "-v=1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-v=1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 5:
	    ac = 2;
	    av[0] = "--v";
	    av[1] = "1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--v 1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 6:
	    ac = 1;
	    av[0] = "--v=1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--v=1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 7:
	    ac = 2;
	    av[0] = "-v";
	    av[1] = "2";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-v 2\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 2) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"2\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 8:
	    ac = 2;
	    av[0] = "-v";
	    av[1] = "4";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-v 4\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (!ret==-1) {
		bu_vls_printf(&parse_msgs, "\nError - expected parser to fail with error and it didn't\n");
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "OK (expected failure) - verbosity failed (4 > 3)\n");
		ret = 0;
	    }
	    break;
	case 9:
	    ac = 2;
	    av[0] = "--verbosity";
	    av[1] = "2";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--verbosity 2\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 2) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"2\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 10:
	    ac = 2;
	    av[0] = "--verbosity";
	    av[1] = "4";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--verbosity 4\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (!ret==-1) {
		bu_vls_printf(&parse_msgs, "\nError - expected parser to fail with error and it didn't\n");
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "OK (expected failure) - verbosity failed (4 > 3)\n");
		ret = 0;
	    }
	    break;
	case 11:
	    ac = 1;
	    av[0] = "--verbosity=2";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--verbosity=2\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || verbosity != 2) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"2\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case 12:
	    ac = 1;
	    av[0] = "--verbosity=4";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--verbosity=4\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (!ret==-1) {
		bu_vls_printf(&parse_msgs, "\nError - expected parser to fail with error and it didn't\n");
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "OK (expected failure) - verbosity failed (4 > 3)\n");
		ret = 0;
	    }
	    break;

    }

    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", unknown[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", unknown[ret - 1]);
    }

    if (!val_ok) ret = -1;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free(av, "free av");
    bu_free(unknown, "free av");
    return ret;
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
dc_color(struct bu_vls *msg, int argc, const char **argv, void *set_c)
{
    struct bu_color *set_color = (struct bu_color *)set_c;
    unsigned int rgb[3];
    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc == 0) {
	return 0;
    }

    /* First, see if the first string converts to rgb */
    if (!bu_str_to_rgb((char *)argv[0], (unsigned char *)&rgb)) {
	/* nope - maybe we have 3 args? */
	if (argc == 3) {
	    struct bu_vls tmp_color = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&tmp_color, "%s/%s/%s", argv[0], argv[1], argv[2]);
	    if (!bu_str_to_rgb(bu_vls_addr(&tmp_color), (unsigned char *)&rgb)) {
		/* Not valid with 3 */
		bu_vls_free(&tmp_color);
		if (msg) bu_vls_sprintf(msg, "No valid color found.\n");
		return -1;
	    } else {
		/* 3 did the job */
		bu_vls_free(&tmp_color);
		if (set_color) (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
		return 3;
	    }
	} else {
	    /* Not valid with 1 and don't have 3 - we require at least one, so
	     * claim one argv as belonging to this option regardless. */
	    if (msg) bu_vls_sprintf(msg, "No valid color found: %s\n", argv[0]);
	    return -1;
	}
    } else {
	/* yep, 1 did the job */
	if (set_color) (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
	return 1;
    }

    return 0;
}

int desc_2(int test_num)
{
    int ret = 0;
    int val_ok = 1;
    int print_help = 0;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    int containers = 6;
    int ac = 0;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",  0, 0, NULL,      (void *)&print_help, "",      help_str);
    BU_OPT(d[1], "C", "color", 1, 3, &dc_color, (void *)&color,      "r/g/b", "Set color");
    BU_OPT_NULL(d[2]);

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case 1:
	    ac = 2;
	    av[0] = "--color";
	    av[1] = "200/10/30";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--color 200/10/30\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || (!NEAR_EQUAL(color.buc_rgb[0], 200.0, SMALL_FASTF) || !NEAR_EQUAL(color.buc_rgb[1], 10, SMALL_FASTF) || !NEAR_EQUAL(color.buc_rgb[2], 30, SMALL_FASTF))) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"200/10/30\" and got value %.0f/%.0f/%.0f\n", color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK (color == %.0f/%.0f/%.0f)\n", color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]);
	    }
	    break;
	case 2:
	    ac = 4;
	    av[0] = "--color";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"--color 200 10 30\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || (!NEAR_EQUAL(color.buc_rgb[0], 200.0, SMALL_FASTF) || !NEAR_EQUAL(color.buc_rgb[1], 10, SMALL_FASTF) || !NEAR_EQUAL(color.buc_rgb[2], 30, SMALL_FASTF))) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"200/10/30\" and got value %0f/%0f/%0f\n", color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK (color == %.0f/%.0f/%.0f)\n", color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]);
	    }

	    break;
    }

    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", unknown[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", unknown[ret - 1]);
    }

    if (!val_ok) ret = -1;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free(av, "free av");
    bu_free(unknown, "free av");
    return ret;
}


int desc_3(int test_num)
{
    static int print_help = 0;
    static int int_num = 0;
    static fastf_t float_num = 0;

    struct bu_opt_desc d[4] = {
	{"h", "help",    0, 0, NULL,            (void *)&print_help, "", help_str},
	{"n", "num",     1, 1, &bu_opt_int,     (void *)&int_num, "#", "Read int"},
	{"f", "fastf_t", 1, 1, &bu_opt_fastf_t, (void *)&float_num, "#", "Read float"},
	BU_OPT_DESC_NULL
    };

    int containers = 6;
    int val_ok = 1;
    int ac = 0;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int ret = 0;

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case 1:
	    ac = 2;
	    av[0] = "-n";
	    av[1] = "1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-n 1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || int_num != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", int_num);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nint_num = %d\n", int_num);
	    }
	    break;
	case 2:
	    ac = 2;
	    av[0] = "-n";
	    av[1] = "-1";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-n -1\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || int_num != -1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"-1\" and got value %d\n", int_num);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nint_num = %d\n", int_num);
	    }

	    break;
	case 3:
	    ac = 2;
	    av[0] = "-f";
	    av[1] = "-3.0e-3";
	    bu_vls_printf(&parse_msgs, "Parsing arg string \"-f -3.0e-3\":");
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    if (ret || !NEAR_EQUAL(float_num, -0.003, SMALL_FASTF)) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"-0.003\" and got value %.3f\n", float_num);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nfloat_num = %0.3f\n", float_num);
	    }
	    break;
    }

    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", unknown[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", unknown[ret - 1]);
    }

    if (!val_ok) ret = -1;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free(av, "free av");
    bu_free(unknown, "free av");
    return ret;

}


int
main(int argc, const char **argv)
{
    int ret = 0;
    long desc_num;
    long test_num;
    char *endptr = NULL;

    /* Sanity check */
    if (argc < 3)
	bu_exit(1, "ERROR: wrong number of parameters - need option num and test num");

    /* Set the option description to used based on the input number */
    desc_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid desc number: %s\n", argv[1]);
    }

    test_num = strtol(argv[2], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid test number: %s\n", argv[2]);
    }

    switch (desc_num) {
	case 0:
	    ret = bu_opt_parse(NULL, 0, NULL, 0, NULL, NULL);
	case 1:
	    ret = desc_1(test_num);
	    break;
	case 2:
	    ret = desc_2(test_num);
	    break;
	case 3:
	    ret = desc_3(test_num);
	    break;
    }

    return ret;
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
