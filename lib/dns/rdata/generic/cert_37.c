/*
 * Copyright (C) 1999 Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

 /* $Id: cert_37.c,v 1.7 1999/05/07 03:24:06 marka Exp $ */

 /* draft-ietf-dnssec-certs-04.txt */

#ifndef RDATA_GENERIC_CERT_37_C
#define RDATA_GENERIC_CERT_37_C

static dns_result_t
fromtext_cert(dns_rdataclass_t class, dns_rdatatype_t type,
	      isc_lex_t *lexer, dns_name_t *origin,
	      isc_boolean_t downcase, isc_buffer_t *target)
{
	isc_token_t token;
	long n;
	dns_secalg_t secalg;
	char *e;
	dns_cert_t cert;

	REQUIRE(type == 37);

	class = class;		/*unused*/
	origin = origin;	/*unused*/
	downcase = downcase;	/*unused*/

	/* cert type */
	RETERR(gettoken(lexer, &token, isc_tokentype_string, ISC_FALSE));
	n = strtol(token.value.as_pointer, &e, 10);
	if (*e != 0) {
		RETERR(dns_cert_fromtext(&cert, &token.value.as_textregion));
	} else {
		if (n < 0 || n > 0xffff)
			return (DNS_R_RANGE);
		cert = n;
	}
	RETERR(uint16_tobuffer(cert, target));
	
	/* key tag */
	RETERR(gettoken(lexer, &token, isc_tokentype_number, ISC_FALSE));
	if (token.value.as_ulong > 0xffff)
		return (DNS_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	/* algorithm */
	RETERR(gettoken(lexer, &token, isc_tokentype_string, ISC_FALSE));
	n = strtol(token.value.as_pointer, &e, 10);
	if (*e != 0) {
		RETERR(dns_secalg_fromtext(&secalg,
					   &token.value.as_textregion));
	} else {
		if (n < 0 || n > 0xff)
			return (DNS_R_RANGE);
		secalg = n;
	}
	RETERR(mem_tobuffer(target, &secalg, 1));

	return (base64_tobuffer(lexer, target, -1));
}

static dns_result_t
totext_cert(dns_rdata_t *rdata, dns_name_t *origin, isc_buffer_t *target) {
	isc_region_t sr;
	char buf[sizeof "64000 "];
	unsigned int n;

	REQUIRE(rdata->type == 37);

	origin = origin;	/*unused*/

	dns_rdata_toregion(rdata, &sr);

	/* type */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	RETERR(dns_cert_totext(n, target));
	RETERR(str_totext(" ", target));

	/* key tag */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	sprintf(buf, "%u ", n);
	RETERR(str_totext(buf, target));

	/* algorithm */
	RETERR(dns_secalg_totext(sr.base[0], target));
	RETERR(str_totext(" ", target));
	isc_region_consume(&sr, 1);

	/* cert */
	return (base64_totext(&sr, target));
}

static dns_result_t
fromwire_cert(dns_rdataclass_t class, dns_rdatatype_t type,
	      isc_buffer_t *source, dns_decompress_t *dctx,
	      isc_boolean_t downcase, isc_buffer_t *target)
{
	isc_region_t sr;

	REQUIRE(type == 37);
	
	class = class;		/*unused*/
	dctx = dctx;		/*unused*/
	downcase = downcase;	/*unused*/

	isc_buffer_active(source, &sr);
	if (sr.length < 5)
		return (DNS_R_UNEXPECTEDEND);

	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static dns_result_t
towire_cert(dns_rdata_t *rdata, dns_compress_t *cctx, isc_buffer_t *target) {
	isc_region_t sr;

	REQUIRE(rdata->type == 37);

	cctx = cctx;	/*unused*/

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static int
compare_cert(dns_rdata_t *rdata1, dns_rdata_t *rdata2) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->class == rdata2->class);
	REQUIRE(rdata1->type == 37);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (compare_region(&r1, &r2));
}

static dns_result_t
fromstruct_cert(dns_rdataclass_t class, dns_rdatatype_t type, void *source,
		isc_buffer_t *target)
{

	REQUIRE(type == 37);
	
	class = class;	/*unused*/

	source = source;
	target = target;

	return (DNS_R_NOTIMPLEMENTED);
}

static dns_result_t
tostruct_cert(dns_rdata_t *rdata, void *target, isc_mem_t *mctx) {

	REQUIRE(rdata->type == 37);
	REQUIRE(target != NULL && target == NULL);
	
	target = target;
	mctx = mctx;

	return (DNS_R_NOTIMPLEMENTED);
}

static void
freestruct_cert(void *target) {
	REQUIRE(target != NULL && target != NULL);
	REQUIRE(ISC_FALSE);	/* XXX */
}
#endif	/* RDATA_GENERIC_CERT_37_C */
