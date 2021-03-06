/*
 * Copyright (C) 2000-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#ifndef GNUTLS_LIB_MEM_H
#define GNUTLS_LIB_MEM_H

#include <config.h>

/* These realloc functions will return ptr if size==0, and will free
 * the ptr if the new allocation failed.
 */
void *gnutls_realloc_fast(void *ptr, size_t size);
void *_gnutls_reallocarray_fast(void *ptr, size_t nmemb, size_t size);

char *_gnutls_strdup(const char *);

void *_gnutls_reallocarray(void *, size_t, size_t);

unsigned _gnutls_mem_is_zero(const uint8_t *ptr, unsigned size);

#define zrelease_mpi_key(mpi) if (*mpi!=NULL) { \
		_gnutls_mpi_clear(*mpi); \
		_gnutls_mpi_release(mpi); \
	}

#define zeroize_key(x, size) gnutls_memset(x, 0, size)

#define zeroize_temp_key zeroize_key
#define zrelease_temp_mpi_key zrelease_mpi_key

#endif /* GNUTLS_LIB_MEM_H */
