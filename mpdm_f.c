/*

    MPDM - Minimum Profit Data Manager
    Copyright (C) 2003/2006 Angel Ortega <angel@triptico.com>

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
#include <wchar.h>

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

#include "mpdm.h"

#ifdef CONFOPT_ICONV

#include <iconv.h>
#include <errno.h>

#endif

/* file structure */
struct mpdm_file
{
	FILE * fd;

#ifdef CONFOPT_ICONV

	iconv_t ic_enc;
	iconv_t ic_dec;

#endif /* CONFOPT_ICONV */
};


/*******************
	Code
********************/


static int get_char(struct mpdm_file * f)
/* reads a character from a file structure */
{
	return(fgetc(f->fd));
}


static void put_char(int c, struct mpdm_file * f)
/* writes a character in a file structure */
{
	fputc(c, f->fd);
}


static wchar_t * read_mbs(struct mpdm_file * f, int * s)
/* reads a multibyte string from a mpdm_file into a dynamic string */
{
	wchar_t * ptr = NULL;
	char * cptr = NULL;
	char tmp[2];
	int c, n = 0;

	tmp[1] = '\0';

	while((c = get_char(f)) != EOF)
	{
		tmp[0] = c;
		if((cptr = mpdm_poke(cptr, &n, tmp, 1, sizeof(char))) == NULL)
			break;

		/* if it's an end of line, finish */
		if(c == '\n')
			break;
	}

	if(cptr != NULL)
	{
		/* do the conversion */
		ptr = mpdm_mbstowcs(cptr, s, -1);

		free(cptr);
	}

	return(ptr);
}


static void write_wcs(struct mpdm_file * f, wchar_t * str)
/* writes a wide string to an struct mpdm_file */
{
	int s, n;
	char * ptr;

	ptr = mpdm_wcstombs(str, &s);

	for(n = 0;n < s;n++)
		put_char(ptr[n], f);

	free(ptr);
}


#ifdef CONFOPT_ICONV

extern int errno;

static wchar_t * read_iconv(struct mpdm_file * f, int * s)
/* reads a multibyte string transforming with iconv */
{
	char tmp[128];
	wchar_t * ptr = NULL;
	int c, i;
	wchar_t wc[2];

	*s = i = 0;
	wc[1] = L'\0';

	/* resets the decoder */
	iconv(f->ic_dec, NULL, NULL, NULL, NULL);

	while((c = get_char(f)) != EOF)
	{
		size_t il, ol;
		char * iptr, * optr;

		tmp[i++] = c;

		/* too big? shouldn't happen */
		if(i == sizeof(tmp))
			break;

		il = i; iptr = tmp;
		ol = sizeof(wchar_t); optr = (char *)wc;

		/* write to file */
		if(iconv(f->ic_dec, &iptr, &il, &optr, &ol) == -1)
		{
			/* found incomplete multibyte character */
			if(errno == EINVAL)
				continue;

			/* otherwise, return '?' */
			wc[0] = L'?';
		}

		i = 0;

		if((ptr = mpdm_poke(ptr, s, wc, 1, sizeof(wchar_t))) == NULL)
			break;

		/* if it's an end of line, finish */
		if(wc[0] == L'\n')
			break;
	}

	return(ptr);
}


static void write_iconv(struct mpdm_file * f, wchar_t * str)
/* writes a wide string to a stream using iconv */
{
	char tmp[128];

	/* resets the encoder */
	iconv(f->ic_enc, NULL, NULL, NULL, NULL);

	/* convert char by char */
	for(;*str != L'\0';str++)
	{
		size_t il, ol;
		char * iptr, * optr;
		int n;

		il = sizeof(wchar_t); iptr = (char *)str;
		ol = sizeof(tmp); optr = tmp;

		/* write to file */
		if(iconv(f->ic_enc, &iptr, &il, &optr, &ol) == -1)
		{
			/* error converting; convert a '?' instead */
			wchar_t q = L'?';

			il = sizeof(wchar_t); iptr = (char *)&q;
			ol = sizeof(tmp); optr = tmp;

			iconv(f->ic_enc, &iptr, &il, &optr, &ol);
		}

		for(n = 0;n < sizeof(tmp) - ol;n++)
			put_char(tmp[n], f);
	}
}

#endif /* CONFOPT_ICONV */


static mpdm_t new_mpdm_file(void)
/* creates a new file value */
{
	mpdm_t v = NULL;
	struct mpdm_file * fs;

	if((fs = malloc(sizeof(struct mpdm_file))) == NULL)
		return(NULL);

	memset(fs, '\0', sizeof(struct mpdm_file));

#ifdef CONFOPT_ICONV

	if((v = mpdm_hget_s(mpdm_root(), L"ENCODING")) != NULL)
	{
		mpdm_t cs = MPDM_2MBS(v->data);

		fs->ic_enc = iconv_open((char *)cs->data, "WCHAR_T");
		fs->ic_dec = iconv_open("WCHAR_T", (char *)cs->data);
	}
	else
		fs->ic_enc = fs->ic_dec = (iconv_t) -1;

#endif

	if((v = mpdm_new(MPDM_FILE|MPDM_FREE, fs,
		sizeof(struct mpdm_file))) == NULL)
		free(fs);

	return(v);
}


