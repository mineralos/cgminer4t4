#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>

#include "logging.h"
#include "miner.h"
#include "util.h"

#include "t4_cmd.h"
#include "t4_clock.h"
#include "t4_common.h"


const struct PLL_Clock PLL_Clk_12Mhz[118]={
    {0,  120,   A4_PLL(1,   80, 3)}, //default
    {1,  125,   A4_PLL(1,   83, 3)}, //
    {2,  129,   A4_PLL(1,   86, 3)},
    {3,  140,   A4_PLL(1,   93, 3)},
    {4,  150,   A4_PLL(1,   50, 2)}, //lym open this
    {5,  159,   A4_PLL(1,   53, 2)},
    {6,  171,   A4_PLL(1,   57, 2)},
    {7,  180,   A4_PLL(1,   60, 2)},
    {8,  189,   A4_PLL(1,   63, 2)},
    {9,  201,   A4_PLL(1,   67, 2)},
    {10, 210,   A4_PLL(1,   70, 2)},
    {11, 219,   A4_PLL(1,   73, 2)},
    {12, 231,   A4_PLL(1,   77, 2)},
    {13, 240,   A4_PLL(1,   80, 2)},
    {14, 249,   A4_PLL(1,   83, 2)},
    {15, 261,   A4_PLL(1,   87, 2)},
    {16, 270,   A4_PLL(1,   90, 2)},
    {17, 279,   A4_PLL(1,   93, 2)},
    {18, 291,   A4_PLL(1,   97, 2)},
    {19, 300,   A4_PLL(1,   50, 1)},
    {20, 312,   A4_PLL(1,   52, 1)},
    {21, 318,   A4_PLL(1,   53, 1)},
    {22, 330,   A4_PLL(1,   55, 1)},
    {23, 342,   A4_PLL(1,   57, 1)},
    {24, 348,   A4_PLL(1,   58, 1)},
    {25, 360,   A4_PLL(1,   60, 1)},
    {26, 372,   A4_PLL(1,   62, 1)},
    {27, 378,   A4_PLL(1,   63, 1)},
    {28, 390,   A4_PLL(1,   65, 1)},
    {29, 402,   A4_PLL(1,   67, 1)},
    {30, 408,   A4_PLL(1,   68, 1)},
    {31, 420,   A4_PLL(1,   70, 1)},
    {32, 432,   A4_PLL(1,   72, 1)},
    {33, 438,   A4_PLL(1,   73, 1)},
    {34, 450,   A4_PLL(1,   75, 1)},
    {35, 462,   A4_PLL(1,   77, 1)},
    {36, 468,   A4_PLL(1,   78, 1)},
    {37, 480,   A4_PLL(1,   80, 1)},
    {38, 492,   A4_PLL(1,   82, 1)},
    {39, 498,   A4_PLL(1,   83, 1)},
    {40, 510,   A4_PLL(1,   85, 1)},
    {41, 522,   A4_PLL(1,   87, 1)},
    {42, 528,   A4_PLL(1,   88, 1)},
    {43, 540,   A4_PLL(1,   90, 1)},
    {44, 552,   A4_PLL(1,   92, 1)},
    {45, 558,   A4_PLL(1,   93, 1)},
    {46, 570,   A4_PLL(1,   95, 1)},
    {47, 582,   A4_PLL(1,   97, 1)},
    {48, 588,   A4_PLL(1,   98, 1)},
    {49, 600,   A4_PLL(1,   50, 0)},
    {50, 612,   A4_PLL(1,   51, 0)},
    {51, 624,   A4_PLL(1,   52, 0)},
    {52, 630,   A4_PLL(2,   105,0)},
    {53, 636,   A4_PLL(1,   53, 0)},
    {54, 648,   A4_PLL(1,   54, 0)},
    {55, 660,   A4_PLL(1,   55, 0)},
    {56, 672,   A4_PLL(1,   56, 0)},
    {57, 684,   A4_PLL(1,   57, 0)},
    {58, 690,   A4_PLL(2,   115,0)},
    {59, 696,   A4_PLL(1,   58, 0)},
    {60, 708,   A4_PLL(1,   59, 0)},
    {61, 720,   A4_PLL(1,   60, 0)},
    {62, 732,   A4_PLL(1,   61, 0)},
    {63, 744,   A4_PLL(1,   62, 0)},
    {64, 750,   A4_PLL(2,   125,0)},
    {65, 756,   A4_PLL(1,   63, 0)},
    {66, 768,   A4_PLL(1,   64, 0)},
    {67, 780,   A4_PLL(1,   65, 0)},
    {68, 792,   A4_PLL(1,   66, 0)},
    {69, 804,   A4_PLL(1,   67, 0)},
    {70, 810,   A4_PLL(2,   135,0)},
    {71, 816,   A4_PLL(1,   68, 0)},
    {72, 828,   A4_PLL(1,   69, 0)},
    {73, 840,   A4_PLL(1,   70, 0)},
    {74, 852,   A4_PLL(1,   71, 0)},
    {75, 864,   A4_PLL(1,   72, 0)},
    {76, 870,   A4_PLL(2,   145,0)},
    {77, 876,   A4_PLL(1,   73, 0)},
    {78, 888,   A4_PLL(1,   74, 0)},
    {79, 900,   A4_PLL(1,   75, 0)},
    {80, 912,   A4_PLL(1,   76, 0)},
    {81, 924,   A4_PLL(1,   77, 0)},
    {82, 930,   A4_PLL(2,   155,0)},
    {83, 936,   A4_PLL(1,   78, 0)},
    {84, 948,   A4_PLL(1,   79, 0)},
    {85, 960,   A4_PLL(1,   80, 0)},
    {86, 972,   A4_PLL(1,   81, 0)},
    {87, 984,   A4_PLL(1,   82, 0)},
    {88, 990,   A4_PLL(2,   165,0)},
    {89, 996,   A4_PLL(1,   83, 0)},
    {90, 1008,  A4_PLL(1,   84, 0)},
    {91, 1020,  A4_PLL(1,   85, 0)},
    {92, 1032,  A4_PLL(1,   86, 0)},
    {93, 1044,  A4_PLL(1,   87, 0)},
    {94, 1050,  A4_PLL(2,   175,0)},
    {95, 1056,  A4_PLL(1,   88, 0)},
    {96, 1068,  A4_PLL(1,   89, 0)},
    {97, 1080,  A4_PLL(1,   90, 0)},
    {98, 1092,  A4_PLL(1,   91, 0)},
    {99, 1104,  A4_PLL(1,   92, 0)},
    {100,1110,  A4_PLL(2,   185,0)},
    {101,1116,  A4_PLL(1,   93, 0)},
    {102,1128,  A4_PLL(1,   94, 0)},
    {103,1140,  A4_PLL(1,   95, 0)},
    {104,1152,  A4_PLL(1,   96, 0)},
    {105,1164,  A4_PLL(1,   97, 0)},
    {106,1170,  A4_PLL(2,   195,0)},
    {107,1176,  A4_PLL(1,   98, 0)},
    {108,1188,  A4_PLL(1,   99, 0)},
    {109,1200,  A4_PLL(1,   100,0)},
    {110,1212,  A4_PLL(1,   101,0)},
    {111,1224,  A4_PLL(1,   102,0)},
    {112,1236,  A4_PLL(1,   103,0)},
    {113,1248,  A4_PLL(1,   104,0)},
    {114,1260,  A4_PLL(1,   105,0)},
    {115,1272,  A4_PLL(1,   106,0)},
    {116,1284,  A4_PLL(1,   107,0)},
    {117,1296,  A4_PLL(1,   108,0)}
};


