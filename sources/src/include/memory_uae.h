/*
 * UAE - The Un*x Amiga Emulator
 *
 * memory management
 *
 * Copyright 1995 Bernd Schmidt
 */

#ifndef UAE_MEMORY_H
#define UAE_MEMORY_H

#ifndef HAS_UAE_PREFS_STRUCT
struct uae_prefs;
#endif // HAS_UAEPREFS_STRUCT

extern void memory_reset (void);
extern void a1000_reset (void);

#ifdef JIT
extern int special_mem;
#define S_READ 1
#define S_WRITE 2

extern uae_u8 *cache_alloc (int);
extern void cache_free (uae_u8*);

#ifdef NATMEM_OFFSET
bool init_shm (void);
void free_shm (void);
bool preinit_shm (void);
#endif

extern bool canbang;
extern int candirect;
#endif

#ifdef ADDRESS_SPACE_24BIT
#define MEMORY_BANKS 256
#define MEMORY_RANGE_MASK ((1<<24)-1)
#else
#define MEMORY_BANKS 65536
#define MEMORY_RANGE_MASK (~0)
#endif

typedef uae_u32 (REGPARAM3 *mem_get_func)(uaecptr) REGPARAM;
typedef void (REGPARAM3 *mem_put_func)(uaecptr, uae_u32) REGPARAM;
typedef uae_u8 *(REGPARAM3 *xlate_func)(uaecptr) REGPARAM;
typedef int (REGPARAM3 *check_func)(uaecptr, uae_u32) REGPARAM;

extern uae_u8 *address_space, *good_address_map;
extern uae_u8 *chipmemory;

extern uae_u32 allocated_chipmem;
extern uae_u32 allocated_fastmem;
extern uae_u32 allocated_bogomem;
extern uae_u32 allocated_gfxmem;
extern uae_u32 allocated_z3fastmem, allocated_z3fastmem2, allocated_z3chipmem;
extern uae_u32 max_z3fastmem;
extern uae_u32 allocated_a3000mem;
extern uae_u32 allocated_cardmem;

extern uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode);
extern void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v);
extern uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode);
extern void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v);

#undef DIRECT_MEMFUNCS_SUCCESSFUL
#include "machdep/maccess.h"
#include "retrodep/memory_uae.h"

#define chipmem_start 0x00000000
#define bogomem_start 0x00C00000
#define cardmem_start 0x00E00000
#define kickmem_start 0x00F80000

#define ROM_SIZE_512 524288
#define ROM_SIZE_256 262144
#define ROM_SIZE_128 131072

extern uaecptr z3fastmem_start, z3fastmem2_start, z3chipmem_start;
extern uaecptr p96ram_start;
extern uaecptr fastmem_start;
extern uaecptr a3000lmem_start, a3000hmem_start;

extern bool ersatzkickfile;
extern bool cloanto_rom, kickstart_rom;
extern uae_u16 kickstart_version;
extern bool uae_boot_rom;
extern int uae_boot_rom_size;
extern uaecptr rtarea_base;

extern uae_u8* baseaddr[];

enum { ABFLAG_UNK = 0, ABFLAG_RAM = 1, ABFLAG_ROM = 2, ABFLAG_ROMIN = 4, ABFLAG_IO = 8, ABFLAG_NONE = 16, ABFLAG_SAFE = 32 };
typedef struct {
	/* These ones should be self-explanatory... */
	mem_get_func lget, wget, bget;
	mem_put_func lput, wput, bput;
	/* Use xlateaddr to translate an Amiga address to a uae_u8 * that can
	 * be used to address memory without calling the wget/wput functions.
	 * This doesn't work for all memory banks, so this function may call
	 * abort(). */
	xlate_func xlateaddr;
	/* To prevent calls to abort(), use check before calling xlateaddr.
	 * It checks not only that the memory bank can do xlateaddr, but also
	 * that the pointer points to an area of at least the specified size.
	 * This is used for example to translate bitplane pointers in custom.c */
	check_func check;
	/* For those banks that refer to real memory, we can save the whole trouble
	of going through function calls, and instead simply grab the memory
	ourselves. This holds the memory address where the start of memory is
	for this particular bank. */
	uae_u8 *baseaddr;
	TCHAR *name;
	/* for instruction opcode/operand fetches */
	mem_get_func lgeti, wgeti;
	int flags;
} addrbank;

