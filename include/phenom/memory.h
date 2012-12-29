/*
 * Copyright 2012 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** Memory management facility.
 *
 * It is important for long-running infrastructure software to maintain
 * information about its memory usage.  This facility allows named memory
 * types to be registered and have stats maintained against them.
 */
#ifndef PHENOM_MEMORY_H
#define PHENOM_MEMORY_H

#include <phenom/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/** represents a registered memory type */
typedef int phenom_memtype_t;
#define PHENOM_MEMTYPE_INVALID -1

/** requests that allocations are zero'd out before being returned */
#define PHENOM_MEM_FLAGS_ZERO 1

/** defines a memory type */
struct phenom_memtype_def {
  /** General category of allocations.
   * Convention is to name it after the module or subsystem.
   * This will be used to construct a counter name of the form:
   * memory/<facility>/<name>
   */
  const char *facility;
  /** Name of this memtype.
   * See note in facility above */
  const char *name;
  /** Size of each distinct object of this type of memory.
   * This may be used as a hint to the underlying allocator,
   * and impacts the metrics that are collected about the
   * allocations.
   * If the item_size is zero, then allocations may be
   * of any size */
  uint64_t item_size;
  /** One of the PHENOM_MEM_FLAGS above */
  unsigned flags;
};
typedef struct phenom_memtype_def phenom_memtype_def_t;

/** Registers a memtype
 *
 * @return the memtype identifier, or PHENOM_MEMTYPE_INVALID if
 * registration failed.
 */
phenom_memtype_t phenom_memtype_register(const phenom_memtype_def_t *def);

/** Registers a block of memtypes in one operation.
 *
 * The definitions MUST all have the same facility name.
 *
 * "defs" must point to the start of an array of "num_types" memtype definition
 * structures.
 *
 * The "types" parameter may be NULL.  If it is not NULL, it must
 * point to the start of an array of "num_types" elements to receive
 * the assigned memtype identifier for each of the registered
 * memtypes.
 *
 * This function always assigns a contiguous block of memtype identifiers.
 * @return the memtype identifier corresponding to the first definition, or
 * PHENOM_MEMTYPE_INVALID if the registration failed.
\code
phenom_memtype_def_t defs[] = {
  { "example", "one", 0, 0 },
  { "example", "two", 0, 0 }
};
struct {
  phenom_memtype_t one, two
} mt;
phenom_memtype_register_block(
  sizeof(defs) / sizeof(defs[0]),
  defs,
  &mt.one);
// Now I can use mt.one and mt.two to allocate
\endcode
 */
phenom_memtype_t phenom_memtype_register_block(
    uint8_t num_types,
    const phenom_memtype_def_t *defs,
    phenom_memtype_t *types);

/** Allocates a fixed-size memory chunk
 *
 * Given a memory type, allocates a block of memory of its defined
 * size and returns it.
 *
 * if PHENOM_MEM_FLAGS_ZERO was specified in the flags of the memtype,
 * the memory region will be cleared to zero before it is returned.
 *
 * It is an error to call this for a memtype that was defined with
 * a 0 size.
 */
void *phenom_mem_alloc(phenom_memtype_t memtype)
#ifdef __GNUC__
  __attribute__((malloc))
#endif
  ;

/** Allocates a variable-size memory chunk
 *
 * Given a memory type that was registered with size 0, allocates
 * a chunk of the specified size and returns it.
 *
 * if PHENOM_MEM_FLAGS_ZERO was specified in the flags of the memtype,
 * the memory region will be cleared to zero before it is returned.
 *
 * It is an error to call this for a memtype that was not defined with
 * a 0 size.
 */
void *phenom_mem_alloc_size(phenom_memtype_t memtype, uint64_t size)
#ifdef __GNUC__
  __attribute__((malloc))
#endif
  ;

/** Reallocates a variable-size memory chunk.
 *
 * Changes the size of the memory pointed to by "ptr" to "size" bytes.
 * The contents of the memory at "ptr" are unchanged to the minimum
 * of the old and new sizes.
 *
 * If the block is grown, the remaining space will hold undefined
 * values unless PHENOM_MEM_FLAGS_ZERO was specified in the memtype.
 *
 * If ptr is NULL, this is equivalent to phenom_mem_alloc_size().
 *
 * If size is 0, this is equivalent to phenom_mem_free().
 *
 * It is an error if ptr was allocated against a different memtype.
 *
 * If the memory was moved, the original ptr value will be freed.
 */
void *phenom_mem_realloc(phenom_memtype_t memtype, void *ptr, uint64_t size);

/** Frees a memory chunk
 *
 * The memory MUST have been allocated via phenom_mem_alloc()
 * or phenom_mem_alloc_size().
 */
void phenom_mem_free(phenom_memtype_t memtype, void *ptr);

/** Data structure for querying memory usage information */
struct phenom_mem_stats {
  /** the definition */
  const phenom_memtype_def_t *def;
  /** current amount of allocated memory in bytes */
  uint64_t bytes;
  /** total number of out-of-memory events (allocation failures) */
  uint64_t oom;
  /** total number of successful allocation events */
  uint64_t allocs;
  /** total number of calls to free */
  uint64_t frees;
  /** total number of calls to realloc (that are not themselves
   * equivalent to an alloc or free) */
  uint64_t reallocs;
};
typedef struct phenom_mem_stats phenom_mem_stats_t;

/** Queries information about the specified memtype.
 *
 * @param memtype the memtype being interrogated
 * @param[out] stats receives the usage information
 */
bool phenom_mem_stat(phenom_memtype_t memtype, phenom_mem_stats_t *stats);

/** Queries information about memtypes in the specified facility.
 *
 * @param facility the facility name of interest
 * @param num_stats the number of elements in the stats array
 * @param[out] stats array of num_stats elements, receives the stats
 * @return the number of stats that were populated.
 */
int phenom_mem_stat_facility(const char *facility,
    int num_stats, phenom_mem_stats_t *stats);

/** Queries information about a range of memtypes in the system.
 *
 * @param start starting memtype in the range
 * @param end ending memtype of the range (exclusive)
 * @param[out] stats array of (end - start) elements to receive stats
 * @return the number of stats that were populated.
 * A short return value indicates that there are no more memtypes beyond
 * the "end" parameter.
 */
int phenom_mem_stat_range(phenom_memtype_t start,
    phenom_memtype_t end, phenom_mem_stats_t *stats);

/** Resolves a memory type by name
 *
 * Intended as a diagnostic/testing aid.
 */
phenom_memtype_t phenom_mem_type_by_name(const char *facility,
    const char *name);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:et:
 */

