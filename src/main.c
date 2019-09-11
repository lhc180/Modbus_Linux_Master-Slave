/*
 * Copyright Â© 2008-2014 StÃ©phane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#ifdef _WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
#endif

/* For MinGW */
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

#include "unit-test.h"

enum {
    TCP,
    TCP_PI,
    RTU
};

#if 0		//server从机

/* server:从机 */
int main(int argc, char* argv[])
{
    int useBackend = RTU;                       //ModBus使用的类型
    modbus_t *ctx = NULL;                       //成功打开设备后返回的结构体指针
    uint8_t *query = NULL;                      //接收到的查询数据
    modbus_mapping_t *modbusMapping = NULL;     //ModBus的线圈，寄存器分布
    int socketNum = -1;                         //使用网络连接时，建立连接成功后返回的套接字
    int ret = -1;                                //返回值
    int headerLength = 0;                       //接收到数据的头长度
    uint8_t i = 0;


    /* 根据所传的参数决定使用哪种方式 */
    if (argc > 1) {
        if (0 == strcmp(argv[1], "tcp")) {
            useBackend = TCP;
        } else if (0 == strcmp(argv[1], "tcppi")) {
            useBackend = TCP_PI;
        } else if (0 == strcmp(argv[1], "rtu")) {
            useBackend = RTU;
        } else {
            printf("Usage:\n  %s [tcp|tcppi|rtu] - Modbus server for unit testing\n\n", argv[0]);
            return -1;
        }
    } else {
        /* By default */
        useBackend = RTU;
    }

    /* 根据不同的连接打开相应的设备 */
    if (TCP == useBackend) {
        ctx = modbus_new_tcp("127.0.0.1", 1502);
        query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    } else if (TCP_PI == useBackend) {
        ctx = modbus_new_tcp_pi("::0", "1502");
        query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    } else {
        ctx = modbus_new_rtu("/dev/ttymxc1", 115200, 'N', 8, 1);
        modbus_set_slave(ctx, SERVER_ID);
        query = malloc(MODBUS_RTU_MAX_ADU_LENGTH);
    }

    headerLength = modbus_get_header_length(ctx);

    modbus_set_debug(ctx, TRUE);        //设置Debug调试

    /* 创建Modbus的mapping */
    modbusMapping = modbus_mapping_new_start_address(
                    UT_BITS_ADDRESS, UT_BITS_NB,
                    UT_INPUT_BITS_ADDRESS, UT_INPUT_BITS_NB,
                    UT_REGISTERS_ADDRESS, UT_REGISTERS_NB_MAX,
                    UT_INPUT_REGISTERS_ADDRESS, UT_INPUT_REGISTERS_NB);
    if (NULL == modbusMapping) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    /* 给比特位和寄存器赋初始值 */
    modbus_set_bits_from_bytes(modbusMapping->tab_bits, 0, UT_BITS_NB, UT_BITS_TAB);
    modbus_set_bits_from_bytes(modbusMapping->tab_input_bits, 0, UT_INPUT_BITS_NB, UT_INPUT_BITS_TAB);
    for (i = 0; i < UT_REGISTERS_NB; i++) {
        modbusMapping->tab_registers[i] = UT_REGISTERS_TAB[i];
    }
    for (i = 0; i < UT_INPUT_REGISTERS_NB; i++) {
        modbusMapping->tab_input_registers[i] = UT_INPUT_REGISTERS_TAB[i];
    }

    /* 建立连接 */
    if (TCP == useBackend) {
        socketNum = modbus_tcp_listen(ctx, 1);
        modbus_tcp_accept(ctx, &socketNum);
    } else if (TCP_PI == useBackend) {
        socketNum = modbus_tcp_pi_listen(ctx, 1);
        modbus_tcp_pi_accept(ctx, &socketNum);
    } else {
        ret = modbus_connect(ctx);
        if (-1 == ret) {
            fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
            modbus_free(ctx);
            return -1;
        }
    }

    /* 从机循环等待接收数据，然后对接收到的数据进行相应的处理 */
    while(1) {

        /* Modbus接收数据 */
        do {
            ret = modbus_receive(ctx, query);       //Filtered queries return 0
        } while(0 == ret);

        /* The connection is not closed on errors which require on reply such as bad CRC in RTU. */
        if (-1 == ret && EMBBADCRC != errno) {
            break;
        }

        /* TODO：对接收到的数据的处理 */
        if (0x01 == query[headerLength]) {                  //Read Bit
            printf("Read Bit\r\n");
        } else if (0x02 == query[headerLength]) {           //Read Input Bit
            printf("Read Input Bit\r\n");
        } else if (0x05 == query[headerLength]) {           //Write Bit
            printf("Write Bit\r\n");
        } else if (0x0F == query[headerLength]) {           //Write Many Bits
            printf("Write Many Bits\r\n");
        } else if (0x03 == query[headerLength]) {           //Read Hold Register
            printf("Read Hold Register\r\n");
        } else if (0x04 == query[headerLength]) {           //Read Input Register
            printf("Read Input Register\r\n");
        } else if (0x06 == query[headerLength]) {           //Write Hold Register
            printf("Write Hold Register\r\n");
        } else if (0x10 == query[headerLength]) {           //Write Many Hold Registers
            printf("Write Many Hold Registers\r\n");
        } else {
            printf("Error\r\n");
        }


        ret = modbus_reply(ctx, query, ret, modbusMapping);
        if (-1 == ret) {
            break;
        }

    }

    printf("Quit the loop: %s\n", modbus_strerror(errno));

    /* 释放所有资源 */
    if (TCP == useBackend) {
        if (-1 != socketNum) {
            close(socketNum);
        }
    }
    modbus_mapping_free(modbusMapping);
    free(query);
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;

}


