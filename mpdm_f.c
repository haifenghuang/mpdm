/*

    mpdm - Minimum Profit Data Manager
    Copyright (C) 2003/2005 Angel Ortega <angel@triptico.com>

    mpdm_f.c - File management

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

#ifdef CONFOPT_UNISTD_H
#include <unistd.h>
#endif

#ifdef CONFOPT_GLOB_H
#include <glob.h>
#endif

#ifdef CONFOPT_WIN32
#include <windows.h>
#include <commctrl.h>
#endif

#ifdef CONFOPT_ICONV
#include <iconv.h>
#endif

#include "mpdm.h"

/* file encoder/decoder information */

static mpdm_v _f_enc=NULL;
static mpdm_v _f_dec=NULL;


/*******************
	Code
********************/

static mpdm_v _tie_file(void)
/* tie for files */
{
	static mpdm_v _tie=NULL;
	mpdm_v t;

	if(_tie == NULL)
	{
		_tie=mpdm_ref(MPDM_A(2));

		/* mpdm_close() is used as the file destructor */
		mpdm_aset(_tie, MPDM_X(mpdm_close), MPDM_TIE_DESTROY);
	}

	if(_f_enc != NULL || _f_dec != NULL)
	{
		/* if file co/decs are defined, clone the tie and
		   store them inside it */
		t=mpdm_clone(_tie);

		mpdm_aset(t, _f_enc, MPDM_TIE_FENC);
		mpdm_aset(t, _f_dec, MPDM_TIE_FDEC);
	}
	else
		t=_tie;

	return(t);
}


#ifdef CONFOPT_ICONV

static mpdm_v _tie_iconv_d(mpdm_v v)
/* destroy tie for iconv objects */
{
	if(v->data != NULL)
	{
		/* closes the iconv data */
		iconv_close((iconv_t *)v->data);

		/* frees the struct itself */
		free(v->data);
		v->data=NULL;
	}

	return(NULL);
}


static mpdm_v _tie_iconv(void)
/* tie for iconv objects */
{
	static mpdm_v _tie=NULL;

	if(_tie == NULL)
	{
		/* creates a clone of the cpy tie */
		_tie=mpdm_ref(mpdm_clone(_mpdm_tie_cpy()));

		/* replaces the destructor with its own */
		mpdm_aset(_tie, MPDM_X(_tie_iconv_d), MPDM_TIE_DESTROY);
	}

	return(_tie);
}


#endif /* CONFOPT_ICONV */


void _mpdm_write_wcs(FILE * f, wchar_t * str)
/* writes a wide string to a stream, converting */
{
	char tmp[MB_CUR_MAX + 1];
	int l,n;

	while(*str != L'\0')
	{
		if((l=wctomb(tmp, *str)) <= 0)
		{
			/* if char couldn't be converted,
			   write a question mark instead */
			l=1;
			tmp[0]='?';
		}

		for(n=0;n < l;n++)
			fputc(tmp[n], f);

		str++;
	}
}


/**
 * mpdm_open - Opens a file.
 * @filename: the file name
 * @mode: an fopen-like mode string
 *
 * Opens a file. If @filename can be open in the specified @mode, an
 * mpdm_v value will be returned containing the file descriptor, or NULL
 * otherwise.
 */
mpdm_v mpdm_open(mpdm_v filename, mpdm_v mode)
{
	FILE * f;
	mpdm_v fd;

	/* convert to mbs,s */
	filename=MPDM_2MBS(filename->data);
	mode=MPDM_2MBS(mode->data);

	if((f=fopen((char *)filename->data, (char *)mode->data)) == NULL)
		return(NULL);

	/* creates the file value */
	fd=MPDM_F(f);

	return(fd);
}


/**
 * mpdm_close - Closes a file descriptor.
 * @fd: the value containing the file descriptor
 *
 * Closes the file descriptor.
 */
mpdm_v mpdm_close(mpdm_v fd)
{
	if((fd->flags & MPDM_FILE) && fd->data != NULL)
		fclose((FILE *)fd->data);

	fd->data=NULL;

	return(NULL);
}


/**
 * mpdm_read - Reads a line from a file descriptor.
 * @fd: the value containing the file descriptor
 *
 * Reads a line from @fd. Returns the line, or NULL on EOF.
 */
mpdm_v mpdm_read(mpdm_v fd)
{
	char line[128];
	mpdm_v v=NULL;
	int i;
	FILE * f;

	if((f=(FILE *)fd->data) == NULL)
		return(NULL);

	while(fgets(line, sizeof(line) - 1, f) != NULL)
	{
		if((i=strlen(line)) == 0)
			continue;

		/* if line includes \n, it's complete */
		if(line[i - 1] == '\n')
		{
			line[i]='\0';
			i=0;
		}

		/* store */
		v=mpdm_strcat(v, MPDM_MBS(line));

		/* exit if the line is completely read */
		if(i == 0) break;
	}

	return(v);
}


/**
 * mpdm_write - Writes a value into a file.
 * @fd: the file descriptor.
 * @v: the value to be written.
 *
 * Writes the @v string value into @fd, using the current encoding.
 */
