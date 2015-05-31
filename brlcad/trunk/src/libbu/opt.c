/*                        O P T . C
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

#include <stdio.h>
#include <string.h>
#include <ctype.h> /* for isspace */

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"

#define BU_OPT_DATA_GET(_od, _name) {\
    BU_GET(_od, struct bu_opt_data); \
    if (_name) { \
	_od->name = _name; \
    } else { \
	_od->name = NULL; \
    } \
    _od->args = NULL; \
    _od->valid = 1; \
    _od->desc = NULL; \
    _od->user_data = NULL; \
}

HIDDEN char *
opt_process(char **eq_arg, const char *opt_candidate)
{
    int offset = 1;
    char *inputcpy;
    char *final_opt;
    char *equal_pos;
    if (!eq_arg && !opt_candidate) return NULL;
    inputcpy = bu_strdup(opt_candidate);
    if (inputcpy[1] == '-') offset++;
    equal_pos = strchr(inputcpy, '=');

    /* If we've got a single opt, things are handled differently */
    if (offset == 1) {
	if (strlen(opt_candidate+offset) == 1) {
	    final_opt = bu_strdup(opt_candidate+offset);
	} else {
	    /* single letter opt, but the string is longer - the
	     * interpretation in this context is everything after
	     * the first letter is arg.*/
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    struct bu_vls varg = BU_VLS_INIT_ZERO;
	    bu_vls_strncat(&vopt, inputcpy+1, 1);
	    bu_vls_sprintf(&varg, "%s", inputcpy);
	    bu_vls_nibble(&varg, 2);

#if 0
	    /* A possible exception is an equals sign, e.g. -s=1024 - in that
	     * instance, the expectation might be that = would be interpreted
	     * as an assignment.  This means that to get the literal =1024 as
	     * an option, you would need a space after the s, e.g.:  -s =1024
	     *
	     * For now, commented out to favor consistent behavior over what
	     * "looks right" - may be worth revisiting or even an option at
	     * some point...*/

	    if (equal_pos && equal_pos == inputcpy+2) {
		bu_vls_nibble(&varg, 1);
	    }
#endif

	    (*eq_arg) = bu_strdup(bu_vls_addr(&varg));
	    final_opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_vls_free(&vopt);
	    bu_vls_free(&varg);
	}
    } else {
	if (equal_pos) {
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    struct bu_vls varg = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&vopt, "%s", inputcpy);
	    bu_vls_trunc(&vopt, -1 * strlen(equal_pos));
	    bu_vls_nibble(&vopt, offset);
	    bu_vls_sprintf(&varg, "%s", inputcpy);
	    bu_vls_nibble(&varg, strlen(inputcpy) - strlen(equal_pos) + 1);
	    (*eq_arg) = bu_strdup(bu_vls_addr(&varg));
	    final_opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_vls_free(&vopt);
	    bu_vls_free(&varg);
	} else {
	    final_opt = bu_strdup(opt_candidate+offset);
	}
    }
    bu_free(inputcpy, "cleanup working copy");
    return final_opt;
}

void
bu_opt_compact(struct bu_ptbl *opts)
{
    int i;
    int ptblpos = BU_PTBL_LEN(opts) - 1;
    struct bu_ptbl tbl;
    bu_ptbl_init(&tbl, 8, "local table");
    while (ptblpos >= 0) {
	struct bu_opt_data *data = (struct bu_opt_data *)BU_PTBL_GET(opts, ptblpos);
	if (!data) {
	    ptblpos--;
	    continue;
	}
	bu_ptbl_ins(&tbl, (long *)data);
	BU_PTBL_CLEAR_I(opts, ptblpos);
	for (i = ptblpos - 1; i >= 0; i--) {
	    struct bu_opt_data *dc = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	    if ((dc && dc->desc && data->desc) && dc->desc->index == data->desc->index) {
		bu_free(dc, "free duplicate");
		BU_PTBL_CLEAR_I(opts, i);
	    }
	}
	ptblpos--;
    }
    bu_ptbl_reset(opts);
    for (i = BU_PTBL_LEN(&tbl) - 1; i >= 0; i--) {
	bu_ptbl_ins(opts, BU_PTBL_GET(&tbl, i));
    }
    bu_ptbl_free(&tbl);
}

/* This implements criteria for deciding when an argv string is
 * an option.  Right now the criteria are:
 *
 * 1.  Must have a '-' char as first character
 * 2.  Must not have white space characters present in the string.
 */