#define CE_MEMBANK_FAST 0
#define CE_MEMBANK_CHIP 1
#define CE_MEMBANK_CIA 2
#define CE_MEMBANK_FAST16BIT 3
extern uae_u8 ce_banktype[65536], ce_cachable[65536];

extern uae_u8 *filesysory;
extern uae_u8 *rtarea;

extern addrbank chipmem_bank;
extern addrbank chipmem_agnus_bank;
extern addrbank chipmem_bank_ce2;
extern addrbank kickmem_bank;
extern addrbank custom_bank;
extern addrbank clock_bank;
extern addrbank cia_bank;
extern addrbank rtarea_bank;
extern addrbank expamem_bank;
extern addrbank fastmem_bank;
extern addrbank gfxmem_bank;
#ifdef GAYLE
extern addrbank gayle_bank;
extern addrbank gayle2_bank;
#endif // GAYLE
extern addrbank mbres_bank;
extern addrbank akiko_bank;
extern addrbank cardmem_bank;

extern void rtarea_init (void);
extern void rtarea_init_mem (void);
extern void rtarea_setup (void);
extern void expamem_init (void);
extern void expamem_reset (void);
extern void expamem_next (void);

extern uae_u32 gfxmem_start;
extern uae_u8 *gfxmemory;
extern uae_u32 gfxmem_mask;
extern uae_u16 last_custom_value1;

/* Default memory access functions */

extern int REGPARAM3 default_check(uaecptr addr, uae_u32 size) REGPARAM;
extern uae_u8 *REGPARAM3 default_xlate(uaecptr addr) REGPARAM;
/* 680x0 opcode fetches */
extern uae_u32 REGPARAM3 dummy_lgeti (uaecptr addr) REGPARAM;
extern uae_u32 REGPARAM3 dummy_wgeti (uaecptr addr) REGPARAM;

#define bankindex(addr) (((uaecptr)(addr)) >> 16)

extern addrbank *mem_banks[MEMORY_BANKS];

#ifdef JIT
extern uae_u8 *baseaddr[MEMORY_BANKS];
#endif

#define get_mem_bank(addr) (*mem_banks[bankindex(addr)])

#ifdef JIT
#define put_mem_bank(addr, b, realstart) { \
	uaecptr idx = bankindex((uae_u32)(addr)); \
	(mem_banks[idx] = (b)); \
	if ((b)->baseaddr) \
	baseaddr[idx] = (b)->baseaddr - (realstart); \
	else \
	baseaddr[idx] = (uae_u8*)(((uae_u8*)b)+1); \
}
#else
#define put_mem_bank(addr, b, realstart) \
	(mem_banks[bankindex(addr)] = (b));
#endif

extern void memory_init (void);
extern void memory_cleanup (void);
extern void map_banks (addrbank *bank, int first, int count, int realsize);
extern void map_banks_cond (addrbank *bank, int first, int count, int realsize);
extern void map_overlay (int chip);
extern void memory_hardreset (int);
extern void memory_clear (void);
extern void free_fastmemory (void);

#define longget(addr) (call_mem_get_func(get_mem_bank(addr).lget, addr))
#define wordget(addr) (call_mem_get_func(get_mem_bank(addr).wget, addr))
#define byteget(addr) (call_mem_get_func(get_mem_bank(addr).bget, addr))
#define longgeti(addr) (call_mem_get_func(get_mem_bank(addr).lgeti, addr))
#define wordgeti(addr) (call_mem_get_func(get_mem_bank(addr).wgeti, addr))
#define longput(addr,l) (call_mem_put_func(get_mem_bank(addr).lput, addr, l))
#define wordput(addr,w) (call_mem_put_func(get_mem_bank(addr).wput, addr, w))
#define byteput(addr,b) (call_mem_put_func(get_mem_bank(addr).bput, addr, b))