int mpdm_write(mpdm_v fd, mpdm_v v)
{
	_mpdm_write_wcs((FILE *)fd->data, mpdm_string(v));

	return(0);
}


/*
mpdm_v mpdm_bread(mpdm_v fd, int size)
{
}


int mpdm_bwrite(mpdm_vfd, mpdm_v v, int size)
{
}
*/


/**
 * mpdm_encoding - Sets the current charset encoding for files.
 * @charset: the charset name.
 *
 * Sets the current charset encoding for files. Future opened
 * files will be assumed to be encoded with @charset, which can
 * be any of the supported charset names (utf-8, iso-8859-1, etc.),
 * and converted on each read / write. If charset is NULL, it
 * is reverted to default charset conversion (i.e. the locale).
 * If @charset is a supported one, zero is returned, or nonzero
 * if it's an unsupported encoding.
 */
int mpdm_encoding(mpdm_v charset)
{
	/* unref current co/decs */
	mpdm_unref(_f_enc);
	mpdm_unref(_f_dec);

	/* reset */
	_f_enc=_f_dec=NULL;

	if(charset != NULL)
	{

#ifdef CONFOPT_ICONV

		mpdm_v e;
		iconv_t ic;

		e=MPDM_2MBS(charset->data);

		/* creates the converters */

		if((ic=iconv_open((char *)e->data, "WCHAR_T")) == NULL)
			return(-1);

		/* encoder */
		_f_enc=mpdm_new(0, &ic, sizeof(iconv_t), _tie_iconv());

		if((ic=iconv_open("WCHAR_T", (char *)e->data)) == NULL)
			return(-2);

		/* decoder */
		_f_dec=mpdm_new(0, &ic, sizeof(iconv_t), _tie_iconv());

#endif /* CONFOPT_ICONV */

	}

	/* ref new co/decs */
	mpdm_ref(_f_enc);
	mpdm_ref(_f_dec);

	return(0);
}


/**
 * mpdm_unlink - Deletes a file.
 * @filename: file name to be deleted
 *
 * Deletes a file.
 */
int mpdm_unlink(mpdm_v filename)
{
	/* convert to mbs */
	filename=MPDM_2MBS(filename->data);

	return(unlink((char *)filename->data));
}


/**
 * mpdm_glob - Executes a file globbing.
 * @spec: Globbing spec
 *
 * Executes a file globbing. @spec is system-dependent, but usually
 * the * and ? metacharacters work everywhere. @spec can contain a
 * directory; if that's the case, the output strings will include it.
 * In any case, each returned value will be suitable for a call to
 * mpdm_open().
 *
 * Returns an array of files that match the globbing (can be an empty
 * array if no file matches), or NULL if globbing is unsupported.
 */
mpdm_v mpdm_glob(mpdm_v spec)
{
	mpdm_v v=NULL;

#ifdef CONFOPT_WIN32

	WIN32_FIND_DATA f;
	HANDLE h;
	char * ptr;
	mpdm_v w;
	mpdm_v s=NULL;

	/* convert to mbs */
	if(spec != NULL)
		spec=MPDM_2MBS(spec->data);
	else
		spec=MPDM_2MBS(L"*.*");

	ptr=(char *)spec->data;

	/* convert MSDOS dir separators into Unix ones */
	for(;*ptr != '\0';ptr++)
	{
		if(*ptr == '\\')
			*ptr='/';
	}

	v=MPDM_A(0);

	if((h=FindFirstFile((char *)spec->data, &f)) != INVALID_HANDLE_VALUE)
	{
		/* if spec includes a directory, store in s */
		if((ptr=strrchr((char *)spec->data, '/')) != NULL)
		{
			*(ptr+1)='\0';
			s=MPDM_S(spec->data);
		}

		do
		{
			/* ignore . and .. */
			if(strcmp(f.cFileName,".") == 0 ||
			   strcmp(f.cFileName,"..") == 0)
				continue;

			/* concat base directory and file names */
			w=mpdm_strcat(s, MPDM_MBS(f.cFileName));

			/* if it's a directory, add a / */
			if(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				w=mpdm_strcat(w, MPDM_LS(L"/"));

			mpdm_apush(v, w);
		}
		while(FindNextFile(h,&f));

		FindClose(h);
	}

#endif

#if CONFOPT_GLOB_H

	/* glob.h support */
	int n;
	glob_t globbuf;
	char * ptr;

	/* convert to mbs */
	if(spec != NULL)
	{
		spec=MPDM_2MBS(spec->data);

		ptr=spec->data;
		if(ptr == NULL || *ptr == '\0')
			ptr="*";
	}
	else
		ptr="*";

	globbuf.gl_offs=1;
	v=MPDM_A(0);

	if(glob(ptr, GLOB_MARK, NULL, &globbuf) == 0)
	{
		for(n=0;globbuf.gl_pathv[n]!=NULL;n++)
			mpdm_apush(v, MPDM_MBS(globbuf.gl_pathv[n]));
	}

	globfree(&globbuf);

#else

	/* no win32 nor glob.h; try workaround */
	/* ... */

#endif

	return(v);
}
