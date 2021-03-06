#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "temperature.h"

const int32_t temperature_table[] = {
    5199,
    5541,
    5906,
    6293,
    6705,
    7143,
    7607,
    8099,
    8621,
    9173,
    9757,
    10387,
    11048,
    11741,
    12469,
    13234,
    14036,
    14878,
    15762,
    16690,
    17664,
    18697,
    19781,
    20919,
    22111,
    23361,
    24670,
    26042,
    27477,
    28980,
    30552,
    32221,
    33974,
    35813,
    37742,
    39766,
    41889,
    44114,
    46447,
    48891,
    51451,
    54047,
    56767,
    59616,
    62598,
    65719,
    68983,
    72396,
    75963,
    79691,
    83583,
    87648,
    91889,
    96314,
    100928,
    105738,
    110751,
    115972,
    121410,
    127070,
    132960,
    139086,
    145457,
    152080,
    158962,
    166111,
    173534,
    181241,
    189239,
    197536,
    206141,
    215063,
    224309,
    233891,
    243814,
    254091,
    264729,
    275737,
    287127,
    298907,
    311088,
    323679,
    336691,
    350133,
    364016,
    378352,
    393152,
    408424,
    424181,
    440434,
    457194,
    474474,
    492283,
    510634,
    529540,
    549010,
    569060,
    589700,
    610942,
    632801,
    655285,
    678412,
    702192,
    726638,
    751761,
    777579,
    804103,
    831345,
    859321,
    888040,
    917519,
    947774,
    978816,
    1010657,
    1043307,
    1076787,
    1111117,
    1146289,
    1182343,
    1219274,
    1257096,
    1295837,
    1335499,
    1376095,
    1417640,
    1460157,
    1504197,
    1549214,
    1595171,
    1642104,
    1690007,
    1738877,
    1788710,
    1839505,
    1891264,
    1944012,
    1997691,
    2052359,
    2107981,
    2164550,
    2222088,
    2280566,
    2340016,
    2400379,
    2461698,
    2523990,
    2587159,
    2651308,
    2716341,
    2782337,
    2849245,
    2917066,
    2985800,
    3055457,
    3126053,
    3197486,
    3269902,
    3343206,
    3417365,
    3492488,
    3568481,
    3645476,
    3723305,
    3802029,
    3881727,
    3962310
};

int32_t convert_temp(int32_t adcValue)
{
    int32_t temperature = -40;
    for(int i=0; i<166;i++) {
        if (adcValue < temperature_table[i]) return temperature;
        temperature++;
    }

    return temperature;
}
