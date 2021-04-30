// global-c.h
//
//

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned char           U1;		/* UBYTE   */
typedef int16_t                 S2;     /* INT     */
typedef uint16_t                U2;     /* UINT    */
typedef int32_t                 S4;     /* LONGINT */

typedef long long               S8;     /* 64 bit long */
typedef double                  F8;     /* DOUBLE */


enum data_type { FREE, BYTE, WORD, DWORD, QWORD, DOUBLE, STRING};

#define TRUE 1
#define FALSE 0
#define MAXLINELEN 512