HIDDEN int
is_opt(const char *opt) {
    size_t i = 0;
    if (!opt) return 0;
    if (!strlen(opt)) return 0;
    if (opt[0] != '-') return 0;
    for (i = 1; i < strlen(opt); i++) {
	if (isspace(opt[i])) return 0;
    }
    return 1;
}

HIDDEN struct bu_ptbl *
bu_opt_parse_internal(int argc, const char **argv, struct bu_opt_desc *ds, struct bu_ptbl *dptbl, struct bu_vls *UNUSED(msgs))
{
    int i = 0;
    int offset = 0;
    const char *ns = NULL;
    struct bu_ptbl *opt_data;
    struct bu_opt_data *unknowns = NULL;
    if (!argv || (!ds && !dptbl) || (ds && dptbl)) return NULL;

    BU_GET(opt_data, struct bu_ptbl);
    bu_ptbl_init(opt_data, 8, "opt_data");

    /* Now identify opt/arg pairs.*/
    while (i < argc) {
	int desc_found = 0;
	int desc_ind = 0;
	size_t arg_cnt = 0;
	char *opt = NULL;
	char *eq_arg = NULL;
	struct bu_opt_data *data = NULL;
	struct bu_opt_desc *desc = NULL;
	/* If 'opt' isn't an option, make a container for non-option values and build it up until
	 * we reach an option */
	if (!is_opt(argv[i])) {
	    if (!unknowns) {
		BU_OPT_DATA_GET(unknowns, NULL);
		BU_GET(unknowns->args, struct bu_ptbl);
		bu_ptbl_init(unknowns->args, 8, "args init");
	    }
	    ns = bu_strdup(argv[i]);
	    bu_ptbl_ins(unknowns->args, (long *)ns);
	    i++;
	    while (i < argc && !is_opt(argv[i])) {
		ns = bu_strdup(argv[i]);
		bu_ptbl_ins(unknowns->args, (long *)ns);
		i++;
	    }
	    continue;
	}

	/* It may be that an = has been used instead of a space.  Handle that, and
	 * strip leading '-' characters.  Also, short-opt options may not have a
	 * space between their option and the argument. That is also handled here */
	opt = opt_process(&eq_arg, argv[i]);

	/* Find the corresponding desc, if we have one */
	if (ds) {
	    desc = &(ds[0]);
	} else {
	    desc = (struct bu_opt_desc *)BU_PTBL_GET(dptbl, 0);
	}
	while (!desc_found && (desc && desc->index != -1)) {
	    if (BU_STR_EQUAL(opt+offset, desc->shortopt) || BU_STR_EQUAL(opt+offset, desc->longopt)) {
		desc_found = 1;
		continue;
	    }
	    desc_ind++;
	    if (ds) {
		desc = &(ds[desc_ind]);
	    } else {
		desc = (struct bu_opt_desc *)BU_PTBL_GET(dptbl, desc_ind);
	    }
	}

	/* If we don't know what we're dealing with, keep going */
	if (!desc_found) {
	    struct bu_vls rebuilt_opt = BU_VLS_INIT_ZERO;
	    if (!unknowns) {
		BU_OPT_DATA_GET(unknowns, NULL);
		BU_GET(unknowns->args, struct bu_ptbl);
		bu_ptbl_init(unknowns->args, 8, "args init");
	    }
	    if (strlen(opt) == 1) {
		bu_vls_sprintf(&rebuilt_opt, "-%s", opt);
	    } else {
		bu_vls_sprintf(&rebuilt_opt, "--%s", opt);
	    }
	    bu_ptbl_ins(unknowns->args, (long *)bu_strdup(bu_vls_addr(&rebuilt_opt)));
	    bu_free(opt, "free opt");
	    bu_vls_free(&rebuilt_opt);
	    if (eq_arg)
		bu_ptbl_ins(unknowns->args, (long *)eq_arg);
	    i++;
	    continue;
	}

	/* Initialize with opt */
	BU_OPT_DATA_GET(data, opt);
	data->desc = desc;
	if (eq_arg) {
	    /* Okay, we actually need it - initialize the arg table */
	    BU_GET(data->args, struct bu_ptbl);
	    bu_ptbl_init(data->args, 8, "args init");
	    bu_ptbl_ins(data->args, (long *)eq_arg);
	    arg_cnt = 1;
	}

	/* handled the option - any remaining processing is on args, if any*/
	i = i + 1;

	/* If we already got an arg from the equals mechanism and we aren't
	 * supposed to have one, we're invalid */
	if (arg_cnt > 0 && desc->arg_cnt_max == 0) data->valid = 0;

	/* If we're looking for args, do so */
	if (desc->arg_cnt_max > 0) {
	    while (arg_cnt < desc->arg_cnt_max && i < argc && !is_opt(argv[i])) {
		ns = bu_strdup(argv[i]);
		if (!data->args) {
		    /* Okay, we actually need it - initialize the arg table */
		    BU_GET(data->args, struct bu_ptbl);
		    bu_ptbl_init(data->args, 8, "args init");
		}
		bu_ptbl_ins(data->args, (long *)ns);
		i++;
		arg_cnt++;
	    }
	    if (arg_cnt < desc->arg_cnt_min) {
		data->valid = 0;
	    }
	}

	/* Now see if we need to validate the arg(s) */
	if (desc->arg_process) {
	    int arg_offset = (*desc->arg_process)(NULL, data);
	    if (arg_offset < 0) {
		/* arg(s) present but not associated with opt, back them out of data
		 *
		 * test example for this - color option
		 *
		 * --color 200/30/10 input output  (1 arg to color, 3 args total)
		 * --color 200 30 10 input output  (3 args to color, 5 total)
		 *
		 *  to handle the color case, need to process all three in first case,
		 *  recognize that first one is sufficient, and release the latter two.
		 *  for the second case all three are necessary
		 *
		 * */
		int len = BU_PTBL_LEN(data->args);
		int j = 0;
		i = i + arg_offset;
		while (j > arg_offset) {
		    bu_free((void *)BU_PTBL_GET(data->args, len + j - 1), "free str");
		    j--;
		}
		bu_ptbl_trunc(data->args, len + arg_offset);
	    }
	}
	bu_ptbl_ins(opt_data, (long *)data);
    }
    if (unknowns) bu_ptbl_ins(opt_data, (long *)unknowns);

    return opt_data;
}