const uint8_t default_reg[118][12] = 
{
    {0x02, 0x50, 0x40, 0xc2, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //120MHz
    {0x02, 0x53, 0x40, 0xc2, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //125MHz
    {0x02, 0x56, 0x40, 0xc2, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //129MHz
    {0x02, 0x5d, 0x40, 0xc2, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //140MHz
    {0x02, 0x32, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //150MHz
    {0x02, 0x35, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //159MHz
    {0x02, 0x39, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //171MHz
    {0x02, 0x3c, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //180MHz
    {0x02, 0x3f, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //189MHz
    {0x02, 0x43, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //201MHz
    {0x02, 0x46, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //210MHz
    {0x02, 0x49, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //219MHz
    {0x02, 0x4d, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //231MHz
    {0x02, 0x50, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //240MHz
    {0x02, 0x53, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //249MHz
    {0x02, 0x57, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //261MHz
    {0x02, 0x5a, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //270MHz
    {0x02, 0x5d, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //279MHz
    {0x02, 0x61, 0x40, 0x82, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //291MHz
    {0x02, 0x32, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //300MHz
    {0x02, 0x34, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //312MHz
    {0x02, 0x35, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //318MHz
    {0x02, 0x37, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //330MHz
    {0x02, 0x39, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //342MHz
    {0x02, 0x3a, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //348MHz
    {0x02, 0x3c, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //360MHz
    {0x02, 0x3e, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //372MHz
    {0x02, 0x3f, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //378MHz
    {0x02, 0x41, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //390MHz
    {0x02, 0x43, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //402MHz
    {0x02, 0x44, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //408MHz
    {0x02, 0x46, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //420MHz
    {0x02, 0x48, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //432MHz
    {0x02, 0x49, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //438MHz
    {0x02, 0x4b, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //450MHz
    {0x02, 0x4d, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //462MHz
    {0x02, 0x4e, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //468MHz
    {0x02, 0x50, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //480MHz
    {0x02, 0x52, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //492MHz
    {0x02, 0x53, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //498MHz
    {0x02, 0x55, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //510MHz
    {0x02, 0x57, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //522MHz
    {0x02, 0x58, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //528MHz
    {0x02, 0x5a, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //540MHz
    {0x02, 0x5c, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //552MHz
    {0x02, 0x5d, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //558MHz
    {0x02, 0x5f, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //570MHz
    {0x02, 0x61, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //582MHz
    {0x02, 0x62, 0x40, 0x42, 0x00, 0x05, 0x50, 0x00, 0x00, 0x04, 0x00, 0x00}, //588MHz
    {0x02, 0x32, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //600MHz
    {0x02, 0x33, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //612MHz
    {0x02, 0x34, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //624MHz
    {0x04, 0x69, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //630MHz
    {0x02, 0x35, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //636MHz
    {0x02, 0x36, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //648MHz
    {0x02, 0x37, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //660MHz
    {0x02, 0x38, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //672MHz
    {0x02, 0x39, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //684MHz
    {0x04, 0x73, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //690MHz
    {0x02, 0x3a, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //696MHz
    {0x02, 0x3b, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //708MHz
    {0x02, 0x3c, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //720MHz
    {0x02, 0x3d, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //732MHz
    {0x02, 0x3e, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //744MHz
    {0x04, 0x7d, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //750MHz
    {0x02, 0x3f, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //756MHz
    {0x02, 0x40, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //768MHz
    {0x02, 0x41, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //780MHz
    {0x02, 0x42, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //792MHz
    {0x02, 0x43, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //804MHz
    {0x04, 0x87, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //810MHz
    {0x02, 0x44, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //816MHz
    {0x02, 0x45, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //828MHz
    {0x02, 0x46, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //840MHz
    {0x02, 0x47, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //852MHz
    {0x02, 0x48, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //864MHz
    {0x04, 0x91, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //870MHz
    {0x02, 0x49, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //876MHz
    {0x02, 0x4a, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //888MHz
    {0x02, 0x4b, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //900MHz
    {0x02, 0x4c, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //912MHz
    {0x02, 0x4d, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //924MHz
    {0x04, 0x9b, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //930MHz
    {0x02, 0x4e, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //936MHz
    {0x02, 0x4f, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //948MHz
    {0x02, 0x50, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //960MHz
    {0x02, 0x51, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //972MHz
    {0x02, 0x52, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //984MHz
    {0x04, 0xa5, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //990MHz
    {0x02, 0x53, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //996MHz
    {0x02, 0x54, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1008MHz
    {0x02, 0x55, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1020MHz
    {0x02, 0x56, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1032MHz
    {0x02, 0x57, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1044MHz
    {0x04, 0xaf, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1050MHz
    {0x02, 0x58, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1056MHz
    {0x02, 0x59, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1068MHz
    {0x02, 0x5a, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1080MHz
    {0x02, 0x5b, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1092MHz
    {0x02, 0x5c, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1104MHz
    {0x04, 0xb9, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1110MHz
    {0x02, 0x5d, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1116MHz
    {0x02, 0x5e, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1128MHz
    {0x02, 0x5f, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1140MHz
    {0x02, 0x60, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1152MHz
    {0x02, 0x61, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1164MHz
    {0x04, 0xc3, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1170MHz
    {0x02, 0x62, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1176MHz
    {0x02, 0x63, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1188MHz
    {0x02, 0x64, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1200MHz
    {0x02, 0x65, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1212MHz
    {0x02, 0x66, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1224MHz
    {0x02, 0x67, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1236MHz
    {0x02, 0x68, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1248MHz
    {0x02, 0x69, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1260MHz
    {0x02, 0x6a, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1272MHz
    {0x02, 0x6b, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1284MHz
    {0x02, 0x6c, 0x40, 0x02, 0x00, 0x05, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, //1296MHz
};


uint32_t Ax_clock_index2pll(int pllClkIdx)
{
    return PLL_Clk_12Mhz[pllClkIdx].speedMHz;
}

int Ax_clock_pll2index(int optPll)
{
    int i;
    int A1Pll;

    if(optPll > 0)
    {
        A1Pll = 0;
        
        if(optPll <= PLL_Clk_12Mhz[0].speedMHz) 
        {
            A1Pll = 0; //found
        }
        else
        {
            for(i = 1; i < 118; i++)
            {
                if((optPll < PLL_Clk_12Mhz[i].speedMHz) && (optPll >= PLL_Clk_12Mhz[i-1].speedMHz))
                {
                    A1Pll=i-1; //found
                    break;
                }
            }
        }

        applog(LOG_DEBUG, "optpll:%d  pllindex:%d  pll:%d", optPll, A1Pll, Ax_clock_index2pll(A1Pll));
    }

    return A1Pll;
}


bool Ax_clock_setpll(struct A1_chain *a1, int pllClkIdx)
{
    int i;
    uint8_t reg[16];

    memset(reg, 0, sizeof(reg));
    memcpy(reg, default_reg[pllClkIdx], REG09_LENGTH);

    for(i = 0; i < 5; i++)
    {
        if(mcompat_cmd_write_register(a1->chain_id, ADDR_BROADCAST, reg, REG09_LENGTH))
        {
            applog(LOG_DEBUG, "set %d PLL %d success", pllClkIdx, Ax_clock_index2pll(pllClkIdx));
            return true;
        }
        else
        {
            applog(LOG_WARNING, "set %d PLL %d fail times:%d", pllClkIdx, Ax_clock_index2pll(pllClkIdx), i);
            usleep(100);
            continue;
        }
    }

    return false;
}


bool Ax_clock_setpll_by_step(struct A1_chain *a1, int pllClkIdx)
{
    int i;
    
    for(i = 0; i < pllClkIdx + 1; i++)
    {
        if(((i % 5) == 0) || (i == pllClkIdx))
        {
            if(!Ax_clock_setpll(a1, i))
            {
                return false;
            }

            usleep(100000); 
        }
    }

    return true;
}
