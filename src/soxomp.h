/* Copyright (c) 2008 Rob Sykes (robs@users.sourceforge.net)
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
#include "soxconfig.h"

#ifdef _OPENMP
  #if _OPENMP >= 200805 /* OpenMP 3.0 */
    #define HAVE_OPENMP 1
  #endif
  #if _OPENMP >= 201107 /* OpenMP 3.1 */
    #define HAVE_OPENMP_3_1 1
  #endif
#endif

#ifdef HAVE_OPENMP
  #include <omp.h>
#else

typedef int omp_lock_t;
typedef int omp_nest_lock_t;

#define omp_set_num_threads(int) (void)0
#define omp_get_num_threads() 1
#define omp_get_max_threads() 1
#define omp_get_thread_num() 0
#define omp_get_num_procs() 1
#define omp_in_parallel() 1

#define omp_set_dynamic(int) (void)0
#define omp_get_dynamic() 0

#define omp_set_nested(int) (void)0
#define omp_get_nested() 0

#define omp_init_lock(omp_lock_t) (void)0
#define omp_destroy_lock(omp_lock_t) (void)0
#define omp_set_lock(omp_lock_t) (void)0
#define omp_unset_lock(omp_lock_t) (void)0
#define omp_test_lock(omp_lock_t) 0

#define omp_init_nest_lock(omp_nest_lock_t) (void)0
#define omp_destroy_nest_lock(omp_nest_lock_t) (void)0
#define omp_set_nest_lock(omp_nest_lock_t) (void)0
#define omp_unset_nest_lock(omp_nest_lock_t) (void)0
#define omp_test_nest_lock(omp_nest_lock_t) 0

#define omp_get_wtime() 0
#define omp_get_wtick() 0

#endif