int
bu_opt_parse(struct bu_ptbl **tbl, struct bu_vls *msgs, int ac, const char **argv, struct bu_opt_desc *ds)
{
    struct bu_ptbl *results = NULL;
    if (!tbl || !argv || !ds) return 1;
    results = bu_opt_parse_internal(ac, argv, ds, NULL, msgs);
    if (results) {
	(*tbl) = results;
	return 0;
    } else {
	return 1;
    }
}

int
bu_opt_parse_str(struct bu_ptbl **tbl, struct bu_vls *msgs, const char *str, struct bu_opt_desc *ds)
{
    int ret = 0;
    char *input = NULL;
    char **argv = NULL;
    int argc = 0;
    if (!tbl || !str || !ds) return 1;
    input = bu_strdup(str);
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    ret = bu_opt_parse(tbl, msgs, argc, (const char **)argv, ds);

    bu_free(input, "free str copy");
    bu_free(argv, "free argv memory");
    return ret;
}

int
bu_opt_parse_dtbl(struct bu_ptbl **tbl, struct bu_vls *msgs, int ac, const char **argv, struct bu_ptbl *dtbl)
{
    struct bu_ptbl *results = NULL;
    if (!tbl || !argv || !dtbl) return 1;
    results = bu_opt_parse_internal(ac, argv, NULL, dtbl, msgs);
    if (results) {
	(*tbl) = results;
	return 0;
    } else {
	return 1;
    }
}

int
bu_opt_parse_str_dtbl(struct bu_ptbl **tbl, struct bu_vls *msgs, const char *str, struct bu_ptbl *dtbl)
{
    int ret = 0;
    char *input = NULL;
    char **argv = NULL;
    int argc = 0;
    if (!tbl || !str || !dtbl) return 1;
    input = bu_strdup(str);
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    ret = bu_opt_parse_dtbl(tbl, msgs, argc, (const char **)argv, dtbl);

    bu_free(input, "free str copy");
    bu_free(argv, "free argv memory");
    return ret;
}