#else 		//client主机

#define BUG_REPORT(_cond, _format, _args ...) \
    printf("\nLine %d: assertion error for '%s': " _format "\n", __LINE__, # _cond, ## _args)

#define ASSERT_TRUE(_cond, _format, __args...) {  \
    if (_cond) {                                  \
        printf("OK\n");                           \
    } else {                                      \
        BUG_REPORT(_cond, _format, ## __args);    \
        goto close;                               \
    }                                             \
};

/* client主机 */
int main (int argc, char* argv[])
{
    int useBackend;             //Modbus的类型
    modbus_t *ctx = NULL;       //成功打开设备后返回的结构体指针
    uint8_t *tabBits = NULL;            //bit位的空间
    uint16_t *tabRegisters = NULL;      //寄存器的空间
    uint16_t *tabRegistersBad = NULL;   //
    int nbPoints;               //空间大小
    int ret = -1;               //返回值
    int i = 0;
    uint8_t value = 0;

    /* 判断Modbus的类型 */
    if (argc > 1) {
        if (0 == strcmp(argv[1], "tcp")) {
            useBackend = TCP;
        } else if (0 == strcmp(argv[1], "tcppi")) {
            useBackend = TCP_PI;
        } else if (0 == strcmp(argv[1], "rtu")) {
            useBackend = RTU;
        } else {
            printf("Usage:\n  %s [tcp|tcppi|rtu] - Modbus client for unit testing\n\n", argv[0]);
            exit(1);
        }
    } else {
        /* By default */
        useBackend = RTU;
    }

    /* 根据Modbus_RTU的类型建立连接 */
    if (TCP == useBackend) {
        ctx = modbus_new_tcp("127.0.0.1", 1502);
    } else if (TCP_PI == useBackend) {
        ctx = modbus_new_tcp_pi("::1", "1502");
    } else {
        ctx = modbus_new_rtu("/dev/ttymxc1", 115200, 'N', 8, 1);
    }
    if (NULL == ctx) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }

    modbus_set_debug(ctx, TRUE);        //设置Dubug模式
    modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL);

    /* 设置从机ID */
    if (RTU == useBackend) {
        modbus_set_slave(ctx, SERVER_ID);
    }

    /* 建立连接 */
    if (-1 == modbus_connect(ctx)) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
    printf("Connection Successful!\r\n");

    /* 为bit和寄存器分配内存空间 */
    nbPoints = (UT_BITS_NB > UT_INPUT_BITS_NB) ? UT_BITS_NB : UT_INPUT_BITS_NB;
    tabBits = (uint8_t *) malloc(nbPoints * sizeof(uint8_t));
    memset(tabBits, 0, nbPoints * sizeof(uint8_t));

    nbPoints = (UT_REGISTERS_NB > UT_INPUT_REGISTERS_NB) ? UT_REGISTERS_NB : UT_INPUT_REGISTERS_NB;
    tabRegisters = (uint16_t *) malloc(nbPoints * sizeof(uint16_t));
    memset(tabRegisters, 0, nbPoints * sizeof(uint16_t));

    /* TODO:对bit，input bit，holding register，input register的读写命令 */

    /** Coil Bits **/
    /* Single Bit */
