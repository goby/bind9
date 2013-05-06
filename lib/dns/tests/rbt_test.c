/*
 * Copyright (C) 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rbt_test.c,v 1.1.14.8 2012/02/10 16:24:37 ckb Exp $ */

/* ! \file */

#include <config.h>
#include <atf-c.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h> /* uintptr_t */
#endif

#include <dns/rbt.h>
#include <dns/fixedname.h>
#include <dns/result.h>
#include <dns/compress.h>
#include "dnstest.h"

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/os.h>
#include <isc/string.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/log.h>
#include <dns/name.h>
#include <dns/result.h>

#include <dst/dst.h>

typedef struct data_holder {
	int len;
	const char *data;
} data_holder_t;

typedef struct rbt_testdata {
	const char *name;
	size_t name_len;
	data_holder_t data;
} rbt_testdata_t;

#define DATA_ITEM(name) { (name), sizeof(name) - 1, { sizeof(name), (name) } }

rbt_testdata_t testdata[] = {
	DATA_ITEM("first.com."),
	DATA_ITEM("one.net."),
	DATA_ITEM("two.com."),
	DATA_ITEM("three.org."),
	DATA_ITEM("asdf.com."),
	DATA_ITEM("ghjkl.com."),
	DATA_ITEM("1.edu."),
	DATA_ITEM("2.edu."),
	DATA_ITEM("3.edu."),
	DATA_ITEM("123.edu."),
	DATA_ITEM("1236.com."),
	DATA_ITEM("and_so_forth.com."),
	DATA_ITEM("thisisalongname.com."),
	DATA_ITEM("a.b."),
	DATA_ITEM("test.net."),
	DATA_ITEM("whoknows.org."),
	DATA_ITEM("blargh.com."),
	DATA_ITEM("www.joe.com."),
	DATA_ITEM("test.com."),
	DATA_ITEM("isc.org."),
	DATA_ITEM("uiop.mil."),
	DATA_ITEM("last.fm."),
	{ NULL, 0, { 0, NULL } }
};

static void
delete_data(void *data, void *arg) {
	UNUSED(arg);
	UNUSED(data);
}

static isc_result_t
write_data(FILE *file, unsigned char *datap, isc_uint32_t serial,
	   isc_sha1_t *sha1) {
	size_t ret = 0;
	data_holder_t *data = (data_holder_t *)datap;
	data_holder_t temp;
	uintptr_t where = ftell(file);

	UNUSED(serial);
	UNUSED(sha1);

	REQUIRE(data != NULL);
	REQUIRE((data->len == 0 && data->data == NULL) ||
		(data->len != 0 && data->data != NULL));

	temp = *data;
	temp.data = (data->len == 0
		     ? NULL
		     : (char *)(where + sizeof(data_holder_t)));

	ret = fwrite(&temp, sizeof(data_holder_t), 1, file);
	if (ret != 1)
		return (ISC_R_FAILURE);
	if (data->len > 0) {
		ret = fwrite(data->data, data->len, 1, file);
		if (ret != 1)
			return (ISC_R_FAILURE);
	}

	return (ISC_R_SUCCESS);
}

static isc_result_t
fix_data(dns_rbtnode_t *p, void *base, size_t max, isc_sha1_t *sha1) {
	data_holder_t *data = p->data;

	UNUSED(base);
	UNUSED(max);

	REQUIRE(data != NULL);
	REQUIRE((data->len == 0 && data->data == NULL) ||
		(data->len != 0 && data->data != NULL));

	UNUSED(sha1);

	printf("fixing data: len %d, data %p\n", data->len, data->data);

	data->data = (data->len == 0)
		? NULL
		: (char *)data + sizeof(data_holder_t);

	return (ISC_R_SUCCESS);
}

/*
 * Load test data into the RBT.
 */
static void
add_test_data(isc_mem_t *mctx, dns_rbt_t *rbt)
{
	char buffer[1024];
	isc_buffer_t b;
	isc_result_t result;
	dns_fixedname_t fname;
	dns_name_t *name;
	dns_compress_t cctx;
	rbt_testdata_t *testdatap = testdata;

	dns_compress_init(&cctx, -1, mctx);

	while (testdatap->name != NULL && testdatap->data.data != NULL) {
		memcpy(buffer, testdatap->name, testdatap->name_len);

		isc_buffer_init(&b, buffer, testdatap->name_len);
		isc_buffer_add(&b, testdatap->name_len);
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			testdatap++;
			continue;
		}

		if (name != NULL) {
			result = dns_rbt_addname(rbt, name, &testdatap->data);
			ATF_CHECK_STREQ(dns_result_totext(result), "success");
		}
		testdatap++;
	}

	dns_compress_invalidate(&cctx);
}

/*
 * Walk the tree and ensure that all the test nodes are present.
 */
static void
check_test_data(dns_rbt_t *rbt)
{
	char buffer[1024];
	char *arg;
	dns_fixedname_t fname;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_buffer_t b;
	data_holder_t *data;
	isc_result_t result;
	dns_name_t *foundname;
	rbt_testdata_t *testdatap = testdata;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);

	while (testdatap->name != NULL && testdatap->data.data != NULL) {
		memcpy(buffer, testdatap->name, testdatap->name_len + 1);
		arg = buffer;

		isc_buffer_init(&b, arg, testdatap->name_len);
		isc_buffer_add(&b, testdatap->name_len);
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			testdatap++;
			continue;
		}

		data = NULL;
		result = dns_rbt_findname(rbt, name, 0, foundname,
					  (void *) &data);
		ATF_CHECK_STREQ(dns_result_totext(result), "success");

		testdatap++;
	}
}

