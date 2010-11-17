/*

    MPDM - Minimum Profit Data Manager
    Copyright (C) 2003/2010 Angel Ortega <angel@triptico.com>

    mpdm_h.c - Hash management

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    http://www.triptico.com

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "mpdm.h"

/** code **/

/* prototype for the one-time wrapper hash function */
static int switch_hash_func(const wchar_t *, int);

/* pointer to the hashing function */
static int (*mpdm_hash_func) (const wchar_t *, int) = switch_hash_func;

static int standard_hash_func(const wchar_t * string, int mod)
/* computes a hashing function on string */
{
	int c;

	for (c = 0; *string != L'\0'; string++)
		c ^= (int) *string;

	return c % mod;
}


static int null_hash_func(const wchar_t * string, int mod)
{
	return *string % mod;
}

static int switch_hash_func(const wchar_t * string, int mod)
/* one-time wrapper for hash method autodetection */
{
	/* commute the real hashing function on
	   having the MPDM_NULL_HASH environment variable set */
	if (getenv("MPDM_NULL_HASH") != NULL)
		mpdm_hash_func = null_hash_func;
	else
		mpdm_hash_func = standard_hash_func;

	/* and fall back to it */
	return mpdm_hash_func(string, mod);
}

#define HASH_BUCKET_S(h, k) mpdm_hash_func(k, mpdm_size(h))
#define HASH_BUCKET(h, k) (mpdm_hash_func(mpdm_string(k), mpdm_size(h)))

/* interface */

/**
 * mpdm_hsize - Returns the number of pairs of a hash.
 * @h: the hash
 *
 * Returns the number of key-value pairs of a hash.
 * [Hashes]
 */
int mpdm_hsize(const mpdm_t h)
{
	int n;
	int ret = 0;

    mpdm_ref(h);

	for (n = 0; n < mpdm_size(h); n++) {
		mpdm_t b = mpdm_aget(h, n);
		ret += mpdm_size(b);
	}

    mpdm_unref(h);

	return ret / 2;
}


static mpdm_t hget(const mpdm_t h, const mpdm_t k, const wchar_t *ks)
{
	mpdm_t b;
	mpdm_t v = NULL;
	int n = 0;

    mpdm_ref(h);
    mpdm_ref(k);

	if (mpdm_size(h)) {
		/* if hash is not empty... */
		if (ks) {
			if ((b = mpdm_aget(h, HASH_BUCKET_S(h, ks))) != NULL)
				n = mpdm_bseek_s(b, ks, 2, NULL);
		}
		else {
			if ((b = mpdm_aget(h, HASH_BUCKET(h, k))) != NULL)
				n = mpdm_bseek(b, k, 2, NULL);
		}

		if (n >= 0)
			v = mpdm_aget(b, n + 1);
	}

    mpdm_unref(k);
    mpdm_unref(h);

	return v;
}


/**
 * mpdm_hget - Gets a value from a hash.
 * @h: the hash
 * @k: the key
 *
 * Gets the value from the hash @h having @k as key, or
 * NULL if the key does not exist.
 * [Hashes]
 */
mpdm_t mpdm_hget(const mpdm_t h, const mpdm_t k)
{
	return hget(h, k, NULL);
}


/**
 * mpdm_hget_s - Gets the value from a hash (string version).
 * @h: the hash
 * @k: the key
 *
 * Gets the value from the hash @h having @k as key, or
 * NULL if the key does not exist.
 * [Hashes]
 */
mpdm_t mpdm_hget_s(const mpdm_t h, const wchar_t *k)
{
	return hget(h, NULL, k);
}


/**
 * mpdm_exists - Tests if a key exists.
 * @h: the hash
 * @k: the key
 *
 * Returns 1 if @k is defined in @h, or 0 othersize.
 * [Hashes]
 */
int mpdm_exists(const mpdm_t h, const mpdm_t k)
{
	mpdm_t b;
	int n;
	int ret = 0;

    mpdm_ref(h);
    mpdm_ref(k);

	if (mpdm_size(h)) {
		/* if hash is not empty... */
		if ((b = mpdm_aget(h, HASH_BUCKET(h, k))) != NULL) {
			/* if bucket exists, binary-seek it */
			if ((n = mpdm_bseek(b, k, 2, NULL)) >= 0)
				ret = 1;
		}
	}

    mpdm_unref(k);
    mpdm_unref(h);

	return ret;
}