#if 0
    ret = modbus_write_bit(ctx, UT_BITS_ADDRESS, ON);
    printf("1/2 modbus_write_bit: ");
    ASSERT_TRUE(ret == 1, "");

    ret = modbus_read_bits(ctx, UT_BITS_ADDRESS, 1, tabBits);
    printf("2/2 modbus_read_bits: ");
    ASSERT_TRUE(ret == 1, "FAILED (nb points %d)\n", ret);
    ASSERT_TRUE(tabBits[0] == ON, "FAILED (%0X != %0X)\n", tabBits[0], ON);
#endif

    /* Multiple Bits */
#if 0
    uint8_t tabValue[UT_BITS_NB];

    modbus_set_bits_from_bytes(tabValue, 0, UT_BITS_NB, UT_BITS_TAB);
    ret = modbus_write_bits(ctx, UT_BITS_ADDRESS, UT_BITS_NB, tabValue);
    printf("1/2 modbus_write_bits: ");
    ASSERT_TRUE(ret == UT_BITS_NB, "");

    ret = modbus_read_bits(ctx, UT_BITS_ADDRESS, UT_BITS_NB, tabBits);
    printf("2/2 modbus_read_bits: ");
    ASSERT_TRUE(ret == UT_BITS_NB, "FAILED (nb points %d)\n", ret);

    i = 0;
    nbPoints = UT_BITS_NB;
    while (nbPoints > 0) {
        int nbBits = (nbPoints > 8) ? 8 : nbPoints;

        value = modbus_get_byte_from_bits(tabBits, i*8, nbBits);
        ASSERT_TRUE(value == UT_BITS_TAB[i], "FAILED (%0X != %0X)\n", value, UT_BITS_TAB[i]);

        nbPoints -= nbBits;
        i++;
    }
    printf("OK\n");
#endif

    /** Discrete Inputs **/
#if 0
    ret = modbus_read_input_bits(ctx, UT_INPUT_BITS_ADDRESS, UT_INPUT_BITS_NB, tabBits);
    printf("1/1 modbus_read_input_bits: ");
    ASSERT_TRUE(ret == UT_INPUT_BITS_NB, "FAILED (nb points %d)\n", ret);

    i = 0;
    nbPoints = UT_INPUT_BITS_NB;
    while (nbPoints > 0) {
        int nbBits = (nbPoints > 8) ? 8 : nbPoints;
        value = modbus_get_byte_from_bits(tabBits, i*8, nbBits);
        ASSERT_TRUE(value == UT_INPUT_BITS_TAB[i], "FAILED (%0X != %0X)\n", value, UT_INPUT_BITS_TAB[i]);

        nbPoints -= nbBits;
        i++;
    }
    printf("OK\n");
#endif


    /** Holding Registers **/
    /* Single Register */