STATIC_INLINE uae_u32 get_long (uaecptr addr)
{
	return longget (addr);
}
STATIC_INLINE uae_u32 get_word (uaecptr addr)
{
	if (addr > 0xffffffff)
	   return 0;
	return wordget (addr);
}
STATIC_INLINE uae_u32 get_byte (uaecptr addr)
{
	return byteget (addr);
}
STATIC_INLINE uae_u32 get_longi(uaecptr addr)
{
	return longgeti (addr);
}
STATIC_INLINE uae_u32 get_wordi(uaecptr addr)
{
	return wordgeti (addr);
}

/*
 * Read a host pointer from addr
 */
#if SIZEOF_VOID_P == 4
# define get_pointer(addr) ((void *)get_long (addr))
#else
# if SIZEOF_VOID_P == 8
STATIC_INLINE void *get_pointer (uaecptr addr)
{
	const unsigned int n = SIZEOF_VOID_P / 4;
	union {
		void    *ptr;
		uae_u32  longs[SIZEOF_VOID_P / 4];
	} p;
	unsigned int i;

	for (i = 0; i < n; i++) {
#ifdef WORDS_BIGENDIAN
		p.longs[i]     = get_long (addr + i * 4);
#else
		p.longs[n - 1 - i] = get_long (addr + i * 4);
#endif
	}
	return p.ptr;
}
# else
#  error "Unknown or unsupported pointer size."
# endif
#endif

STATIC_INLINE void put_long (uaecptr addr, uae_u32 l)
{
	longput(addr, l);
}
STATIC_INLINE void put_word (uaecptr addr, uae_u32 w)
{
	if (addr > 0xffffffff)
	   return;
	wordput(addr, w);
}
STATIC_INLINE void put_byte (uaecptr addr, uae_u32 b)
{
	byteput(addr, b);
}

extern void put_long_slow (uaecptr addr, uae_u32 v);
extern void put_word_slow (uaecptr addr, uae_u32 v);
extern void put_byte_slow (uaecptr addr, uae_u32 v);
extern uae_u32 get_long_slow (uaecptr addr);
extern uae_u32 get_word_slow (uaecptr addr);
extern uae_u32 get_byte_slow (uaecptr addr);


/*
 * Store host pointer v at addr
 */
#if SIZEOF_VOID_P == 4
# define put_pointer(addr, p) (put_long ((addr), (uae_u32)(p)))
#else
# if SIZEOF_VOID_P == 8
STATIC_INLINE void put_pointer (uaecptr addr, void *v)
{
	const unsigned int n = SIZEOF_VOID_P / 4;
	union {
		void    *ptr;
		uae_u32  longs[SIZEOF_VOID_P / 4];
	} p;
	unsigned int i;

	p.ptr = v;

	for (i = 0; i < n; i++) {
#ifdef WORDS_BIGENDIAN
		put_long (addr + i * 4, p.longs[i]);
#else
		put_long (addr + i * 4, p.longs[n - 1 - i]);
#endif
	}
}
# endif
#endif

STATIC_INLINE uae_u8 *get_real_address (uaecptr addr)
{
	return get_mem_bank (addr).xlateaddr(addr);
}

STATIC_INLINE int valid_address (uaecptr addr, uae_u32 size)
{
	return get_mem_bank (addr).check(addr, size);
}

STATIC_INLINE void put_quad_host(void *addr, uae_u64 v)
{
	do_put_mem_long((uae_u32*)addr, v >> 32);
	do_put_mem_long(((uae_u32*)addr) + 1, (uae_u32)v);
}
STATIC_INLINE void put_long_host(void *addr, uae_u32 v)
{
	do_put_mem_long((uae_u32*)addr, v);
}
STATIC_INLINE void put_word_host(void *addr, uae_u16 v)
{
	do_put_mem_word((uae_u16*)addr, v);
}
STATIC_INLINE void put_byte_host(void *addr, uae_u8 v)
{
	*((uae_u8*)addr) = v;
}
STATIC_INLINE uae_u64 get_quad_host(void *addr)
{
	uae_u64 v = ((uae_u64)do_get_mem_long((uae_u32*)addr)) << 32;
	v |= do_get_mem_long(((uae_u32*)addr) + 1);
	return v;
}
STATIC_INLINE uae_u32 get_long_host(void *addr)
{
	return do_get_mem_long((uae_u32*)addr);
}
STATIC_INLINE uae_u16 get_word_host(void *addr)
{
	return do_get_mem_word((uae_u16*)addr);
}
STATIC_INLINE uae_u32 get_byte_host(void *addr)
{
	return *((uae_u8*)addr);
}

