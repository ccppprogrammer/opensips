/*
 * Copyright (C) 2018 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,USA
 */

#include <tap.h>

#include "../../str.h"
#include "../../cachedb/cachedb.h"
#include "../../cachedb/cachedb_cap.h"
#include "../../sr_module.h"
#include "../../modparam.h"

extern cachedb_engine* lookup_cachedb(str *name);

int dict_cmp(const cdb_dict_t *a, const cdb_dict_t *b);
int val_cmp(const cdb_val_t *v1, const cdb_val_t *v2);

cdb_val_t *dict_fetch(const str *key, const cdb_dict_t *dict)
{
	struct list_head *_;
	cdb_kv_t *pair;

	list_for_each(_, dict) {
		pair = list_entry(_, cdb_kv_t, list);

		if (!str_strcmp(&pair->key.name, key))
			return &pair->val;
	}

	return NULL;
}

int dict_cmp(const cdb_dict_t *a, const cdb_dict_t *b)
{
	const struct list_head *p1, *p2;
	cdb_kv_t *pair;
	cdb_val_t *val;

	/* different # of pairs? */
	for (p1 = a, p2 = b; p1 != a && p2 != b; p1 = p1->next, p2 = p2->next)
		{}
	if (p1 != a || p2 != b)
		return 1;

	list_for_each(p1, a) {
		pair = list_entry(p1, cdb_kv_t, list);

		val = dict_fetch(&pair->key.name, b);
		if (!val)
			return 1;

		if (val_cmp(&pair->val, val))
			return 1;
	}

	return 0;
}

int val_cmp(const cdb_val_t *v1, const cdb_val_t *v2)
{
	switch (v1->type) {
	case CDB_INT32:
		return v1->val.i32 != v2->val.i32;
	case CDB_INT64:
		return v1->val.i64 != v2->val.i64;
	case CDB_DICT:
		return dict_cmp(&v1->val.dict, &v2->val.dict);
	case CDB_STR:
		return str_strcmp(&v1->val.st, &v2->val.st);
	case CDB_NULL:
		return 0;
		break;
	default:
		LM_BUG("unsupported type: %d\n", v1->type);
		return 1;
	}
}

int has_kv(const cdb_dict_t *haystack, const char *key, const cdb_val_t *val)
{
	str skey;
	cdb_val_t *needle;

	init_str(&skey, key);

	needle = dict_fetch(&skey, haystack);
	if (!needle || needle->type != val->type)
		return 0;

	return !val_cmp(needle, val);
}

int res_has_kv(const cdb_res_t *res, const char *key, const cdb_val_t *val)
{
	struct list_head *_;
	cdb_row_t *row;

	list_for_each(_, &res->rows) {
		row = list_entry(_, cdb_row_t, list);
		if (has_kv(&row->dict, key, val))
			return 1;
	}

	return 0;
}

int test_get_rows_int32(cachedb_funcs *api, cachedb_con *con)
{
	return 1;
}

int test_get_rows_int64(cachedb_funcs *api, cachedb_con *con)
{
	return 1;
}

int test_get_rows_dict(cachedb_funcs *api, cachedb_con *con)
{
	return 1;
}

int test_get_rows_null(cachedb_funcs *api, cachedb_con *con)
{
	return 1;
}

