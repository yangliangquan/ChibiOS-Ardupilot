/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * CH32H417 memory setup (V5F core).
 *
 * CodeFlash is 960KB and entirely zero-wait (datasheet). V3F owns the
 * first 64KB at 0x00000000 (its own linker script), the EFL storage
 * partition takes the top 128KB, and this V5F image gets the middle
 * 768KB starting at 0x00010000. RAM: ITCM for fast-running code, DTCM
 * for data/bss, heap and stacks, and the upper 384KB of the 512KB
 * shared SRAM for more fast code (V3F owns its first 128KB).
 */

OUTPUT_ARCH( "riscv" )

MEMORY
{
    /* V5F code flash, XIP at zero wait states. */
    flash0 (rx)  : org = 0x00010000, len = 768k
    /* EFL storage partition (logs/parameters), programming alias
       0x080D0000. Reservation only, no sections are placed here;
       keep CH32H417_EFL_BASE_ADDR/SIZE in hal_efl_lld.h in sync. */
    flash1 (rx)  : org = 0x000D0000, len = 128k
    /* ITCM: the .text image runs here (copied from flash0 by crt0
       before the first C call), plus opt-in .itcm sections handled
       by CRT1 area 1 (.ram1_init). */
    ram1   (rwx) : org = 0x200A0000, len = 128k
    /* DTCM: data/bss, heap and stacks (CRT1 area 0). The first 512
       bytes are hardware-owned (the hardware stacking region sits at
       the DTCM base, RM HW_POPDM_ADDR) and are not general RAM. */
    ram0   (wx)  : org = 0x200C0200, len = 0x3FE00
    /* Shared SRAM upper part: fast-running code (CRT1 area 2:
       sections .ram2_init and .sram). V3F owns 0x20100000..0x20120000. */
    ram2   (rwx) : org = 0x20120000, len = 384k
}

/* For each data/text section two region are defined, a virtual region
   and a load region (_LMA suffix).*/

/* Flash region to be used for exception vectors.*/
REGION_ALIAS("VECTORS_FLASH", flash0);
REGION_ALIAS("VECTORS_FLASH_LMA", flash0);

/* Flash region to be used for constructors and destructors.*/
REGION_ALIAS("XTORS_FLASH", flash0);
REGION_ALIAS("XTORS_FLASH_LMA", flash0);

/* Code text runs from ITCM, loaded from the code flash (crt0 copies
   it before the first C call); the FLASH aliases are kept so that
   retargeting .text back to XIP is a two-line change in rules_code.ld. */
REGION_ALIAS("TEXT_FLASH", flash0);
REGION_ALIAS("TEXT_FLASH_LMA", flash0);

/* Flash region to be used for code text.*/
REGION_ALIAS("TEXT_RAM", ram1);
REGION_ALIAS("TEXT_RAM_LMA", flash0);

/* Flash region to be used for read only data.*/
REGION_ALIAS("RODATA_FLASH", flash0);
REGION_ALIAS("RODATA_FLASH_LMA", flash0);

/* Flash region to be used for various.*/
REGION_ALIAS("VARIOUS_FLASH", flash0);
REGION_ALIAS("VARIOUS_FLASH_LMA", flash0);

/* Flash region to be used for RAM(n) initialization data.*/
REGION_ALIAS("RAM_INIT_FLASH_LMA", flash0);

/* RAM region to be used for Main stack. This stack accommodates the processing
   of all exceptions and interrupts.*/
REGION_ALIAS("MAIN_STACK_RAM", ram0);

/* RAM region to be used for the process stack. This is the stack used by
   the main() function.*/
REGION_ALIAS("PROCESS_STACK_RAM", ram0);

/* RAM region to be used for data segment.*/
REGION_ALIAS("DATA_RAM", ram0);
REGION_ALIAS("DATA_RAM_LMA", flash0);

/* RAM region to be used for BSS segment.*/
REGION_ALIAS("BSS_RAM", ram0);

/* RAM region to be used for the default heap.*/
REGION_ALIAS("HEAP_RAM", ram0);

/* Generic rules inclusion.*/
INCLUDE rules.ld