extern int addr_valid (const TCHAR*, uaecptr,uae_u32);

/* For faster access in custom chip emulation.  */
extern void REGPARAM3 chipmem_lput (uaecptr, uae_u32) REGPARAM;
extern void REGPARAM3 chipmem_wput (uaecptr, uae_u32) REGPARAM;
extern void REGPARAM3 chipmem_bput (uaecptr, uae_u32) REGPARAM;

extern uae_u32 REGPARAM3 chipmem_agnus_wget (uaecptr) REGPARAM;
extern void REGPARAM3 chipmem_agnus_wput (uaecptr, uae_u32) REGPARAM;

extern uae_u32 chipmem_mask, kickmem_mask;
extern uae_u8 *kickmemory;
extern uae_u32 kickmem_size;
extern addrbank dummy_bank;

/* 68020+ Chip RAM DMA contention emulation */
extern void REGPARAM3 chipmem_bput_c2 (uaecptr, uae_u32) REGPARAM;

extern uae_u32 (REGPARAM3 *chipmem_lget_indirect)(uaecptr) REGPARAM;
extern uae_u32 (REGPARAM3 *chipmem_wget_indirect)(uaecptr) REGPARAM;
extern uae_u32 (REGPARAM3 *chipmem_bget_indirect)(uaecptr) REGPARAM;
extern void (REGPARAM3 *chipmem_lput_indirect)(uaecptr, uae_u32) REGPARAM;
extern void (REGPARAM3 *chipmem_wput_indirect)(uaecptr, uae_u32) REGPARAM;
extern void (REGPARAM3 *chipmem_bput_indirect)(uaecptr, uae_u32) REGPARAM;
extern int (REGPARAM3 *chipmem_check_indirect)(uaecptr, uae_u32) REGPARAM;
extern uae_u8 *(REGPARAM3 *chipmem_xlate_indirect)(uaecptr) REGPARAM;

#ifdef NATMEM_OFFSET

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif // HAVE_SYS_TYPES_H
#ifndef __key_t_defined
  typedef int key_t;
#endif // __key_t_defined

typedef struct shmpiece_reg {
	uae_u8 *native_address;
	int id;
	uae_u32 size;
	const TCHAR *name;
	struct shmpiece_reg *next;
	struct shmpiece_reg *prev;
} shmpiece;

extern shmpiece *shm_start;

struct shmid_ds;

/* Prototypes from src/memory.c used elsewhere, too */
int my_shmdt (const void *shmaddr);
void *my_shmat (int shmid, void *shmaddr, int shmflg);
int my_shmget (key_t key, size_t size, int shmflg, const TCHAR *name);
int my_shmctl (int shmid, int cmd, struct shmid_ds *buf);

#endif

extern uae_u8 *mapped_malloc (size_t, const TCHAR*);
extern void mapped_free (uae_u8 *);
extern void clearexec (void);
extern void mapkick (void);
extern void a3000_fakekick (int);

extern uaecptr strcpyha_safe (uaecptr dst, const uae_char *src);
extern uae_char *strcpyah_safe (uae_char *dst, uaecptr src, int maxsize);
extern void memcpyha_safe (uaecptr dst, const uae_u8 *src, int size);
//extern void memcpyha (uaecptr dst, const uae_u8 *src, int size);
extern void memcpyah_safe (uae_u8 *dst, uaecptr src, int size);
extern void memcpyah (uae_u8 *dst, uaecptr src, int size);

extern uae_s32 getz2size (struct uae_prefs *p);
extern ULONG getz2endaddr (void);

#if 0
void *arm_memset(void *s, int c, size_t n);
void *arm_memcpy(void *dest, const void *src, size_t n);
#define uae4all_memclr(p,l) arm_memset(p,0,l)
#define uae4all_memcpy arm_memcpy
#else
#define uae4all_memcpy memcpy
#define uae4all_memclr(p,l) memset(p, 0, l)
#endif

#endif /* UAE_MEMORY_H */
