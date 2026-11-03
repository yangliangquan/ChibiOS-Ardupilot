/*
    ChibiOS - Copyright (C) 2020 Patrick Seidel

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/* Memory rules for the CH32H417 port: three CRT1 RAM areas.
   ram0 = DTCM (data/bss/heap/stacks), ram1 = ITCM (fast code),
   ram2 = shared SRAM upper part (fast code). Initialized content of
   ram1/ram2 is loaded from flash0 and copied by __init_ram_areas();
   place fast code with __attribute__((section(".itcm"))) or
   section(".sram") (also collected below). The flash placeholder
   regions were removed together with their blocks. */

__ram0_base__           = ORIGIN(ram0);
__ram0_size__           = LENGTH(ram0);
__ram0_end__            = __ram0_base__ + __ram0_size__;

__ram1_base__           = ORIGIN(ram1);
__ram1_size__           = LENGTH(ram1);
__ram1_end__            = __ram1_base__ + __ram1_size__;

__ram2_base__           = ORIGIN(ram2);
__ram2_size__           = LENGTH(ram2);
__ram2_end__            = __ram2_base__ + __ram2_size__;

__flash0_base__         = ORIGIN(flash0);
__flash0_size__         = LENGTH(flash0);
__flash0_end__          = __flash0_base__ + __flash0_size__;

SECTIONS
{
    .ram0_init : ALIGN(4)
    {
        __ram0_init_text__ = LOADADDR(.ram0_init);
        __ram0_init__ = .;
        KEEP(*(.ram0_init))
        KEEP(*(.ram0_init.*))
        . = ALIGN(4);
    } > ram0 AT > RAM_INIT_FLASH_LMA

    .ram0 (NOLOAD) : ALIGN(4)
    {
        __ram0_clear__ = .;
        *(.ram0_clear)
        *(.ram0_clear.*)
        . = ALIGN(4);
        __ram0_noinit__ = .;
        *(.ram0)
        *(.ram0.*)
        . = ALIGN(4);
        __ram0_free__ = .;
    } > ram0

    .ram1_init : ALIGN(4)
    {
        __ram1_init_text__ = LOADADDR(.ram1_init);
        __ram1_init__ = .;
        KEEP(*(.ram1_init))
        KEEP(*(.ram1_init.*))
        KEEP(*(.itcm))
        KEEP(*(.itcm.*))
        . = ALIGN(4);
    } > ram1 AT > RAM_INIT_FLASH_LMA

    .ram1 (NOLOAD) : ALIGN(4)
    {
        __ram1_clear__ = .;
        *(.ram1_clear)
        *(.ram1_clear.*)
        . = ALIGN(4);
        __ram1_noinit__ = .;
        *(.ram1)
        *(.ram1.*)
        . = ALIGN(4);
        __ram1_free__ = .;
    } > ram1

    .ram2_init : ALIGN(4)
    {
        __ram2_init_text__ = LOADADDR(.ram2_init);
        __ram2_init__ = .;
        KEEP(*(.ram2_init))
        KEEP(*(.ram2_init.*))
        KEEP(*(.sram))
        KEEP(*(.sram.*))
        . = ALIGN(4);
    } > ram2 AT > RAM_INIT_FLASH_LMA

    .ram2 (NOLOAD) : ALIGN(4)
    {
        __ram2_clear__ = .;
        *(.ram2_clear)
        *(.ram2_clear.*)
        . = ALIGN(4);
        __ram2_noinit__ = .;
        *(.ram2)
        *(.ram2.*)
        . = ALIGN(4);
        __ram2_free__ = .;
    } > ram2

    .flash0 : ALIGN(4)
    {
        __flash0_init__ = .;
        KEEP(*(.flash0_init))
        KEEP(*(.flash0_init.*))
        __flash0_free__ = .;
    } > flash0

    /* The default heap uses the (statically) unused part of a RAM section.*/
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_base__ = .;
        . = ORIGIN(HEAP_RAM) + LENGTH(HEAP_RAM);
        __heap_end__ = .;
    } > HEAP_RAM
}