#if 0
    ret = modbus_write_register(ctx, UT_REGISTERS_ADDRESS, 0x1234);
    printf("1/2 modbus_write_register: ");
    ASSERT_TRUE(ret == 1, "");

    ret = modbus_read_registers(ctx, UT_REGISTERS_ADDRESS, 1, tabRegisters);
    printf("2/2 modbus_read_registers: ");
    ASSERT_TRUE(ret == 1, "FAILED (nb points %d)\n", ret);
    ASSERT_TRUE(tabRegisters[0] == 0x1234, "FAILED (%0X != %0X)\n", tabRegisters[0], 0x1234);
#endif

    /* Many Registers */
#if 0
    ret = modbus_write_registers(ctx, UT_REGISTERS_ADDRESS, UT_REGISTERS_NB, UT_REGISTERS_TAB);
    printf("1/5 modbus_write_registers: ");
    ASSERT_TRUE(ret == UT_REGISTERS_NB, "");

    ret = modbus_read_registers(ctx, UT_REGISTERS_ADDRESS, UT_REGISTERS_NB, tabRegisters);
    printf("2/5 modbus_read_registers: ");
    ASSERT_TRUE(ret == UT_REGISTERS_NB, "FAILED (nb points %d)\n", ret);

    for (i=0; i < UT_REGISTERS_NB; i++) {
        ASSERT_TRUE(tabRegisters[i] == UT_REGISTERS_TAB[i],
                    "FAILED (%0X != %0X)\n",
                    tabRegisters[i], UT_REGISTERS_TAB[i]);
    }

    ret = modbus_read_registers(ctx, UT_REGISTERS_ADDRESS, 0, tabRegisters);
    printf("3/5 modbus_read_registers (0): ");
    ASSERT_TRUE(ret == 0, "FAILED (nb_points %d)\n", ret);

    nbPoints = (UT_REGISTERS_NB > UT_INPUT_REGISTERS_NB) ? UT_REGISTERS_NB : UT_INPUT_REGISTERS_NB;
    memset(tabRegisters, 0, nbPoints * sizeof(uint16_t));

    /* Write registers to zero from tabRegisters and store read registers
       into tabRegisters. So the read registers must set to 0, except the
       first one because there is an offset of 1 register on write. */
    ret = modbus_write_and_read_registers(ctx,
                                         UT_REGISTERS_ADDRESS + 1,
                                         UT_REGISTERS_NB - 1,
                                         tabRegisters,
                                         UT_REGISTERS_ADDRESS,
                                         UT_REGISTERS_NB,
                                         tabRegisters);
    printf("4/5 modbus_write_and_read_registers: ");
    ASSERT_TRUE(ret == UT_REGISTERS_NB, "FAILED (nb points %d != %d)\n", ret, UT_REGISTERS_NB);

    ASSERT_TRUE(tabRegisters[0] == UT_REGISTERS_TAB[0],
                "FAILED (%0X != %0X)\n",
                tabRegisters[0], UT_REGISTERS_TAB[0]);

    for (i=1; i < UT_REGISTERS_NB; i++) {
        ASSERT_TRUE(tabRegisters[i] == 0,
                    "FAILED (%0X != %0X)\n",
                    tabRegisters[i], 0);
    }
#endif

    /** Input Registers **/
#if 1
    ret = modbus_read_input_registers(ctx, UT_INPUT_REGISTERS_ADDRESS,
                                     UT_INPUT_REGISTERS_NB,
                                     tabRegisters);
    printf("1/1 modbus_read_input_registers: ");
    ASSERT_TRUE(ret == UT_INPUT_REGISTERS_NB, "FAILED (nb points %d)\n", ret);

    for (i=0; i < UT_INPUT_REGISTERS_NB; i++) {
        ASSERT_TRUE(tabRegisters[i] == UT_INPUT_REGISTERS_TAB[i],
                    "FAILED (%0X != %0X)\n",
                    tabRegisters[i], UT_INPUT_REGISTERS_TAB[i]);
    }
