#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef __WACOM_DEVS_H__
#define __WACOM_DEVS_H__

#ifndef DOXYGEN_IGNORE_THIS

typedef struct
{
    const char *test_id;
    const char *descfile;
    const char *name;
    const char *stylus;
    const char *eraser;
    const char *cursor;
    const char *pad;
    const char *touch;
} Tablet;

void PrintTo(const Tablet& t, ::std::ostream *os) {
    *os << "Tablet tested: " << t.test_id << " " << t.name;
}

Tablet tablets[] = {
    { "00", "Wacom-PTK-540WL.desc",              "PTK-540WL",                    "stylus", "eraser", "cursor", "pad",    "touch"  },
    { "01", "Wacom-Bamboo-16FG-4x5-Finger.desc", "Wacom Bamboo 16FG 4x5 Finger", NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "02", "Wacom-Bamboo-16FG-4x5-Pen.desc",    "Wacom Bamboo 16FG 4x5 Pen",    "stylus", NULL,     NULL,     NULL,     NULL     },
    { "03", "Wacom-Bamboo-One.desc",             "Wacom Bamboo1",                "stylus", NULL,     NULL,     NULL,     NULL     },
    { "04", "Wacom-Bamboo-2FG-4x5-Finger.desc",  "Wacom Bamboo 2FG 4x5 Finger",  NULL,     NULL,     NULL,     "pad",    "touch"  },
    { "05", "Wacom-Bamboo-2FG-4x5-Pen.desc",     "Wacom Bamboo 2FG 4x5 Pen",     "stylus", "eraser", NULL,     NULL,     NULL     },
    { "06", "Wacom-Cintiq-12WX.desc",            "Wacom Cintiq 12WX",            "stylus", "eraser", NULL,     "pad",    NULL     },
    { "07", "Wacom-Cintiq-21UX2.desc",           "Wacom Cintiq 21UX2",           "stylus", "eraser", NULL,     "pad",    NULL     },
    { "08", "Wacom-Intuos3-4x6.desc",            "Wacom Intuos3 4x6",            "stylus", NULL,     NULL,     NULL,     NULL     },
    { "09", "Wacom-Intuos4-6x9.desc",            "Wacom Intuos4 6x9",            "stylus", "eraser", "cursor", NULL,     NULL     },
    { "10", "Wacom-Intuos5-touch-M-Finger.desc", "Wacom Intuos5 touch M Finger", NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "11", "Wacom-Intuos5-touch-M-Pen.desc",    "Wacom Intuos5 touch M Pen",    "stylus", "eraser", "cursor", "pad",    NULL     },
    { "12", "Wacom-ISDv4-E6-Finger.desc",        "Wacom ISDv4 E6 Finger",        NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "13", "Wacom-ISDv4-E6-Pen.desc",           "Wacom ISDv4 E6 Pen",           "stylus", "eraser", NULL,     NULL,     NULL     },
    { "14", "Wacom-Intuos4-8x13.desc",           "Wacom Intuos4 8x13",           "stylus", "eraser", "cursor", NULL,     NULL     },
};

#endif /* DOXYGEN_IGNORE_THIS */

#endif
