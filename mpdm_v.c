/*

    fdm - Filp Data Manager
    Copyright (C) 2003/2004 Angel Ortega <angel@triptico.com>

    fdm_v.c - Basic value management

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

#include "fdm.h"

/*******************
	Data
********************/

/* control structure */

static struct
{
	fdm_v root;		/* the root hash */
	fdm_v head;		/* head of values */
	fdm_v tail;		/* tail of values */
	int count;		/* total count */
	int lcount;		/* last count */
} _fdm;


/*******************
	Code
********************/

#define _fdm_alloc() (fdm_v)malloc(sizeof(struct _fdm_v))
#define _fdm_free(v) free(v)


fdm_v _fdm_new(int flags, void * data, int size)
/* Creates a new value (overriding cache) */
{
	fdm_v v;

	/* a size of -1 means 'calculate it' */
	if(size == -1)
	{
		/* only size of string values can be calculated */
		if(data == NULL || !(flags & FDM_STRING))
			return(NULL);

		size=strlen((char *) data);
	}

	/* alloc new value and init */
	if((v=_fdm_alloc()) == NULL)
		return(NULL);

	memset(v, '\0', sizeof(struct _fdm_v));

	v->flags=flags;

	if(flags & FDM_MULTIPLE)
		fdm_aexpand(v, 0, size);
	else
	if(flags & FDM_COPY)
	{
		v->flags |= FDM_FREE;
		v->size=size;

		/* alloc new space for data */
		if((v->data=malloc(size + 1)) == NULL)
			return(NULL);

		/* zero or copy the block */
		if(data == NULL)
			memset(v->data, '\0', size);
		else
		{
			if(flags & FDM_STRING)
				strcpy(v->data, data);
			else
				memcpy(v->data, data, size);
		}
	}
	else
	{
		v->data=data;
		v->size=size;
	}

	/* add to the value chain and count */
	if(_fdm.head == NULL) _fdm.head=v;
	if(_fdm.tail != NULL) _fdm.tail->next=v;
	_fdm.tail=v;
	_fdm.count ++;

	return(v);
}


fdm_v _fdm_cache(int tag, void * data, int size)
{
	static fdm_v _cache=NULL;
	fdm_v v=NULL;

	/* first time initialization */
	if(_cache == NULL)
	{
		_cache=_fdm_new(FDM_MULTIPLE, NULL, 256);
		fdm_ref(_cache);
	}

	/* try one-char cached values */
	if(data != NULL && (tag & FDM_STRING) && ! (tag & FDM_FREE))
	{
		int c=-1;
		unsigned char * ptr;

		ptr=data;

		if(*ptr == '\0')
			c=0;
		else
		if(*(ptr + 1) == '\0')
			c=*ptr;

		if(c != -1)
		{
			if((v=fdm_aget(_cache, c)) == NULL)
			{
				v=_fdm_new(tag, data, size);
				fdm_aset(_cache, v, c);
			}
		}
	}

	return(v);
}


/**
 * fdm_new - Creates a new value.
 * @flags: flags
 * @data: pointer to real data
 * @size: size of data
 *
 * Creates a new value. @flags is an or-ed set of flags,
 * @data is a pointer to the data the value will
 * store and @size the size of these data. The flags in @tag define
 * how the data will be stored and its behaviour:
 *
 * If the FDM_COPY flag is set, the value will store a copy of the data
 * using an allocated block of memory. Otherwise, the @data pointer is
 * stored inside the value as is. FDM_COPY implies FDM_FREE.
 *
 * If FDM_STRING is set, it means @data can be treated as a string
 * (i.e., operations like strcmp() or strlen() can be done on it).
 * Otherwise, @data is treated as an opaque value. For FDM_STRING
 * values, @size can be -1 to force a calculation using strlen().
 * This flag is incompatible with FDM_MULTIPLE.
 *
 * IF FDM_MULTIPLE is set, it means the value itself is an array
 * of values. @Size indicates the number of elements instead of
 * a quantity in bytes. FDM_MULTIPLE implies FDM_COPY and not
 * FDM_STRING, and @data is ignored.
 *
 * If FDM_FREE is set, memory in @data will be released using free()
 * when the value is destroyed.
 *
 * There are other informative flags that do nothing but describe
 * the information stored in the value: they are FDM_HASH, FDM_FILE
 * and FDM_BINCODE.
 */