static void
data_printer(FILE *out, void *datap)
{
	data_holder_t *data = (data_holder_t *)datap;

	fprintf(out, "%d bytes, %s", data->len, data->data);
}

ATF_TC(isc_rbt);
ATF_TC_HEAD(isc_rbt, tc) {
	atf_tc_set_md_var(tc, "descr", "Test the creation of an rbt");
}
ATF_TC_BODY(isc_rbt, tc) {
	dns_rbt_t *rbt = NULL;
	isc_result_t result;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_STREQ(dns_result_totext(result), "success");
	result = dns_rbt_create(mctx, delete_data, NULL, &rbt);
	ATF_CHECK_STREQ(dns_result_totext(result), "success");

	add_test_data(mctx, rbt);

	check_test_data(rbt);

	dns_rbt_printall(rbt, data_printer);

	dns_rbt_destroy(&rbt);

	dns_test_end();
}

ATF_TC(isc_serialize_rbt);
ATF_TC_HEAD(isc_serialize_rbt, tc) {
	atf_tc_set_md_var(tc, "descr", "Test writing an rbt to file");
}
ATF_TC_BODY(isc_serialize_rbt, tc) {
	dns_rbt_t *rbt = NULL;
	isc_result_t result;
	FILE *rbtfile = NULL;
	dns_rbt_t *rbt_deserialized = NULL;
	long offset;
	int fd;
	off_t filesize = 0;
	char *base;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_STREQ(dns_result_totext(result), "success");
	result = dns_rbt_create(mctx, delete_data, NULL, &rbt);
	ATF_CHECK_STREQ(dns_result_totext(result), "success");

	add_test_data(mctx, rbt);

	dns_rbt_printall(rbt, data_printer);

	/*
	 * Serialize the tree.
	 */
	printf("serialization begins.\n");
	rbtfile = fopen("./zone.bin", "w+b");
	ATF_REQUIRE(rbtfile != NULL);
	result = dns_rbt_serialize_tree(rbtfile, rbt, write_data, 0, &offset);
	ATF_REQUIRE(result == ISC_R_SUCCESS);
	dns_rbt_destroy(&rbt);

	/*
	 * Deserialize the tree
	 */
	printf("deserialization begins.\n");

#ifndef MAP_FILE
#define MAP_FILE 0
#endif
	/*
	 * Map in the whole file in one go
	 */
	fd = open("zone.bin", O_RDWR);
	isc_file_getsizefd(fd, &filesize);
	base = mmap(NULL, filesize,
		    PROT_READ|PROT_WRITE,
		    MAP_FILE|MAP_PRIVATE, fd, 0);
	ATF_REQUIRE(base != NULL && base != MAP_FAILED);

	result = dns_rbt_deserialize_tree(base, filesize, 0, mctx,
					  delete_data, NULL,
					  fix_data, NULL, &rbt_deserialized);

	/* Test to make sure we have a valid tree */
	ATF_REQUIRE(result == ISC_R_SUCCESS);
	if (rbt_deserialized == NULL)
		atf_tc_fail("deserialized rbt is null!"); /* Abort execution. */

	check_test_data(rbt_deserialized);

	dns_rbt_printall(rbt_deserialized, data_printer);

	dns_rbt_destroy(&rbt_deserialized);
	munmap(base, filesize);
	unlink("zone.bin");
	dns_test_end();
}

ATF_TC(dns_rbt_serialize_align);
ATF_TC_HEAD(dns_rbt_serialize_align, tc) {
	atf_tc_set_md_var(tc, "descr", "Test the dns_rbt_serialize_align() function.");
}
ATF_TC_BODY(dns_rbt_serialize_align, tc) {
	UNUSED(tc);

	ATF_CHECK(dns_rbt_serialize_align(0) == 0);
	ATF_CHECK(dns_rbt_serialize_align(1) == 8);
	ATF_CHECK(dns_rbt_serialize_align(2) == 8);
	ATF_CHECK(dns_rbt_serialize_align(3) == 8);
	ATF_CHECK(dns_rbt_serialize_align(4) == 8);
	ATF_CHECK(dns_rbt_serialize_align(5) == 8);
	ATF_CHECK(dns_rbt_serialize_align(6) == 8);
	ATF_CHECK(dns_rbt_serialize_align(7) == 8);
	ATF_CHECK(dns_rbt_serialize_align(8) == 8);
	ATF_CHECK(dns_rbt_serialize_align(9) == 16);
	ATF_CHECK(dns_rbt_serialize_align(0xff) == 0x100);
	ATF_CHECK(dns_rbt_serialize_align(0x301) == 0x308);
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, isc_rbt);
	ATF_TP_ADD_TC(tp, isc_serialize_rbt);
	ATF_TP_ADD_TC(tp, dns_rbt_serialize_align);

	return (atf_no_error());
}