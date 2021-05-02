/*
 * This file global.h is part of l1vm-data.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2021
 *
 * L1vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * L1vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with L1vm.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <cctype>
#include <cstring>
#include <ctime>
#include <regex>
#include <signal.h>

#include <pthread.h>

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
