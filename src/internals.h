#ifndef MRUBY_GEMCUT_INTERNALS_H
#define MRUBY_GEMCUT_INTERNALS_H 1

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/value.h>
#include <mruby/data.h>
#include <mruby-gemcut.h>
#include <stdlib.h>
#include "compat.h"

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199900L) || (defined(__cplusplus) && __cplusplus >= 201100L)
# define TODO(MESG)       _Pragma(TODO_01(message("TODO: " MESG)))
# define TODO_01(ENTITY)  TODO_02(ENTITY)
# define TODO_02(ENTITY)  #ENTITY
#else
# define TODO(MESG)
#endif

#endif /* MRUBY_GEMCUT_INTERNALS_H */