#endif



close:
    /* Free the memory */
    free(tabBits);
    free(tabRegisters);

    /* Close the connection */
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}


#endif




































/**
 *               ii.                                         ;9ABH,
 *              SA391,                                    .r9GG35&G
 *              &#ii13Gh;                               i3X31i;:,rB1
 *              iMs,:,i5895,                         .5G91:,:;:s1:8A
 *               33::::,,;5G5,                     ,58Si,,:::,sHX;iH1
 *                Sr.,:;rs13BBX35hh11511h5Shhh5S3GAXS:.,,::,,1AG3i,GG
 *                .G51S511sr;;iiiishS8G89Shsrrsh59S;.,,,,,..5A85Si,h8
 *               :SB9s:,............................,,,.,,,SASh53h,1G.
 *            .r18S;..,,,,,,,,,,,,,,,,,,,,,,,,,,,,,....,,.1H315199,rX,
 *          ;S89s,..,,,,,,,,,,,,,,,,,,,,,,,....,,.......,,,;r1ShS8,;Xi
 *        i55s:.........,,,,,,,,,,,,,,,,.,,,......,.....,,....r9&5.:X1
 *       59;.....,.     .,,,,,,,,,,,...        .............,..:1;.:&s
 *      s8,..;53S5S3s.   .,,,,,,,.,..      i15S5h1:.........,,,..,,:99
 *      93.:39s:rSGB@A;  ..,,,,.....    .SG3hhh9G&BGi..,,,,,,,,,,,,.,83
 *      G5.G8  9#@@@@@X. .,,,,,,.....  iA9,.S&B###@@Mr...,,,,,,,,..,.;Xh
 *      Gs.X8 S@@@@@@@B:..,,,,,,,,,,. rA1 ,A@@@@@@@@@H:........,,,,,,.iX:
 *     ;9. ,8A#@@@@@@#5,.,,,,,,,,,... 9A. 8@@@@@@@@@@M;    ....,,,,,,,,S8
 *     X3    iS8XAHH8s.,,,,,,,,,,...,..58hH@@@@@@@@@Hs       ...,,,,,,,:Gs
 *    r8,        ,,,...,,,,,,,,,,.....  ,h8XABMMHX3r.          .,,,,,,,.rX:
 *   :9, .    .:,..,:;;;::,.,,,,,..          .,,.               ..,,,,,,.59
 *  .Si      ,:.i8HBMMMMMB&5,....                    .            .,,,,,.sMr
 *  SS       :: h@@@@@@@@@@#; .                     ...  .         ..,,,,iM5
 *  91  .    ;:.,1&@@@@@@MXs.                            .          .,,:,:&S
 *  hS ....  .:;,,,i3MMS1;..,..... .  .     ...                     ..,:,.99
 *  ,8; ..... .,:,..,8Ms:;,,,...                                     .,::.83
 *   s&: ....  .sS553B@@HX3s;,.    .,;13h.                            .:::&1
 *    SXr  .  ...;s3G99XA&X88Shss11155hi.                             ,;:h&,
 *     iH8:  . ..   ,;iiii;,::,,,,,.                                 .;irHA
 *      ,8X5;   .     .......                                       ,;iihS8Gi
 *         1831,                                                 .,;irrrrrs&@
 *           ;5A8r.                                            .:;iiiiirrss1H
 *             :X@H3s.......                                .,:;iii;iiiiirsrh
 *              r#h:;,...,,.. .,,:;;;;;:::,...              .:;;;;;;iiiirrss1
 *             ,M8 ..,....,.....,,::::::,,...         .     .,;;;iiiiiirss11h
 *             8B;.,,,,,,,.,.....          .           ..   .:;;;;iirrsss111h
 *            i@5,:::,,,,,,,,.... .                   . .:::;;;;;irrrss111111
 *            9Bi,:,,,,......                        ..r91;;;;;iirrsss1ss1111
 *
 *								狗头保佑，永无BUG！
 */
