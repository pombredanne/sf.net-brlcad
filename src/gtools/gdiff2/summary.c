/*                     G D I F F 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "./gdiff2.h"


/*******************************************************************/
/* Output generators for diff log */
/*******************************************************************/

static int
diff_log_add(const char *attr_name, const char *attr_val, void *data)
{
    bu_vls_printf((struct bu_vls *)data, "+ %s: %s\n", attr_name, attr_val);
    return 0;
}

static int
diff_log_del(const char *attr_name, const char *UNUSED(attr_val), void *data)
{
    bu_vls_printf((struct bu_vls *)data, "- %s\n", attr_name);
    return 0;
}

static int
diff_log_chgd(const char *attr_name, const char *attr_val_left, const char *attr_val_right, void *data)
{
    bu_vls_printf((struct bu_vls *)data, "- %s: %s\n+ %s: %s\n", attr_name, attr_val_left, attr_name, attr_val_right);
    return 0;
}

static int
diff_log_unchgd(const char *attr_name, const char *attr_val, void *data)
{
    bu_vls_printf((struct bu_vls *)data, "  %s: %s\n", attr_name, attr_val);
    return 0;
}

void
diff_attrs_print(struct diff_result *dr, struct diff_state *state, struct bu_vls *diff_log) {
    struct bu_vls tmp_param_str = BU_VLS_INIT_ZERO;
    struct bu_vls tmp_attr_str = BU_VLS_INIT_ZERO;
    if (state->use_params == 1) {
	int (*add_func)(const char *, const char *, void *) = NULL;
	int (*del_func)(const char *, const char *, void *) = NULL;
	int (*chgd_func)(const char *, const char *, const char *, void *) = NULL;
	int (*unchgd_func)(const char *, const char *, void *) = NULL;
	if (state->return_added && dr->param_state & DIFF_ADDED) add_func = diff_log_add;
	if (state->return_removed && dr->param_state & DIFF_REMOVED) del_func = diff_log_del;
	if (state->return_changed && dr->param_state & DIFF_CHANGED) chgd_func = diff_log_chgd;
	if (state->return_unchanged && dr->param_state & DIFF_UNCHANGED) unchgd_func = diff_log_unchgd;
	(void)db_avs_diff(dr->left_param_avs, dr->right_param_avs, state->diff_tol, add_func, del_func, chgd_func, unchgd_func, (void *)&tmp_param_str);
	if (strlen(bu_vls_addr(&tmp_param_str))) {
	    bu_vls_printf(diff_log, "Object %s parameters:\n", dr->obj_name);
	    bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&tmp_param_str));
	}

    }
    if (state->use_attrs == 1) {
	int (*add_func)(const char *, const char *, void *) = NULL;
	int (*del_func)(const char *, const char *, void *) = NULL;
	int (*chgd_func)(const char *, const char *, const char *, void *) = NULL;
	int (*unchgd_func)(const char *, const char *, void *) = NULL;
	if (state->return_added && dr->attr_state & DIFF_ADDED) add_func = diff_log_add;
	if (state->return_removed && dr->attr_state & DIFF_REMOVED) del_func = diff_log_del;
	if (state->return_changed && dr->attr_state & DIFF_CHANGED) chgd_func = diff_log_chgd;
	if (state->return_unchanged && dr->attr_state & DIFF_UNCHANGED) unchgd_func = diff_log_unchgd;
	(void)db_avs_diff(dr->left_attr_avs, dr->right_attr_avs, state->diff_tol, add_func, del_func, chgd_func, unchgd_func, (void *)&tmp_attr_str);
	if (strlen(bu_vls_addr(&tmp_attr_str))) {
	    bu_vls_printf(diff_log, "Object %s attributes:\n", dr->obj_name);
	    bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&tmp_attr_str));
	}
    }

    bu_vls_free(&tmp_param_str);
    bu_vls_free(&tmp_attr_str);
}