struct bu_opt_data *
bu_opt_find_name(const char *name, struct bu_ptbl *opts)
{
    size_t i;
    if (!opts) return NULL;

    for (i = 0; i < BU_PTBL_LEN(opts); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	/* Don't check the unknown opts - they were already marked as not
	 * valid opts per the current descriptions in the parsing pass */
	if (!name && !opt->name) return opt;
	if (!opt->name) continue;
	if (!opt->desc) continue;
	if (BU_STR_EQUAL(opt->desc->shortopt, name) || BU_STR_EQUAL(opt->desc->longopt, name)) {
	    /* option culling guarantees us one "winner" if multiple instances
	     * of an option were originally supplied, so if we find a match we
	     * have found what we wanted.  Now, just need to check validity */
	    return (opt->valid) ? opt : NULL;
	}
    }
    return NULL;
}

struct bu_opt_data *
bu_opt_find(int key, struct bu_ptbl *opts)
{
    size_t i;
    if (!opts) return NULL;

    for (i = 0; i < BU_PTBL_LEN(opts); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	/* Don't check the unknown opts - they were already marked as not
	 * valid opts per the current descriptions in the parsing pass */
	if (key == -1 && !opt->name) return opt;
	if (!opt->name) continue;
	if (!opt->desc) continue;
	if (key == opt->desc->index) {
	    /* option culling guarantees us one "winner" if multiple instances
	     * of an option were originally supplied, so if we find a match we
	     * have found what we wanted.  Now, just need to check validity */
	    return (opt->valid) ? opt : NULL;
	}
    }
    return NULL;
}

void
bu_opt_data_init(struct bu_opt_data *d)
{
    if (!d) return;
    d->desc = NULL;
    d->valid = 0;
    d->name = NULL;
    d->args = NULL;
    d->user_data = NULL;
}

void
bu_opt_data_free(struct bu_opt_data *d)
{
    if (!d) return;
    if (d->name) bu_free((char *)d->name, "free data name");
    if (d->user_data) bu_free(d->user_data, "free user data");
    if (d->args) {
	size_t i;
	for (i = 0; i < BU_PTBL_LEN(d->args); i++) {
	    char *arg = (char *)BU_PTBL_GET(d->args, i);
	    bu_free(arg, "free arg");
	}
	bu_ptbl_free(d->args);
	BU_PUT(d->args, struct bu_ptbl);
    }
}


void
bu_opt_data_free_tbl(struct bu_ptbl *tbl)
{
    size_t i;
    if (!tbl) return;
    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(tbl, i);
	bu_opt_data_free(opt);
    }
    bu_ptbl_free(tbl);
    BU_PUT(tbl, struct bu_ptbl);
}

const char *
bu_opt_data_arg(struct bu_opt_data *d, size_t ind)
{
    if (!d) return NULL;
    if (d->args) {
	if (ind > (BU_PTBL_LEN(d->args) - 1)) return NULL;
	return (const char *)BU_PTBL_GET(d->args, ind);
    }
    return NULL;
}

void
bu_opt_data_print(const char *title, struct bu_ptbl *data)
{
    size_t i = 0;
    size_t j = 0;
    int offset_1 = 3;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    if (!data || BU_PTBL_LEN(data) == 0) return;
    if (title) {
	bu_vls_sprintf(&log, "%s\n", title);
    } else {
	bu_vls_sprintf(&log, "Options:\n");
    }
    for (i = 0; i < BU_PTBL_LEN(data); i++) {
	struct bu_opt_data *d = (struct bu_opt_data *)BU_PTBL_GET(data, i);
	if (d->name) {
	    bu_vls_printf(&log, "%*s%s", offset_1, " ", d->name);
	    if (d->valid) {
		bu_vls_printf(&log, "\t(valid)");
	    } else {
		bu_vls_printf(&log, "\t(invalid)");
	    }
	    if (d->desc && d->desc->arg_cnt_max > 0) {
		if (d->args && BU_PTBL_LEN(d->args) > 0) {
		    bu_vls_printf(&log, ": ");
		    for (j = 0; j < BU_PTBL_LEN(d->args) - 1; j++) {
			bu_vls_printf(&log, "%s, ", bu_opt_data_arg(d, j));
		    }
		    bu_vls_printf(&log, "%s\n", bu_opt_data_arg(d, BU_PTBL_LEN(d->args) - 1));
		}
	    } else {
		bu_vls_printf(&log, "\n");
	    }
	} else {
	    bu_vls_printf(&log, "%*s(unknown): ", offset_1, " ", d->name);
	    if (d->args && BU_PTBL_LEN(d->args) > 0) {
		for (j = 0; j < BU_PTBL_LEN(d->args) - 1; j++) {
		    bu_vls_printf(&log, "%s ", bu_opt_data_arg(d, j));
		}
		bu_vls_printf(&log, "%s\n", bu_opt_data_arg(d, BU_PTBL_LEN(d->args) - 1));
	    }
	}
    }
    bu_log("%s", bu_vls_addr(&log));
    bu_vls_free(&log);
}