static int test_get_rows_str(cachedb_funcs *api, cachedb_con *con)
{
	cdb_key_t key;
	str sa = str_init("A"), sb = str_init("B"), sc = str_init("C"),
	    sd = str_init("D");
	int_str_t isv;
	cdb_filter_t *filter;
	cdb_res_t res;
	cdb_val_t cdb_val;

	if (CACHEDB_CAPABILITY(api, CACHEDB_CAP_TRUNCATE))
		ok(api->truncate(con) == 0, "truncate");

	init_str(&key.name, "tgr_1");
	ok(api->set(con, &key.name, &sa, 0) == 0, "test_get_rows: set A");

	init_str(&key.name, "tgr_2");
	ok(api->set(con, &key.name, &sb, 0) == 0, "test_get_rows: set B");

	init_str(&key.name, "tgr_3");
	ok(api->set(con, &key.name, &sc, 0) == 0, "test_get_rows: set C");

	init_str(&key.name, "tgr_4");
	ok(api->set(con, &key.name, &sd, 0) == 0, "test_get_rows: set D");

	memset(&key, 0, sizeof key);
	init_str(&key.name, "opensips");

	isv.is_str = 1;
	isv.s = sd;
	cdb_val.type = CDB_STR;

	/* single filter tests */

	filter = cdb_append_filter(NULL, &key, CDB_OP_LE, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 4 items");
	ok(res.count == 4, "test_get_rows: have 4 items");
	cdb_val.val.st = sa; ok(res_has_kv(&res, "opensips", &cdb_val), "has A");
	cdb_val.val.st = sb; ok(res_has_kv(&res, "opensips", &cdb_val), "has B");
	cdb_val.val.st = sc; ok(res_has_kv(&res, "opensips", &cdb_val), "has C");
	cdb_val.val.st = sd; ok(res_has_kv(&res, "opensips", &cdb_val), "has D");
	init_str(&cdb_val.val.st, "Z");
	ok(!res_has_kv(&res, "opensips", &cdb_val), "!has Z");
	cdb_free_rows(&res);
	cdb_free_filters(filter);

	filter = cdb_append_filter(NULL, &key, CDB_OP_LT, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 3 items");
	ok(res.count == 3, "test_get_rows: have 3 items");
	cdb_val.val.st = sa; ok(res_has_kv(&res, "opensips", &cdb_val), "has A");
	cdb_val.val.st = sb; ok(res_has_kv(&res, "opensips", &cdb_val), "has B");
	cdb_val.val.st = sc; ok(res_has_kv(&res, "opensips", &cdb_val), "has C");
	cdb_val.val.st = sd; ok(!res_has_kv(&res, "opensips", &cdb_val), "!has D");
	cdb_free_rows(&res);
	cdb_free_filters(filter);

	init_str(&isv.s, "A");
	filter = cdb_append_filter(NULL, &key, CDB_OP_LT, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 0 items");
	ok(res.count == 0, "test_get_rows: have 0 items");
	cdb_val.val.st = sa; ok(!res_has_kv(&res, "opensips", &cdb_val), "!has A");
	cdb_val.val.st = sc; ok(!res_has_kv(&res, "opensips", &cdb_val), "!has C");
	cdb_free_rows(&res);
	cdb_free_filters(filter);

	filter = cdb_append_filter(NULL, &key, CDB_OP_LE, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 1 item");
	ok(res.count == 1, "test_get_rows: have 1 item");
	cdb_val.val.st = sa; ok(res_has_kv(&res, "opensips", &cdb_val), "has A");
	cdb_val.val.st = sb; ok(!res_has_kv(&res, "opensips", &cdb_val), "!has B");
	cdb_free_rows(&res);
	cdb_free_filters(filter);

	/* multi filter tests */

	init_str(&isv.s, "D");
	filter = cdb_append_filter(NULL, &key, CDB_OP_LT, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 3 items");
	ok(res.count == 3, "test_get_rows: have 3 items");
	init_str(&isv.s, "A");
	filter = cdb_append_filter(filter, &key, CDB_OP_GE, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 3 items");
	ok(res.count == 3, "test_get_rows: have 3 items");
	init_str(&isv.s, "B");
	filter = cdb_append_filter(filter, &key, CDB_OP_GT, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 1 item");
	ok(res.count == 1, "test_get_rows: have 1 item");
	init_str(&isv.s, "C");
	filter = cdb_append_filter(filter, &key, CDB_OP_EQ, &isv);
	ok(api->get_rows(con, filter, &res) == 0, "test_get_rows: get 1 item");
	ok(res.count == 1, "test_get_rows: have 1 item");
	cdb_free_rows(&res);
	cdb_free_filters(filter);

	if (CACHEDB_CAPABILITY(api, CACHEDB_CAP_TRUNCATE))
		ok(api->truncate(con) == 0, "truncate");

	return 1;
}

static int test_get_rows(cachedb_funcs *api, cachedb_con *con)
{
	int rc = 1;

	rc &= ok(test_get_rows_str(api, con), "test_get_rows - str tests");
	todo();
	rc &= ok(test_get_rows_int32(api, con), "test_get_rows - int32 tests");
	rc &= ok(test_get_rows_int64(api, con), "test_get_rows - int64 tests");
	rc &= ok(test_get_rows_dict(api, con), "test_get_rows - dict tests");
	rc &= ok(test_get_rows_null(api, con), "test_get_rows - null tests");
	end_todo;

	return rc;
}

static int test_set_cols(cachedb_funcs *api, cachedb_con *con)
{
	cdb_filter_t *filter;
	int_str_t isv;
	cdb_key_t key;
	str subkey;
	cdb_dict_t pairs;
	cdb_kv_t *pair, *dict_pair;

	cdb_pkey_init(&key, "aor");
	init_str(&isv.s, "foobar@opensips.org"); isv.is_str = 1;
	filter = cdb_append_filter(NULL, &key, CDB_OP_EQ, &isv);

	cdb_dict_init(&pairs);

	cdb_key_init(&key, "key"); init_str(&subkey, "subkey-null");
	pair = cdb_mk_pair(&key, &subkey);
	pair->val.type = CDB_NULL;
	cdb_dict_add(pair, &pairs);

	cdb_key_init(&key, "key"); init_str(&subkey, "subkey-32bit");
	pair = cdb_mk_pair(&key, &subkey);
	pair->val.type = CDB_INT32;
	pair->val.val.i32 = 2147483647;
	cdb_dict_add(pair, &pairs);

	cdb_key_init(&key, "key"); init_str(&subkey, "subkey-64bit");
	pair = cdb_mk_pair(&key, &subkey);
	pair->val.type = CDB_INT64;
	pair->val.val.i64 = 9223372036854775807;
	cdb_dict_add(pair, &pairs);

	cdb_key_init(&key, "key"); init_str(&subkey, "subkey-str");
	pair = cdb_mk_pair(&key, &subkey);
	pair->val.type = CDB_STR;
	init_str(&pair->val.val.st, pkg_strdup("31337"));
	cdb_dict_add(pair, &pairs);

	/* set a dict subkey which contains a dict */

	cdb_key_init(&key, "key"); init_str(&subkey, "subkey-dict");
	dict_pair = cdb_mk_pair(&key, &subkey);
	dict_pair->val.type = CDB_DICT;
	cdb_dict_init(&dict_pair->val.val.dict);
	cdb_key_init(&key, "foo");
	pair = cdb_mk_pair(&key, NULL);
	pair->val.type = CDB_INT32;
	pair->val.val.i32 = 373333337;
	cdb_dict_add(pair, &dict_pair->val.val.dict);
	cdb_key_init(&key, "bar");
	pair = cdb_mk_pair(&key, NULL);
	pair->val.type = CDB_STR;
	init_str(&pair->val.val.st, pkg_strdup("hello, world!"));
	cdb_dict_add(pair, &dict_pair->val.val.dict);
	cdb_dict_add(dict_pair, &pairs);

	ok(api->set_cols(con, filter, &pairs) == 0, "test_set_cols");
	cdb_free_entries(&pairs);
	cdb_free_filters(filter);

	return 1;
}

static void test_cachedb_api(const char *cachedb_name)
{
	str key = str_init("foo"),
	    val = str_init("bar"), cdb_str;
	cachedb_engine *cde;
	cachedb_con *con;

	init_str(&cdb_str, cachedb_name);

	/* high-level cachedb API (called by script, MI) */

	/* TODO: write tests for signed integers (may be broken in current Mongo) */

	ok(cachedb_store(&cdb_str, &key, &val, 0) == 1, "store key");

	memset(&val, 0, sizeof val);
	ok(cachedb_fetch(&cdb_str, &key, &val) == 1, "fetch key");
	cmp_mem(val.s, "bar", 3, "check fetched value");

	ok(cachedb_remove(&cdb_str, &key) == 1, "remove key");

	ok(cachedb_fetch(&cdb_str, &key, &val) == -2, "fetch non-existing key");

	/* low-level cachedb API (called by core and modules) */

	cde = lookup_cachedb(&cdb_str);
	if (!ok(cde != NULL, "have cachedb engine"))
		return;

	con = cde->default_connection;
	if (!ok(con != NULL, "engine has con"))
		return;

	if (CACHEDB_CAPABILITY(&cde->cdb_func, CACHEDB_CAP_GET_ROWS))
		ok(test_get_rows(&cde->cdb_func, con), "multi-row fetch");

	todo();
	if (CACHEDB_CAPABILITY(&cde->cdb_func, CACHEDB_CAP_SET_COLS))
		ok(test_set_cols(&cde->cdb_func, con), "multi-col set");

	if (CACHEDB_CAPABILITY(&cde->cdb_func, CACHEDB_CAP_UNSET_COLS))
		ok(cde->cdb_func.unset_cols(con, NULL, NULL, 0) == 0, "multi-col unset");

	end_todo;
}

void init_cachedb_tests(void)
{
	if (load_module("cachedb_mongodb.so") != 0) {
		printf("failed to load mongo\n");
		exit(-1);
	}

	if (set_mod_param_regex("cachedb_mongodb", "cachedb_url", STR_PARAM,
	    "mongodb://10.0.0.4:27017/OpensipsTests.OpensipsTests") != 0) {
		printf("failed to set mongo url\n");
		exit(-1);
	}
}

void test_cachedb_backends(void)
{
	test_cachedb_api("mongodb");
	todo();
	test_cachedb_api("cassandra");
	end_todo;
}
