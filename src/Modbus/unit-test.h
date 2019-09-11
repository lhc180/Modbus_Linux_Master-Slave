/*
 * Copyright © 2008-2014 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _UNIT_TEST_H_
#define _UNIT_TEST_H_

/* Constants defined by configure.ac */
#define HAVE_INTTYPES_H @HAVE_INTTYPES_H@
#define HAVE_STDINT_H @HAVE_STDINT_H@

#ifdef HAVE_INTTYPES_H
    #include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
    #ifndef _MSC_VER
        #include <stdint.h>
    #else
        #include "stdint.h"
    #endif
#endif

#define SERVER_ID         1


const uint16_t UT_BITS_ADDRESS = 0x0;
const uint16_t UT_BITS_NB = 0x10;
const uint8_t UT_BITS_TAB[] = {0xA5};


const uint16_t UT_INPUT_BITS_ADDRESS = 0x10;
const uint16_t UT_INPUT_BITS_NB = 0x10;
const uint8_t UT_INPUT_BITS_TAB[] = {0xDB};


const uint16_t UT_REGISTERS_ADDRESS = 0x20;
const uint16_t UT_REGISTERS_NB = 0xA;
const uint16_t UT_REGISTERS_NB_MAX = 0x10;
const uint16_t UT_REGISTERS_TAB[] = {0x022B, 0x0001, 0x0064, 0x4538, 0xAC8E, 0x220B, 0x0F01, 0x6004, 0x4C85, 0xEA83};


const uint16_t UT_INPUT_REGISTERS_ADDRESS = 0x30;
const uint16_t UT_INPUT_REGISTERS_NB = 0x8;
const uint16_t UT_INPUT_REGISTERS_TAB[] = {0x0001, 0x0020, 0x0300, 0x4000, 0x0055, 0x0660, 0x7700, 0x0808};

const float UT_REAL = 123456.00;

const uint32_t UT_IREAL_ABCD = 0x0020F147;
const uint32_t UT_IREAL_DCBA = 0x47F12000;
const uint32_t UT_IREAL_BADC = 0x200047F1;
const uint32_t UT_IREAL_CDAB = 0xF1470020;


#endif /* _UNIT_TEST_H_ */
