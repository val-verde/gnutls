/*
 * Copyright (C) 2013-2016 Nikos Mavrogiannopoulos
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/* This file contains functions to handle X.509 certificate generation.
 */

#include "gnutls_int.h"

#include <datum.h>
#include <global.h>
#include "errors.h"
#include <common.h>
#include <x509.h>
#include <x509_b64.h>
#include <c-ctype.h>

typedef int (*set_dn_func) (void *, const char *oid, unsigned int raw_flag,
			    const void *name, unsigned int name_size);

static
int dn_attr_crt_set(set_dn_func f, void *crt, const gnutls_datum_t * name,
		    const gnutls_datum_t * val, unsigned is_raw)
{
	char _oid[MAX_OID_SIZE];
	gnutls_datum_t tmp;
	const char *oid;
	int ret;
	unsigned i,j;

	if (name->size == 0 || val->size == 0)
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

	if (c_isdigit(name->data[0]) != 0) {
		if (name->size >= sizeof(_oid))
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

		memcpy(_oid, name->data, name->size);
		_oid[name->size] = 0;

		oid = _oid;

		if (gnutls_x509_dn_oid_known(oid) == 0 && !is_raw) {
			_gnutls_debug_log("Unknown OID: '%s'\n", oid);
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		}
	} else {
		oid =
		    _gnutls_ldap_string_to_oid((char *) name->data,
					       name->size);
	}

	if (oid == NULL) {
		_gnutls_debug_log("Unknown DN attribute: '%.*s'\n",
				  (int) name->size, name->data);
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
	}

	if (is_raw) {
		gnutls_datum_t hex = {val->data+1, val->size-1};

		ret = gnutls_hex_decode2(&hex, &tmp);
		if (ret < 0)
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
	} else {
		tmp.size = val->size;
		tmp.data = gnutls_malloc(tmp.size+1);
		if (tmp.data == NULL) {
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
		}

		for (j=i=0;i<tmp.size;i++) {
			if (1+j!=val->size && val->data[j] == '\\' &&
			    (val->data[j+1] == ',' || val->data[j+1] == '#' || val->data[j+1] == ' ')) {
				tmp.data[i] = val->data[j+1];
				j+=2;
				tmp.size--;
			} else {
				tmp.data[i] = val->data[j++];
			}
		}
		tmp.data[tmp.size] = 0;
	}

	ret = f(crt, oid, is_raw, tmp.data, tmp.size);
	gnutls_free(tmp.data);

	if (ret < 0)
		return gnutls_assert_val(ret);

	return 0;
}

static int read_attr_and_val(const char **ptr,
			     gnutls_datum_t * name, gnutls_datum_t * val, unsigned *is_raw)
{
	const unsigned char *p = (void *) *ptr;

	*is_raw = 0;

	/* skip any space */
	while (c_isspace(*p))
		p++;

	/* Read the name */
	name->data = (void *) p;
	while (*p != '=' && *p != 0 && !c_isspace(*p))
		p++;

	name->size = p - name->data;

	/* skip any space */
	while (c_isspace(*p))
		p++;

	if (*p != '=')
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
	p++;

	while (c_isspace(*p))
		p++;

	if (*p == '#') {
		*is_raw = 1;
	}

	/* Read value */
	val->data = (void *) p;
	while (*p != 0 && (*p != ',' || (*p == ',' && *(p - 1) == '\\'))
	       && *p != '\n') {
		p++;
	}
	val->size = p - (val->data);

	/* remove spaces from the end */
	while(val->size > 0 && c_isspace(val->data[val->size-1])) {
		if (val->size-2 > 0 && val->data[val->size-2] == '\\')
			break;
		val->size--;
	}

	if (val->size == 0 || name->size == 0)
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

	*ptr = (void *) p;

	return 0;
}

static int
crt_set_dn(set_dn_func f, void *crt, const char *dn, const char **err)
{
	const char *p = dn;
	int ret;
	gnutls_datum_t name, val;
	unsigned is_raw;

	if (crt == NULL || dn == NULL)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	/* For each element */
	while (*p != 0 && *p != '\n') {
		if (err)
			*err = p;

		is_raw = 0;
		ret = read_attr_and_val(&p, &name, &val, &is_raw);
		if (ret < 0)
			return gnutls_assert_val(ret);

		/* skip spaces and look for comma */
		while (c_isspace(*p))
			p++;

		ret = dn_attr_crt_set(f, crt, &name, &val, is_raw);
		if (ret < 0)
			return gnutls_assert_val(ret);

		if (err)
			*err = p;

		if (*p != ',' && *p != 0 && *p != '\n')
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		if (*p == ',')
			p++;
	}

	return 0;
}


/**
 * gnutls_x509_crt_set_dn:
 * @crt: a certificate of type #gnutls_x509_crt_t
 * @dn: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided certificate.
 * The input string should be plain ASCII or UTF-8 encoded. On
 * DN parsing error %GNUTLS_E_PARSING_ERROR is returned.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crt_set_dn(gnutls_x509_crt_t crt, const char *dn,
		       const char **err)
{
	return crt_set_dn((set_dn_func) gnutls_x509_crt_set_dn_by_oid, crt,
			  dn, err);
}

/**
 * gnutls_x509_crt_set_issuer_dn:
 * @crt: a certificate of type #gnutls_x509_crt_t
 * @dn: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided certificate.
 * The input string should be plain ASCII or UTF-8 encoded. On
 * DN parsing error %GNUTLS_E_PARSING_ERROR is returned.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crt_set_issuer_dn(gnutls_x509_crt_t crt, const char *dn,
			      const char **err)
{
	return crt_set_dn((set_dn_func)
			  gnutls_x509_crt_set_issuer_dn_by_oid, crt, dn,
			  err);
}

/**
 * gnutls_x509_crq_set_dn:
 * @crq: a certificate of type #gnutls_x509_crq_t
 * @dn: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided certificate.
 * The input string should be plain ASCII or UTF-8 encoded. On
 * DN parsing error %GNUTLS_E_PARSING_ERROR is returned.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crq_set_dn(gnutls_x509_crq_t crq, const char *dn,
		       const char **err)
{
	return crt_set_dn((set_dn_func) gnutls_x509_crq_set_dn_by_oid, crq,
			  dn, err);
}

static
int set_dn_by_oid(gnutls_x509_dn_t dn, const char *oid, unsigned int raw_flag, const void *name, unsigned name_size)
{
	return _gnutls_x509_set_dn_oid(dn->asn, "", oid, raw_flag, name, name_size);
}

/**
 * gnutls_x509_dn_set_str:
 * @dn: a pointer to DN
 * @str: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided DN structure.
 * The input string should be plain ASCII or UTF-8 encoded. On
 * DN parsing error %GNUTLS_E_PARSING_ERROR is returned.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.5.3
 **/
int
gnutls_x509_dn_set_str(gnutls_x509_dn_t dn, const char *str, const char **err)
{
	if (dn == NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	return crt_set_dn((set_dn_func) set_dn_by_oid, dn,
			  str, err);
}
