/*

    mpdm - Minimum Profit Data Manager
    Copyright (C) 2003/2004 Angel Ortega <angel@triptico.com>

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

/*******************
	Code
********************/

static int _mpdm_hash_func(wchar_t * string, int mod)
/* computes a hashing function on string */
{
	int c;

	for(c=0;*string != L'\0';string++)
		c=(128 * c + (int)*string) % mod;

	return(c);
}


/**
 * mpdm_hsize - Returns the number of pairs of a hash
 * @h: the hash
 *
 * Returns the number of key-value pairs of a hash.
 */
int mpdm_hsize(mpdm_v h)
{
	int r;
	mpdm_v t;

	if((t=mpdm_get_tie(h, MPDM_TIE_HSIZE)) != NULL)
		r=mpdm_ival(mpdm_exec(t, h));
	else
		r=h->ival;

	return(r);
}


#define HASH_BUCKET(h, k) (_mpdm_hash_func(mpdm_string(k), mpdm_size(h)))

/**
 * mpdm_hget - Gets an value from a hash.
 * @h: the hash
 * @k: the key
 *
 * Gets the value from the hash @h having @k as key, or
 * NULL if the key does not exist.
 */
mpdm_v mpdm_hget(mpdm_v h, mpdm_v k)
{
	int n;
	mpdm_v b;
	mpdm_v v = NULL;

	if((b=mpdm_get_tie(h, MPDM_TIE_HGET)) != NULL)
	{
		mpdm_ndv w;
		mpdm_v av[2];

		MPDM_ND_A(w,av);
		av[0]=h; av[1]=k;

		v=mpdm_exec(b, &w);
	}
	else
	if(mpdm_size(h))
	{
		if((b=mpdm_aget(h, HASH_BUCKET(h, k))) != NULL)
		{
			if((n=mpdm_abseek(b, k, 2, NULL)) >= 0)
				v=mpdm_aget(b, n + 1);
		}
	}

	return(v);
}


/**
 * mpdm_hset - Sets a value in a hash.
 * @h: the hash
 * @k: the key
 * @v: the value
 *
 * Sets the value @v to the key @k in the hash @h. Returns
 * the previous value of the key, or NULL if the key was
 * previously undefined.
 */
mpdm_v mpdm_hset(mpdm_v h, mpdm_v k, mpdm_v v)
{
	int n, pos;
	mpdm_v b;
	mpdm_v p = NULL;

	if((b=mpdm_get_tie(h, MPDM_TIE_HSET)) != NULL)
	{
		mpdm_ndv w;
		mpdm_v av[3];

		MPDM_ND_A(w,av);
		av[0]=h; av[1]=k; av[2]=v;

		return(mpdm_exec(b, &w));
	}

	/* if hash is empty, create an optimal number of buckets */
	if(mpdm_size(h) == 0)
		mpdm_aexpand(h, 0, 31);

	n=HASH_BUCKET(h, k);

	if((b=mpdm_aget(h, n)) != NULL)
	{
		if((n=mpdm_abseek(b, k, 2, &pos)) < 0)
		{
			/* the pair does not exist: create it */
			n = pos;
			mpdm_aexpand(b, n, 2);
			mpdm_aset(b, k, n);

			/* increment number of pairs */
			h->ival ++;
		}

		/* sets the value for the key */
		p=mpdm_aset(b, v, n + 1);
	}
	else
	{
		/* the bucket does not exist; create it */
		b=MPDM_A(2);

		/* insert now both key and value */
		mpdm_aset(b, k, 0);
		mpdm_aset(b, v, 1);

		/* put the bucket into the hash */
		mpdm_aset(h, b, n);

		/* increment number of pairs */
		h->ival ++;
	}

	return(p);
}


/**
 * mpdm_hdel - Deletes a key from a hash.
 * @h: the hash
 * @k: the key
 *
 * Deletes the key @k from the hash @h. Returns the previous
 * value, or NULL if the key was not defined.
 */
mpdm_v mpdm_hdel(mpdm_v h, mpdm_v k)
{
	int n;
	mpdm_v b;
	mpdm_v v = NULL;

	if((b=mpdm_get_tie(h, MPDM_TIE_HDEL)) != NULL)
	{
		mpdm_ndv w;
		mpdm_v av[2];

		MPDM_ND_A(w,av);
		av[0]=h; av[1]=k;

		v=mpdm_exec(b, &w);
	}
	else
	if((b=mpdm_aget(h, HASH_BUCKET(h, k))) != NULL)
	{
		if((n=mpdm_abseek(b, k, 2, NULL)) >= 0)
		{
			/* the pair exists: set both to NULL */
			mpdm_aset(b, NULL, n);
			v=mpdm_aset(b, NULL, n + 1);

			/* collapse the bucket */
			mpdm_acollapse(b, n, 2);

			/* decrement number of pairs */
			h->ival --;
		}
	}

	return(v);
}


/**
 * mpdm_hkeys - Returns the keys of a hash.
 * @h: the hash
 *
 * Returns an array containing all the keys of the @h hash.
 */
mpdm_v mpdm_hkeys(mpdm_v h)
{
	int n,m,i;
	mpdm_v a;
	mpdm_v b;

	if((b=mpdm_get_tie(h, MPDM_TIE_HKEYS)) != NULL)
		return(mpdm_exec(b, h));

	a=MPDM_A(mpdm_hsize(h));

	for(n=i=0;n < mpdm_size(h);n++)
	{
		if((b=mpdm_aget(h, n)) != NULL)
		{
			for(m=0;m < mpdm_size(b);m += 2)
				mpdm_aset(a, mpdm_aget(b, m), i++);
		}
	}

	return(a);
}


static mpdm_v _mpdm_sym(mpdm_v r, mpdm_v k, mpdm_v v, int s)
{
	int n;
	mpdm_v p;

	if(r == NULL)
		r=mpdm_root();

	/* splits the path, if needed */
	if(k->flags & MPDM_MULTIPLE)
		p=k;
	else
	{
		mpdm_ndv d;

		MPDM_ND_LS(d, L".");
		p=mpdm_asplit(&d, k);
	}

	for(n=0;n < mpdm_size(p) - s;n++)
	{
		/* try each component as a hash, then as array */
		if(r->flags & MPDM_HASH)
			r=mpdm_hget(r, mpdm_aget(p, n));
		else
		if(r->flags & MPDM_MULTIPLE)
		{
			int i=mpdm_ival(mpdm_aget(p, n));
			r=mpdm_aget(r, i);
		}
		else
			r=NULL;

		if(r == NULL)
			break;
	}

	/* if want to set, do it */
	if(s && r != NULL)
	{
		if(r->flags & MPDM_HASH)
			r=mpdm_hset(r, mpdm_aget(p, n), v);
		else
		{
			int i=mpdm_ival(mpdm_aget(p, n));
			r=mpdm_aset(r, v, i);
		}
	}

	return(r);
}


mpdm_v mpdm_sget(mpdm_v r, mpdm_v k)
{
	return(_mpdm_sym(r, k, NULL, 0));
}


mpdm_v mpdm_sset(mpdm_v r, mpdm_v k, mpdm_v v)
{
	return(_mpdm_sym(r, k, v, 1));
}
