/* libSoX minimal glob for MS-Windows: (c) 2009 SoX contributors
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

#include "win32-glob.h"
#include <errno.h>
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

extern void *lsx_malloc(size_t size);
extern void *lsx_realloc_array(void *p, size_t n, size_t size);
#define lsx_valloc(v,n)  v = lsx_realloc_array(NULL, (n), sizeof(*(v)))

typedef struct file_entry
{
    char name[MAX_PATH];
    struct file_entry *next;
} file_entry;

extern int _snprintf(
   char *buffer,
   size_t count,
   const char *format,
   ...
);

static int
insert(
    const char* path,
    const char* name,
    file_entry** phead)
{
    int len;
    file_entry* cur = lsx_malloc(sizeof(file_entry));

    len = _snprintf(cur->name, MAX_PATH, "%s%s", path, name);
    cur->name[MAX_PATH - 1] = 0;
    cur->next = *phead;
    *phead = cur;

    return len < 0 || len >= MAX_PATH ? ENAMETOOLONG : 0;
}

static int
entry_comparer(
    const void* pv1,
    const void* pv2)
{
	const file_entry* const * pe1 = pv1;
	const file_entry* const * pe2 = pv2;
    return _stricmp((*pe1)->name, (*pe2)->name);
}

int
glob(
    const char *pattern,
    int flags,
    void *unused,
    glob_t *pglob)
{
    char path[MAX_PATH];
    file_entry *head = NULL;
    int err = 0;
    size_t len;
    unsigned entries = 0;
    WIN32_FIND_DATAA finddata;
    HANDLE hfindfile = FindFirstFileA(pattern, &finddata);

    if (!pattern || flags != (flags & GLOB_FLAGS) || unused || !pglob)
    {
        errno = EINVAL;
        return EINVAL;
    }

    /* The terminating nul is included in the MAX_PATH (260) characters */
    if (strlen(pattern) > MAX_PATH - 1)
    {
        errno = ENAMETOOLONG;
        return ENAMETOOLONG;
    }
    strncpy(path, pattern, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    len = strlen(path);
    while (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\')
        len--;
    path[len] = 0;

    if (hfindfile == INVALID_HANDLE_VALUE)
    {
        if (flags & GLOB_NOCHECK)
        {
            err = insert("", pattern, &head);
            entries++;
        }
    }
    else
    {
        do
        {
            err = insert(path, finddata.cFileName, &head);
            entries++;
        } while (!err && FindNextFileA(hfindfile, &finddata));

        FindClose(hfindfile);
    }

    if (err == 0)
    {
        lsx_valloc(pglob->gl_pathv, entries + 1);
        pglob->gl_pathc = entries;
        pglob->gl_pathv[entries] = NULL;
        for (; head; head = head->next, entries--)
            pglob->gl_pathv[entries - 1] = (char*)head;
        qsort(pglob->gl_pathv, pglob->gl_pathc, sizeof(char*), entry_comparer);
    }
    else if (pglob)
    {
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
    }

    if (err)
    {
        file_entry *cur;
        while (head)
        {
            cur = head;
            head = head->next;
            free(cur);
        }

        errno = err;
    }

    return err;
}

void
globfree(
    glob_t* pglob)
{
    if (pglob)
    {
        char** cur;
        for (cur = pglob->gl_pathv; *cur; cur++)
        {
            free(*cur);
        }

        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
    }
}