static mpdm_t hset(mpdm_t h, mpdm_t k, const wchar_t *ks, mpdm_t v)
{
	mpdm_t b, r;
	int n;

    mpdm_ref(h);
    mpdm_ref(k);
    mpdm_ref(v);

	/* if hash is empty, create an optimal number of buckets */
	if (mpdm_size(h) == 0)
		mpdm_expand(h, 0, mpdm->hash_buckets);

	n = ks ? HASH_BUCKET_S(h, ks) : HASH_BUCKET(h, k);

	if ((b = mpdm_aget(h, n)) != NULL) {
		int pos;

		/* bucket exists; try to find the key there */
		n = ks ? mpdm_bseek_s(b, ks, 2, &pos) : mpdm_bseek(b, k, 2, &pos);

		if (n < 0) {
			/* the pair does not exist: create it */
			n = pos;
			mpdm_expand(b, n, 2);

			mpdm_aset(b, ks ? MPDM_S(ks) : k, n);
		}
	}
	else {
		/* the bucket does not exist; create it */
		b = MPDM_A(2);

		/* put the bucket into the hash */
		mpdm_aset(h, b, n);

		/* offset 0 */
		n = 0;

		/* insert the key */
		mpdm_aset(b, ks ? MPDM_S(ks) : k, n);
	}

	r = mpdm_aset(b, v, n + 1);

    mpdm_unref(v);
    mpdm_unref(k);
    mpdm_unref(h);

    return r;
}


/**
 * mpdm_hset - Sets a value in a hash.
 * @h: the hash
 * @k: the key
 * @v: the value
 *
 * Sets the value @v to the key @k in the hash @h. Returns
 * the new value (versions prior to 1.0.10 returned the old
 * value).
 * [Hashes]
 */
mpdm_t mpdm_hset(mpdm_t h, mpdm_t k, mpdm_t v)
{
	return hset(h, k, NULL, v);
}


/**
 * mpdm_hset_s - Sets a value in a hash (string version).
 * @h: the hash
 * @k: the key
 * @v: the value
 *
 * Sets the value @v to the key @k in the hash @h. Returns
 * the new value (versions prior to 1.0.10 returned the old
 * value).
 * [Hashes]
 */
mpdm_t mpdm_hset_s(mpdm_t h, const wchar_t *k, mpdm_t v)
{
	return hset(h, NULL, k, v);
}


/**
 * mpdm_hdel - Deletes a key from a hash.
 * @h: the hash
 * @k: the key
 *
 * Deletes the key @k from the hash @h. Returns NULL
 * (versions prior to 1.0.10 returned the deleted value).
 * [Hashes]
 */
mpdm_t mpdm_hdel(mpdm_t h, const mpdm_t k)
{
	mpdm_t b;
	int n;

    mpdm_ref(h);
    mpdm_ref(k);

	if ((b = mpdm_aget(h, HASH_BUCKET(h, k))) != NULL) {
		/* bucket exists */
		if ((n = mpdm_bseek(b, k, 2, NULL)) >= 0) {
			/* collapse the bucket */
			mpdm_collapse(b, n, 2);
		}
	}

    mpdm_unref(k);
    mpdm_unref(h);

	return NULL;
}


/**
 * mpdm_keys - Returns the keys of a hash.
 * @h: the hash
 *
 * Returns an array containing all the keys of the @h hash.
 * [Hashes]
 * [Arrays]
 */
mpdm_t mpdm_keys(const mpdm_t h)
{
	int n, c;
	mpdm_t a, k;

	mpdm_ref(h);

	/* create an array with the same number of elements */
	a = MPDM_A(mpdm_hsize(h));

    mpdm_ref(a);

	c = n = 0;
	while (mpdm_iterator(h, &c, &k, NULL))
		mpdm_aset(a, k, n++);

    mpdm_unrefnd(a);

	mpdm_unref(h);

	return a;
}