HIDDEN void
bu_opt_desc_init_entry(struct bu_opt_desc *d)
{
    if (!d) return;
    d->index = -1;
    d->arg_cnt_min = 0;
    d->arg_cnt_max = 0;
    d->shortopt = NULL;
    d->longopt = NULL;
    d->arg_process = NULL;
    d->shortopt_doc = NULL;
    d->longopt_doc = NULL;
    d->help_string = NULL;
}

HIDDEN void
bu_opt_desc_set(struct bu_opt_desc *d, int ind,
	size_t min, size_t max, const char *shortopt,
	const char *longopt, bu_opt_arg_process_t arg_process,
	const char *shortopt_doc, const char *longopt_doc, const char *help_str)
{
    if (!d) return;
    d->index = ind;
    d->arg_cnt_min = min;
    d->arg_cnt_max = max;
    d->shortopt = (shortopt) ? bu_strdup(shortopt) : NULL;
    d->longopt = (longopt) ? bu_strdup(longopt) : NULL;
    d->arg_process = arg_process;
    d->shortopt_doc = (shortopt_doc) ? bu_strdup(shortopt_doc) : NULL;
    d->longopt_doc = (longopt_doc) ? bu_strdup(longopt_doc) : NULL;
    d->help_string = (help_str) ? bu_strdup(help_str) : NULL;;
}

HIDDEN void
bu_opt_desc_free_entry(struct bu_opt_desc *d)
{
    if (!d) return;
    if (d->shortopt) bu_free((char *)d->shortopt, "shortopt");
    if (d->longopt) bu_free((char *)d->longopt, "longopt");
    if (d->shortopt_doc) bu_free((char *)d->shortopt_doc, "shortopt_doc");
    if (d->longopt_doc) bu_free((char *)d->longopt_doc, "longopt_doc");
    if (d->help_string) bu_free((char *)d->help_string, "help_string");
}
void
bu_opt_desc_init(bu_opt_dtbl_t **dtbl, struct bu_opt_desc *ds)
{
    size_t i;
    size_t array_cnt = 0;
    struct bu_opt_desc *cd;
    struct bu_ptbl *tbl;
    if (!dtbl) return;
    BU_GET(tbl, struct bu_ptbl);
    if (!(!ds || ds[0].index == -1)) {
	while (ds[array_cnt].index != -1) array_cnt++;
	bu_ptbl_init(tbl, array_cnt + 1, "new ptbl");
	for (i = 0; i < array_cnt; i++) {
	    struct bu_opt_desc *d = &(ds[i]);
	    BU_GET(cd, struct bu_opt_desc);
	    bu_opt_desc_init_entry(cd);
	    bu_opt_desc_set(cd, d->index, d->arg_cnt_min, d->arg_cnt_max,
		    d->shortopt, d->longopt, d->arg_process,
		    d->shortopt_doc, d->longopt_doc,
		    d->help_string);
	    bu_ptbl_ins(tbl, (long *)cd);
	}
    } else {
	bu_ptbl_init(tbl, 8, "new ptbl");
    }
    BU_GET(cd, struct bu_opt_desc);
    bu_opt_desc_init_entry(cd);
    bu_ptbl_ins(tbl, (long *)cd);
    (*dtbl) = tbl;
}

void
bu_opt_desc_add(bu_opt_dtbl_t *tbl, int ind, size_t min, size_t max, const char *shortopt,
	const char *longopt, bu_opt_arg_process_t arg_process,
	const char *shortopt_doc, const char *longopt_doc, const char *help_str)
{
    struct bu_opt_desc *d;
    long *dn;
    if (!tbl || (!shortopt && !longopt)) return;
    dn = BU_PTBL_GET(tbl, BU_PTBL_LEN(tbl) - 1);
    bu_ptbl_rm(tbl, dn);
    BU_GET(d, struct bu_opt_desc);
    bu_opt_desc_init_entry(d);
    bu_opt_desc_set(d, ind, min, max, shortopt,
	    longopt, arg_process, shortopt_doc,
	    longopt_doc, help_str);
    bu_ptbl_ins(tbl, (long *)d);
    bu_ptbl_ins(tbl, dn);
}

