/*
 * Copyright 2017-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_flexspi_nor_boot.h"

extern void ResetISR(void);

AT_QUICKACCESS_SECTION_DATA(const ivt image_vector_table);
/*************************************
 *  IVT Data
 *************************************/
const ivt image_vector_table = {
    IVT_HEADER,                    /* IVT Header */
    (uint32_t)ResetISR,            /* Image Entry Function */
    IVT_RSVD,                      /* Reserved = 0 */
    (uint32_t)DCD_ADDRESS,         /* Address where DCD information is stored */
    (uint32_t)BOOT_DATA_ADDRESS,   /* Address where BOOT Data Structure is stored */
    (uint32_t)&image_vector_table, /* Pointer to IVT Self (absolute address */
    (uint32_t)CSF_ADDRESS,         /* Address where CSF file is stored */
    IVT_RSVD                       /* Reserved = 0 */
};

extern uint32_t _image_start;
extern uint32_t _image_size;

AT_QUICKACCESS_SECTION_DATA(const BOOT_DATA_T g_boot_data);
/*************************************
 *  Boot Data
 *************************************/
const BOOT_DATA_T g_boot_data = {
    (uint32_t)&_image_start, /* boot start location */
    (uint32_t)&_image_size,  /* size */
    PLUGIN_FLAG, /* Plugin flag*/
    0xFFFFFFFFU  /* empty - extra data word */
};
