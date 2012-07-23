typedef struct
{
    const char *descfile;
    const char *name;
    const char *stylus;
    const char *eraser;
    const char *cursor;
    const char *pad;
    const char *touch;
} Tablet;

Tablet tablets[] = {
    { "Wacom-PTK-540WL.desc",              "PTK-540WL",                    "stylus", "eraser", "cursor", "pad",    "touch"  },
    { "Wacom-Bamboo-16FG-4x5-Finger.desc", "Wacom Bamboo 16FG 4x5 Finger", NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "Wacom-Bamboo-16FG-4x5-Pen.desc",    "Wacom Bamboo 16FG 4x5 Pen",    "stylus", NULL,     NULL,     NULL,     NULL     },
    { "Wacom-Bamboo-One.desc",             "Wacom Bamboo1",                "stylus", NULL,     NULL,     NULL,     NULL     },
    { "Wacom-Bamboo-2FG-4x5-Finger.desc",  "Wacom Bamboo 2FG 4x5 Finger",  NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "Wacom-Bamboo-2FG-4x5-Pen.desc",     "Wacom Bamboo 2FG 4x5 Pen",     "stylus", "eraser", NULL,     "pad",    NULL     },
    { "Wacom-Cintiq-12WX.desc",            "Wacom Cintiq 12WX",            "stylus", "eraser", NULL,     "pad",    NULL     },
    { "Wacom-Cintiq-21UX2.desc",           "Wacom Cintiq 21UX2",           "stylus", "eraser", NULL,     "pad",    NULL     },
    { "Wacom-Intuos3-4x6.desc",            "Wacom Intuos3 4x6",            "stylus", NULL,     NULL,     NULL,     NULL     },
    { "Wacom-Intuos4-6x9.desc",            "Wacom Intuos4 6x9",            "stylus", "eraser", "cursor", NULL,     NULL     },
    { "Wacom-Intuos5-touch-M-Finger.desc", "Wacom Intuos5 touch M Finger", NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "Wacom-Intuos5-touch-M-Pen.desc",    "Wacom Intuos5 touch M Pen",    "stylus", "eraser", "cursor", "pad",    NULL     },
    { "Wacom-ISDv4-E6-Finger.desc",        "Wacom ISDv4 E6 Finger",        NULL,     NULL,     NULL,     NULL,     "touch"  },
    { "Wacom-ISDv4-E6-Pen.desc",           "Wacom ISDv4 E6 Pen",           "stylus", "eraser", NULL,     NULL,     NULL     }
};