void
bu_opt_desc_del(bu_opt_dtbl_t *tbl, int key)
{
    size_t i = 0;
    struct bu_ptbl tmp_tbl;
    if (!tbl || key < 0) return;
    bu_ptbl_init(&tmp_tbl, 64, "tmp tbl");
    for (i = 0; i < BU_PTBL_LEN(tbl) - 1; i++) {
	struct bu_opt_desc *d = (struct bu_opt_desc *)BU_PTBL_GET(tbl, i);
	if (d->index == key) {
	    bu_ptbl_ins(&tmp_tbl, (long *)d);
	}
    }
    for (i = 0; i < BU_PTBL_LEN(&tmp_tbl); i++) {
	bu_ptbl_rm(tbl, BU_PTBL_GET(&tmp_tbl, i));
    }
    bu_ptbl_free(&tmp_tbl);
}

void
bu_opt_desc_del_name(bu_opt_dtbl_t *tbl, const char *name)
{
    size_t i = 0;
    struct bu_ptbl tmp_tbl;
    if (!tbl || !name) return;
    bu_ptbl_init(&tmp_tbl, 64, "tmp tbl");
    for (i = 0; i < BU_PTBL_LEN(tbl) - 1; i++) {
	struct bu_opt_desc *d = (struct bu_opt_desc *)BU_PTBL_GET(tbl, i);
	if (BU_STR_EQUAL(d->shortopt, name) || BU_STR_EQUAL(d->longopt, name)) {
	    bu_ptbl_ins(&tmp_tbl, (long *)d);
	}
    }
    for (i = 0; i < BU_PTBL_LEN(&tmp_tbl); i++) {
	bu_ptbl_rm(tbl, BU_PTBL_GET(&tmp_tbl, i));
    }
    bu_ptbl_free(&tmp_tbl);
}

void
bu_opt_desc_free(bu_opt_dtbl_t *tbl)
{
    size_t i;
    if (!tbl) return;
    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct bu_opt_desc *opt = (struct bu_opt_desc *)BU_PTBL_GET(tbl, i);
	bu_opt_desc_free_entry(opt);
    }
    bu_ptbl_free(tbl);
    BU_PUT(tbl, struct bu_ptbl);
}

HIDDEN void
wrap_help(struct bu_vls *help, int indent, int offset, int len)
{
    int i = 0;
    char *input = NULL;
    char **argv = NULL;
    int argc = 0;
    struct bu_vls new_help = BU_VLS_INIT_ZERO;
    struct bu_vls working = BU_VLS_INIT_ZERO;
    bu_vls_trunc(&working, 0);
    bu_vls_trunc(&new_help, 0);

    input = bu_strdup(bu_vls_addr(help));
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    for (i = 0; i < argc; i++) {
	int avl = strlen(argv[i]);
	if ((int)bu_vls_strlen(&working) + avl + 1 > len) {
	    bu_vls_printf(&new_help, "%s\n%*s", bu_vls_addr(&working), offset+indent, " ");
	    bu_vls_trunc(&working, 0);
	}
	bu_vls_printf(&working, "%s ", argv[i]);
    }
    bu_vls_printf(&new_help, "%s", bu_vls_addr(&working));

    bu_vls_sprintf(help, "%s", bu_vls_addr(&new_help));
    bu_vls_free(&new_help);
    bu_vls_free(&working);
    bu_free(input, "input");
    bu_free(argv, "argv");
}