void
diff_summarize(struct bu_vls *diff_log, const struct bu_ptbl *results, struct diff_state *state)
{
    int i = 0;
    struct bu_vls added_list = BU_VLS_INIT_ZERO;
    struct bu_vls removed_list = BU_VLS_INIT_ZERO;
    struct bu_vls changed_list = BU_VLS_INIT_ZERO;
    struct bu_vls unchanged_list = BU_VLS_INIT_ZERO;
    struct bu_vls added_tmp = BU_VLS_INIT_ZERO;
    struct bu_vls removed_tmp = BU_VLS_INIT_ZERO;
    struct bu_vls changed_tmp = BU_VLS_INIT_ZERO;
    struct bu_vls unchanged_tmp = BU_VLS_INIT_ZERO;


    if (state->verbosity < 1 || !results)
	return;

    if (state->verbosity == 1) {
	if (state->return_added == 1) bu_vls_sprintf(&added_list, "Added:\n");
	if (state->return_removed == 1) bu_vls_sprintf(&removed_list, "Removed:\n");
	if (state->return_changed == 1) bu_vls_sprintf(&changed_list, "Changed:\n");
	if (state->return_unchanged == 1) bu_vls_sprintf(&unchanged_list, "Unchanged:\n");
    }

    for (i = 0; i < (int)BU_PTBL_LEN(results); i++) {
	struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(results, i);
	if (dr->param_state == DIFF_ADDED && state->return_added == 1) {
	    switch (state->verbosity) {
		case 1:
		    if (strlen(bu_vls_addr(&added_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			bu_vls_printf(&added_list, "%s\n", bu_vls_addr(&added_tmp));
			bu_vls_sprintf(&added_tmp, "%s", dr->obj_name);
		    } else {
			if (strlen(bu_vls_addr(&added_tmp)) > 0) bu_vls_printf(&added_tmp, ", ");
			bu_vls_printf(&added_tmp, "%s", dr->obj_name);
		    }
		    break;
		case 2:
		    bu_vls_printf(diff_log, "A %s\n", dr->obj_name);
		    break;
		case 3:
		    diff_attrs_print(dr, state, diff_log);
		    break;
		default:
		    break;
	    }
	}
	if (dr->param_state == DIFF_REMOVED && state->return_removed == 1) {
	    switch (state->verbosity) {
		case 1:
		    if (strlen(bu_vls_addr(&removed_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			bu_vls_printf(&removed_list, "%s\n", bu_vls_addr(&removed_tmp));
			bu_vls_sprintf(&removed_tmp, "%s", dr->obj_name);
		    } else {
			if (strlen(bu_vls_addr(&removed_tmp)) > 0) bu_vls_printf(&removed_tmp, ", ");
			bu_vls_printf(&removed_tmp, "%s", dr->obj_name);
		    }
		    break;
		case 2:
		    bu_vls_printf(diff_log, "D %s\n", dr->obj_name);
		    break;
		case 3:
		    diff_attrs_print(dr, state, diff_log);
		    break;
		default:
		    break;
	    }
	}
	if (dr->param_state == DIFF_UNCHANGED && dr->attr_state == DIFF_UNCHANGED && state->return_unchanged == 1) {
		switch (state->verbosity) {
		    case 1:
			if (strlen(bu_vls_addr(&unchanged_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			    bu_vls_printf(&unchanged_list, "%s\n", bu_vls_addr(&unchanged_tmp));
			    bu_vls_sprintf(&unchanged_tmp, "%s", dr->obj_name);
			} else {
			    if (strlen(bu_vls_addr(&unchanged_tmp)) > 0) bu_vls_printf(&unchanged_tmp, ", ");
			    bu_vls_printf(&unchanged_tmp, "%s", dr->obj_name);
			}
			break;
		    case 2:
			bu_vls_printf(diff_log, "  %s\n", dr->obj_name);
			break;
		    case 3:
			diff_attrs_print(dr, state, diff_log);
			break;
		    default:
			break;
		}
	}

	if ((dr->param_state != DIFF_UNCHANGED || dr->attr_state != DIFF_UNCHANGED) && dr->param_state != DIFF_ADDED && dr->param_state != DIFF_REMOVED && state->return_changed == 1) {
	    switch (state->verbosity) {
		case 1:

		    if (strlen(bu_vls_addr(&changed_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			bu_vls_printf(&changed_list, "%s\n", bu_vls_addr(&changed_tmp));
			bu_vls_sprintf(&changed_tmp, "%s", dr->obj_name);
		    } else {
			if (strlen(bu_vls_addr(&changed_tmp)) > 0) bu_vls_printf(&changed_tmp, ", ");
			bu_vls_printf(&changed_tmp, "%s", dr->obj_name);
		    }
		    break;
		case 2:
		    bu_vls_printf(diff_log, "M %s\n", dr->obj_name);
		    break;
		case 3:
		    diff_attrs_print(dr, state, diff_log);
		    break;
		default:
		    break;
	    }
	}
    }

    if (state->verbosity == 1) {
	bu_vls_printf(&added_list, "%s\n", bu_vls_addr(&added_tmp));
	bu_vls_printf(&removed_list, "%s\n", bu_vls_addr(&removed_tmp));
	bu_vls_printf(&changed_list, "%s\n", bu_vls_addr(&changed_tmp));
	bu_vls_printf(&unchanged_list, "%s\n", bu_vls_addr(&unchanged_tmp));
	if (state->return_added == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&added_list));
	if (state->return_removed == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&removed_list));
	if (state->return_changed == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&changed_list));
	if (state->return_unchanged == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&unchanged_list));
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