/**
 * mpdm_interator - Iterates through the content of a hash or array.
 * @h: the hash (or array)
 * @context: A pointer to an opaque context
 * @v1: a pointer to a value
 * @v2: another pointer to a value
 *
 * Iterates through the content of a hash, filling the @v1 and @v2
 * pointers with key-value pairs on each call until the hash is
 * exhausted. If @h is an array, only the @v1 pointer is filled.
 * @v1 and @v2 pointers can be NULL.
 *
 * The @context pointer to integer is opaque and should be
 * initialized to zero on the first call.
 *
 * Returns 0 if no more data is left in @h.
 * [Hashes]
 * [Arrays]
 */
int mpdm_iterator(mpdm_t h, int *context, mpdm_t *v1, mpdm_t *v2)
{
	int ret = 0;
	mpdm_t w1, w2;

	mpdm_ref(h);

	if (v1 == NULL)
		v1 = &w1;

	if (v2 == NULL)
		v2 = &w2;

	if (MPDM_IS_HASH(h)) {
		int bi, ei;

		if (mpdm_size(h)) {
			/* get bucket and element index */
			bi = (*context) % mpdm_size(h);
			ei = (*context) / mpdm_size(h);

			while (ret == 0 && bi < mpdm_size(h)) {
				mpdm_t b;

				/* if bucket is empty or there is no more elements in it, pick next */
				if ((b = mpdm_aget(h, bi)) == NULL || ei >= mpdm_size(b)) {
					ei = 0;
					bi++;
					continue;
				}

				/* get pair */
				*v1 = mpdm_aget(b, ei++);
				*v2 = mpdm_aget(b, ei++);

				/* update context */
				*context = (ei * mpdm_size(h)) + bi;
				ret = 1;
			}
		}
	}
	else
	if (MPDM_IS_ARRAY(h)) {
		if (*context < mpdm_size(h)) {
			*v1 = mpdm_aget(h, (*context)++);
			ret = 1;
		}
	}

	mpdm_unref(h);

	return ret;
}


static mpdm_t mpdm_sym(mpdm_t r, mpdm_t k, mpdm_t v, int s)
{
	int n;
	mpdm_t p, w;

	if (r == NULL)
		r = mpdm_root();

	mpdm_ref(r);
	mpdm_ref(k);
	mpdm_ref(v);

	/* splits the path, if needed */
	if (k->flags & MPDM_MULTIPLE)
		p = mpdm_ref(k);
	else
		p = mpdm_ref(mpdm_split_s(L".", k));

	w = r;

	for (n = 0; n < mpdm_size(p) - s; n++) {

		/* is executable? run it and take its output */
		while (MPDM_IS_EXEC(w))
			w = mpdm_exec(w, NULL, NULL);

		if (MPDM_IS_HASH(w))
			w = mpdm_hget(w, mpdm_aget(p, n));
		else
		if (MPDM_IS_ARRAY(w)) {
			int i = mpdm_ival(mpdm_aget(p, n));
			w = mpdm_aget(w, i);
		}
		else {
			mpdm_unref(mpdm_ref(w));
			w = NULL;
		}

		if (w == NULL)
			break;
	}

	/* if want to set, do it */
	if (s && w != NULL) {
		/* resolve executable values again */
		while (MPDM_IS_EXEC(w))
			w = mpdm_exec(w, NULL, NULL);

		if (w->flags & MPDM_HASH)
			w = mpdm_hset(w, mpdm_aget(p, n), v);
		else {
			int i = mpdm_ival(mpdm_aget(p, n));
			w = mpdm_aset(w, v, i);
		}
	}

    mpdm_unref(p);
	mpdm_unref(v);
	mpdm_unref(k);
	mpdm_unref(r);

	return w;
}


mpdm_t mpdm_sget(mpdm_t r, mpdm_t k)
{
	return mpdm_sym(r, k, NULL, 0);
}


mpdm_t mpdm_sset(mpdm_t r, mpdm_t k, mpdm_t v)
{
	return mpdm_sym(r, k, v, 1);
}
