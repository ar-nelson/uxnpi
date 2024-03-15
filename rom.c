const u16 uxn_rom[] = {
    0x0120, 0x8043, 0x3720, 0xf020, 0x807f, 0x3708, 0xf020, 0x80e0,
    0x370a, 0xf020, 0x80c0, 0x370c, 0x2280, 0x8036, 0x3f01, 0x0020,
    0x3920, 0x0280, 0x8031, 0x3624, 0x0180, 0x803f, 0x3104, 0x0120,
    0x2ed4, 0x0220, 0x2e2f, 0x0220, 0x2e71, 0x0220, 0x2eae, 0x0320,
    0x2e3a, 0x8000, 0x3000, 0x2321, 0x0080, 0x8031, 0x3002, 0x0020,
    0x3848, 0x2880, 0x8037, 0x3004, 0x0020, 0x3950, 0x2a80, 0x8037,
    0x0f01, 0x0305, 0x0480, 0x801f, 0x0500, 0x3080, 0x203f, 0x9103,
    0x8038, 0x372c, 0x80cf, 0x172f, 0x0f80, 0x801c, 0x0500, 0x3080,
    0x203f, 0x9103, 0x8038, 0x372c, 0x2880, 0x2036, 0x0800, 0x8038,
    0x3728, 0x80cf, 0x172f, 0x8003, 0x1f04, 0x0080, 0x8005, 0x3f30,
    0x0320, 0x3891, 0x2c80, 0x8037, 0x3628, 0x0020, 0x3808, 0x2880,
    0xcf37, 0x2f80, 0x8017, 0x1c0f, 0x0080, 0x8005, 0x3f30, 0x0320,
    0x3891, 0x2c80, 0x8037, 0x3628, 0x0020, 0x3808, 0x2880, 0x4f37,
    0x2f80, 0x0017, 0x1080, 0x0080, 0x8003, 0x1f30, 0x0080, 0x2005,
    0x9103, 0x8038, 0x372c, 0x8003, 0x1f30, 0x0080, 0x8005, 0x3002,
    0x0020, 0x3940, 0x8038, 0x3728, 0x0480, 0x2030, 0x5000, 0x8039,
    0x372a, 0x0180, 0x2f80, 0x0317, 0x3080, 0x801f, 0x0500, 0x0480,
    0x2030, 0x4000, 0x3839, 0x2a80, 0x8037, 0x3002, 0x0020, 0x3950,
    0x2880, 0x8037, 0x8001, 0x172f, 0x8a01, 0xab80, 0x220d, 0x206c,
    0x8103, 0x2c80, 0x8037, 0x8000, 0x0300, 0x0f80, 0x801c, 0x1f40,
    0x0180, 0x801f, 0x0500, 0x0280, 0x2030, 0x4000, 0x3839, 0x2880,
    0x0337, 0xf080, 0x801c, 0x1f01, 0x0080, 0x8005, 0x3004, 0x0020,
    0x3940, 0x8038, 0x372a, 0x8003, 0x172f, 0x8901, 0xca80, 0x220d,
    0x806c, 0x8010, 0x8f00, 0x8003, 0x1f02, 0x0080, 0x8005, 0x3f40,
    0x0480, 0x2030, 0x4000, 0x3839, 0x032f, 0x0380, 0x801c, 0x0500,
    0x4080, 0x203f, 0x4000, 0x8038, 0x3002, 0x0020, 0x3808, 0x6f38,
    0x804f, 0x2000, 0xe702, 0x012e, 0x808a, 0x0dc9, 0x6c22, 0x1080,
    0x0080, 0x038f, 0x0280, 0x801f, 0x0500, 0x4080, 0x803f, 0x3004,
    0x2f38, 0x8003, 0x1c03, 0x0080, 0x8005, 0x3f40, 0x0020, 0x3840,
    0x0280, 0x2030, 0x0800, 0x3838, 0x4f6f, 0x8080, 0x0220, 0x2ee7,
    0x8a01, 0xcd80, 0x220d, 0x186c, 0x200f, 0x8103, 0x2c80, 0x8037,
    0x372a, 0x2880, 0x8037, 0xcf00, 0x8018, 0x172f, 0x2880, 0x2036,
    0x0800, 0x8038, 0x3728, 0x1080, 0x18cf, 0x2f80, 0x8017, 0x3628,
    0x0020, 0x3908, 0x2880, 0x8037, 0x362a, 0x0020, 0x3808, 0x2a80,
    0x8037, 0xcf20, 0x8018, 0x172f, 0x2880, 0x2036, 0x0800, 0x8038,
    0x3728, 0x3080, 0x184f, 0x2f80, 0x6c17, 0x0480, 0x2030, 0x4000,
    0x8039, 0x372a, 0x0280, 0x2030, 0x4800, 0x8038, 0x3728, 0x0080,
    0x2e80, 0x8017, 0x3002, 0x0020, 0x3849, 0x2880, 0x8037, 0x8001,
    0x172e, 0x0280, 0x2030, 0x4a00, 0x8038, 0x3728, 0x0280, 0x2e80,
    0x8017, 0x3002, 0x0020, 0x384b, 0x2880, 0x8037, 0x8003, 0x172e,
    0x0f6c, 0x6738, 0xdf5f, 0xbfbf, 0x00bf, 0x1807, 0x2320, 0x4844,
    0x0048, 0x827c, 0x8282, 0x8282, 0x007c, 0x1030, 0x1010, 0x1010,
    0x0010, 0x827c, 0x7c02, 0x8080, 0x00fe, 0x827c, 0x1c02, 0x8202,
    0x007c, 0x140c, 0x4424, 0xfe84, 0x0004, 0x80fe, 0x7c80, 0x8202,
    0x007c, 0x827c, 0xfc80, 0x8282, 0x007c, 0x827c, 0x1e02, 0x0202,
    0x0002, 0x827c, 0x7c82, 0x8282, 0x007c, 0x827c, 0x7e82, 0x8202,
    0x007c, 0x827c, 0x7e02, 0x8282, 0x007e, 0x82fc, 0xfc82, 0x8282,
    0x00fc, 0x827c, 0x8080, 0x8280, 0x007c, 0x82fc, 0x8282, 0x8282,
    0x00fc, 0x827c, 0xf080, 0x8280, 0x007c, 0x827c, 0xf080, 0x8080,
    0x8080,
};
