/*

    mpdm - Minimum Profit Data Manager
    Copyright (C) 2003/2004 Angel Ortega <angel@triptico.com>

    mpdm_r.c - Regular expressions

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

#include "mpdm.h"

#ifdef CONFOPT_PCRE
#include <pcreposix.h>
#endif

#ifdef CONFOPT_SYSTEM_REGEX
#include <regex.h>
#endif

#ifdef CONFOPT_INCLUDED_REGEX
#include "gnu_regex.h"
#endif

/*******************
	Data
********************/

/* the regex cache */
mpdm_v _mpdm_regex_cache=NULL;
mpdm_v _mpdm_iregex_cache=NULL;

/*******************
	Code
********************/

mpdm_v _mpdm_regcomp(mpdm_v r, char * flags)
{
	mpdm_v cr;
	mpdm_v c;
	int f;

	/* if cache does not exist, create it */
	if(_mpdm_regex_cache == NULL)
	{
		mpdm_ref(_mpdm_regex_cache=MPDM_H(0));
		mpdm_ref(_mpdm_iregex_cache=MPDM_H(0));
	}

	/* flags */
	if(flags != NULL && strchr(flags, 'i') != NULL)
	{
		c=_mpdm_iregex_cache;
		f=REG_ICASE;
	}
	else
	{
		c=_mpdm_regex_cache;
		f=0;
	}

	/* search the regex in the cache */
	if((cr=mpdm_hget(c, r)) == NULL)
	{
		/* not found; regex must be compiled */
		cr=mpdm_new(MPDM_COPY, NULL, sizeof(regex_t));

		if(regcomp((regex_t *) cr->data,
			(char *) r->data, REG_EXTENDED | f))
			return(NULL);

		/* stores */
		mpdm_hset(c, r, cr);
	}

	return(cr);
}


/**
 * mpdm_regex - Matches a regular expression
 * @r: the regular expression
 * @v: the value to be matched
 * @offset: offset from the start of v->data
 * @flags: flags
 *
 * Matches a regular expression against a value. @flags is a char *
 * that can contain an 'i' for case-insensitive matches, or NULL.
 *
 * If the regex is matched, returns a two value array containing
 * the start and stop offsets of the matched string, or NULL otherwise.
 */
mpdm_v mpdm_regex(mpdm_v r, mpdm_v v, int offset, char * flags)
{
	mpdm_v cr;
	mpdm_v w=NULL;
	regmatch_t rm;

	/* compile the regex */
	if((cr=_mpdm_regcomp(r, flags)) != NULL)
	{
		/* match? */
		if(regexec((regex_t *) cr->data,
			(char *) v->data + offset, 1,
			&rm, offset > 0 ? REG_NOTBOL : 0) == 0)
		{
			/* found! */
			w=MPDM_A(2);

			mpdm_aset(w, MPDM_I(offset + rm.rm_so), 0);
			mpdm_aset(w, MPDM_I(offset + rm.rm_eo), 1);
		}
	}

	return(w);
}


/**
 * mpdm_sregex - Matches and substitutes a regular expression
 * @r: the regular expression
 * @v: the value to be matched
 * @s: the string that will substitute the matched string
 * @offset: offset from the start of v->data
 * @flags: flags
 *
 * Matches a regular expression against a value, and substitutes the
 * found substring with @s. @flags is a char * that can contain an 'i'
 * for case-insensitive matches, a 'g' for a global substitution
 * (substitute all ocurrences of the regex, not only the first one), or NULL.
 *
 * Returns the modified string, or the original one if no substitutions
 * were done.
 */
mpdm_v mpdm_sregex(mpdm_v r, mpdm_v v, mpdm_v s, int offset, char * flags)
{
	mpdm_v cr;
	char * ptr;
	regmatch_t rm;
	int f, i;
	int g=0;

	/* flags */
	if(flags != NULL)
		g=strchr(flags, 'g') != NULL ? 1 : 0;

	/* compile the regex */
	if((cr=_mpdm_regcomp(r, flags)) != NULL)
	{
		do
		{
			ptr=(char *) v->data + offset;

			/* try match */
			f=!regexec((regex_t *) cr->data, ptr,
				1, &rm, offset > 0 ? REG_NOTBOL : 0);

			if(f)
			{
				/* size of the found string */
				i=rm.rm_eo - rm.rm_so;

				/* do the substitution */
				v=mpdm_splice(v, s, offset + rm.rm_so, i);
				v=mpdm_aget(v, 0);

				/* move on */
				offset+=rm.rm_so;
				if(s != NULL) offset+=s->size;
			}

		} while(f && g);
	}

	return(v);
}