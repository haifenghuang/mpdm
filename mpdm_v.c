/*

    fdm - Filp Data Manager
    Copyright (C) 2003 Angel Ortega <angel@triptico.com>

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
	fdm_v head;		/* head of values */
	fdm_v tail;		/* tail of values */
	int count;		/* total count */
} _fdm;


/*******************
	Code
********************/

/**
 * fdm_ref - Increments the reference count.
 * @v: the value
 *
 * Increments the reference count.
 */
int fdm_ref(fdm_v v)
{
	return(v->ref++);
}


/**
 * fdm_unref - Decrements the reference count.
 * @v: the value
 *
 * Decrements the reference count.
 */
int fdm_unref(fdm_v v)
{
	return(v->ref--);
}


static void _fdm_nref(fdm_v v[], int count, int ref)
{
	int n;

	for(n=0;n < count;n++)
	{
		if(v[n] != NULL)
			v[n]->ref += ref;
	}
}


/**
 * fdm_new - Creates a new value.
 * @tag: flags and type
 * @data: pointer to real data
 * @size: size of data
 *
 * Creates a new value. @tag is an or-ed set of flags and an optional
 * user-defined type, @data is a pointer to the data the value will
 * store and @size the size of these data. The flags in @tag define
 * how the data will be stored and its behaviour:
 *
 * If the FDM_COPY flag is set, the value will store a copy of the data
 * using an allocated block of memory. Otherwise, the @data pointer is
 * stored inside the value as is.
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
 * FDM_STRING. For FDM_MULTIPLE values, @data is usually NULL
 * (meaning to allocate a zero-initialized array of @size values),
 * but can also be the @data pointer of another multiple value;
 * in this case, the values will be re-referenced.
 */
fdm_v fdm_new(int tag, void * data, int size)
{
	fdm_v v;

	/* sanity checks */
	if(tag & FDM_MULTIPLE)
	{
		/* multiple values are always copies and never strings */
		tag |= FDM_COPY;
		tag &= ~ FDM_STRING;
	}

	/* a size of -1 means 'calculate it' */
	if(size == -1)
	{
		/* only size of string values can be calculated */
		if(data == NULL || !(tag & FDM_STRING))
			return(NULL);

		size=strlen((char *) data);
	}

	/* alloc new value */
	if((v=(fdm_v) malloc(sizeof(struct _fdm_v))) == NULL)
		return(NULL);

	memset(v, '\0', sizeof(struct _fdm_v));
	v->tag=tag;
	v->size=size;

	if((tag & FDM_COPY) && size)
	{
		int s;

		s=(tag & FDM_MULTIPLE) ? s * sizeof(fdm_v) : s;

		/* alloc new space for data */
		if((v->data=malloc(s)) == NULL)
			return(NULL);

		/* zero or copy data */
		if(data == NULL)
			memset(v->data, '\0', s);
		else
		{
			memcpy(v->data, data, s);

			/* if data is multiple, re-reference its elements */
			if(tag & FDM_MULTIPLE)
				_fdm_nref((fdm_v *)v->data, size, 1);
		}
	}
	else
		v->data=data;

	/* add to the value chain and count */
	if(_fdm.head == NULL) _fdm.head=v;
	if(_fdm.tail != NULL) _fdm.tail->next=v;
	_fdm.count ++;

	return(v);
}


/**
 * fdm_sweep - Sweeps unused values
 * @count: number of values to be swept
 *
 * Sweeps unused values. @count is the number of values to be
 * checked for deletion; if it's -1, all currently stored
 * values are checked. A special value of zero in @count
 * tell fdm_sweep() to check a small group of them on each call.
 */
void fdm_sweep(int count)
{
	static int lcount=0;
	int n;

	/* if it's worthless, don't do it */
	if(_fdm.count < 16)
		return;

	/* if count is -1, sweep all */
	if(count == -1) count=_fdm.count;

	/* if count is zero, sweep 'some' values */
	if(count == 0) count=_fdm.count - lcount + 2;

	for(n=0;n < count;n++)
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

			/* unref all elements if multiple */
			if(v->tag & FDM_MULTIPLE)
				_fdm_nref((fdm_v *)v->data, v->size, -1);

			/* free data if local copy */
			if(v->tag & FDM_COPY) free(v->data);

			/* free the value itself */
			free(v);

			/* one value less */
			_fdm.count --;
		}
	}

	lcount=_fdm.count;
}


int fdm_cmp(fdm_v v1, fdm_v v2)
{
	/* if both values are strings, compare as such */
	if((v1->tag & FDM_STRING) && (v2->tag & FDM_STRING))
		return(strcmp((char *)v1->data, (char *)v2->data));

	/* in any other case, compare just pointers */
	return(v1->data - v2->data);
}


void fdm_poke(fdm_v v, char c, int offset)
{
	char * ptr;

	if(offset >= v->size)
	{
		/* increase size */
		v->size = offset + 32;
		v->data=realloc(v->data, v->size);
	}

	ptr=(char *)v->data;
	ptr[offset]=c;
}


fdm_v fdm_splice(fdm_v v, int offset, int size, char * new)
{
	int n, i;
	fdm_v w;
	char * ptr;

	/* creates the new value */
	w=fdm_new(v->tag, NULL, 0);

	ptr=(char *)v->data;

	/* copies source until offset */
	for(n=0;n < offset && ptr[n];n++)
		fdm_poke(w, ptr[n], n);

	/* skips size characters */
	i=n;
	for(;n < offset + size && ptr[n];n++);

	/* inserts new string */
	for(;new != NULL && *new != '\0';new++,i++)
		fdm_poke(w, *new, i);

	/* continue adding */
	for(;ptr[n];n++, i++)
		fdm_poke(w, ptr[n], i);

	/* null terminate */
	fdm_poke(w, '\0', i);

	return(w);
}
