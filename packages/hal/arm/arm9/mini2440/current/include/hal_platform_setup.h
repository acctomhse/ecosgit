#ifndef CYGONCE_HAL_PLATFORM_SETUP_H
#define CYGONCE_HAL_PLATFORM_SETUP_H
//=============================================================================
//
//      hal_platform_setup.h
//
//      Platform specific support for HAL (assembly code)
//
//=============================================================================
// ####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later
// version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License
// along with eCos; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// As a special exception, if other files instantiate templates or use
// macros or inline functions from this file, or you compile this file
// and link it with other works to produce a work based on this file,
// this file does not by itself cause the resulting work to be covered by
// the GNU General Public License. However the source code for this file
// must still be made available in accordance with section (3) of the GNU
// General Public License v2.
//
// This exception does not invalidate any other reasons why a work based
// on this file might be covered by the GNU General Public License.
// -------------------------------------------
// ####ECOSGPLCOPYRIGHTEND####
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):		T.C. Chiu <TCChiu@sq.com.tw>
// Contributors:	T.C. Chiu <TCChiu@sq.com.tw>
// Date:		2009-12-19
// Purpose:		ARM9/MINI2440 platform specific support routines
// Description:
// Usage:		#include <cyg/hal/hal_platform_setup.h>
//	Only used by "vectors.S"
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/system.h>		// system-wide configuration info
#include CYGBLD_HAL_VARIANT_H		// variant specific configuration
#include CYGBLD_HAL_PLATFORM_H		// platform specific configuration
#include CYGHWR_MEMORY_LAYOUT_H
#include <cyg/hal/hal_mmu.h>		// MMU definitions
#include <cyg/hal/mini2440.h>		// board specific memory configuration
#include <cyg/hal/hal_s3c2440x.h>

// S3C2440: Mpll = (2*m * Fin) / (p * 2^s), UPLL = (m * Fin) / (p * 2^s)
// m = M (the value for divider M) + 8
// p = P (the value for divider P) + 2
#define M_MDIV		0x5c
#define M_PDIV		1
#if (CYGNUM_HAL_ARM_S3C2440X_CPU_CLOCK == 400000000)
#define M_SDIV		1	// Fin=12.0MHz Fout=400.0MHz
#elif (CYGNUM_HAL_ARM_S3C2440X_CPU_CLOCK == 200000000)
#define M_SDIV		2	// Fin=12.0MHz Fout=200.0MHz
#else
#error CYGNUM_HAL_ARM_S3C2440X_CPU_CLOCK not set to the right value
#endif

#define U_MDIV		0x38
#define U_PDIV		2
#define U_SDIV		2	// Fin=12.0MHz Fout=48.0MHz


#if defined(CYG_HAL_STARTUP_ROM) || defined(CYG_HAL_STARTUP_ROMRAM)
#define PLATFORM_SETUP1			_platform_setup1
#define CYGHWR_HAL_ARM_HAS_MMU
#define CYGSEM_HAL_ROM_RESET_USES_JUMP

// We need this here - can't rely on a translation table until MMU has
// been initialized
#define	CYGHWR_LED_MACRO \
	ldr	r0, =GPFDAT; \
	ldr	r1, [r0]; \
	bic	r1, r1, #(0xf<<4); \
	orr	r1, r1, #((0xf&~(\x))<<4); \
	str	r1, [r0]; 

	.macro RAW_LED_MACRO x
	CYGHWR_LED_MACRO
	.endm

	// Configure GPF[4:7] as Output & pull-up turned off
	.macro	RAW_LED_PORT_INIT_MACRO
	ldr	r0, =GPFUP
	ldr	r1, [r0]
	orr	r1, r1, #((1<<7)|(1<<6)|(1<<5)|(1<<4))
	str	r1, [r0]

	RAW_LED_MACRO	0

	ldr	r0, =GPFCON
	ldr	r1, [r0]
	orr	r1, r1, #((1<<14)|(1<<12)|(1<<10)|(1<<8))
	str	r1, [r0]
	.endm

// This macro represents the initial startup code for the platform
	.macro	_platform_setup1
#ifndef CYG_HAL_STARTUP_RAM
	ldr	r0, =WTCON		// watch dog disable
	ldr	r1, =0x0
	str	r1, [r0]
#endif
	RAW_LED_PORT_INIT_MACRO

#ifndef CYG_HAL_STARTUP_RAM
	ldr	r0, =INTMSK
	ldr	r1, =0xFFFFFFFF		// all interrupt disable
	str	r1, [r0]

	ldr	r0, =INTSUBMSK
	ldr	r1, =0xFFFF		// all sub interrupt disable
	str	r1, [r0]

	RAW_LED_MACRO	1

	// Disable MMU stuff and caches
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x00001000	// clear bits 12 (---I ---- ---- ----)
	bic	r0, r0, #0x0000000F	// clear bits 3:0 (---I ---- ---- WCAM)
	mcr	p15, 0, r0, c1, c0, 0

	nop
	nop

	// Invalidate DCache
	mov	r0, #0
	mcr	p15, 0, r0, c7, c6, 0	// clear data cache

	// To reduce PLL lock time, adjust the LOCKTIME register.
	ldr	r0, =LOCKTIME
	ldr	r1, =0xFFFFFFFF
	str	r1, [r0]

	// We must set ratios, set memctl, then change FCLK.
	ldr     r0,=CLKDIVN		// Set ratios 1:4:8 for FCLK:HCLK:PCLK
	ldr     r1,=(5)
	str     r1,[r0]

	// MMU_SetAsyncBusMode
	// Must select, since we're setting HDIVN=1
        // change to asynchronous bus mode