HIDDEN const char *
bu_opt_describe_internal_ascii(struct bu_opt_desc *ds, bu_opt_dtbl_t *tbl, struct bu_opt_desc_opts *settings)
{
    size_t i = 0;
    size_t j = 0;
    size_t opt_cnt = 0;
    int offset = 2;
    int opt_cols = 28;
    int desc_cols = 50;
    /*
    bu_opt_desc_t desc_type = BU_OPT_FULL;
    bu_opt_format_t format_type = BU_OPT_ASCII;
    */
    const char *finalized;
    struct bu_vls description = BU_VLS_INIT_ZERO;
    int *status;
    if (((!ds || ds[0].index == -1) && !tbl) || (ds && tbl)) return NULL;

    if (settings) {
	offset = settings->offset;
	opt_cols = settings->option_columns;
	desc_cols = settings->description_columns;
    }

    if (ds) {
	while (ds[i].index != -1) i++;
    } else {
	int tbl_len = BU_PTBL_LEN(tbl) - 1;
	i = (tbl_len < 0) ? 0 : tbl_len;
    }
    if (i == 0) return NULL;
    opt_cnt = i;
    status = (int *)bu_calloc(opt_cnt, sizeof(int), "opt status");
    i = 0;
    while (i < opt_cnt) {
	struct bu_opt_desc *curr;
	struct bu_opt_desc *d;
	curr = (ds) ? &(ds[i]) : (struct bu_opt_desc *)BU_PTBL_GET(tbl, i) ;
	if (!status[i]) {
	    struct bu_vls opts = BU_VLS_INIT_ZERO;
	    struct bu_vls help_str = BU_VLS_INIT_ZERO;

	    /* We handle all entries with the same key in the same
	     * pass, so set the status flags accordingly */
	    j = i;
	    while (j < opt_cnt) {
		d = (ds) ? &(ds[j]) : (struct bu_opt_desc *)BU_PTBL_GET(tbl, j);
		if (d->index == curr->index) {
		    status[j] = 1;
		}
		j++;
	    }

	    /* Collect the short options first - may be multiple instances with
	     * the same index defining aliases, so accumulate all of them. */
	    j = i;
	    while (j < opt_cnt) {
		d = (ds) ? &(ds[j]) : (struct bu_opt_desc *)BU_PTBL_GET(tbl, j);
		if (d->index == curr->index) {
		    int new_len = strlen(d->shortopt_doc);
		    if (new_len > 0) {
			if ((int)bu_vls_strlen(&opts) + new_len + offset + 2 > opt_cols + desc_cols) {
			    bu_vls_printf(&description, "%*s%s\n", offset, " ", bu_vls_addr(&opts));
			    bu_vls_sprintf(&opts, "%s, ", d->shortopt_doc);
			} else {
			    bu_vls_printf(&opts, "%s, ", d->shortopt_doc);
			}
		    }
		    /* While we're at it, pick up the string.  The last string with
		     * a matching key wins, as long as its not empty */
		    if (strlen(d->help_string) > 0) bu_vls_sprintf(&help_str, "%s", d->help_string);
		}
		j++;
	    }

	    /* Now do the long opts */
	    j = i;
	    while (j < opt_cnt) {
		d = (ds) ? &(ds[j]) : (struct bu_opt_desc *)BU_PTBL_GET(tbl, j);
		if (d->index == curr->index) {
		    int new_len = strlen(d->longopt_doc);
		    if (new_len > 0) {
			if ((int)bu_vls_strlen(&opts) + new_len + offset + 2 > opt_cols + desc_cols) {
			    bu_vls_printf(&description, "%*s%s\n", offset, " ", bu_vls_addr(&opts));
			    bu_vls_sprintf(&opts, "%s, ", d->longopt_doc);
			} else {
			    bu_vls_printf(&opts, "%s, ", d->longopt_doc);
			}
		    }
		}
		j++;
	    }

	    bu_vls_trunc(&opts, -2);
	    bu_vls_printf(&description, "%*s%s", offset, " ", bu_vls_addr(&opts));
	    if ((int)bu_vls_strlen(&opts) > opt_cols) {
		bu_vls_printf(&description, "\n%*s", opt_cols + offset, " ");
	    } else {
		bu_vls_printf(&description, "%*s", opt_cols - (int)bu_vls_strlen(&opts), " ");
	    }
	    if ((int)bu_vls_strlen(&help_str) > desc_cols) {
		wrap_help(&help_str, offset, opt_cols+offset, desc_cols);
	    }
	    bu_vls_printf(&description, "%*s%s\n", offset, " ", bu_vls_addr(&help_str));
	    bu_vls_free(&help_str);
	    bu_vls_free(&opts);
	    status[i] = 1;
	}
	i++;
    }
    finalized = bu_strdup(bu_vls_addr(&description));
    bu_vls_free(&description);
    return finalized;
}

const char *
bu_opt_describe(struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings)
{
    if (!ds) return NULL;
    if (!settings) return bu_opt_describe_internal_ascii(ds, NULL, NULL);
    return NULL;
}

const char *
bu_opt_describe_dtbl(bu_opt_dtbl_t *dtbl, struct bu_opt_desc_opts *settings)
{
    if (!dtbl) return NULL;
    if (!settings) return bu_opt_describe_internal_ascii(NULL, dtbl, NULL);
    return NULL;
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