static void destroy_mpdm_file(mpdm_t v)
/* destroys and file value */
{
	struct mpdm_file * fs = v->data;

	if(fs != NULL)
	{
#ifdef CONFOPT_ICONV

		if(fs->ic_enc != (iconv_t) -1)
		{
			iconv_close(fs->ic_enc);
			fs->ic_enc = (iconv_t) -1;
		}

		if(fs->ic_dec != (iconv_t) -1)
		{
			iconv_close(fs->ic_dec);
			fs->ic_dec = (iconv_t) -1;
		}
#endif

		free(fs);
		v->data = NULL;
	}
}


/** interface **/

wchar_t * mpdm_read_mbs(FILE * f, int * s)
/* reads a multibyte string from a stream into a dynamic string */
{
	struct mpdm_file fs;

	/* reset the structure */
	memset(&fs, '\0', sizeof(fs));
	fs.fd = f;

	return(read_mbs(&fs, s));
}


void mpdm_write_wcs(FILE * f, wchar_t * str)
/* writes a wide string to a stream */
{
	struct mpdm_file fs;

	/* reset the structure */
	memset(&fs, '\0', sizeof(fs));
	fs.fd = f;

	write_wcs(&fs, str);
}


mpdm_t mpdm_new_f(FILE * f)
/* creates a new file value from a FILE * */
{
	mpdm_t v = NULL;

	if(f == NULL) return(NULL);

	if((v = new_mpdm_file()) != NULL)
	{
		struct mpdm_file * fs = v->data;
		fs->fd = f;
	}

	return(v);
}


/**
 * mpdm_open - Opens a file.
 * @filename: the file name
 * @mode: an fopen-like mode string
 *
 * Opens a file. If @filename can be open in the specified @mode, an
 * mpdm_t value will be returned containing the file descriptor, or NULL
 * otherwise.
 * [File Management]
 */
mpdm_t mpdm_open(mpdm_t filename, mpdm_t mode)
{
	FILE * f;

	if(filename == NULL || mode == NULL)
		return(NULL);

	/* convert to mbs,s */
	filename = MPDM_2MBS(filename->data);
	mode = MPDM_2MBS(mode->data);

	f = fopen((char *)filename->data, (char *)mode->data);

	return(MPDM_F(f));
}


/**
 * mpdm_close - Closes a file descriptor.
 * @fd: the value containing the file descriptor
 *
 * Closes the file descriptor.
 * [File Management]
 */
mpdm_t mpdm_close(mpdm_t fd)
{
	struct mpdm_file * fs = fd->data;

	if((fd->flags & MPDM_FILE) && fs != NULL)
	{
		if(fs->fd != NULL)
			fclose(fs->fd);

		destroy_mpdm_file(fd);
	}

	return(NULL);
}


/**
 * mpdm_read - Reads a line from a file descriptor.
 * @fd: the value containing the file descriptor
 *
 * Reads a line from @fd. Returns the line, or NULL on EOF.
 * [File Management]
 * [Character Set Conversion]
 */
mpdm_t mpdm_read(mpdm_t fd)
{
	mpdm_t v = NULL;
	wchar_t * ptr;
	int s;
	struct mpdm_file * fs = fd->data;

	if(fs == NULL)
		return(NULL);

#ifdef CONFOPT_ICONV

	if(fs->ic_dec != (iconv_t) -1)
		ptr = read_iconv(fs, &s);
	else

#endif /* CONFOPT_ICONV */

		ptr = read_mbs(fs, &s);

	if(ptr != NULL)
		v = MPDM_ENS(ptr, s);

	return(v);
}


/**
 * mpdm_write - Writes a value into a file.
 * @fd: the file descriptor.
 * @v: the value to be written.
 *
 * Writes the @v string value into @fd, using the current encoding.
 * [File Management]
 * [Character Set Conversion]
 */
int mpdm_write(mpdm_t fd, mpdm_t v)
{
	struct mpdm_file * fs = fd->data;

	if(fs == NULL)
		return(-1);

#ifdef CONFOPT_ICONV

	if(fs->ic_enc != (iconv_t) -1)
		write_iconv(fs, mpdm_string(v));
	else

#endif /* CONFOPT_ICONV */

		write_wcs(fs, mpdm_string(v));

	return(0);
}


/*
mpdm_t mpdm_bread(mpdm_t fd, int size)
{
}


int mpdm_bwrite(mpdm_tfd, mpdm_t v, int size)
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
 * is reverted to default charset conversion (i.e. the one defined
 * in the locale).
 * Returns a negative number if @charset is unsupported, or zero
 * if no errors were found.
 * [File Management]
 * [Character Set Conversion]
 */
