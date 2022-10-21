#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef ALLOC
	#define ALLOC(size) calloc(size, 1)
#endif

#ifndef FREE
	#define FREE(ptr) free(ptr)
#endif
