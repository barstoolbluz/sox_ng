/* libSoX internal functions that apply to both formats and effects
 * All public functions & data are prefixed with lsx_ .
 *
 * Copyright (c) 2008 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "sox_i.h"

#ifdef HAVE_IO_H
  #include <io.h>
#endif

#ifdef HAVE_UNISTD_H
  #include <unistd.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
  #define MKTEMP_X _O_BINARY|_O_TEMPORARY
#else
  #define MKTEMP_X 0
#endif

#ifndef HAVE_MKSTEMP
  #include <fcntl.h>
  #define mkstemp(t) open(mktemp(t), MKTEMP_X|O_RDWR|O_TRUNC|O_CREAT, S_IREAD|S_IWRITE)
  #define FAKE_MKSTEMP "fake "
#else
  #define FAKE_MKSTEMP
#endif

#ifdef _WIN32
static int check_dir(char * buf, size_t buflen, char const * name)
{
  struct stat st;
  if (!name || lsx_stat(name, &st) || (st.st_mode & S_IFMT) != S_IFDIR)
  {
    return 0;
  }
  else
  {
    strncpy(buf, name, buflen - 1);
    buf[buflen - 1] = 0;
    return strlen(name) == strlen(buf);
  }
}
#endif

/* Windows won't let you delete an open file so we have to remember them
 * and delete them at the end. Every effect that calls lsx_tmpfile() is
 * obbliged to close the file description it returned using lsx_close_tmpfile()
 * otherwise temp files don't get deleted on Windows.
 */
#ifdef _WIN32
/* A simple linked list. No limits here! */
typedef struct cell {
    char *name;        /* Pathname of the temporary file */
    int   fd;          /* What file descriptor was it assigned? */
    struct cell *next; /* Pointer to next cell in the linked list */
} cell_t;

static cell_t *tmpfiles = NULL; /* Head of the linked list of temp files */
#endif

/* Callers of lsx_tmpfile() should call lsx_close_tmpfile() when exiting
 * with the file pointer they were given by lsx_tmpfile(), otherwise
 * temporary files aren't deleted on Windows.
 */
FILE * lsx_tmpfile(void)
{
  char const * path = sox_globals.tmp_path;

  /*
  On Win32, tmpfile() is broken - it creates the file in the root directory of
  the current drive (the user probably doesn't have permission to write there!)
  instead of in a valid temporary directory (like TEMP or TMP). So if tmp_path
  is null, figure out a reasonable default.
  To force use of tmpfile(), set sox_globals.tmp_path = "".
  */
#ifdef _WIN32
  if (!path)
  {
    static char default_path[260] = "";
    if (default_path[0] == 0
        && !check_dir(default_path, sizeof(default_path), getenv("TEMP"))
        && !check_dir(default_path, sizeof(default_path), getenv("TMP"))
    #ifdef __CYGWIN__
        && !check_dir(default_path, sizeof(default_path), "/tmp")
    #endif
        )
    {
      strcpy(default_path, ".");
    }

    path = default_path;
  }
#endif

  if (path && path[0]) {
    /* Emulate tmpfile (delete on close); tmp dir is given tmp_path: */
    char const * const end = "/libSoX.tmp.XXXXXX";
    char * name = lsx_malloc(strlen(path) + strlen(end) + 1);
    int fildes;
    strcpy(name, path);
    strcat(name, end);
    fildes = mkstemp(name);

    if (fildes == -1) {
      free(name);
      return NULL;
    }

#ifdef _WIN32
    {
      /* We tack new ones on the head of the list
       * and then delete them in reverse order. */
      cell_t *new = lsx_malloc(sizeof(cell_t));
      new->name = strdup(name);
      new->fd = fildes;
      new->next = tmpfiles;
      tmpfiles = new;
    }
#endif

#ifdef HAVE_UNISTD_H
    lsx_debug(FAKE_MKSTEMP "mkstemp, name=%s (unlinked)", name);
# ifndef _WIN32
    lsx_unlink(name);
# endif
#else
    lsx_debug(FAKE_MKSTEMP "mkstemp, name=%s (O_TEMPORARY)", name);
#endif
    free(name);
    return fdopen(fildes, "w+b");
  }

  /* Use standard tmpfile (delete on close); tmp dir is undefined: */
  lsx_debug("tmpfile()");
  return tmpfile();
}

void lsx_close_tmpfile(FILE *fp)
{
#ifdef _WIN32
  cell_t *cp;
  cell_t **prevptr; /* pointer to "next" field of previous cell or to "tmpfiles"
                     * - the pointer to overwrite to delete this cell. */

  /* Find the one to delete. */
  prevptr = &tmpfiles;
  for (cp=tmpfiles; cp != NULL; cp=cp->next) {
    if (fileno(fp) == cp->fd) {
      /* Close before deleting or Windows might refuse */
      fclose(fp);
      unlink(cp->name);
      /* Remove the cell from the list */
      free(cp->name);
      *prevptr = cp->next;
      free(cp);
      return;
    }
    prevptr = &(cp->next);
  }
#endif
  fclose(fp);
}