int mpdm_encoding(mpdm_t charset)
{
	int ret = -1;

#ifdef CONFOPT_ICONV

	if(charset != NULL)
	{
		iconv_t ic;
		mpdm_t cs = MPDM_2MBS(charset->data);

		/* tries to create an encoder and a decoder for this charset */

		if((ic = iconv_open("WCHAR_T", (char *)cs->data)) == (iconv_t) -1)
			return(-1);

		iconv_close(ic);

		if((ic = iconv_open((char *)cs->data, "WCHAR_T")) == (iconv_t) -1)
			return(-2);

		iconv_close(ic);
	}

	/* can create; store and exit */

	mpdm_hset_s(mpdm_root(), L"ENCODING", charset);

	ret = 0;

#endif

	return(ret);
}


/**
 * mpdm_unlink - Deletes a file.
 * @filename: file name to be deleted
 *
 * Deletes a file.
 * [File Management]
 */
int mpdm_unlink(mpdm_t filename)
{
	/* convert to mbs */
	filename = MPDM_2MBS(filename->data);

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
 * [File Management]
 */
mpdm_t mpdm_glob(mpdm_t spec)
{
	mpdm_t v = NULL;

#ifdef CONFOPT_WIN32

	WIN32_FIND_DATA f;
	HANDLE h;
	char * ptr;
	mpdm_t w;
	mpdm_t s = NULL;

	/* convert to mbs */
	if(spec != NULL)
		spec = MPDM_2MBS(spec->data);
	else
		spec = MPDM_2MBS(L"*.*");

	ptr = (char *)spec->data;

	/* convert MSDOS dir separators into Unix ones */
	for(;*ptr != '\0';ptr++)
	{
		if(*ptr == '\\')
			*ptr = '/';
	}

	v = MPDM_A(0);

	if((h = FindFirstFile((char *)spec->data, &f)) != INVALID_HANDLE_VALUE)
	{
		/* if spec includes a directory, store in s */
		if((ptr = strrchr((char *)spec->data, '/')) != NULL)
		{
			*(ptr + 1) = '\0';
			s = MPDM_S(spec->data);
		}

		do
		{
			/* ignore . and .. */
			if(strcmp(f.cFileName,".") == 0 ||
			   strcmp(f.cFileName,"..") == 0)
				continue;

			/* concat base directory and file names */
			w = mpdm_strcat(s, MPDM_MBS(f.cFileName));

			/* if it's a directory, add a / */
			if(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				w = mpdm_strcat(w, MPDM_LS(L"/"));

			mpdm_push(v, w);
		}
		while(FindNextFile(h, &f));

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
		spec = MPDM_2MBS(spec->data);

		ptr = spec->data;
		if(ptr == NULL || *ptr == '\0')
			ptr = "*";
	}
	else
		ptr = "*";

	globbuf.gl_offs = 1;
	v = MPDM_A(0);

	if(glob(ptr, GLOB_MARK, NULL, &globbuf) == 0)
	{
		for(n = 0;globbuf.gl_pathv[n] != NULL;n++)
			mpdm_push(v, MPDM_MBS(globbuf.gl_pathv[n]));
	}

	globfree(&globbuf);

#else

	/* no win32 nor glob.h; try workaround */
	/* ... */

#endif

	return(v);
}


/**
 * mpdm_popen - Opens a pipe.
 * @prg: the program to pipe
 * @mode: an fopen-like mode string
 *
 * Opens a pipe to a program. If @prg can be open in the specified @mode, an
 * mpdm_t value will be returned containing the file descriptor, or NULL
 * otherwise.
 * [File Management]
 */
mpdm_t mpdm_popen(mpdm_t prg, mpdm_t mode)
{
	FILE * f;
	mpdm_t v;
	int ok = 0;

	if(prg == NULL || mode == NULL)
		return(NULL);

	if((v = new_mpdm_file()) == NULL)
		return(NULL);

	/* convert to mbs,s */
	prg = MPDM_2MBS(prg->data);
	mode = MPDM_2MBS(mode->data);

#ifdef CONFOPT_WIN32

#else /* CONFOPT_WIN32 */

	if((f = popen((char *)prg->data, (char *)mode->data)) == NULL)
	{
		struct mpdm_file * fs = v->data;
		fs->fd = f;

		ok = 1;
	}

#endif /* CONFOPT_WIN32 */

	if(!ok)
	{
		destroy_mpdm_file(v);
		v = NULL;
	}

	return(v);
}


/**
 * mpdm_pclose - Closes a pipe.
 * @fd: the value containing the file descriptor
 *
 * Closes a pipe.
 * [File Management]
 */
mpdm_t mpdm_pclose(mpdm_t fd)
{
	struct mpdm_file * fs = fd->data;

	if((fd->flags & MPDM_FILE) && fs != NULL)
	{
#ifdef CONFOPT_WIN32

#else /* CONFOPT_WIN32 */

		if(fs->fd != NULL)
			pclose(fs->fd);

#endif /* CONFOPT_WIN32 */

		destroy_mpdm_file(fd);
	}

	return(NULL);
}