#define R1_iA		(1<<31)
#define R1_nF		(1<<30)
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #(R1_nF | R1_iA)
	mcr	p15, 0, r0, c1, c0, 0

	// Set memory control registers
	adr	r0, 1f
	ldr	r1, =BWSCON		// BWSCON Address
	add	r2, r0, #13*4		// End address of SMRDATA
0:
	ldr	r3, [r0], #4
	str	r3, [r1], #4
	cmp	r2, r0
	bne	0b
	b	2f

1:
// Memory configuration should be optimized for best performance
// The following parameter is not optimized.
	.long	(0+(B1_BWSCON<<4)+(B2_BWSCON<<8)+(B3_BWSCON<<12)+(B4_BWSCON<<16)+(B5_BWSCON<<20)+(B6_BWSCON<<24)+(B7_BWSCON<<28))
	.long	((B0_Tacs<<13)+(B0_Tcos<<11)+(B0_Tacc<<8)+(B0_Tcoh<<6)+(B0_Tah<<4)+(B0_Tacp<<2)+(B0_PMC))	//GCS0
	.long	((B1_Tacs<<13)+(B1_Tcos<<11)+(B1_Tacc<<8)+(B1_Tcoh<<6)+(B1_Tah<<4)+(B1_Tacp<<2)+(B1_PMC))	//GCS1
	.long	((B2_Tacs<<13)+(B2_Tcos<<11)+(B2_Tacc<<8)+(B2_Tcoh<<6)+(B2_Tah<<4)+(B2_Tacp<<2)+(B2_PMC))	//GCS2
	.long	((B3_Tacs<<13)+(B3_Tcos<<11)+(B3_Tacc<<8)+(B3_Tcoh<<6)+(B3_Tah<<4)+(B3_Tacp<<2)+(B3_PMC))	//GCS3
	.long	((B4_Tacs<<13)+(B4_Tcos<<11)+(B4_Tacc<<8)+(B4_Tcoh<<6)+(B4_Tah<<4)+(B4_Tacp<<2)+(B4_PMC))	//GCS4
	.long	((B5_Tacs<<13)+(B5_Tcos<<11)+(B5_Tacc<<8)+(B5_Tcoh<<6)+(B5_Tah<<4)+(B5_Tacp<<2)+(B5_PMC))	//GCS5
	.long	((B6_MT<<15)+(B6_Trcd<<2)+(B6_SCAN))	// GCS6
	.long	((B7_MT<<15)+(B7_Trcd<<2)+(B7_SCAN))	// GCS7
	.long	((REFEN<<23)+(TREFMD<<22)+(Trp<<20)+(Trc<<18)+(Tchr<<16)+REFCNT)
	.long	0x32			// SCLK power saving mode, BANKSIZE 128M/128M
	.long	0x30			// MRSR6 CL=3clk
	.long	0x30			// MRSR7

2:
	RAW_LED_MACRO	2

	// Configure UPLL
	ldr	r0, =UPLLCON
	ldr	r1, =((U_MDIV<<12) + (U_PDIV<<4) + U_SDIV)
	str	r1, [r0]

	mov	r0, #4000
3:
	subs	r0, r0, #1
	bne	3b 

	// Configure MPLL
	ldr	r0, =MPLLCON
	ldr	r1, =((M_MDIV<<12) + (M_PDIV<<4) + M_SDIV)
	str	r1, [r0]

	mov	r0, #8000
4:
	subs	r0, r0, #1
	bne	4b 

#endif /* !CYG_HAL_STARTUP_RAM */

	// Set up a stack [for calling C code]
	ldr	r1, =__startup_stack
	ldr	r2, =MINI2440_SDRAM_PHYS_BASE
	orr	sp, r1, r2

	// Create MMU tables
	RAW_LED_MACRO	3
	bl	hal_mmu_init
	RAW_LED_MACRO	4

	// Enable MMU
	ldr	r2, =10f
#ifdef CYG_HAL_STARTUP_ROMRAM
	ldr	r1, =__exception_handlers
	ldr	r9, =0x80000000
	sub	r1, r2, r1
	add	r2, r9, r1			// r9 has ROM offset
#endif
	ldr	r1, =MMU_Control_Init|MMU_Control_M
	mcr	MMU_CP, 0, r1, MMU_Control, c0
	mov	pc, r2				// Change address spaces
	nop
	nop
	nop
10:
	RAW_LED_MACRO	5

#ifdef CYG_HAL_STARTUP_ROMRAM
	mov	r0, r9				// Relocate FLASH/ROM to RAM
	ldr	r1, =__exception_handlers	// ram base & length
	ldr	r2, =__rom_data_end
20:
	ldr	r3, [r0], #4
	str	r3, [r1], #4
	cmp	r1, r2
	bne	20b
	ldr	r0, =30f
	mov	pc, r0
	nop
	nop
	nop
	nop
30:
#endif
	RAW_LED_MACRO	6
	.endm

#else // defined(CYG_HAL_STARTUP_ROM) || defined(CYG_HAL_STARTUP_ROMRAM)
#define PLATFORM_SETUP1
#endif

#endif // CYGONCE_HAL_PLATFORM_SETUP_H
//-----------------------------------------------------------------------------
// end of hal_platform_setup.h