fdm_v fdm_new(int tag, void * data, int size)
{
	fdm_v v;

	if((v=_fdm_cache(tag, data, size)) == NULL)
		v=_fdm_new(tag, data, size);

	return(v);
}


fdm_v _fdm_inew(int ival)
{
	fdm_v v;
	char tmp[32];

	/* creates the visual representation */
	snprintf(tmp, sizeof(tmp) - 1, "%d", ival);

	v=fdm_new(FDM_COPY | FDM_STRING | FDM_IVAL, tmp, -1);
	v->ival=ival;

	return(v);
}


/**
 * fdm_ref - Increments the reference count of a value.
 * @v: the value
 *
 * Increments the reference count of a value.
 */
int fdm_ref(fdm_v v)
{
	return(v->ref++);
}


/**
 * fdm_unref - Decrements the reference count of a value.
 * @v: the value
 *
 * Decrements the reference count of a value.
 */
int fdm_unref(fdm_v v)
{
	return(v->ref--);
}


/**
 * fdm_sweep - Sweeps unreferenced values
 * @count: number of values to be swept
 *
 * Destroys values with a reference count of 0. @count is the
 * number of values to be checked for deletion; special values of
 * @count are -1, that forces a check of all currently known values
 * (can be time-consuming) and 0, which tells fdm_sweep() to check a
 * small group of them on each call.
 */
void fdm_sweep(int count)
{
	/* if it's worthless, don't do it */
	if(_fdm.count < 16)
	{
		/* store current count, to avoid a sweeping
		   rush when the threshold is crossed */
		_fdm.lcount=_fdm.count;
		return;
	}

	/* if count is -1, sweep all */
	if(count == -1) count=_fdm.count * 2;

	/* if count is zero, sweep 'some' values */
	if(count == 0) count=_fdm.count - _fdm.lcount + 2;

	while(count > 0)
	{
		/* is the value referenced? */
		if(_fdm.head->ref)
		{
			/* yes; rotate to next */
			_fdm.tail->next=_fdm.head;
			_fdm.head=_fdm.head->next;
			_fdm.tail=_fdm.tail->next;
			_fdm.tail->next=NULL;
		}
		else
		{
			fdm_v v;

			/* value is to be destroyed */
			v=_fdm.head;
			_fdm.head=_fdm.head->next;

			/* destroy */
			if(v->flags & FDM_MULTIPLE)
				fdm_acollapse(v, 0, v->size);
			else
			if(v->flags & FDM_FREE)
				free(v->data);

			/* free the value itself */
			_fdm_free(v);

			/* one value less */
			_fdm.count--;
		}

		count--;
	}

	_fdm.lcount=_fdm.count;
}


/**
 * fdm_clone - Creates a clone of a value
 * @v: the value
 *
 * Creates a clone of a value. If the value is multiple, a new value will
 * be created containing clones of all its elements; otherwise,
 * the same unchanged value is returned.
 */
fdm_v fdm_clone(fdm_v v)
{
	fdm_v w;
	int n;

	/* if NULL or value is not multiple, return as is */
	if(v == NULL || !(v->flags & FDM_MULTIPLE))
		return(v);

	/* create new */
	w=fdm_new(v->flags, NULL, v->size);

	/* fills each element with duplicates of the original */
	for(n=0;n < v->size;n++)
		fdm_aset(w, fdm_clone(fdm_aget(v, n)), n);

	return(w);
}


/**
 * fdm_root - Returns the root hash.
 *
 * Returns the root hash. This hash is stored internally and can be used
 * as a kind of global symbol table.
 */
fdm_v fdm_root(void)
{
	if(_fdm.root == NULL)
	{
		_fdm.root=FDM_H(0);
		fdm_ref(_fdm.root);
	}

	return(_fdm.root);
}
