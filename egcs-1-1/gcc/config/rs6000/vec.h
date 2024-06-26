static struct builtin B_vec_vabsfp = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vabsfp", "vabsfp", CODE_FOR_xfx_complex, B_UID(0), NULL };
static struct builtin B_vec_vabssh = { { &T_vec_s16, NULL, NULL, }, "x", &T_vec_s16, 1, FALSE, FALSE, 0, "vec_vabssh", "vabssh", CODE_FOR_xfx_complex, B_UID(1), NULL };
static struct builtin B_vec_vabssw = { { &T_vec_s32, NULL, NULL, }, "x", &T_vec_s32, 1, FALSE, FALSE, 0, "vec_vabssw", "vabssw", CODE_FOR_xfx_complex, B_UID(2), NULL };
static struct builtin B_vec_vabssb = { { &T_vec_s8, NULL, NULL, }, "x", &T_vec_s8, 1, FALSE, FALSE, 0, "vec_vabssb", "vabssb", CODE_FOR_xfx_complex, B_UID(3), NULL };
static struct builtin B_vec_vabsssh = { { &T_vec_s16, NULL, NULL, }, "x", &T_vec_s16, 1, FALSE, FALSE, 0, "vec_vabsssh", "vabsssh", CODE_FOR_xfx_complex, B_UID(4), NULL };
static struct builtin B_vec_vabsssw = { { &T_vec_s32, NULL, NULL, }, "x", &T_vec_s32, 1, FALSE, FALSE, 0, "vec_vabsssw", "vabsssw", CODE_FOR_xfx_complex, B_UID(5), NULL };
static struct builtin B_vec_vabsssb = { { &T_vec_s8, NULL, NULL, }, "x", &T_vec_s8, 1, FALSE, FALSE, 0, "vec_vabsssb", "vabsssb", CODE_FOR_xfx_complex, B_UID(6), NULL };
static struct builtin B1_vec_vadduhm = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vadduhm:1", "vadduhm", CODE_FOR_xfxx_simple, B_UID(7), NULL };
static struct builtin B2_vec_vadduhm = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vadduhm:2", "vadduhm", CODE_FOR_xfxx_simple, B_UID(8), NULL };
static struct builtin B1_vec_vadduwm = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vadduwm:1", "vadduwm", CODE_FOR_xfxx_simple, B_UID(9), NULL };
static struct builtin B2_vec_vadduwm = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vadduwm:2", "vadduwm", CODE_FOR_xfxx_simple, B_UID(10), NULL };
static struct builtin B1_vec_vaddubm = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vaddubm:1", "vaddubm", CODE_FOR_xfxx_simple, B_UID(11), NULL };
static struct builtin B2_vec_vaddubm = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vaddubm:2", "vaddubm", CODE_FOR_xfxx_simple, B_UID(12), NULL };
static struct builtin B_vec_vaddfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vaddfp", "vaddfp", CODE_FOR_xfxx_fp, B_UID(13), NULL };
static struct builtin B3_vec_vadduhm = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vadduhm:3", "vadduhm", CODE_FOR_xfxx_simple, B_UID(14), NULL };
static struct builtin B4_vec_vadduhm = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vadduhm:4", "vadduhm", CODE_FOR_xfxx_simple, B_UID(15), NULL };
static struct builtin B3_vec_vadduwm = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vadduwm:3", "vadduwm", CODE_FOR_xfxx_simple, B_UID(16), NULL };
static struct builtin B4_vec_vadduwm = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vadduwm:4", "vadduwm", CODE_FOR_xfxx_simple, B_UID(17), NULL };
static struct builtin B3_vec_vaddubm = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vaddubm:3", "vaddubm", CODE_FOR_xfxx_simple, B_UID(18), NULL };
static struct builtin B4_vec_vaddubm = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vaddubm:4", "vaddubm", CODE_FOR_xfxx_simple, B_UID(19), NULL };
static struct builtin B5_vec_vadduhm = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vadduhm:5", "vadduhm", CODE_FOR_xfxx_simple, B_UID(20), NULL };
static struct builtin B6_vec_vadduhm = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vadduhm:6", "vadduhm", CODE_FOR_xfxx_simple, B_UID(21), NULL };
static struct builtin B5_vec_vadduwm = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vadduwm:5", "vadduwm", CODE_FOR_xfxx_simple, B_UID(22), NULL };
static struct builtin B6_vec_vadduwm = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vadduwm:6", "vadduwm", CODE_FOR_xfxx_simple, B_UID(23), NULL };
static struct builtin B5_vec_vaddubm = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vaddubm:5", "vaddubm", CODE_FOR_xfxx_simple, B_UID(24), NULL };
static struct builtin B6_vec_vaddubm = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vaddubm:6", "vaddubm", CODE_FOR_xfxx_simple, B_UID(25), NULL };
static struct builtin B_vec_vaddcuw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vaddcuw", "vaddcuw", CODE_FOR_xfxx_simple, B_UID(26), NULL };
static struct builtin B1_vec_vaddshs = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vaddshs:1", "vaddshs", CODE_FOR_xfxx_simple, B_UID(27), NULL };
static struct builtin B1_vec_vadduhs = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vadduhs:1", "vadduhs", CODE_FOR_xfxx_simple, B_UID(28), NULL };
static struct builtin B1_vec_vaddsws = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vaddsws:1", "vaddsws", CODE_FOR_xfxx_simple, B_UID(29), NULL };
static struct builtin B1_vec_vadduws = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vadduws:1", "vadduws", CODE_FOR_xfxx_simple, B_UID(30), NULL };
static struct builtin B1_vec_vaddsbs = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vaddsbs:1", "vaddsbs", CODE_FOR_xfxx_simple, B_UID(31), NULL };
static struct builtin B1_vec_vaddubs = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vaddubs:1", "vaddubs", CODE_FOR_xfxx_simple, B_UID(32), NULL };
static struct builtin B2_vec_vaddshs = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vaddshs:2", "vaddshs", CODE_FOR_xfxx_simple, B_UID(33), NULL };
static struct builtin B3_vec_vaddshs = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vaddshs:3", "vaddshs", CODE_FOR_xfxx_simple, B_UID(34), NULL };
static struct builtin B2_vec_vaddsws = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vaddsws:2", "vaddsws", CODE_FOR_xfxx_simple, B_UID(35), NULL };
static struct builtin B3_vec_vaddsws = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vaddsws:3", "vaddsws", CODE_FOR_xfxx_simple, B_UID(36), NULL };
static struct builtin B2_vec_vaddsbs = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vaddsbs:2", "vaddsbs", CODE_FOR_xfxx_simple, B_UID(37), NULL };
static struct builtin B3_vec_vaddsbs = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vaddsbs:3", "vaddsbs", CODE_FOR_xfxx_simple, B_UID(38), NULL };
static struct builtin B2_vec_vadduhs = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vadduhs:2", "vadduhs", CODE_FOR_xfxx_simple, B_UID(39), NULL };
static struct builtin B3_vec_vadduhs = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vadduhs:3", "vadduhs", CODE_FOR_xfxx_simple, B_UID(40), NULL };
static struct builtin B2_vec_vadduws = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vadduws:2", "vadduws", CODE_FOR_xfxx_simple, B_UID(41), NULL };
static struct builtin B3_vec_vadduws = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vadduws:3", "vadduws", CODE_FOR_xfxx_simple, B_UID(42), NULL };
static struct builtin B2_vec_vaddubs = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vaddubs:2", "vaddubs", CODE_FOR_xfxx_simple, B_UID(43), NULL };
static struct builtin B3_vec_vaddubs = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vaddubs:3", "vaddubs", CODE_FOR_xfxx_simple, B_UID(44), NULL };
static struct builtin B1_vec_all_eq = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:1", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(45), NULL };
static struct builtin B2_vec_all_eq = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:2", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(46), NULL };
static struct builtin B3_vec_all_eq = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:3", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(47), NULL };
static struct builtin B4_vec_all_eq = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:4", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(48), NULL };
static struct builtin B5_vec_all_eq = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:5", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(49), NULL };
static struct builtin B6_vec_all_eq = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:6", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(50), NULL };
static struct builtin B7_vec_all_eq = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:7", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(51), NULL };
static struct builtin B8_vec_all_eq = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:8", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(52), NULL };
static struct builtin B9_vec_all_eq = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:9", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(53), NULL };
static struct builtin B10_vec_all_eq = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:10", "vcmpeqfp.", CODE_FOR_j_24_t_fxx_simple, B_UID(54), NULL };
static struct builtin B11_vec_all_eq = { { &T_vec_p16, &T_vec_p16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:11", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(55), NULL };
static struct builtin B12_vec_all_eq = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:12", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(56), NULL };
static struct builtin B13_vec_all_eq = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:13", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(57), NULL };
static struct builtin B14_vec_all_eq = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:14", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(58), NULL };
static struct builtin B15_vec_all_eq = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:15", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(59), NULL };
static struct builtin B16_vec_all_eq = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:16", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(60), NULL };
static struct builtin B17_vec_all_eq = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:17", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(61), NULL };
static struct builtin B18_vec_all_eq = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:18", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(62), NULL };
static struct builtin B19_vec_all_eq = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:19", "vcmpequh.", CODE_FOR_j_24_t_fxx_simple, B_UID(63), NULL };
static struct builtin B20_vec_all_eq = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:20", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(64), NULL };
static struct builtin B21_vec_all_eq = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:21", "vcmpequw.", CODE_FOR_j_24_t_fxx_simple, B_UID(65), NULL };
static struct builtin B22_vec_all_eq = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:22", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(66), NULL };
static struct builtin B23_vec_all_eq = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_eq:23", "vcmpequb.", CODE_FOR_j_24_t_fxx_simple, B_UID(67), NULL };
static struct builtin B1_vec_all_ge = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:1", "vcmpgtsh.", CODE_FOR_j_26_t_frxx_simple, B_UID(68), NULL };
static struct builtin B2_vec_all_ge = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:2", "vcmpgtuh.", CODE_FOR_j_26_t_frxx_simple, B_UID(69), NULL };
static struct builtin B3_vec_all_ge = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:3", "vcmpgtsw.", CODE_FOR_j_26_t_frxx_simple, B_UID(70), NULL };
static struct builtin B4_vec_all_ge = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:4", "vcmpgtuw.", CODE_FOR_j_26_t_frxx_simple, B_UID(71), NULL };
static struct builtin B5_vec_all_ge = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:5", "vcmpgtsb.", CODE_FOR_j_26_t_frxx_simple, B_UID(72), NULL };
static struct builtin B6_vec_all_ge = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:6", "vcmpgtub.", CODE_FOR_j_26_t_frxx_simple, B_UID(73), NULL };
static struct builtin B7_vec_all_ge = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_ge:7", "vcmpgefp.", CODE_FOR_j_24_t_fxx_simple, B_UID(74), NULL };
static struct builtin B8_vec_all_ge = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:8", "vcmpgtsh.", CODE_FOR_j_26_t_frxx_simple, B_UID(75), NULL };
static struct builtin B9_vec_all_ge = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:9", "vcmpgtsh.", CODE_FOR_j_26_t_frxx_simple, B_UID(76), NULL };
static struct builtin B10_vec_all_ge = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:10", "vcmpgtsw.", CODE_FOR_j_26_t_frxx_simple, B_UID(77), NULL };
static struct builtin B11_vec_all_ge = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:11", "vcmpgtsw.", CODE_FOR_j_26_t_frxx_simple, B_UID(78), NULL };
static struct builtin B12_vec_all_ge = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:12", "vcmpgtsb.", CODE_FOR_j_26_t_frxx_simple, B_UID(79), NULL };
static struct builtin B13_vec_all_ge = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:13", "vcmpgtsb.", CODE_FOR_j_26_t_frxx_simple, B_UID(80), NULL };
static struct builtin B14_vec_all_ge = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:14", "vcmpgtuh.", CODE_FOR_j_26_t_frxx_simple, B_UID(81), NULL };
static struct builtin B15_vec_all_ge = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:15", "vcmpgtuh.", CODE_FOR_j_26_t_frxx_simple, B_UID(82), NULL };
static struct builtin B16_vec_all_ge = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:16", "vcmpgtuw.", CODE_FOR_j_26_t_frxx_simple, B_UID(83), NULL };
static struct builtin B17_vec_all_ge = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:17", "vcmpgtuw.", CODE_FOR_j_26_t_frxx_simple, B_UID(84), NULL };
static struct builtin B18_vec_all_ge = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:18", "vcmpgtub.", CODE_FOR_j_26_t_frxx_simple, B_UID(85), NULL };
static struct builtin B19_vec_all_ge = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_ge:19", "vcmpgtub.", CODE_FOR_j_26_t_frxx_simple, B_UID(86), NULL };
static struct builtin B1_vec_all_gt = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:1", "vcmpgtsh.", CODE_FOR_j_24_t_fxx_simple, B_UID(87), NULL };
static struct builtin B2_vec_all_gt = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:2", "vcmpgtuh.", CODE_FOR_j_24_t_fxx_simple, B_UID(88), NULL };
static struct builtin B3_vec_all_gt = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:3", "vcmpgtsw.", CODE_FOR_j_24_t_fxx_simple, B_UID(89), NULL };
static struct builtin B4_vec_all_gt = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:4", "vcmpgtuw.", CODE_FOR_j_24_t_fxx_simple, B_UID(90), NULL };
static struct builtin B5_vec_all_gt = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:5", "vcmpgtsb.", CODE_FOR_j_24_t_fxx_simple, B_UID(91), NULL };
static struct builtin B6_vec_all_gt = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:6", "vcmpgtub.", CODE_FOR_j_24_t_fxx_simple, B_UID(92), NULL };
static struct builtin B7_vec_all_gt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:7", "vcmpgtfp.", CODE_FOR_j_24_t_fxx_simple, B_UID(93), NULL };
static struct builtin B8_vec_all_gt = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:8", "vcmpgtsh.", CODE_FOR_j_24_t_fxx_simple, B_UID(94), NULL };
static struct builtin B9_vec_all_gt = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:9", "vcmpgtsh.", CODE_FOR_j_24_t_fxx_simple, B_UID(95), NULL };
static struct builtin B10_vec_all_gt = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:10", "vcmpgtsw.", CODE_FOR_j_24_t_fxx_simple, B_UID(96), NULL };
static struct builtin B11_vec_all_gt = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:11", "vcmpgtsw.", CODE_FOR_j_24_t_fxx_simple, B_UID(97), NULL };
static struct builtin B12_vec_all_gt = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:12", "vcmpgtsb.", CODE_FOR_j_24_t_fxx_simple, B_UID(98), NULL };
static struct builtin B13_vec_all_gt = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:13", "vcmpgtsb.", CODE_FOR_j_24_t_fxx_simple, B_UID(99), NULL };
static struct builtin B14_vec_all_gt = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:14", "vcmpgtuh.", CODE_FOR_j_24_t_fxx_simple, B_UID(100), NULL };
static struct builtin B15_vec_all_gt = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:15", "vcmpgtuh.", CODE_FOR_j_24_t_fxx_simple, B_UID(101), NULL };
static struct builtin B16_vec_all_gt = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:16", "vcmpgtuw.", CODE_FOR_j_24_t_fxx_simple, B_UID(102), NULL };
static struct builtin B17_vec_all_gt = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:17", "vcmpgtuw.", CODE_FOR_j_24_t_fxx_simple, B_UID(103), NULL };
static struct builtin B18_vec_all_gt = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:18", "vcmpgtub.", CODE_FOR_j_24_t_fxx_simple, B_UID(104), NULL };
static struct builtin B19_vec_all_gt = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc24t, 2, FALSE, FALSE, 0, "vec_all_gt:19", "vcmpgtub.", CODE_FOR_j_24_t_fxx_simple, B_UID(105), NULL };
static struct builtin B_vec_all_in = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_in", "vcmpbfp.", CODE_FOR_j_26_t_fxx_simple, B_UID(106), NULL };
static struct builtin B1_vec_all_le = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:1", "vcmpgtsh.", CODE_FOR_j_26_t_fxx_simple, B_UID(107), NULL };
static struct builtin B2_vec_all_le = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:2", "vcmpgtuh.", CODE_FOR_j_26_t_fxx_simple, B_UID(108), NULL };
static struct builtin B3_vec_all_le = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:3", "vcmpgtsw.", CODE_FOR_j_26_t_fxx_simple, B_UID(109), NULL };
static struct builtin B4_vec_all_le = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:4", "vcmpgtuw.", CODE_FOR_j_26_t_fxx_simple, B_UID(110), NULL };
static struct builtin B5_vec_all_le = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:5", "vcmpgtsb.", CODE_FOR_j_26_t_fxx_simple, B_UID(111), NULL };
static struct builtin B6_vec_all_le = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:6", "vcmpgtub.", CODE_FOR_j_26_t_fxx_simple, B_UID(112), NULL };
static struct builtin B7_vec_all_le = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_le:7", "vcmpgefp.", CODE_FOR_j_24_t_frxx_simple, B_UID(113), NULL };
static struct builtin B8_vec_all_le = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:8", "vcmpgtsh.", CODE_FOR_j_26_t_fxx_simple, B_UID(114), NULL };
static struct builtin B9_vec_all_le = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:9", "vcmpgtsh.", CODE_FOR_j_26_t_fxx_simple, B_UID(115), NULL };
static struct builtin B10_vec_all_le = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:10", "vcmpgtsw.", CODE_FOR_j_26_t_fxx_simple, B_UID(116), NULL };
static struct builtin B11_vec_all_le = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:11", "vcmpgtsw.", CODE_FOR_j_26_t_fxx_simple, B_UID(117), NULL };
static struct builtin B12_vec_all_le = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:12", "vcmpgtsb.", CODE_FOR_j_26_t_fxx_simple, B_UID(118), NULL };
static struct builtin B13_vec_all_le = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:13", "vcmpgtsb.", CODE_FOR_j_26_t_fxx_simple, B_UID(119), NULL };
static struct builtin B14_vec_all_le = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:14", "vcmpgtuh.", CODE_FOR_j_26_t_fxx_simple, B_UID(120), NULL };
static struct builtin B15_vec_all_le = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:15", "vcmpgtuh.", CODE_FOR_j_26_t_fxx_simple, B_UID(121), NULL };
static struct builtin B16_vec_all_le = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:16", "vcmpgtuw.", CODE_FOR_j_26_t_fxx_simple, B_UID(122), NULL };
static struct builtin B17_vec_all_le = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:17", "vcmpgtuw.", CODE_FOR_j_26_t_fxx_simple, B_UID(123), NULL };
static struct builtin B18_vec_all_le = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:18", "vcmpgtub.", CODE_FOR_j_26_t_fxx_simple, B_UID(124), NULL };
static struct builtin B19_vec_all_le = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_le:19", "vcmpgtub.", CODE_FOR_j_26_t_fxx_simple, B_UID(125), NULL };
static struct builtin B1_vec_all_lt = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:1", "vcmpgtsh.", CODE_FOR_j_24_t_frxx_simple, B_UID(126), NULL };
static struct builtin B2_vec_all_lt = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:2", "vcmpgtuh.", CODE_FOR_j_24_t_frxx_simple, B_UID(127), NULL };
static struct builtin B3_vec_all_lt = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:3", "vcmpgtsw.", CODE_FOR_j_24_t_frxx_simple, B_UID(128), NULL };
static struct builtin B4_vec_all_lt = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:4", "vcmpgtuw.", CODE_FOR_j_24_t_frxx_simple, B_UID(129), NULL };
static struct builtin B5_vec_all_lt = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:5", "vcmpgtsb.", CODE_FOR_j_24_t_frxx_simple, B_UID(130), NULL };
static struct builtin B6_vec_all_lt = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:6", "vcmpgtub.", CODE_FOR_j_24_t_frxx_simple, B_UID(131), NULL };
static struct builtin B7_vec_all_lt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:7", "vcmpgtfp.", CODE_FOR_j_24_t_frxx_simple, B_UID(132), NULL };
static struct builtin B8_vec_all_lt = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:8", "vcmpgtsh.", CODE_FOR_j_24_t_frxx_simple, B_UID(133), NULL };
static struct builtin B9_vec_all_lt = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:9", "vcmpgtsh.", CODE_FOR_j_24_t_frxx_simple, B_UID(134), NULL };
static struct builtin B10_vec_all_lt = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:10", "vcmpgtsw.", CODE_FOR_j_24_t_frxx_simple, B_UID(135), NULL };
static struct builtin B11_vec_all_lt = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:11", "vcmpgtsw.", CODE_FOR_j_24_t_frxx_simple, B_UID(136), NULL };
static struct builtin B12_vec_all_lt = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:12", "vcmpgtsb.", CODE_FOR_j_24_t_frxx_simple, B_UID(137), NULL };
static struct builtin B13_vec_all_lt = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:13", "vcmpgtsb.", CODE_FOR_j_24_t_frxx_simple, B_UID(138), NULL };
static struct builtin B14_vec_all_lt = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:14", "vcmpgtuh.", CODE_FOR_j_24_t_frxx_simple, B_UID(139), NULL };
static struct builtin B15_vec_all_lt = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:15", "vcmpgtuh.", CODE_FOR_j_24_t_frxx_simple, B_UID(140), NULL };
static struct builtin B16_vec_all_lt = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:16", "vcmpgtuw.", CODE_FOR_j_24_t_frxx_simple, B_UID(141), NULL };
static struct builtin B17_vec_all_lt = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:17", "vcmpgtuw.", CODE_FOR_j_24_t_frxx_simple, B_UID(142), NULL };
static struct builtin B18_vec_all_lt = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:18", "vcmpgtub.", CODE_FOR_j_24_t_frxx_simple, B_UID(143), NULL };
static struct builtin B19_vec_all_lt = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc24tr, 2, FALSE, FALSE, 0, "vec_all_lt:19", "vcmpgtub.", CODE_FOR_j_24_t_frxx_simple, B_UID(144), NULL };
static struct builtin B_vec_all_nan = { { &T_vec_f32, NULL, NULL, }, "x", &T_cc26td, 1, FALSE, FALSE, 0, "vec_all_nan", "vcmpeqfp.", CODE_FOR_j_26_t_fx_simple, B_UID(145), NULL };
static struct builtin B1_vec_all_ne = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:1", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(146), NULL };
static struct builtin B2_vec_all_ne = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:2", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(147), NULL };
static struct builtin B3_vec_all_ne = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:3", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(148), NULL };
static struct builtin B4_vec_all_ne = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:4", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(149), NULL };
static struct builtin B5_vec_all_ne = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:5", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(150), NULL };
static struct builtin B6_vec_all_ne = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:6", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(151), NULL };
static struct builtin B7_vec_all_ne = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:7", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(152), NULL };
static struct builtin B8_vec_all_ne = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:8", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(153), NULL };
static struct builtin B9_vec_all_ne = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:9", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(154), NULL };
static struct builtin B10_vec_all_ne = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:10", "vcmpeqfp.", CODE_FOR_j_26_t_fxx_simple, B_UID(155), NULL };
static struct builtin B11_vec_all_ne = { { &T_vec_p16, &T_vec_p16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:11", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(156), NULL };
static struct builtin B12_vec_all_ne = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:12", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(157), NULL };
static struct builtin B13_vec_all_ne = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:13", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(158), NULL };
static struct builtin B14_vec_all_ne = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:14", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(159), NULL };
static struct builtin B15_vec_all_ne = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:15", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(160), NULL };
static struct builtin B16_vec_all_ne = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:16", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(161), NULL };
static struct builtin B17_vec_all_ne = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:17", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(162), NULL };
static struct builtin B18_vec_all_ne = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:18", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(163), NULL };
static struct builtin B19_vec_all_ne = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:19", "vcmpequh.", CODE_FOR_j_26_t_fxx_simple, B_UID(164), NULL };
static struct builtin B20_vec_all_ne = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:20", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(165), NULL };
static struct builtin B21_vec_all_ne = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:21", "vcmpequw.", CODE_FOR_j_26_t_fxx_simple, B_UID(166), NULL };
static struct builtin B22_vec_all_ne = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:22", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(167), NULL };
static struct builtin B23_vec_all_ne = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ne:23", "vcmpequb.", CODE_FOR_j_26_t_fxx_simple, B_UID(168), NULL };
static struct builtin B_vec_all_nge = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_nge", "vcmpgefp.", CODE_FOR_j_26_t_fxx_simple, B_UID(169), NULL };
static struct builtin B_vec_all_ngt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26t, 2, FALSE, FALSE, 0, "vec_all_ngt", "vcmpgtfp.", CODE_FOR_j_26_t_fxx_simple, B_UID(170), NULL };
static struct builtin B_vec_all_nle = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_nle", "vcmpgefp.", CODE_FOR_j_26_t_frxx_simple, B_UID(171), NULL };
static struct builtin B_vec_all_nlt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26tr, 2, FALSE, FALSE, 0, "vec_all_nlt", "vcmpgtfp.", CODE_FOR_j_26_t_frxx_simple, B_UID(172), NULL };
static struct builtin B_vec_all_numeric = { { &T_vec_f32, NULL, NULL, }, "x", &T_cc24td, 1, FALSE, FALSE, 0, "vec_all_numeric", "vcmpeqfp.", CODE_FOR_j_24_t_fx_simple, B_UID(173), NULL };
static struct builtin B1_vec_vand = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 2, "vec_vand:1", "vand", CODE_FOR_xfxx_simple, B_UID(174), NULL };
static struct builtin B2_vec_vand = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vand:2", "vand", CODE_FOR_xfxx_simple, B_UID(175), NULL };
static struct builtin B3_vec_vand = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vand:3", "vand", CODE_FOR_xfxx_simple, B_UID(176), NULL };
static struct builtin B4_vec_vand = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 2, "vec_vand:4", "vand", CODE_FOR_xfxx_simple, B_UID(177), NULL };
static struct builtin B5_vec_vand = { { &T_vec_b32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vand:5", "vand", CODE_FOR_xfxx_simple, B_UID(178), NULL };
static struct builtin B6_vec_vand = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vand:6", "vand", CODE_FOR_xfxx_simple, B_UID(179), NULL };
static struct builtin B7_vec_vand = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vand:7", "vand", CODE_FOR_xfxx_simple, B_UID(180), NULL };
static struct builtin B8_vec_vand = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 2, "vec_vand:8", "vand", CODE_FOR_xfxx_simple, B_UID(181), NULL };
static struct builtin B9_vec_vand = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vand:9", "vand", CODE_FOR_xfxx_simple, B_UID(182), NULL };
static struct builtin B10_vec_vand = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vand:10", "vand", CODE_FOR_xfxx_simple, B_UID(183), NULL };
static struct builtin B11_vec_vand = { { &T_vec_f32, &T_vec_b32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vand:11", "vand", CODE_FOR_xfxx_simple, B_UID(184), NULL };
static struct builtin B12_vec_vand = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vand:12", "vand", CODE_FOR_xfxx_simple, B_UID(185), NULL };
static struct builtin B13_vec_vand = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vand:13", "vand", CODE_FOR_xfxx_simple, B_UID(186), NULL };
static struct builtin B14_vec_vand = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vand:14", "vand", CODE_FOR_xfxx_simple, B_UID(187), NULL };
static struct builtin B15_vec_vand = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vand:15", "vand", CODE_FOR_xfxx_simple, B_UID(188), NULL };
static struct builtin B16_vec_vand = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vand:16", "vand", CODE_FOR_xfxx_simple, B_UID(189), NULL };
static struct builtin B17_vec_vand = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vand:17", "vand", CODE_FOR_xfxx_simple, B_UID(190), NULL };
static struct builtin B18_vec_vand = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vand:18", "vand", CODE_FOR_xfxx_simple, B_UID(191), NULL };
static struct builtin B19_vec_vand = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vand:19", "vand", CODE_FOR_xfxx_simple, B_UID(192), NULL };
static struct builtin B20_vec_vand = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vand:20", "vand", CODE_FOR_xfxx_simple, B_UID(193), NULL };
static struct builtin B21_vec_vand = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vand:21", "vand", CODE_FOR_xfxx_simple, B_UID(194), NULL };
static struct builtin B22_vec_vand = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vand:22", "vand", CODE_FOR_xfxx_simple, B_UID(195), NULL };
static struct builtin B23_vec_vand = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vand:23", "vand", CODE_FOR_xfxx_simple, B_UID(196), NULL };
static struct builtin B24_vec_vand = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vand:24", "vand", CODE_FOR_xfxx_simple, B_UID(197), NULL };
static struct builtin B1_vec_vandc = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 1, "vec_vandc:1", "vandc", CODE_FOR_xfxx_simple, B_UID(198), NULL };
static struct builtin B2_vec_vandc = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vandc:2", "vandc", CODE_FOR_xfxx_simple, B_UID(199), NULL };
static struct builtin B3_vec_vandc = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vandc:3", "vandc", CODE_FOR_xfxx_simple, B_UID(200), NULL };
static struct builtin B4_vec_vandc = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 1, "vec_vandc:4", "vandc", CODE_FOR_xfxx_simple, B_UID(201), NULL };
static struct builtin B5_vec_vandc = { { &T_vec_b32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vandc:5", "vandc", CODE_FOR_xfxx_simple, B_UID(202), NULL };
static struct builtin B6_vec_vandc = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vandc:6", "vandc", CODE_FOR_xfxx_simple, B_UID(203), NULL };
static struct builtin B7_vec_vandc = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vandc:7", "vandc", CODE_FOR_xfxx_simple, B_UID(204), NULL };
static struct builtin B8_vec_vandc = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 1, "vec_vandc:8", "vandc", CODE_FOR_xfxx_simple, B_UID(205), NULL };
static struct builtin B9_vec_vandc = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vandc:9", "vandc", CODE_FOR_xfxx_simple, B_UID(206), NULL };
static struct builtin B10_vec_vandc = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vandc:10", "vandc", CODE_FOR_xfxx_simple, B_UID(207), NULL };
static struct builtin B11_vec_vandc = { { &T_vec_f32, &T_vec_b32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vandc:11", "vandc", CODE_FOR_xfxx_simple, B_UID(208), NULL };
static struct builtin B12_vec_vandc = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vandc:12", "vandc", CODE_FOR_xfxx_simple, B_UID(209), NULL };
static struct builtin B13_vec_vandc = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vandc:13", "vandc", CODE_FOR_xfxx_simple, B_UID(210), NULL };
static struct builtin B14_vec_vandc = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vandc:14", "vandc", CODE_FOR_xfxx_simple, B_UID(211), NULL };
static struct builtin B15_vec_vandc = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vandc:15", "vandc", CODE_FOR_xfxx_simple, B_UID(212), NULL };
static struct builtin B16_vec_vandc = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vandc:16", "vandc", CODE_FOR_xfxx_simple, B_UID(213), NULL };
static struct builtin B17_vec_vandc = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vandc:17", "vandc", CODE_FOR_xfxx_simple, B_UID(214), NULL };
static struct builtin B18_vec_vandc = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vandc:18", "vandc", CODE_FOR_xfxx_simple, B_UID(215), NULL };
static struct builtin B19_vec_vandc = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vandc:19", "vandc", CODE_FOR_xfxx_simple, B_UID(216), NULL };
static struct builtin B20_vec_vandc = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vandc:20", "vandc", CODE_FOR_xfxx_simple, B_UID(217), NULL };
static struct builtin B21_vec_vandc = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vandc:21", "vandc", CODE_FOR_xfxx_simple, B_UID(218), NULL };
static struct builtin B22_vec_vandc = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vandc:22", "vandc", CODE_FOR_xfxx_simple, B_UID(219), NULL };
static struct builtin B23_vec_vandc = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vandc:23", "vandc", CODE_FOR_xfxx_simple, B_UID(220), NULL };
static struct builtin B24_vec_vandc = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vandc:24", "vandc", CODE_FOR_xfxx_simple, B_UID(221), NULL };
static struct builtin B1_vec_any_eq = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:1", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(222), NULL };
static struct builtin B2_vec_any_eq = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:2", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(223), NULL };
static struct builtin B3_vec_any_eq = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:3", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(224), NULL };
static struct builtin B4_vec_any_eq = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:4", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(225), NULL };
static struct builtin B5_vec_any_eq = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:5", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(226), NULL };
static struct builtin B6_vec_any_eq = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:6", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(227), NULL };
static struct builtin B7_vec_any_eq = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:7", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(228), NULL };
static struct builtin B8_vec_any_eq = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:8", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(229), NULL };
static struct builtin B9_vec_any_eq = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:9", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(230), NULL };
static struct builtin B10_vec_any_eq = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:10", "vcmpeqfp.", CODE_FOR_j_26_f_fxx_simple, B_UID(231), NULL };
static struct builtin B11_vec_any_eq = { { &T_vec_p16, &T_vec_p16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:11", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(232), NULL };
static struct builtin B12_vec_any_eq = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:12", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(233), NULL };
static struct builtin B13_vec_any_eq = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:13", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(234), NULL };
static struct builtin B14_vec_any_eq = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:14", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(235), NULL };
static struct builtin B15_vec_any_eq = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:15", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(236), NULL };
static struct builtin B16_vec_any_eq = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:16", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(237), NULL };
static struct builtin B17_vec_any_eq = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:17", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(238), NULL };
static struct builtin B18_vec_any_eq = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:18", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(239), NULL };
static struct builtin B19_vec_any_eq = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:19", "vcmpequh.", CODE_FOR_j_26_f_fxx_simple, B_UID(240), NULL };
static struct builtin B20_vec_any_eq = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:20", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(241), NULL };
static struct builtin B21_vec_any_eq = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:21", "vcmpequw.", CODE_FOR_j_26_f_fxx_simple, B_UID(242), NULL };
static struct builtin B22_vec_any_eq = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:22", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(243), NULL };
static struct builtin B23_vec_any_eq = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_eq:23", "vcmpequb.", CODE_FOR_j_26_f_fxx_simple, B_UID(244), NULL };
static struct builtin B1_vec_any_ge = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:1", "vcmpgtsh.", CODE_FOR_j_24_f_frxx_simple, B_UID(245), NULL };
static struct builtin B2_vec_any_ge = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:2", "vcmpgtuh.", CODE_FOR_j_24_f_frxx_simple, B_UID(246), NULL };
static struct builtin B3_vec_any_ge = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:3", "vcmpgtsw.", CODE_FOR_j_24_f_frxx_simple, B_UID(247), NULL };
static struct builtin B4_vec_any_ge = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:4", "vcmpgtuw.", CODE_FOR_j_24_f_frxx_simple, B_UID(248), NULL };
static struct builtin B5_vec_any_ge = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:5", "vcmpgtsb.", CODE_FOR_j_24_f_frxx_simple, B_UID(249), NULL };
static struct builtin B6_vec_any_ge = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:6", "vcmpgtub.", CODE_FOR_j_24_f_frxx_simple, B_UID(250), NULL };
static struct builtin B7_vec_any_ge = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_ge:7", "vcmpgefp.", CODE_FOR_j_26_f_fxx_simple, B_UID(251), NULL };
static struct builtin B8_vec_any_ge = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:8", "vcmpgtsh.", CODE_FOR_j_24_f_frxx_simple, B_UID(252), NULL };
static struct builtin B9_vec_any_ge = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:9", "vcmpgtsh.", CODE_FOR_j_24_f_frxx_simple, B_UID(253), NULL };
static struct builtin B10_vec_any_ge = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:10", "vcmpgtsw.", CODE_FOR_j_24_f_frxx_simple, B_UID(254), NULL };
static struct builtin B11_vec_any_ge = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:11", "vcmpgtsw.", CODE_FOR_j_24_f_frxx_simple, B_UID(255), NULL };
static struct builtin B12_vec_any_ge = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:12", "vcmpgtsb.", CODE_FOR_j_24_f_frxx_simple, B_UID(256), NULL };
static struct builtin B13_vec_any_ge = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:13", "vcmpgtsb.", CODE_FOR_j_24_f_frxx_simple, B_UID(257), NULL };
static struct builtin B14_vec_any_ge = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:14", "vcmpgtuh.", CODE_FOR_j_24_f_frxx_simple, B_UID(258), NULL };
static struct builtin B15_vec_any_ge = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:15", "vcmpgtuh.", CODE_FOR_j_24_f_frxx_simple, B_UID(259), NULL };
static struct builtin B16_vec_any_ge = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:16", "vcmpgtuw.", CODE_FOR_j_24_f_frxx_simple, B_UID(260), NULL };
static struct builtin B17_vec_any_ge = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:17", "vcmpgtuw.", CODE_FOR_j_24_f_frxx_simple, B_UID(261), NULL };
static struct builtin B18_vec_any_ge = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:18", "vcmpgtub.", CODE_FOR_j_24_f_frxx_simple, B_UID(262), NULL };
static struct builtin B19_vec_any_ge = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_ge:19", "vcmpgtub.", CODE_FOR_j_24_f_frxx_simple, B_UID(263), NULL };
static struct builtin B1_vec_any_gt = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:1", "vcmpgtsh.", CODE_FOR_j_26_f_fxx_simple, B_UID(264), NULL };
static struct builtin B2_vec_any_gt = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:2", "vcmpgtuh.", CODE_FOR_j_26_f_fxx_simple, B_UID(265), NULL };
static struct builtin B3_vec_any_gt = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:3", "vcmpgtsw.", CODE_FOR_j_26_f_fxx_simple, B_UID(266), NULL };
static struct builtin B4_vec_any_gt = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:4", "vcmpgtuw.", CODE_FOR_j_26_f_fxx_simple, B_UID(267), NULL };
static struct builtin B5_vec_any_gt = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:5", "vcmpgtsb.", CODE_FOR_j_26_f_fxx_simple, B_UID(268), NULL };
static struct builtin B6_vec_any_gt = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:6", "vcmpgtub.", CODE_FOR_j_26_f_fxx_simple, B_UID(269), NULL };
static struct builtin B7_vec_any_gt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:7", "vcmpgtfp.", CODE_FOR_j_26_f_fxx_simple, B_UID(270), NULL };
static struct builtin B8_vec_any_gt = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:8", "vcmpgtsh.", CODE_FOR_j_26_f_fxx_simple, B_UID(271), NULL };
static struct builtin B9_vec_any_gt = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:9", "vcmpgtsh.", CODE_FOR_j_26_f_fxx_simple, B_UID(272), NULL };
static struct builtin B10_vec_any_gt = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:10", "vcmpgtsw.", CODE_FOR_j_26_f_fxx_simple, B_UID(273), NULL };
static struct builtin B11_vec_any_gt = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:11", "vcmpgtsw.", CODE_FOR_j_26_f_fxx_simple, B_UID(274), NULL };
static struct builtin B12_vec_any_gt = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:12", "vcmpgtsb.", CODE_FOR_j_26_f_fxx_simple, B_UID(275), NULL };
static struct builtin B13_vec_any_gt = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:13", "vcmpgtsb.", CODE_FOR_j_26_f_fxx_simple, B_UID(276), NULL };
static struct builtin B14_vec_any_gt = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:14", "vcmpgtuh.", CODE_FOR_j_26_f_fxx_simple, B_UID(277), NULL };
static struct builtin B15_vec_any_gt = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:15", "vcmpgtuh.", CODE_FOR_j_26_f_fxx_simple, B_UID(278), NULL };
static struct builtin B16_vec_any_gt = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:16", "vcmpgtuw.", CODE_FOR_j_26_f_fxx_simple, B_UID(279), NULL };
static struct builtin B17_vec_any_gt = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:17", "vcmpgtuw.", CODE_FOR_j_26_f_fxx_simple, B_UID(280), NULL };
static struct builtin B18_vec_any_gt = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:18", "vcmpgtub.", CODE_FOR_j_26_f_fxx_simple, B_UID(281), NULL };
static struct builtin B19_vec_any_gt = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_gt:19", "vcmpgtub.", CODE_FOR_j_26_f_fxx_simple, B_UID(282), NULL };
static struct builtin B1_vec_any_le = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:1", "vcmpgtsh.", CODE_FOR_j_24_f_fxx_simple, B_UID(283), NULL };
static struct builtin B2_vec_any_le = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:2", "vcmpgtuh.", CODE_FOR_j_24_f_fxx_simple, B_UID(284), NULL };
static struct builtin B3_vec_any_le = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:3", "vcmpgtsw.", CODE_FOR_j_24_f_fxx_simple, B_UID(285), NULL };
static struct builtin B4_vec_any_le = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:4", "vcmpgtuw.", CODE_FOR_j_24_f_fxx_simple, B_UID(286), NULL };
static struct builtin B5_vec_any_le = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:5", "vcmpgtsb.", CODE_FOR_j_24_f_fxx_simple, B_UID(287), NULL };
static struct builtin B6_vec_any_le = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:6", "vcmpgtub.", CODE_FOR_j_24_f_fxx_simple, B_UID(288), NULL };
static struct builtin B7_vec_any_le = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_le:7", "vcmpgefp.", CODE_FOR_j_26_f_frxx_simple, B_UID(289), NULL };
static struct builtin B8_vec_any_le = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:8", "vcmpgtsh.", CODE_FOR_j_24_f_fxx_simple, B_UID(290), NULL };
static struct builtin B9_vec_any_le = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:9", "vcmpgtsh.", CODE_FOR_j_24_f_fxx_simple, B_UID(291), NULL };
static struct builtin B10_vec_any_le = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:10", "vcmpgtsw.", CODE_FOR_j_24_f_fxx_simple, B_UID(292), NULL };
static struct builtin B11_vec_any_le = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:11", "vcmpgtsw.", CODE_FOR_j_24_f_fxx_simple, B_UID(293), NULL };
static struct builtin B12_vec_any_le = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:12", "vcmpgtsb.", CODE_FOR_j_24_f_fxx_simple, B_UID(294), NULL };
static struct builtin B13_vec_any_le = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:13", "vcmpgtsb.", CODE_FOR_j_24_f_fxx_simple, B_UID(295), NULL };
static struct builtin B14_vec_any_le = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:14", "vcmpgtuh.", CODE_FOR_j_24_f_fxx_simple, B_UID(296), NULL };
static struct builtin B15_vec_any_le = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:15", "vcmpgtuh.", CODE_FOR_j_24_f_fxx_simple, B_UID(297), NULL };
static struct builtin B16_vec_any_le = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:16", "vcmpgtuw.", CODE_FOR_j_24_f_fxx_simple, B_UID(298), NULL };
static struct builtin B17_vec_any_le = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:17", "vcmpgtuw.", CODE_FOR_j_24_f_fxx_simple, B_UID(299), NULL };
static struct builtin B18_vec_any_le = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:18", "vcmpgtub.", CODE_FOR_j_24_f_fxx_simple, B_UID(300), NULL };
static struct builtin B19_vec_any_le = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_le:19", "vcmpgtub.", CODE_FOR_j_24_f_fxx_simple, B_UID(301), NULL };
static struct builtin B1_vec_any_lt = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:1", "vcmpgtsh.", CODE_FOR_j_26_f_frxx_simple, B_UID(302), NULL };
static struct builtin B2_vec_any_lt = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:2", "vcmpgtuh.", CODE_FOR_j_26_f_frxx_simple, B_UID(303), NULL };
static struct builtin B3_vec_any_lt = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:3", "vcmpgtsw.", CODE_FOR_j_26_f_frxx_simple, B_UID(304), NULL };
static struct builtin B4_vec_any_lt = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:4", "vcmpgtuw.", CODE_FOR_j_26_f_frxx_simple, B_UID(305), NULL };
static struct builtin B5_vec_any_lt = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:5", "vcmpgtsb.", CODE_FOR_j_26_f_frxx_simple, B_UID(306), NULL };
static struct builtin B6_vec_any_lt = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:6", "vcmpgtub.", CODE_FOR_j_26_f_frxx_simple, B_UID(307), NULL };
static struct builtin B7_vec_any_lt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:7", "vcmpgtfp.", CODE_FOR_j_26_f_frxx_simple, B_UID(308), NULL };
static struct builtin B8_vec_any_lt = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:8", "vcmpgtsh.", CODE_FOR_j_26_f_frxx_simple, B_UID(309), NULL };
static struct builtin B9_vec_any_lt = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:9", "vcmpgtsh.", CODE_FOR_j_26_f_frxx_simple, B_UID(310), NULL };
static struct builtin B10_vec_any_lt = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:10", "vcmpgtsw.", CODE_FOR_j_26_f_frxx_simple, B_UID(311), NULL };
static struct builtin B11_vec_any_lt = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:11", "vcmpgtsw.", CODE_FOR_j_26_f_frxx_simple, B_UID(312), NULL };
static struct builtin B12_vec_any_lt = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:12", "vcmpgtsb.", CODE_FOR_j_26_f_frxx_simple, B_UID(313), NULL };
static struct builtin B13_vec_any_lt = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:13", "vcmpgtsb.", CODE_FOR_j_26_f_frxx_simple, B_UID(314), NULL };
static struct builtin B14_vec_any_lt = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:14", "vcmpgtuh.", CODE_FOR_j_26_f_frxx_simple, B_UID(315), NULL };
static struct builtin B15_vec_any_lt = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:15", "vcmpgtuh.", CODE_FOR_j_26_f_frxx_simple, B_UID(316), NULL };
static struct builtin B16_vec_any_lt = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:16", "vcmpgtuw.", CODE_FOR_j_26_f_frxx_simple, B_UID(317), NULL };
static struct builtin B17_vec_any_lt = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:17", "vcmpgtuw.", CODE_FOR_j_26_f_frxx_simple, B_UID(318), NULL };
static struct builtin B18_vec_any_lt = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:18", "vcmpgtub.", CODE_FOR_j_26_f_frxx_simple, B_UID(319), NULL };
static struct builtin B19_vec_any_lt = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc26fr, 2, FALSE, FALSE, 0, "vec_any_lt:19", "vcmpgtub.", CODE_FOR_j_26_f_frxx_simple, B_UID(320), NULL };
static struct builtin B_vec_any_nan = { { &T_vec_f32, NULL, NULL, }, "x", &T_cc24fd, 1, FALSE, FALSE, 0, "vec_any_nan", "vcmpeqfp.", CODE_FOR_j_24_f_fx_simple, B_UID(321), NULL };
static struct builtin B1_vec_any_ne = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:1", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(322), NULL };
static struct builtin B2_vec_any_ne = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:2", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(323), NULL };
static struct builtin B3_vec_any_ne = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:3", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(324), NULL };
static struct builtin B4_vec_any_ne = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:4", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(325), NULL };
static struct builtin B5_vec_any_ne = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:5", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(326), NULL };
static struct builtin B6_vec_any_ne = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:6", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(327), NULL };
static struct builtin B7_vec_any_ne = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:7", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(328), NULL };
static struct builtin B8_vec_any_ne = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:8", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(329), NULL };
static struct builtin B9_vec_any_ne = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:9", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(330), NULL };
static struct builtin B10_vec_any_ne = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:10", "vcmpeqfp.", CODE_FOR_j_24_f_fxx_simple, B_UID(331), NULL };
static struct builtin B11_vec_any_ne = { { &T_vec_p16, &T_vec_p16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:11", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(332), NULL };
static struct builtin B12_vec_any_ne = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:12", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(333), NULL };
static struct builtin B13_vec_any_ne = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:13", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(334), NULL };
static struct builtin B14_vec_any_ne = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:14", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(335), NULL };
static struct builtin B15_vec_any_ne = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:15", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(336), NULL };
static struct builtin B16_vec_any_ne = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:16", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(337), NULL };
static struct builtin B17_vec_any_ne = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:17", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(338), NULL };
static struct builtin B18_vec_any_ne = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:18", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(339), NULL };
static struct builtin B19_vec_any_ne = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:19", "vcmpequh.", CODE_FOR_j_24_f_fxx_simple, B_UID(340), NULL };
static struct builtin B20_vec_any_ne = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:20", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(341), NULL };
static struct builtin B21_vec_any_ne = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:21", "vcmpequw.", CODE_FOR_j_24_f_fxx_simple, B_UID(342), NULL };
static struct builtin B22_vec_any_ne = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:22", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(343), NULL };
static struct builtin B23_vec_any_ne = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ne:23", "vcmpequb.", CODE_FOR_j_24_f_fxx_simple, B_UID(344), NULL };
static struct builtin B_vec_any_nge = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_nge", "vcmpgefp.", CODE_FOR_j_24_f_fxx_simple, B_UID(345), NULL };
static struct builtin B_vec_any_ngt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24f, 2, FALSE, FALSE, 0, "vec_any_ngt", "vcmpgtfp.", CODE_FOR_j_24_f_fxx_simple, B_UID(346), NULL };
static struct builtin B_vec_any_nle = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_nle", "vcmpgefp.", CODE_FOR_j_24_f_frxx_simple, B_UID(347), NULL };
static struct builtin B_vec_any_nlt = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc24fr, 2, FALSE, FALSE, 0, "vec_any_nlt", "vcmpgtfp.", CODE_FOR_j_24_f_frxx_simple, B_UID(348), NULL };
static struct builtin B_vec_any_numeric = { { &T_vec_f32, NULL, NULL, }, "x", &T_cc26fd, 1, FALSE, FALSE, 0, "vec_any_numeric", "vcmpeqfp.", CODE_FOR_j_26_f_fx_simple, B_UID(349), NULL };
static struct builtin B_vec_any_out = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_cc26f, 2, FALSE, FALSE, 0, "vec_any_out", "vcmpbfp.", CODE_FOR_j_26_f_fxx_simple, B_UID(350), NULL };
static struct builtin B_vec_vavgsh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vavgsh", "vavgsh", CODE_FOR_xfxx_simple, B_UID(351), NULL };
static struct builtin B_vec_vavgsw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vavgsw", "vavgsw", CODE_FOR_xfxx_simple, B_UID(352), NULL };
static struct builtin B_vec_vavgsb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vavgsb", "vavgsb", CODE_FOR_xfxx_simple, B_UID(353), NULL };
static struct builtin B_vec_vavguh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vavguh", "vavguh", CODE_FOR_xfxx_simple, B_UID(354), NULL };
static struct builtin B_vec_vavguw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vavguw", "vavguw", CODE_FOR_xfxx_simple, B_UID(355), NULL };
static struct builtin B_vec_vavgub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vavgub", "vavgub", CODE_FOR_xfxx_simple, B_UID(356), NULL };
static struct builtin B_vec_vrfip = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vrfip", "vrfip", CODE_FOR_xfx_fp, B_UID(357), NULL };
static struct builtin B_vec_vcmpbfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vcmpbfp", "vcmpbfp", CODE_FOR_xfxx_simple, B_UID(358), NULL };
static struct builtin B_vec_vcmpeqfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 7, "vec_vcmpeqfp", "vcmpeqfp", CODE_FOR_xfxx_simple, B_UID(359), NULL };
static struct builtin B1_vec_vcmpequh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 7, "vec_vcmpequh:1", "vcmpequh", CODE_FOR_xfxx_simple, B_UID(360), NULL };
static struct builtin B1_vec_vcmpequw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 7, "vec_vcmpequw:1", "vcmpequw", CODE_FOR_xfxx_simple, B_UID(361), NULL };
static struct builtin B1_vec_vcmpequb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 7, "vec_vcmpequb:1", "vcmpequb", CODE_FOR_xfxx_simple, B_UID(362), NULL };
static struct builtin B2_vec_vcmpequh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 7, "vec_vcmpequh:2", "vcmpequh", CODE_FOR_xfxx_simple, B_UID(363), NULL };
static struct builtin B2_vec_vcmpequw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 7, "vec_vcmpequw:2", "vcmpequw", CODE_FOR_xfxx_simple, B_UID(364), NULL };
static struct builtin B2_vec_vcmpequb = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 7, "vec_vcmpequb:2", "vcmpequb", CODE_FOR_xfxx_simple, B_UID(365), NULL };
static struct builtin B_vec_vcmpgefp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpgefp", "vcmpgefp", CODE_FOR_xfxx_simple, B_UID(366), NULL };
static struct builtin B_vec_vcmpgtfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpgtfp", "vcmpgtfp", CODE_FOR_xfxx_simple, B_UID(367), NULL };
static struct builtin B_vec_vcmpgtsh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vcmpgtsh", "vcmpgtsh", CODE_FOR_xfxx_simple, B_UID(368), NULL };
static struct builtin B_vec_vcmpgtsw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpgtsw", "vcmpgtsw", CODE_FOR_xfxx_simple, B_UID(369), NULL };
static struct builtin B_vec_vcmpgtsb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vcmpgtsb", "vcmpgtsb", CODE_FOR_xfxx_simple, B_UID(370), NULL };
static struct builtin B_vec_vcmpgtuh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vcmpgtuh", "vcmpgtuh", CODE_FOR_xfxx_simple, B_UID(371), NULL };
static struct builtin B_vec_vcmpgtuw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpgtuw", "vcmpgtuw", CODE_FOR_xfxx_simple, B_UID(372), NULL };
static struct builtin B_vec_vcmpgtub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vcmpgtub", "vcmpgtub", CODE_FOR_xfxx_simple, B_UID(373), NULL };
static struct builtin B_vec_vcmplefp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmplefp", "vcmplefp", CODE_FOR_xfxx_simple, B_UID(374), NULL };
static struct builtin B_vec_vcmpltfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpltfp", "vcmpltfp", CODE_FOR_xfxx_simple, B_UID(375), NULL };
static struct builtin B_vec_vcmpltsh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vcmpltsh", "vcmpltsh", CODE_FOR_xfxx_simple, B_UID(376), NULL };
static struct builtin B_vec_vcmpltsw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpltsw", "vcmpltsw", CODE_FOR_xfxx_simple, B_UID(377), NULL };
static struct builtin B_vec_vcmpltsb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vcmpltsb", "vcmpltsb", CODE_FOR_xfxx_simple, B_UID(378), NULL };
static struct builtin B_vec_vcmpltuh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vcmpltuh", "vcmpltuh", CODE_FOR_xfxx_simple, B_UID(379), NULL };
static struct builtin B_vec_vcmpltuw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vcmpltuw", "vcmpltuw", CODE_FOR_xfxx_simple, B_UID(380), NULL };
static struct builtin B_vec_vcmpltub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vcmpltub", "vcmpltub", CODE_FOR_xfxx_simple, B_UID(381), NULL };
static struct builtin B_vec_vcfsx = { { &T_vec_s32, &T_immed_u5, NULL, }, "xB", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vcfsx", "vcfsx", CODE_FOR_xfxB_fp, B_UID(382), NULL };
static struct builtin B_vec_vcfux = { { &T_vec_u32, &T_immed_u5, NULL, }, "xB", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vcfux", "vcfux", CODE_FOR_xfxB_fp, B_UID(383), NULL };
static struct builtin B_vec_vctsxs = { { &T_vec_f32, &T_immed_u5, NULL, }, "xB", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vctsxs", "vctsxs", CODE_FOR_xfxB_fp, B_UID(384), NULL };
static struct builtin B_vec_vctuxs = { { &T_vec_f32, &T_immed_u5, NULL, }, "xB", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vctuxs", "vctuxs", CODE_FOR_xfxB_fp, B_UID(385), NULL };
static struct builtin B_vec_dss = { { &T_immed_u2, NULL, NULL, }, "D", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_dss", "dss", CODE_FOR_vfD_load, B_UID(386), NULL };
static struct builtin B_vec_dssall = { { NULL, NULL, NULL, }, "", &T_volatile_void, 0, FALSE, FALSE, 0, "vec_dssall", "dssall", CODE_FOR_vf_load, B_UID(387), NULL };
static struct builtin B1_vec_dst = { { &T_float_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:1", "dst", CODE_FOR_vlfiiD_load, B_UID(388), NULL };
static struct builtin B2_vec_dst = { { &T_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:2", "dst", CODE_FOR_vlfiiD_load, B_UID(389), NULL };
static struct builtin B3_vec_dst = { { &T_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:3", "dst", CODE_FOR_vlfiiD_load, B_UID(390), NULL };
static struct builtin B4_vec_dst = { { &T_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:4", "dst", CODE_FOR_vlfiiD_load, B_UID(391), NULL };
static struct builtin B5_vec_dst = { { &T_signed_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:5", "dst", CODE_FOR_vlfiiD_load, B_UID(392), NULL };
static struct builtin B6_vec_dst = { { &T_unsigned_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:6", "dst", CODE_FOR_vlfiiD_load, B_UID(393), NULL };
static struct builtin B7_vec_dst = { { &T_unsigned_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:7", "dst", CODE_FOR_vlfiiD_load, B_UID(394), NULL };
static struct builtin B8_vec_dst = { { &T_unsigned_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:8", "dst", CODE_FOR_vlfiiD_load, B_UID(395), NULL };
static struct builtin B9_vec_dst = { { &T_unsigned_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:9", "dst", CODE_FOR_vlfiiD_load, B_UID(396), NULL };
static struct builtin B10_vec_dst = { { &T_vec_b16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:10", "dst", CODE_FOR_vlfiiD_load, B_UID(397), NULL };
static struct builtin B11_vec_dst = { { &T_vec_b32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:11", "dst", CODE_FOR_vlfiiD_load, B_UID(398), NULL };
static struct builtin B12_vec_dst = { { &T_vec_b8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:12", "dst", CODE_FOR_vlfiiD_load, B_UID(399), NULL };
static struct builtin B13_vec_dst = { { &T_vec_f32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:13", "dst", CODE_FOR_vlfiiD_load, B_UID(400), NULL };
static struct builtin B14_vec_dst = { { &T_vec_p16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:14", "dst", CODE_FOR_vlfiiD_load, B_UID(401), NULL };
static struct builtin B15_vec_dst = { { &T_vec_s16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:15", "dst", CODE_FOR_vlfiiD_load, B_UID(402), NULL };
static struct builtin B16_vec_dst = { { &T_vec_s32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:16", "dst", CODE_FOR_vlfiiD_load, B_UID(403), NULL };
static struct builtin B17_vec_dst = { { &T_vec_s8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:17", "dst", CODE_FOR_vlfiiD_load, B_UID(404), NULL };
static struct builtin B18_vec_dst = { { &T_vec_u16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:18", "dst", CODE_FOR_vlfiiD_load, B_UID(405), NULL };
static struct builtin B19_vec_dst = { { &T_vec_u32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:19", "dst", CODE_FOR_vlfiiD_load, B_UID(406), NULL };
static struct builtin B20_vec_dst = { { &T_vec_u8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dst:20", "dst", CODE_FOR_vlfiiD_load, B_UID(407), NULL };
static struct builtin B1_vec_dstst = { { &T_float_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:1", "dstst", CODE_FOR_vlfiiD_load, B_UID(408), NULL };
static struct builtin B2_vec_dstst = { { &T_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:2", "dstst", CODE_FOR_vlfiiD_load, B_UID(409), NULL };
static struct builtin B3_vec_dstst = { { &T_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:3", "dstst", CODE_FOR_vlfiiD_load, B_UID(410), NULL };
static struct builtin B4_vec_dstst = { { &T_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:4", "dstst", CODE_FOR_vlfiiD_load, B_UID(411), NULL };
static struct builtin B5_vec_dstst = { { &T_signed_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:5", "dstst", CODE_FOR_vlfiiD_load, B_UID(412), NULL };
static struct builtin B6_vec_dstst = { { &T_unsigned_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:6", "dstst", CODE_FOR_vlfiiD_load, B_UID(413), NULL };
static struct builtin B7_vec_dstst = { { &T_unsigned_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:7", "dstst", CODE_FOR_vlfiiD_load, B_UID(414), NULL };
static struct builtin B8_vec_dstst = { { &T_unsigned_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:8", "dstst", CODE_FOR_vlfiiD_load, B_UID(415), NULL };
static struct builtin B9_vec_dstst = { { &T_unsigned_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:9", "dstst", CODE_FOR_vlfiiD_load, B_UID(416), NULL };
static struct builtin B10_vec_dstst = { { &T_vec_b16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:10", "dstst", CODE_FOR_vlfiiD_load, B_UID(417), NULL };
static struct builtin B11_vec_dstst = { { &T_vec_b32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:11", "dstst", CODE_FOR_vlfiiD_load, B_UID(418), NULL };
static struct builtin B12_vec_dstst = { { &T_vec_b8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:12", "dstst", CODE_FOR_vlfiiD_load, B_UID(419), NULL };
static struct builtin B13_vec_dstst = { { &T_vec_f32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:13", "dstst", CODE_FOR_vlfiiD_load, B_UID(420), NULL };
static struct builtin B14_vec_dstst = { { &T_vec_p16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:14", "dstst", CODE_FOR_vlfiiD_load, B_UID(421), NULL };
static struct builtin B15_vec_dstst = { { &T_vec_s16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:15", "dstst", CODE_FOR_vlfiiD_load, B_UID(422), NULL };
static struct builtin B16_vec_dstst = { { &T_vec_s32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:16", "dstst", CODE_FOR_vlfiiD_load, B_UID(423), NULL };
static struct builtin B17_vec_dstst = { { &T_vec_s8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:17", "dstst", CODE_FOR_vlfiiD_load, B_UID(424), NULL };
static struct builtin B18_vec_dstst = { { &T_vec_u16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:18", "dstst", CODE_FOR_vlfiiD_load, B_UID(425), NULL };
static struct builtin B19_vec_dstst = { { &T_vec_u32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:19", "dstst", CODE_FOR_vlfiiD_load, B_UID(426), NULL };
static struct builtin B20_vec_dstst = { { &T_vec_u8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstst:20", "dstst", CODE_FOR_vlfiiD_load, B_UID(427), NULL };
static struct builtin B1_vec_dststt = { { &T_float_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:1", "dststt", CODE_FOR_vlfiiD_load, B_UID(428), NULL };
static struct builtin B2_vec_dststt = { { &T_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:2", "dststt", CODE_FOR_vlfiiD_load, B_UID(429), NULL };
static struct builtin B3_vec_dststt = { { &T_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:3", "dststt", CODE_FOR_vlfiiD_load, B_UID(430), NULL };
static struct builtin B4_vec_dststt = { { &T_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:4", "dststt", CODE_FOR_vlfiiD_load, B_UID(431), NULL };
static struct builtin B5_vec_dststt = { { &T_signed_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:5", "dststt", CODE_FOR_vlfiiD_load, B_UID(432), NULL };
static struct builtin B6_vec_dststt = { { &T_unsigned_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:6", "dststt", CODE_FOR_vlfiiD_load, B_UID(433), NULL };
static struct builtin B7_vec_dststt = { { &T_unsigned_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:7", "dststt", CODE_FOR_vlfiiD_load, B_UID(434), NULL };
static struct builtin B8_vec_dststt = { { &T_unsigned_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:8", "dststt", CODE_FOR_vlfiiD_load, B_UID(435), NULL };
static struct builtin B9_vec_dststt = { { &T_unsigned_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:9", "dststt", CODE_FOR_vlfiiD_load, B_UID(436), NULL };
static struct builtin B10_vec_dststt = { { &T_vec_b16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:10", "dststt", CODE_FOR_vlfiiD_load, B_UID(437), NULL };
static struct builtin B11_vec_dststt = { { &T_vec_b32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:11", "dststt", CODE_FOR_vlfiiD_load, B_UID(438), NULL };
static struct builtin B12_vec_dststt = { { &T_vec_b8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:12", "dststt", CODE_FOR_vlfiiD_load, B_UID(439), NULL };
static struct builtin B13_vec_dststt = { { &T_vec_f32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:13", "dststt", CODE_FOR_vlfiiD_load, B_UID(440), NULL };
static struct builtin B14_vec_dststt = { { &T_vec_p16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:14", "dststt", CODE_FOR_vlfiiD_load, B_UID(441), NULL };
static struct builtin B15_vec_dststt = { { &T_vec_s16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:15", "dststt", CODE_FOR_vlfiiD_load, B_UID(442), NULL };
static struct builtin B16_vec_dststt = { { &T_vec_s32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:16", "dststt", CODE_FOR_vlfiiD_load, B_UID(443), NULL };
static struct builtin B17_vec_dststt = { { &T_vec_s8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:17", "dststt", CODE_FOR_vlfiiD_load, B_UID(444), NULL };
static struct builtin B18_vec_dststt = { { &T_vec_u16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:18", "dststt", CODE_FOR_vlfiiD_load, B_UID(445), NULL };
static struct builtin B19_vec_dststt = { { &T_vec_u32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:19", "dststt", CODE_FOR_vlfiiD_load, B_UID(446), NULL };
static struct builtin B20_vec_dststt = { { &T_vec_u8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dststt:20", "dststt", CODE_FOR_vlfiiD_load, B_UID(447), NULL };
static struct builtin B1_vec_dstt = { { &T_float_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:1", "dstt", CODE_FOR_vlfiiD_load, B_UID(448), NULL };
static struct builtin B2_vec_dstt = { { &T_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:2", "dstt", CODE_FOR_vlfiiD_load, B_UID(449), NULL };
static struct builtin B3_vec_dstt = { { &T_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:3", "dstt", CODE_FOR_vlfiiD_load, B_UID(450), NULL };
static struct builtin B4_vec_dstt = { { &T_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:4", "dstt", CODE_FOR_vlfiiD_load, B_UID(451), NULL };
static struct builtin B5_vec_dstt = { { &T_signed_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:5", "dstt", CODE_FOR_vlfiiD_load, B_UID(452), NULL };
static struct builtin B6_vec_dstt = { { &T_unsigned_char_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:6", "dstt", CODE_FOR_vlfiiD_load, B_UID(453), NULL };
static struct builtin B7_vec_dstt = { { &T_unsigned_int_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:7", "dstt", CODE_FOR_vlfiiD_load, B_UID(454), NULL };
static struct builtin B8_vec_dstt = { { &T_unsigned_long_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:8", "dstt", CODE_FOR_vlfiiD_load, B_UID(455), NULL };
static struct builtin B9_vec_dstt = { { &T_unsigned_short_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:9", "dstt", CODE_FOR_vlfiiD_load, B_UID(456), NULL };
static struct builtin B10_vec_dstt = { { &T_vec_b16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:10", "dstt", CODE_FOR_vlfiiD_load, B_UID(457), NULL };
static struct builtin B11_vec_dstt = { { &T_vec_b32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:11", "dstt", CODE_FOR_vlfiiD_load, B_UID(458), NULL };
static struct builtin B12_vec_dstt = { { &T_vec_b8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:12", "dstt", CODE_FOR_vlfiiD_load, B_UID(459), NULL };
static struct builtin B13_vec_dstt = { { &T_vec_f32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:13", "dstt", CODE_FOR_vlfiiD_load, B_UID(460), NULL };
static struct builtin B14_vec_dstt = { { &T_vec_p16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:14", "dstt", CODE_FOR_vlfiiD_load, B_UID(461), NULL };
static struct builtin B15_vec_dstt = { { &T_vec_s16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:15", "dstt", CODE_FOR_vlfiiD_load, B_UID(462), NULL };
static struct builtin B16_vec_dstt = { { &T_vec_s32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:16", "dstt", CODE_FOR_vlfiiD_load, B_UID(463), NULL };
static struct builtin B17_vec_dstt = { { &T_vec_s8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:17", "dstt", CODE_FOR_vlfiiD_load, B_UID(464), NULL };
static struct builtin B18_vec_dstt = { { &T_vec_u16_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:18", "dstt", CODE_FOR_vlfiiD_load, B_UID(465), NULL };
static struct builtin B19_vec_dstt = { { &T_vec_u32_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:19", "dstt", CODE_FOR_vlfiiD_load, B_UID(466), NULL };
static struct builtin B20_vec_dstt = { { &T_vec_u8_ptr, &T_int, &T_immed_u2, }, "iiD", &T_volatile_void, 3, TRUE, FALSE, 0, "vec_dstt:20", "dstt", CODE_FOR_vlfiiD_load, B_UID(467), NULL };
static struct builtin B_vec_vexptefp = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vexptefp", "vexptefp", CODE_FOR_xfx_fp, B_UID(468), NULL };
static struct builtin B_vec_vrfim = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vrfim", "vrfim", CODE_FOR_xfx_fp, B_UID(469), NULL };
static struct builtin B1_vec_lvx = { { &T_int, &T_float_ptr, NULL, }, "ii", &T_vec_f32, 2, TRUE, FALSE, 0, "vec_lvx:1", "lvx", CODE_FOR_xlfii_load, B_UID(470), NULL };
static struct builtin B2_vec_lvx = { { &T_int, &T_int_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvx:2", "lvx", CODE_FOR_xlfii_load, B_UID(471), NULL };
static struct builtin B3_vec_lvx = { { &T_int, &T_long_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvx:3", "lvx", CODE_FOR_xlfii_load, B_UID(472), NULL };
static struct builtin B4_vec_lvx = { { &T_int, &T_short_ptr, NULL, }, "ii", &T_vec_s16, 2, TRUE, FALSE, 0, "vec_lvx:4", "lvx", CODE_FOR_xlfii_load, B_UID(473), NULL };
static struct builtin B5_vec_lvx = { { &T_int, &T_signed_char_ptr, NULL, }, "ii", &T_vec_s8, 2, TRUE, FALSE, 0, "vec_lvx:5", "lvx", CODE_FOR_xlfii_load, B_UID(474), NULL };
static struct builtin B6_vec_lvx = { { &T_int, &T_unsigned_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, FALSE, 0, "vec_lvx:6", "lvx", CODE_FOR_xlfii_load, B_UID(475), NULL };
static struct builtin B7_vec_lvx = { { &T_int, &T_unsigned_int_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvx:7", "lvx", CODE_FOR_xlfii_load, B_UID(476), NULL };
static struct builtin B8_vec_lvx = { { &T_int, &T_unsigned_long_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvx:8", "lvx", CODE_FOR_xlfii_load, B_UID(477), NULL };
static struct builtin B9_vec_lvx = { { &T_int, &T_unsigned_short_ptr, NULL, }, "ii", &T_vec_u16, 2, TRUE, FALSE, 0, "vec_lvx:9", "lvx", CODE_FOR_xlfii_load, B_UID(478), NULL };
static struct builtin B10_vec_lvx = { { &T_int, &T_vec_b16_ptr, NULL, }, "ii", &T_vec_b16, 2, TRUE, FALSE, 0, "vec_lvx:10", "lvx", CODE_FOR_xlfii_load, B_UID(479), NULL };
static struct builtin B11_vec_lvx = { { &T_int, &T_vec_b32_ptr, NULL, }, "ii", &T_vec_b32, 2, TRUE, FALSE, 0, "vec_lvx:11", "lvx", CODE_FOR_xlfii_load, B_UID(480), NULL };
static struct builtin B12_vec_lvx = { { &T_int, &T_vec_b8_ptr, NULL, }, "ii", &T_vec_b8, 2, TRUE, FALSE, 0, "vec_lvx:12", "lvx", CODE_FOR_xlfii_load, B_UID(481), NULL };
static struct builtin B13_vec_lvx = { { &T_int, &T_vec_f32_ptr, NULL, }, "ii", &T_vec_f32, 2, TRUE, FALSE, 0, "vec_lvx:13", "lvx", CODE_FOR_xlfii_load, B_UID(482), NULL };
static struct builtin B14_vec_lvx = { { &T_int, &T_vec_p16_ptr, NULL, }, "ii", &T_vec_p16, 2, TRUE, FALSE, 0, "vec_lvx:14", "lvx", CODE_FOR_xlfii_load, B_UID(483), NULL };
static struct builtin B15_vec_lvx = { { &T_int, &T_vec_s16_ptr, NULL, }, "ii", &T_vec_s16, 2, TRUE, FALSE, 0, "vec_lvx:15", "lvx", CODE_FOR_xlfii_load, B_UID(484), NULL };
static struct builtin B16_vec_lvx = { { &T_int, &T_vec_s32_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvx:16", "lvx", CODE_FOR_xlfii_load, B_UID(485), NULL };
static struct builtin B17_vec_lvx = { { &T_int, &T_vec_s8_ptr, NULL, }, "ii", &T_vec_s8, 2, TRUE, FALSE, 0, "vec_lvx:17", "lvx", CODE_FOR_xlfii_load, B_UID(486), NULL };
static struct builtin B18_vec_lvx = { { &T_int, &T_vec_u16_ptr, NULL, }, "ii", &T_vec_u16, 2, TRUE, FALSE, 0, "vec_lvx:18", "lvx", CODE_FOR_xlfii_load, B_UID(487), NULL };
static struct builtin B19_vec_lvx = { { &T_int, &T_vec_u32_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvx:19", "lvx", CODE_FOR_xlfii_load, B_UID(488), NULL };
static struct builtin B20_vec_lvx = { { &T_int, &T_vec_u8_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, FALSE, 0, "vec_lvx:20", "lvx", CODE_FOR_xlfii_load, B_UID(489), NULL };
static struct builtin B1_vec_lvewx = { { &T_int, &T_float_ptr, NULL, }, "ii", &T_vec_f32, 2, TRUE, FALSE, 0, "vec_lvewx:1", "lvewx", CODE_FOR_xlfii_load, B_UID(490), NULL };
static struct builtin B2_vec_lvewx = { { &T_int, &T_int_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvewx:2", "lvewx", CODE_FOR_xlfii_load, B_UID(491), NULL };
static struct builtin B3_vec_lvewx = { { &T_int, &T_long_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvewx:3", "lvewx", CODE_FOR_xlfii_load, B_UID(492), NULL };
static struct builtin B1_vec_lvehx = { { &T_int, &T_short_ptr, NULL, }, "ii", &T_vec_s16, 2, TRUE, FALSE, 0, "vec_lvehx:1", "lvehx", CODE_FOR_xlfii_load, B_UID(493), NULL };
static struct builtin B1_vec_lvebx = { { &T_int, &T_signed_char_ptr, NULL, }, "ii", &T_vec_s8, 2, TRUE, FALSE, 0, "vec_lvebx:1", "lvebx", CODE_FOR_xlfii_load, B_UID(494), NULL };
static struct builtin B2_vec_lvebx = { { &T_int, &T_unsigned_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, FALSE, 0, "vec_lvebx:2", "lvebx", CODE_FOR_xlfii_load, B_UID(495), NULL };
static struct builtin B4_vec_lvewx = { { &T_int, &T_unsigned_int_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvewx:4", "lvewx", CODE_FOR_xlfii_load, B_UID(496), NULL };
static struct builtin B5_vec_lvewx = { { &T_int, &T_unsigned_long_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvewx:5", "lvewx", CODE_FOR_xlfii_load, B_UID(497), NULL };
static struct builtin B2_vec_lvehx = { { &T_int, &T_unsigned_short_ptr, NULL, }, "ii", &T_vec_u16, 2, TRUE, FALSE, 0, "vec_lvehx:2", "lvehx", CODE_FOR_xlfii_load, B_UID(498), NULL };
static struct builtin B1_vec_lvxl = { { &T_int, &T_float_ptr, NULL, }, "ii", &T_vec_f32, 2, TRUE, FALSE, 0, "vec_lvxl:1", "lvxl", CODE_FOR_xlfii_load, B_UID(499), NULL };
static struct builtin B2_vec_lvxl = { { &T_int, &T_int_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvxl:2", "lvxl", CODE_FOR_xlfii_load, B_UID(500), NULL };
static struct builtin B3_vec_lvxl = { { &T_int, &T_long_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvxl:3", "lvxl", CODE_FOR_xlfii_load, B_UID(501), NULL };
static struct builtin B4_vec_lvxl = { { &T_int, &T_short_ptr, NULL, }, "ii", &T_vec_s16, 2, TRUE, FALSE, 0, "vec_lvxl:4", "lvxl", CODE_FOR_xlfii_load, B_UID(502), NULL };
static struct builtin B5_vec_lvxl = { { &T_int, &T_signed_char_ptr, NULL, }, "ii", &T_vec_s8, 2, TRUE, FALSE, 0, "vec_lvxl:5", "lvxl", CODE_FOR_xlfii_load, B_UID(503), NULL };
static struct builtin B6_vec_lvxl = { { &T_int, &T_unsigned_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, FALSE, 0, "vec_lvxl:6", "lvxl", CODE_FOR_xlfii_load, B_UID(504), NULL };
static struct builtin B7_vec_lvxl = { { &T_int, &T_unsigned_int_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvxl:7", "lvxl", CODE_FOR_xlfii_load, B_UID(505), NULL };
static struct builtin B8_vec_lvxl = { { &T_int, &T_unsigned_long_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvxl:8", "lvxl", CODE_FOR_xlfii_load, B_UID(506), NULL };
static struct builtin B9_vec_lvxl = { { &T_int, &T_unsigned_short_ptr, NULL, }, "ii", &T_vec_u16, 2, TRUE, FALSE, 0, "vec_lvxl:9", "lvxl", CODE_FOR_xlfii_load, B_UID(507), NULL };
static struct builtin B10_vec_lvxl = { { &T_int, &T_vec_b16_ptr, NULL, }, "ii", &T_vec_b16, 2, TRUE, FALSE, 0, "vec_lvxl:10", "lvxl", CODE_FOR_xlfii_load, B_UID(508), NULL };
static struct builtin B11_vec_lvxl = { { &T_int, &T_vec_b32_ptr, NULL, }, "ii", &T_vec_b32, 2, TRUE, FALSE, 0, "vec_lvxl:11", "lvxl", CODE_FOR_xlfii_load, B_UID(509), NULL };
static struct builtin B12_vec_lvxl = { { &T_int, &T_vec_b8_ptr, NULL, }, "ii", &T_vec_b8, 2, TRUE, FALSE, 0, "vec_lvxl:12", "lvxl", CODE_FOR_xlfii_load, B_UID(510), NULL };
static struct builtin B13_vec_lvxl = { { &T_int, &T_vec_f32_ptr, NULL, }, "ii", &T_vec_f32, 2, TRUE, FALSE, 0, "vec_lvxl:13", "lvxl", CODE_FOR_xlfii_load, B_UID(511), NULL };
static struct builtin B14_vec_lvxl = { { &T_int, &T_vec_p16_ptr, NULL, }, "ii", &T_vec_p16, 2, TRUE, FALSE, 0, "vec_lvxl:14", "lvxl", CODE_FOR_xlfii_load, B_UID(512), NULL };
static struct builtin B15_vec_lvxl = { { &T_int, &T_vec_s16_ptr, NULL, }, "ii", &T_vec_s16, 2, TRUE, FALSE, 0, "vec_lvxl:15", "lvxl", CODE_FOR_xlfii_load, B_UID(513), NULL };
static struct builtin B16_vec_lvxl = { { &T_int, &T_vec_s32_ptr, NULL, }, "ii", &T_vec_s32, 2, TRUE, FALSE, 0, "vec_lvxl:16", "lvxl", CODE_FOR_xlfii_load, B_UID(514), NULL };
static struct builtin B17_vec_lvxl = { { &T_int, &T_vec_s8_ptr, NULL, }, "ii", &T_vec_s8, 2, TRUE, FALSE, 0, "vec_lvxl:17", "lvxl", CODE_FOR_xlfii_load, B_UID(515), NULL };
static struct builtin B18_vec_lvxl = { { &T_int, &T_vec_u16_ptr, NULL, }, "ii", &T_vec_u16, 2, TRUE, FALSE, 0, "vec_lvxl:18", "lvxl", CODE_FOR_xlfii_load, B_UID(516), NULL };
static struct builtin B19_vec_lvxl = { { &T_int, &T_vec_u32_ptr, NULL, }, "ii", &T_vec_u32, 2, TRUE, FALSE, 0, "vec_lvxl:19", "lvxl", CODE_FOR_xlfii_load, B_UID(517), NULL };
static struct builtin B20_vec_lvxl = { { &T_int, &T_vec_u8_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, FALSE, 0, "vec_lvxl:20", "lvxl", CODE_FOR_xlfii_load, B_UID(518), NULL };
static struct builtin B_vec_vlogefp = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vlogefp", "vlogefp", CODE_FOR_xfx_fp, B_UID(519), NULL };
static struct builtin B1_vec_lvsl = { { &T_int, &T_float_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:1", "lvsl", CODE_FOR_xfii_load, B_UID(520), NULL };
static struct builtin B2_vec_lvsl = { { &T_int, &T_int_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:2", "lvsl", CODE_FOR_xfii_load, B_UID(521), NULL };
static struct builtin B3_vec_lvsl = { { &T_int, &T_long_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:3", "lvsl", CODE_FOR_xfii_load, B_UID(522), NULL };
static struct builtin B4_vec_lvsl = { { &T_int, &T_short_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:4", "lvsl", CODE_FOR_xfii_load, B_UID(523), NULL };
static struct builtin B5_vec_lvsl = { { &T_int, &T_signed_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:5", "lvsl", CODE_FOR_xfii_load, B_UID(524), NULL };
static struct builtin B6_vec_lvsl = { { &T_int, &T_unsigned_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:6", "lvsl", CODE_FOR_xfii_load, B_UID(525), NULL };
static struct builtin B7_vec_lvsl = { { &T_int, &T_unsigned_int_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:7", "lvsl", CODE_FOR_xfii_load, B_UID(526), NULL };
static struct builtin B8_vec_lvsl = { { &T_int, &T_unsigned_long_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:8", "lvsl", CODE_FOR_xfii_load, B_UID(527), NULL };
static struct builtin B9_vec_lvsl = { { &T_int, &T_unsigned_short_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 8, "vec_lvsl:9", "lvsl", CODE_FOR_xfii_load, B_UID(528), NULL };
static struct builtin B1_vec_lvsr = { { &T_int, &T_float_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:1", "lvsr", CODE_FOR_xfii_load, B_UID(529), NULL };
static struct builtin B2_vec_lvsr = { { &T_int, &T_int_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:2", "lvsr", CODE_FOR_xfii_load, B_UID(530), NULL };
static struct builtin B3_vec_lvsr = { { &T_int, &T_long_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:3", "lvsr", CODE_FOR_xfii_load, B_UID(531), NULL };
static struct builtin B4_vec_lvsr = { { &T_int, &T_short_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:4", "lvsr", CODE_FOR_xfii_load, B_UID(532), NULL };
static struct builtin B5_vec_lvsr = { { &T_int, &T_signed_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:5", "lvsr", CODE_FOR_xfii_load, B_UID(533), NULL };
static struct builtin B6_vec_lvsr = { { &T_int, &T_unsigned_char_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:6", "lvsr", CODE_FOR_xfii_load, B_UID(534), NULL };
static struct builtin B7_vec_lvsr = { { &T_int, &T_unsigned_int_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:7", "lvsr", CODE_FOR_xfii_load, B_UID(535), NULL };
static struct builtin B8_vec_lvsr = { { &T_int, &T_unsigned_long_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:8", "lvsr", CODE_FOR_xfii_load, B_UID(536), NULL };
static struct builtin B9_vec_lvsr = { { &T_int, &T_unsigned_short_ptr, NULL, }, "ii", &T_vec_u8, 2, TRUE, TRUE, 9, "vec_lvsr:9", "lvsr", CODE_FOR_xfii_load, B_UID(537), NULL };
static struct builtin B_vec_vmaddfp = { { &T_vec_f32, &T_vec_f32, &T_vec_f32, }, "xxx", &T_vec_f32, 3, FALSE, FALSE, 0, "vec_vmaddfp", "vmaddfp", CODE_FOR_xfxxx_fp, B_UID(538), NULL };
static struct builtin B_vec_vmhaddshs = { { &T_vec_s16, &T_vec_s16, &T_vec_s16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vmhaddshs", "vmhaddshs", CODE_FOR_xfxxx_complex, B_UID(539), NULL };
static struct builtin B1_vec_vmaxsh = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vmaxsh:1", "vmaxsh", CODE_FOR_xfxx_simple, B_UID(540), NULL };
static struct builtin B1_vec_vmaxuh = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vmaxuh:1", "vmaxuh", CODE_FOR_xfxx_simple, B_UID(541), NULL };
static struct builtin B1_vec_vmaxsw = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vmaxsw:1", "vmaxsw", CODE_FOR_xfxx_simple, B_UID(542), NULL };
static struct builtin B1_vec_vmaxuw = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vmaxuw:1", "vmaxuw", CODE_FOR_xfxx_simple, B_UID(543), NULL };
static struct builtin B1_vec_vmaxsb = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vmaxsb:1", "vmaxsb", CODE_FOR_xfxx_simple, B_UID(544), NULL };
static struct builtin B1_vec_vmaxub = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vmaxub:1", "vmaxub", CODE_FOR_xfxx_simple, B_UID(545), NULL };
static struct builtin B_vec_vmaxfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vmaxfp", "vmaxfp", CODE_FOR_xfxx_simple, B_UID(546), NULL };
static struct builtin B2_vec_vmaxsh = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vmaxsh:2", "vmaxsh", CODE_FOR_xfxx_simple, B_UID(547), NULL };
static struct builtin B3_vec_vmaxsh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vmaxsh:3", "vmaxsh", CODE_FOR_xfxx_simple, B_UID(548), NULL };
static struct builtin B2_vec_vmaxsw = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vmaxsw:2", "vmaxsw", CODE_FOR_xfxx_simple, B_UID(549), NULL };
static struct builtin B3_vec_vmaxsw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vmaxsw:3", "vmaxsw", CODE_FOR_xfxx_simple, B_UID(550), NULL };
static struct builtin B2_vec_vmaxsb = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vmaxsb:2", "vmaxsb", CODE_FOR_xfxx_simple, B_UID(551), NULL };
static struct builtin B3_vec_vmaxsb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vmaxsb:3", "vmaxsb", CODE_FOR_xfxx_simple, B_UID(552), NULL };
static struct builtin B2_vec_vmaxuh = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vmaxuh:2", "vmaxuh", CODE_FOR_xfxx_simple, B_UID(553), NULL };
static struct builtin B3_vec_vmaxuh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vmaxuh:3", "vmaxuh", CODE_FOR_xfxx_simple, B_UID(554), NULL };
static struct builtin B2_vec_vmaxuw = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vmaxuw:2", "vmaxuw", CODE_FOR_xfxx_simple, B_UID(555), NULL };
static struct builtin B3_vec_vmaxuw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vmaxuw:3", "vmaxuw", CODE_FOR_xfxx_simple, B_UID(556), NULL };
static struct builtin B2_vec_vmaxub = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vmaxub:2", "vmaxub", CODE_FOR_xfxx_simple, B_UID(557), NULL };
static struct builtin B3_vec_vmaxub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vmaxub:3", "vmaxub", CODE_FOR_xfxx_simple, B_UID(558), NULL };
static struct builtin B1_vec_vmrghh = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vmrghh:1", "vmrghh", CODE_FOR_xfxx_perm, B_UID(559), NULL };
static struct builtin B1_vec_vmrghw = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vmrghw:1", "vmrghw", CODE_FOR_xfxx_perm, B_UID(560), NULL };
static struct builtin B1_vec_vmrghb = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vmrghb:1", "vmrghb", CODE_FOR_xfxx_perm, B_UID(561), NULL };
static struct builtin B2_vec_vmrghw = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vmrghw:2", "vmrghw", CODE_FOR_xfxx_perm, B_UID(562), NULL };
static struct builtin B2_vec_vmrghh = { { &T_vec_p16, &T_vec_p16, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vmrghh:2", "vmrghh", CODE_FOR_xfxx_perm, B_UID(563), NULL };
static struct builtin B3_vec_vmrghh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vmrghh:3", "vmrghh", CODE_FOR_xfxx_perm, B_UID(564), NULL };
static struct builtin B3_vec_vmrghw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vmrghw:3", "vmrghw", CODE_FOR_xfxx_perm, B_UID(565), NULL };
static struct builtin B2_vec_vmrghb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vmrghb:2", "vmrghb", CODE_FOR_xfxx_perm, B_UID(566), NULL };
static struct builtin B4_vec_vmrghh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vmrghh:4", "vmrghh", CODE_FOR_xfxx_perm, B_UID(567), NULL };
static struct builtin B4_vec_vmrghw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vmrghw:4", "vmrghw", CODE_FOR_xfxx_perm, B_UID(568), NULL };
static struct builtin B3_vec_vmrghb = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vmrghb:3", "vmrghb", CODE_FOR_xfxx_perm, B_UID(569), NULL };
static struct builtin B1_vec_vmrglh = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vmrglh:1", "vmrglh", CODE_FOR_xfxx_perm, B_UID(570), NULL };
static struct builtin B1_vec_vmrglw = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vmrglw:1", "vmrglw", CODE_FOR_xfxx_perm, B_UID(571), NULL };
static struct builtin B1_vec_vmrglb = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vmrglb:1", "vmrglb", CODE_FOR_xfxx_perm, B_UID(572), NULL };
static struct builtin B2_vec_vmrglw = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vmrglw:2", "vmrglw", CODE_FOR_xfxx_perm, B_UID(573), NULL };
static struct builtin B2_vec_vmrglh = { { &T_vec_p16, &T_vec_p16, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vmrglh:2", "vmrglh", CODE_FOR_xfxx_perm, B_UID(574), NULL };
static struct builtin B3_vec_vmrglh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vmrglh:3", "vmrglh", CODE_FOR_xfxx_perm, B_UID(575), NULL };
static struct builtin B3_vec_vmrglw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vmrglw:3", "vmrglw", CODE_FOR_xfxx_perm, B_UID(576), NULL };
static struct builtin B2_vec_vmrglb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vmrglb:2", "vmrglb", CODE_FOR_xfxx_perm, B_UID(577), NULL };
static struct builtin B4_vec_vmrglh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vmrglh:4", "vmrglh", CODE_FOR_xfxx_perm, B_UID(578), NULL };
static struct builtin B4_vec_vmrglw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vmrglw:4", "vmrglw", CODE_FOR_xfxx_perm, B_UID(579), NULL };
static struct builtin B3_vec_vmrglb = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vmrglb:3", "vmrglb", CODE_FOR_xfxx_perm, B_UID(580), NULL };
static struct builtin B_vec_mfvscr = { { NULL, NULL, NULL, }, "", &T_volatile_vec_u16, 0, FALSE, FALSE, 0, "vec_mfvscr", "mfvscr", CODE_FOR_vxf_fxu, B_UID(581), NULL };
static struct builtin B1_vec_vminsh = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vminsh:1", "vminsh", CODE_FOR_xfxx_simple, B_UID(582), NULL };
static struct builtin B1_vec_vminuh = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vminuh:1", "vminuh", CODE_FOR_xfxx_simple, B_UID(583), NULL };
static struct builtin B1_vec_vminsw = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vminsw:1", "vminsw", CODE_FOR_xfxx_simple, B_UID(584), NULL };
static struct builtin B1_vec_vminuw = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vminuw:1", "vminuw", CODE_FOR_xfxx_simple, B_UID(585), NULL };
static struct builtin B1_vec_vminsb = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vminsb:1", "vminsb", CODE_FOR_xfxx_simple, B_UID(586), NULL };
static struct builtin B1_vec_vminub = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vminub:1", "vminub", CODE_FOR_xfxx_simple, B_UID(587), NULL };
static struct builtin B_vec_vminfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vminfp", "vminfp", CODE_FOR_xfxx_simple, B_UID(588), NULL };
static struct builtin B2_vec_vminsh = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vminsh:2", "vminsh", CODE_FOR_xfxx_simple, B_UID(589), NULL };
static struct builtin B3_vec_vminsh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vminsh:3", "vminsh", CODE_FOR_xfxx_simple, B_UID(590), NULL };
static struct builtin B2_vec_vminsw = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vminsw:2", "vminsw", CODE_FOR_xfxx_simple, B_UID(591), NULL };
static struct builtin B3_vec_vminsw = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vminsw:3", "vminsw", CODE_FOR_xfxx_simple, B_UID(592), NULL };
static struct builtin B2_vec_vminsb = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vminsb:2", "vminsb", CODE_FOR_xfxx_simple, B_UID(593), NULL };
static struct builtin B3_vec_vminsb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vminsb:3", "vminsb", CODE_FOR_xfxx_simple, B_UID(594), NULL };
static struct builtin B2_vec_vminuh = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vminuh:2", "vminuh", CODE_FOR_xfxx_simple, B_UID(595), NULL };
static struct builtin B3_vec_vminuh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vminuh:3", "vminuh", CODE_FOR_xfxx_simple, B_UID(596), NULL };
static struct builtin B2_vec_vminuw = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vminuw:2", "vminuw", CODE_FOR_xfxx_simple, B_UID(597), NULL };
static struct builtin B3_vec_vminuw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vminuw:3", "vminuw", CODE_FOR_xfxx_simple, B_UID(598), NULL };
static struct builtin B2_vec_vminub = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vminub:2", "vminub", CODE_FOR_xfxx_simple, B_UID(599), NULL };
static struct builtin B3_vec_vminub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vminub:3", "vminub", CODE_FOR_xfxx_simple, B_UID(600), NULL };
static struct builtin B1_vec_vmladduhm = { { &T_vec_s16, &T_vec_s16, &T_vec_s16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vmladduhm:1", "vmladduhm", CODE_FOR_xfxxx_complex, B_UID(601), NULL };
static struct builtin B2_vec_vmladduhm = { { &T_vec_s16, &T_vec_u16, &T_vec_u16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vmladduhm:2", "vmladduhm", CODE_FOR_xfxxx_complex, B_UID(602), NULL };
static struct builtin B3_vec_vmladduhm = { { &T_vec_u16, &T_vec_s16, &T_vec_s16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vmladduhm:3", "vmladduhm", CODE_FOR_xfxxx_complex, B_UID(603), NULL };
static struct builtin B4_vec_vmladduhm = { { &T_vec_u16, &T_vec_u16, &T_vec_u16, }, "xxx", &T_vec_u16, 3, FALSE, FALSE, 0, "vec_vmladduhm:4", "vmladduhm", CODE_FOR_xfxxx_complex, B_UID(604), NULL };
static struct builtin B_vec_vmhraddshs = { { &T_vec_s16, &T_vec_s16, &T_vec_s16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vmhraddshs", "vmhraddshs", CODE_FOR_xfxxx_complex, B_UID(605), NULL };
static struct builtin B_vec_vmsumshm = { { &T_vec_s16, &T_vec_s16, &T_vec_s32, }, "xxx", &T_vec_s32, 3, FALSE, FALSE, 0, "vec_vmsumshm", "vmsumshm", CODE_FOR_xfxxx_complex, B_UID(606), NULL };
static struct builtin B_vec_vmsummbm = { { &T_vec_s8, &T_vec_u8, &T_vec_s32, }, "xxx", &T_vec_s32, 3, FALSE, FALSE, 0, "vec_vmsummbm", "vmsummbm", CODE_FOR_xfxxx_complex, B_UID(607), NULL };
static struct builtin B_vec_vmsumuhm = { { &T_vec_u16, &T_vec_u16, &T_vec_u32, }, "xxx", &T_vec_u32, 3, FALSE, FALSE, 0, "vec_vmsumuhm", "vmsumuhm", CODE_FOR_xfxxx_complex, B_UID(608), NULL };
static struct builtin B_vec_vmsumubm = { { &T_vec_u8, &T_vec_u8, &T_vec_u32, }, "xxx", &T_vec_u32, 3, FALSE, FALSE, 0, "vec_vmsumubm", "vmsumubm", CODE_FOR_xfxxx_complex, B_UID(609), NULL };
static struct builtin B_vec_vmsumshs = { { &T_vec_s16, &T_vec_s16, &T_vec_s32, }, "xxx", &T_vec_s32, 3, FALSE, FALSE, 0, "vec_vmsumshs", "vmsumshs", CODE_FOR_xfxxx_complex, B_UID(610), NULL };
static struct builtin B_vec_vmsumuhs = { { &T_vec_u16, &T_vec_u16, &T_vec_u32, }, "xxx", &T_vec_u32, 3, FALSE, FALSE, 0, "vec_vmsumuhs", "vmsumuhs", CODE_FOR_xfxxx_complex, B_UID(611), NULL };
static struct builtin B1_vec_mtvscr = { { &T_vec_b16, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:1", "mtvscr", CODE_FOR_vfx_fxu, B_UID(612), NULL };
static struct builtin B2_vec_mtvscr = { { &T_vec_b32, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:2", "mtvscr", CODE_FOR_vfx_fxu, B_UID(613), NULL };
static struct builtin B3_vec_mtvscr = { { &T_vec_b8, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:3", "mtvscr", CODE_FOR_vfx_fxu, B_UID(614), NULL };
static struct builtin B4_vec_mtvscr = { { &T_vec_p16, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:4", "mtvscr", CODE_FOR_vfx_fxu, B_UID(615), NULL };
static struct builtin B5_vec_mtvscr = { { &T_vec_s16, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:5", "mtvscr", CODE_FOR_vfx_fxu, B_UID(616), NULL };
static struct builtin B6_vec_mtvscr = { { &T_vec_s32, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:6", "mtvscr", CODE_FOR_vfx_fxu, B_UID(617), NULL };
static struct builtin B7_vec_mtvscr = { { &T_vec_s8, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:7", "mtvscr", CODE_FOR_vfx_fxu, B_UID(618), NULL };
static struct builtin B8_vec_mtvscr = { { &T_vec_u16, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:8", "mtvscr", CODE_FOR_vfx_fxu, B_UID(619), NULL };
static struct builtin B9_vec_mtvscr = { { &T_vec_u32, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:9", "mtvscr", CODE_FOR_vfx_fxu, B_UID(620), NULL };
static struct builtin B10_vec_mtvscr = { { &T_vec_u8, NULL, NULL, }, "x", &T_volatile_void, 1, FALSE, FALSE, 0, "vec_mtvscr:10", "mtvscr", CODE_FOR_vfx_fxu, B_UID(621), NULL };
static struct builtin B_vec_vmulesh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vmulesh", "vmulesh", CODE_FOR_xfxx_complex, B_UID(622), NULL };
static struct builtin B_vec_vmulesb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vmulesb", "vmulesb", CODE_FOR_xfxx_complex, B_UID(623), NULL };
static struct builtin B_vec_vmuleuh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vmuleuh", "vmuleuh", CODE_FOR_xfxx_complex, B_UID(624), NULL };
static struct builtin B_vec_vmuleub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vmuleub", "vmuleub", CODE_FOR_xfxx_complex, B_UID(625), NULL };
static struct builtin B_vec_vmulosh = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vmulosh", "vmulosh", CODE_FOR_xfxx_complex, B_UID(626), NULL };
static struct builtin B_vec_vmulosb = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vmulosb", "vmulosb", CODE_FOR_xfxx_complex, B_UID(627), NULL };
static struct builtin B_vec_vmulouh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vmulouh", "vmulouh", CODE_FOR_xfxx_complex, B_UID(628), NULL };
static struct builtin B_vec_vmuloub = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vmuloub", "vmuloub", CODE_FOR_xfxx_complex, B_UID(629), NULL };
static struct builtin B_vec_vnmsubfp = { { &T_vec_f32, &T_vec_f32, &T_vec_f32, }, "xxx", &T_vec_f32, 3, FALSE, FALSE, 0, "vec_vnmsubfp", "vnmsubfp", CODE_FOR_xfxxx_fp, B_UID(630), NULL };
static struct builtin B1_vec_vnor = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vnor:1", "vnor", CODE_FOR_xfxx_simple, B_UID(631), NULL };
static struct builtin B2_vec_vnor = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vnor:2", "vnor", CODE_FOR_xfxx_simple, B_UID(632), NULL };
static struct builtin B3_vec_vnor = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vnor:3", "vnor", CODE_FOR_xfxx_simple, B_UID(633), NULL };
static struct builtin B4_vec_vnor = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vnor:4", "vnor", CODE_FOR_xfxx_simple, B_UID(634), NULL };
static struct builtin B5_vec_vnor = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vnor:5", "vnor", CODE_FOR_xfxx_simple, B_UID(635), NULL };
static struct builtin B6_vec_vnor = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vnor:6", "vnor", CODE_FOR_xfxx_simple, B_UID(636), NULL };
static struct builtin B7_vec_vnor = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vnor:7", "vnor", CODE_FOR_xfxx_simple, B_UID(637), NULL };
static struct builtin B8_vec_vnor = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vnor:8", "vnor", CODE_FOR_xfxx_simple, B_UID(638), NULL };
static struct builtin B9_vec_vnor = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vnor:9", "vnor", CODE_FOR_xfxx_simple, B_UID(639), NULL };
static struct builtin B10_vec_vnor = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vnor:10", "vnor", CODE_FOR_xfxx_simple, B_UID(640), NULL };
static struct builtin B1_vec_vor = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 2, "vec_vor:1", "vor", CODE_FOR_xfxx_simple, B_UID(641), NULL };
static struct builtin B2_vec_vor = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vor:2", "vor", CODE_FOR_xfxx_simple, B_UID(642), NULL };
static struct builtin B3_vec_vor = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vor:3", "vor", CODE_FOR_xfxx_simple, B_UID(643), NULL };
static struct builtin B4_vec_vor = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 2, "vec_vor:4", "vor", CODE_FOR_xfxx_simple, B_UID(644), NULL };
static struct builtin B5_vec_vor = { { &T_vec_b32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vor:5", "vor", CODE_FOR_xfxx_simple, B_UID(645), NULL };
static struct builtin B6_vec_vor = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vor:6", "vor", CODE_FOR_xfxx_simple, B_UID(646), NULL };
static struct builtin B7_vec_vor = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vor:7", "vor", CODE_FOR_xfxx_simple, B_UID(647), NULL };
static struct builtin B8_vec_vor = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 2, "vec_vor:8", "vor", CODE_FOR_xfxx_simple, B_UID(648), NULL };
static struct builtin B9_vec_vor = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vor:9", "vor", CODE_FOR_xfxx_simple, B_UID(649), NULL };
static struct builtin B10_vec_vor = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vor:10", "vor", CODE_FOR_xfxx_simple, B_UID(650), NULL };
static struct builtin B11_vec_vor = { { &T_vec_f32, &T_vec_b32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vor:11", "vor", CODE_FOR_xfxx_simple, B_UID(651), NULL };
static struct builtin B12_vec_vor = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 2, "vec_vor:12", "vor", CODE_FOR_xfxx_simple, B_UID(652), NULL };
static struct builtin B13_vec_vor = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vor:13", "vor", CODE_FOR_xfxx_simple, B_UID(653), NULL };
static struct builtin B14_vec_vor = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 2, "vec_vor:14", "vor", CODE_FOR_xfxx_simple, B_UID(654), NULL };
static struct builtin B15_vec_vor = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vor:15", "vor", CODE_FOR_xfxx_simple, B_UID(655), NULL };
static struct builtin B16_vec_vor = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 2, "vec_vor:16", "vor", CODE_FOR_xfxx_simple, B_UID(656), NULL };
static struct builtin B17_vec_vor = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vor:17", "vor", CODE_FOR_xfxx_simple, B_UID(657), NULL };
static struct builtin B18_vec_vor = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 2, "vec_vor:18", "vor", CODE_FOR_xfxx_simple, B_UID(658), NULL };
static struct builtin B19_vec_vor = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vor:19", "vor", CODE_FOR_xfxx_simple, B_UID(659), NULL };
static struct builtin B20_vec_vor = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 2, "vec_vor:20", "vor", CODE_FOR_xfxx_simple, B_UID(660), NULL };
static struct builtin B21_vec_vor = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vor:21", "vor", CODE_FOR_xfxx_simple, B_UID(661), NULL };
static struct builtin B22_vec_vor = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 2, "vec_vor:22", "vor", CODE_FOR_xfxx_simple, B_UID(662), NULL };
static struct builtin B23_vec_vor = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vor:23", "vor", CODE_FOR_xfxx_simple, B_UID(663), NULL };
static struct builtin B24_vec_vor = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 2, "vec_vor:24", "vor", CODE_FOR_xfxx_simple, B_UID(664), NULL };
static struct builtin B1_vec_vpkuhum = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vpkuhum:1", "vpkuhum", CODE_FOR_xfxx_perm, B_UID(665), NULL };
static struct builtin B1_vec_vpkuwum = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vpkuwum:1", "vpkuwum", CODE_FOR_xfxx_perm, B_UID(666), NULL };
static struct builtin B2_vec_vpkuhum = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vpkuhum:2", "vpkuhum", CODE_FOR_xfxx_perm, B_UID(667), NULL };
static struct builtin B2_vec_vpkuwum = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vpkuwum:2", "vpkuwum", CODE_FOR_xfxx_perm, B_UID(668), NULL };
static struct builtin B3_vec_vpkuhum = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vpkuhum:3", "vpkuhum", CODE_FOR_xfxx_perm, B_UID(669), NULL };
static struct builtin B3_vec_vpkuwum = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vpkuwum:3", "vpkuwum", CODE_FOR_xfxx_perm, B_UID(670), NULL };
static struct builtin B_vec_vpkpx = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vpkpx", "vpkpx", CODE_FOR_xfxx_perm, B_UID(671), NULL };
static struct builtin B_vec_vpkshss = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vpkshss", "vpkshss", CODE_FOR_xfxx_perm, B_UID(672), NULL };
static struct builtin B_vec_vpkswss = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vpkswss", "vpkswss", CODE_FOR_xfxx_perm, B_UID(673), NULL };
static struct builtin B_vec_vpkuhus = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vpkuhus", "vpkuhus", CODE_FOR_xfxx_perm, B_UID(674), NULL };
static struct builtin B_vec_vpkuwus = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vpkuwus", "vpkuwus", CODE_FOR_xfxx_perm, B_UID(675), NULL };
static struct builtin B_vec_vpkshus = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vpkshus", "vpkshus", CODE_FOR_xfxx_perm, B_UID(676), NULL };
static struct builtin B_vec_vpkswus = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vpkswus", "vpkswus", CODE_FOR_xfxx_perm, B_UID(677), NULL };
static struct builtin B1_vec_vperm = { { &T_vec_b16, &T_vec_b16, &T_vec_u8, }, "xxx", &T_vec_b16, 3, FALSE, FALSE, 0, "vec_vperm:1", "vperm", CODE_FOR_xfxxx_perm, B_UID(678), NULL };
static struct builtin B2_vec_vperm = { { &T_vec_b32, &T_vec_b32, &T_vec_u8, }, "xxx", &T_vec_b32, 3, FALSE, FALSE, 0, "vec_vperm:2", "vperm", CODE_FOR_xfxxx_perm, B_UID(679), NULL };
static struct builtin B3_vec_vperm = { { &T_vec_b8, &T_vec_b8, &T_vec_u8, }, "xxx", &T_vec_b8, 3, FALSE, FALSE, 0, "vec_vperm:3", "vperm", CODE_FOR_xfxxx_perm, B_UID(680), NULL };
static struct builtin B4_vec_vperm = { { &T_vec_f32, &T_vec_f32, &T_vec_u8, }, "xxx", &T_vec_f32, 3, FALSE, FALSE, 0, "vec_vperm:4", "vperm", CODE_FOR_xfxxx_perm, B_UID(681), NULL };
static struct builtin B5_vec_vperm = { { &T_vec_p16, &T_vec_p16, &T_vec_u8, }, "xxx", &T_vec_p16, 3, FALSE, FALSE, 0, "vec_vperm:5", "vperm", CODE_FOR_xfxxx_perm, B_UID(682), NULL };
static struct builtin B6_vec_vperm = { { &T_vec_s16, &T_vec_s16, &T_vec_u8, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vperm:6", "vperm", CODE_FOR_xfxxx_perm, B_UID(683), NULL };
static struct builtin B7_vec_vperm = { { &T_vec_s32, &T_vec_s32, &T_vec_u8, }, "xxx", &T_vec_s32, 3, FALSE, FALSE, 0, "vec_vperm:7", "vperm", CODE_FOR_xfxxx_perm, B_UID(684), NULL };
static struct builtin B8_vec_vperm = { { &T_vec_s8, &T_vec_s8, &T_vec_u8, }, "xxx", &T_vec_s8, 3, FALSE, FALSE, 0, "vec_vperm:8", "vperm", CODE_FOR_xfxxx_perm, B_UID(685), NULL };
static struct builtin B9_vec_vperm = { { &T_vec_u16, &T_vec_u16, &T_vec_u8, }, "xxx", &T_vec_u16, 3, FALSE, FALSE, 0, "vec_vperm:9", "vperm", CODE_FOR_xfxxx_perm, B_UID(686), NULL };
static struct builtin B10_vec_vperm = { { &T_vec_u32, &T_vec_u32, &T_vec_u8, }, "xxx", &T_vec_u32, 3, FALSE, FALSE, 0, "vec_vperm:10", "vperm", CODE_FOR_xfxxx_perm, B_UID(687), NULL };
static struct builtin B11_vec_vperm = { { &T_vec_u8, &T_vec_u8, &T_vec_u8, }, "xxx", &T_vec_u8, 3, FALSE, FALSE, 0, "vec_vperm:11", "vperm", CODE_FOR_xfxxx_perm, B_UID(688), NULL };
static struct builtin B_vec_vrefp = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vrefp", "vrefp", CODE_FOR_xfx_fp, B_UID(689), NULL };
static struct builtin B1_vec_vrlh = { { &T_vec_s16, &T_vec_u16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vrlh:1", "vrlh", CODE_FOR_xfxx_simple, B_UID(690), NULL };
static struct builtin B1_vec_vrlw = { { &T_vec_s32, &T_vec_u32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vrlw:1", "vrlw", CODE_FOR_xfxx_simple, B_UID(691), NULL };
static struct builtin B1_vec_vrlb = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vrlb:1", "vrlb", CODE_FOR_xfxx_simple, B_UID(692), NULL };
static struct builtin B2_vec_vrlh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vrlh:2", "vrlh", CODE_FOR_xfxx_simple, B_UID(693), NULL };
static struct builtin B2_vec_vrlw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vrlw:2", "vrlw", CODE_FOR_xfxx_simple, B_UID(694), NULL };
static struct builtin B2_vec_vrlb = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vrlb:2", "vrlb", CODE_FOR_xfxx_simple, B_UID(695), NULL };
static struct builtin B_vec_vrfin = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vrfin", "vrfin", CODE_FOR_xfx_fp, B_UID(696), NULL };
static struct builtin B_vec_vrsqrtefp = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vrsqrtefp", "vrsqrtefp", CODE_FOR_xfx_fp, B_UID(697), NULL };
static struct builtin B1_vec_vsel = { { &T_vec_b16, &T_vec_b16, &T_vec_b16, }, "xxx", &T_vec_b16, 3, FALSE, FALSE, 0, "vec_vsel:1", "vsel", CODE_FOR_xfxxx_simple, B_UID(698), NULL };
static struct builtin B2_vec_vsel = { { &T_vec_b16, &T_vec_b16, &T_vec_u16, }, "xxx", &T_vec_b16, 3, FALSE, FALSE, 0, "vec_vsel:2", "vsel", CODE_FOR_xfxxx_simple, B_UID(699), NULL };
static struct builtin B3_vec_vsel = { { &T_vec_b32, &T_vec_b32, &T_vec_b32, }, "xxx", &T_vec_b32, 3, FALSE, FALSE, 0, "vec_vsel:3", "vsel", CODE_FOR_xfxxx_simple, B_UID(700), NULL };
static struct builtin B4_vec_vsel = { { &T_vec_b32, &T_vec_b32, &T_vec_u32, }, "xxx", &T_vec_b32, 3, FALSE, FALSE, 0, "vec_vsel:4", "vsel", CODE_FOR_xfxxx_simple, B_UID(701), NULL };
static struct builtin B5_vec_vsel = { { &T_vec_b8, &T_vec_b8, &T_vec_b8, }, "xxx", &T_vec_b8, 3, FALSE, FALSE, 0, "vec_vsel:5", "vsel", CODE_FOR_xfxxx_simple, B_UID(702), NULL };
static struct builtin B6_vec_vsel = { { &T_vec_b8, &T_vec_b8, &T_vec_u8, }, "xxx", &T_vec_b8, 3, FALSE, FALSE, 0, "vec_vsel:6", "vsel", CODE_FOR_xfxxx_simple, B_UID(703), NULL };
static struct builtin B7_vec_vsel = { { &T_vec_f32, &T_vec_f32, &T_vec_b32, }, "xxx", &T_vec_f32, 3, FALSE, FALSE, 0, "vec_vsel:7", "vsel", CODE_FOR_xfxxx_simple, B_UID(704), NULL };
static struct builtin B8_vec_vsel = { { &T_vec_f32, &T_vec_f32, &T_vec_u32, }, "xxx", &T_vec_f32, 3, FALSE, FALSE, 0, "vec_vsel:8", "vsel", CODE_FOR_xfxxx_simple, B_UID(705), NULL };
static struct builtin B9_vec_vsel = { { &T_vec_s16, &T_vec_s16, &T_vec_b16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vsel:9", "vsel", CODE_FOR_xfxxx_simple, B_UID(706), NULL };
static struct builtin B10_vec_vsel = { { &T_vec_s16, &T_vec_s16, &T_vec_u16, }, "xxx", &T_vec_s16, 3, FALSE, FALSE, 0, "vec_vsel:10", "vsel", CODE_FOR_xfxxx_simple, B_UID(707), NULL };
static struct builtin B11_vec_vsel = { { &T_vec_s32, &T_vec_s32, &T_vec_b32, }, "xxx", &T_vec_s32, 3, FALSE, FALSE, 0, "vec_vsel:11", "vsel", CODE_FOR_xfxxx_simple, B_UID(708), NULL };
static struct builtin B12_vec_vsel = { { &T_vec_s32, &T_vec_s32, &T_vec_u32, }, "xxx", &T_vec_s32, 3, FALSE, FALSE, 0, "vec_vsel:12", "vsel", CODE_FOR_xfxxx_simple, B_UID(709), NULL };
static struct builtin B13_vec_vsel = { { &T_vec_s8, &T_vec_s8, &T_vec_b8, }, "xxx", &T_vec_s8, 3, FALSE, FALSE, 0, "vec_vsel:13", "vsel", CODE_FOR_xfxxx_simple, B_UID(710), NULL };
static struct builtin B14_vec_vsel = { { &T_vec_s8, &T_vec_s8, &T_vec_u8, }, "xxx", &T_vec_s8, 3, FALSE, FALSE, 0, "vec_vsel:14", "vsel", CODE_FOR_xfxxx_simple, B_UID(711), NULL };
static struct builtin B15_vec_vsel = { { &T_vec_u16, &T_vec_u16, &T_vec_b16, }, "xxx", &T_vec_u16, 3, FALSE, FALSE, 0, "vec_vsel:15", "vsel", CODE_FOR_xfxxx_simple, B_UID(712), NULL };
static struct builtin B16_vec_vsel = { { &T_vec_u16, &T_vec_u16, &T_vec_u16, }, "xxx", &T_vec_u16, 3, FALSE, FALSE, 0, "vec_vsel:16", "vsel", CODE_FOR_xfxxx_simple, B_UID(713), NULL };
static struct builtin B17_vec_vsel = { { &T_vec_u32, &T_vec_u32, &T_vec_b32, }, "xxx", &T_vec_u32, 3, FALSE, FALSE, 0, "vec_vsel:17", "vsel", CODE_FOR_xfxxx_simple, B_UID(714), NULL };
static struct builtin B18_vec_vsel = { { &T_vec_u32, &T_vec_u32, &T_vec_u32, }, "xxx", &T_vec_u32, 3, FALSE, FALSE, 0, "vec_vsel:18", "vsel", CODE_FOR_xfxxx_simple, B_UID(715), NULL };
static struct builtin B19_vec_vsel = { { &T_vec_u8, &T_vec_u8, &T_vec_b8, }, "xxx", &T_vec_u8, 3, FALSE, FALSE, 0, "vec_vsel:19", "vsel", CODE_FOR_xfxxx_simple, B_UID(716), NULL };
static struct builtin B20_vec_vsel = { { &T_vec_u8, &T_vec_u8, &T_vec_u8, }, "xxx", &T_vec_u8, 3, FALSE, FALSE, 0, "vec_vsel:20", "vsel", CODE_FOR_xfxxx_simple, B_UID(717), NULL };
static struct builtin B1_vec_vslh = { { &T_vec_s16, &T_vec_u16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vslh:1", "vslh", CODE_FOR_xfxx_simple, B_UID(718), NULL };
static struct builtin B1_vec_vslw = { { &T_vec_s32, &T_vec_u32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vslw:1", "vslw", CODE_FOR_xfxx_simple, B_UID(719), NULL };
static struct builtin B1_vec_vslb = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vslb:1", "vslb", CODE_FOR_xfxx_simple, B_UID(720), NULL };
static struct builtin B2_vec_vslh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vslh:2", "vslh", CODE_FOR_xfxx_simple, B_UID(721), NULL };
static struct builtin B2_vec_vslw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vslw:2", "vslw", CODE_FOR_xfxx_simple, B_UID(722), NULL };
static struct builtin B2_vec_vslb = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vslb:2", "vslb", CODE_FOR_xfxx_simple, B_UID(723), NULL };
static struct builtin B1_vec_vsldoi = { { &T_vec_f32, &T_vec_f32, &T_immed_u4, }, "xxC", &T_vec_f32, 3, FALSE, FALSE, 3, "vec_vsldoi:1", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(724), NULL };
static struct builtin B2_vec_vsldoi = { { &T_vec_p16, &T_vec_p16, &T_immed_u4, }, "xxC", &T_vec_p16, 3, FALSE, FALSE, 3, "vec_vsldoi:2", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(725), NULL };
static struct builtin B3_vec_vsldoi = { { &T_vec_s16, &T_vec_s16, &T_immed_u4, }, "xxC", &T_vec_s16, 3, FALSE, FALSE, 3, "vec_vsldoi:3", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(726), NULL };
static struct builtin B4_vec_vsldoi = { { &T_vec_s32, &T_vec_s32, &T_immed_u4, }, "xxC", &T_vec_s32, 3, FALSE, FALSE, 3, "vec_vsldoi:4", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(727), NULL };
static struct builtin B5_vec_vsldoi = { { &T_vec_s8, &T_vec_s8, &T_immed_u4, }, "xxC", &T_vec_s8, 3, FALSE, FALSE, 3, "vec_vsldoi:5", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(728), NULL };
static struct builtin B6_vec_vsldoi = { { &T_vec_u16, &T_vec_u16, &T_immed_u4, }, "xxC", &T_vec_u16, 3, FALSE, FALSE, 3, "vec_vsldoi:6", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(729), NULL };
static struct builtin B7_vec_vsldoi = { { &T_vec_u32, &T_vec_u32, &T_immed_u4, }, "xxC", &T_vec_u32, 3, FALSE, FALSE, 3, "vec_vsldoi:7", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(730), NULL };
static struct builtin B8_vec_vsldoi = { { &T_vec_u8, &T_vec_u8, &T_immed_u4, }, "xxC", &T_vec_u8, 3, FALSE, FALSE, 3, "vec_vsldoi:8", "vsldoi", CODE_FOR_xfxxC_perm, B_UID(731), NULL };
static struct builtin B1_vec_vsl = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsl:1", "vsl", CODE_FOR_xfxx_simple, B_UID(732), NULL };
static struct builtin B2_vec_vsl = { { &T_vec_b16, &T_vec_u32, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsl:2", "vsl", CODE_FOR_xfxx_simple, B_UID(733), NULL };
static struct builtin B3_vec_vsl = { { &T_vec_b16, &T_vec_u8, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsl:3", "vsl", CODE_FOR_xfxx_simple, B_UID(734), NULL };
static struct builtin B4_vec_vsl = { { &T_vec_b32, &T_vec_u16, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vsl:4", "vsl", CODE_FOR_xfxx_simple, B_UID(735), NULL };
static struct builtin B5_vec_vsl = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vsl:5", "vsl", CODE_FOR_xfxx_simple, B_UID(736), NULL };
static struct builtin B6_vec_vsl = { { &T_vec_b32, &T_vec_u8, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vsl:6", "vsl", CODE_FOR_xfxx_simple, B_UID(737), NULL };
static struct builtin B7_vec_vsl = { { &T_vec_b8, &T_vec_u16, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vsl:7", "vsl", CODE_FOR_xfxx_simple, B_UID(738), NULL };
static struct builtin B8_vec_vsl = { { &T_vec_b8, &T_vec_u32, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vsl:8", "vsl", CODE_FOR_xfxx_simple, B_UID(739), NULL };
static struct builtin B9_vec_vsl = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vsl:9", "vsl", CODE_FOR_xfxx_simple, B_UID(740), NULL };
static struct builtin B10_vec_vsl = { { &T_vec_p16, &T_vec_u16, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsl:10", "vsl", CODE_FOR_xfxx_simple, B_UID(741), NULL };
static struct builtin B11_vec_vsl = { { &T_vec_p16, &T_vec_u32, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsl:11", "vsl", CODE_FOR_xfxx_simple, B_UID(742), NULL };
static struct builtin B12_vec_vsl = { { &T_vec_p16, &T_vec_u8, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsl:12", "vsl", CODE_FOR_xfxx_simple, B_UID(743), NULL };
static struct builtin B13_vec_vsl = { { &T_vec_s16, &T_vec_u16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsl:13", "vsl", CODE_FOR_xfxx_simple, B_UID(744), NULL };
static struct builtin B14_vec_vsl = { { &T_vec_s16, &T_vec_u32, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsl:14", "vsl", CODE_FOR_xfxx_simple, B_UID(745), NULL };
static struct builtin B15_vec_vsl = { { &T_vec_s16, &T_vec_u8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsl:15", "vsl", CODE_FOR_xfxx_simple, B_UID(746), NULL };
static struct builtin B16_vec_vsl = { { &T_vec_s32, &T_vec_u16, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsl:16", "vsl", CODE_FOR_xfxx_simple, B_UID(747), NULL };
static struct builtin B17_vec_vsl = { { &T_vec_s32, &T_vec_u32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsl:17", "vsl", CODE_FOR_xfxx_simple, B_UID(748), NULL };
static struct builtin B18_vec_vsl = { { &T_vec_s32, &T_vec_u8, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsl:18", "vsl", CODE_FOR_xfxx_simple, B_UID(749), NULL };
static struct builtin B19_vec_vsl = { { &T_vec_s8, &T_vec_u16, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsl:19", "vsl", CODE_FOR_xfxx_simple, B_UID(750), NULL };
static struct builtin B20_vec_vsl = { { &T_vec_s8, &T_vec_u32, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsl:20", "vsl", CODE_FOR_xfxx_simple, B_UID(751), NULL };
static struct builtin B21_vec_vsl = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsl:21", "vsl", CODE_FOR_xfxx_simple, B_UID(752), NULL };
static struct builtin B22_vec_vsl = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsl:22", "vsl", CODE_FOR_xfxx_simple, B_UID(753), NULL };
static struct builtin B23_vec_vsl = { { &T_vec_u16, &T_vec_u32, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsl:23", "vsl", CODE_FOR_xfxx_simple, B_UID(754), NULL };
static struct builtin B24_vec_vsl = { { &T_vec_u16, &T_vec_u8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsl:24", "vsl", CODE_FOR_xfxx_simple, B_UID(755), NULL };
static struct builtin B25_vec_vsl = { { &T_vec_u32, &T_vec_u16, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsl:25", "vsl", CODE_FOR_xfxx_simple, B_UID(756), NULL };
static struct builtin B26_vec_vsl = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsl:26", "vsl", CODE_FOR_xfxx_simple, B_UID(757), NULL };
static struct builtin B27_vec_vsl = { { &T_vec_u32, &T_vec_u8, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsl:27", "vsl", CODE_FOR_xfxx_simple, B_UID(758), NULL };
static struct builtin B28_vec_vsl = { { &T_vec_u8, &T_vec_u16, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsl:28", "vsl", CODE_FOR_xfxx_simple, B_UID(759), NULL };
static struct builtin B29_vec_vsl = { { &T_vec_u8, &T_vec_u32, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsl:29", "vsl", CODE_FOR_xfxx_simple, B_UID(760), NULL };
static struct builtin B30_vec_vsl = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsl:30", "vsl", CODE_FOR_xfxx_simple, B_UID(761), NULL };
static struct builtin B1_vec_vslo = { { &T_vec_f32, &T_vec_s8, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vslo:1", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(762), NULL };
static struct builtin B2_vec_vslo = { { &T_vec_f32, &T_vec_u8, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vslo:2", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(763), NULL };
static struct builtin B3_vec_vslo = { { &T_vec_p16, &T_vec_s8, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vslo:3", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(764), NULL };
static struct builtin B4_vec_vslo = { { &T_vec_p16, &T_vec_u8, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vslo:4", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(765), NULL };
static struct builtin B5_vec_vslo = { { &T_vec_s16, &T_vec_s8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vslo:5", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(766), NULL };
static struct builtin B6_vec_vslo = { { &T_vec_s16, &T_vec_u8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vslo:6", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(767), NULL };
static struct builtin B7_vec_vslo = { { &T_vec_s32, &T_vec_s8, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vslo:7", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(768), NULL };
static struct builtin B8_vec_vslo = { { &T_vec_s32, &T_vec_u8, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vslo:8", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(769), NULL };
static struct builtin B9_vec_vslo = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vslo:9", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(770), NULL };
static struct builtin B10_vec_vslo = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vslo:10", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(771), NULL };
static struct builtin B11_vec_vslo = { { &T_vec_u16, &T_vec_s8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vslo:11", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(772), NULL };
static struct builtin B12_vec_vslo = { { &T_vec_u16, &T_vec_u8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vslo:12", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(773), NULL };
static struct builtin B13_vec_vslo = { { &T_vec_u32, &T_vec_s8, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vslo:13", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(774), NULL };
static struct builtin B14_vec_vslo = { { &T_vec_u32, &T_vec_u8, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vslo:14", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(775), NULL };
static struct builtin B15_vec_vslo = { { &T_vec_u8, &T_vec_s8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vslo:15", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(776), NULL };
static struct builtin B16_vec_vslo = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vslo:16", "vslo", CODE_FOR_xfxx_perm_bug, B_UID(777), NULL };
static struct builtin B1_vec_vsplth = { { &T_vec_b16, &T_immed_u5, NULL, }, "xB", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsplth:1", "vsplth", CODE_FOR_xfxB_perm, B_UID(778), NULL };
static struct builtin B1_vec_vspltw = { { &T_vec_b32, &T_immed_u5, NULL, }, "xB", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vspltw:1", "vspltw", CODE_FOR_xfxB_perm, B_UID(779), NULL };
static struct builtin B1_vec_vspltb = { { &T_vec_b8, &T_immed_u5, NULL, }, "xB", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vspltb:1", "vspltb", CODE_FOR_xfxB_perm, B_UID(780), NULL };
static struct builtin B2_vec_vspltw = { { &T_vec_f32, &T_immed_u5, NULL, }, "xB", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vspltw:2", "vspltw", CODE_FOR_xfxB_perm, B_UID(781), NULL };
static struct builtin B2_vec_vsplth = { { &T_vec_p16, &T_immed_u5, NULL, }, "xB", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsplth:2", "vsplth", CODE_FOR_xfxB_perm, B_UID(782), NULL };
static struct builtin B3_vec_vsplth = { { &T_vec_s16, &T_immed_u5, NULL, }, "xB", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsplth:3", "vsplth", CODE_FOR_xfxB_perm, B_UID(783), NULL };
static struct builtin B3_vec_vspltw = { { &T_vec_s32, &T_immed_u5, NULL, }, "xB", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vspltw:3", "vspltw", CODE_FOR_xfxB_perm, B_UID(784), NULL };
static struct builtin B2_vec_vspltb = { { &T_vec_s8, &T_immed_u5, NULL, }, "xB", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vspltb:2", "vspltb", CODE_FOR_xfxB_perm, B_UID(785), NULL };
static struct builtin B4_vec_vsplth = { { &T_vec_u16, &T_immed_u5, NULL, }, "xB", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsplth:4", "vsplth", CODE_FOR_xfxB_perm, B_UID(786), NULL };
static struct builtin B4_vec_vspltw = { { &T_vec_u32, &T_immed_u5, NULL, }, "xB", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vspltw:4", "vspltw", CODE_FOR_xfxB_perm, B_UID(787), NULL };
static struct builtin B3_vec_vspltb = { { &T_vec_u8, &T_immed_u5, NULL, }, "xB", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vspltb:3", "vspltb", CODE_FOR_xfxB_perm, B_UID(788), NULL };
static struct builtin B_vec_vspltish = { { &T_immed_s5, NULL, NULL, }, "A", &T_vec_s16, 1, FALSE, FALSE, 5, "vec_vspltish", "vspltish", CODE_FOR_xfA_perm, B_UID(789), NULL };
static struct builtin B_vec_vspltisw = { { &T_immed_s5, NULL, NULL, }, "A", &T_vec_s32, 1, FALSE, FALSE, 6, "vec_vspltisw", "vspltisw", CODE_FOR_xfA_perm, B_UID(790), NULL };
static struct builtin B_vec_vspltisb = { { &T_immed_s5, NULL, NULL, }, "A", &T_vec_s8, 1, FALSE, FALSE, 4, "vec_vspltisb", "vspltisb", CODE_FOR_xfA_perm, B_UID(791), NULL };
static struct builtin B_vec_splat_u16 = { { &T_immed_s5, NULL, NULL, }, "A", &T_vec_u16, 1, FALSE, FALSE, 5, "vec_splat_u16", "vspltish", CODE_FOR_xfA_perm, B_UID(792), NULL };
static struct builtin B_vec_splat_u32 = { { &T_immed_s5, NULL, NULL, }, "A", &T_vec_u32, 1, FALSE, FALSE, 6, "vec_splat_u32", "vspltisw", CODE_FOR_xfA_perm, B_UID(793), NULL };
static struct builtin B_vec_splat_u8 = { { &T_immed_s5, NULL, NULL, }, "A", &T_vec_u8, 1, FALSE, FALSE, 4, "vec_splat_u8", "vspltisb", CODE_FOR_xfA_perm, B_UID(794), NULL };
static struct builtin B1_vec_vsrh = { { &T_vec_s16, &T_vec_u16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsrh:1", "vsrh", CODE_FOR_xfxx_simple, B_UID(795), NULL };
static struct builtin B1_vec_vsrw = { { &T_vec_s32, &T_vec_u32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsrw:1", "vsrw", CODE_FOR_xfxx_simple, B_UID(796), NULL };
static struct builtin B1_vec_vsrb = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsrb:1", "vsrb", CODE_FOR_xfxx_simple, B_UID(797), NULL };
static struct builtin B2_vec_vsrh = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsrh:2", "vsrh", CODE_FOR_xfxx_simple, B_UID(798), NULL };
static struct builtin B2_vec_vsrw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsrw:2", "vsrw", CODE_FOR_xfxx_simple, B_UID(799), NULL };
static struct builtin B2_vec_vsrb = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsrb:2", "vsrb", CODE_FOR_xfxx_simple, B_UID(800), NULL };
static struct builtin B1_vec_vsrah = { { &T_vec_s16, &T_vec_u16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsrah:1", "vsrah", CODE_FOR_xfxx_simple, B_UID(801), NULL };
static struct builtin B1_vec_vsraw = { { &T_vec_s32, &T_vec_u32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsraw:1", "vsraw", CODE_FOR_xfxx_simple, B_UID(802), NULL };
static struct builtin B1_vec_vsrab = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsrab:1", "vsrab", CODE_FOR_xfxx_simple, B_UID(803), NULL };
static struct builtin B2_vec_vsrah = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsrah:2", "vsrah", CODE_FOR_xfxx_simple, B_UID(804), NULL };
static struct builtin B2_vec_vsraw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsraw:2", "vsraw", CODE_FOR_xfxx_simple, B_UID(805), NULL };
static struct builtin B2_vec_vsrab = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsrab:2", "vsrab", CODE_FOR_xfxx_simple, B_UID(806), NULL };
static struct builtin B1_vec_vsr = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsr:1", "vsr", CODE_FOR_xfxx_simple, B_UID(807), NULL };
static struct builtin B2_vec_vsr = { { &T_vec_b16, &T_vec_u32, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsr:2", "vsr", CODE_FOR_xfxx_simple, B_UID(808), NULL };
static struct builtin B3_vec_vsr = { { &T_vec_b16, &T_vec_u8, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 0, "vec_vsr:3", "vsr", CODE_FOR_xfxx_simple, B_UID(809), NULL };
static struct builtin B4_vec_vsr = { { &T_vec_b32, &T_vec_u16, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vsr:4", "vsr", CODE_FOR_xfxx_simple, B_UID(810), NULL };
static struct builtin B5_vec_vsr = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vsr:5", "vsr", CODE_FOR_xfxx_simple, B_UID(811), NULL };
static struct builtin B6_vec_vsr = { { &T_vec_b32, &T_vec_u8, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 0, "vec_vsr:6", "vsr", CODE_FOR_xfxx_simple, B_UID(812), NULL };
static struct builtin B7_vec_vsr = { { &T_vec_b8, &T_vec_u16, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vsr:7", "vsr", CODE_FOR_xfxx_simple, B_UID(813), NULL };
static struct builtin B8_vec_vsr = { { &T_vec_b8, &T_vec_u32, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vsr:8", "vsr", CODE_FOR_xfxx_simple, B_UID(814), NULL };
static struct builtin B9_vec_vsr = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 0, "vec_vsr:9", "vsr", CODE_FOR_xfxx_simple, B_UID(815), NULL };
static struct builtin B10_vec_vsr = { { &T_vec_p16, &T_vec_u16, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsr:10", "vsr", CODE_FOR_xfxx_simple, B_UID(816), NULL };
static struct builtin B11_vec_vsr = { { &T_vec_p16, &T_vec_u32, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsr:11", "vsr", CODE_FOR_xfxx_simple, B_UID(817), NULL };
static struct builtin B12_vec_vsr = { { &T_vec_p16, &T_vec_u8, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsr:12", "vsr", CODE_FOR_xfxx_simple, B_UID(818), NULL };
static struct builtin B13_vec_vsr = { { &T_vec_s16, &T_vec_u16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsr:13", "vsr", CODE_FOR_xfxx_simple, B_UID(819), NULL };
static struct builtin B14_vec_vsr = { { &T_vec_s16, &T_vec_u32, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsr:14", "vsr", CODE_FOR_xfxx_simple, B_UID(820), NULL };
static struct builtin B15_vec_vsr = { { &T_vec_s16, &T_vec_u8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsr:15", "vsr", CODE_FOR_xfxx_simple, B_UID(821), NULL };
static struct builtin B16_vec_vsr = { { &T_vec_s32, &T_vec_u16, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsr:16", "vsr", CODE_FOR_xfxx_simple, B_UID(822), NULL };
static struct builtin B17_vec_vsr = { { &T_vec_s32, &T_vec_u32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsr:17", "vsr", CODE_FOR_xfxx_simple, B_UID(823), NULL };
static struct builtin B18_vec_vsr = { { &T_vec_s32, &T_vec_u8, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsr:18", "vsr", CODE_FOR_xfxx_simple, B_UID(824), NULL };
static struct builtin B19_vec_vsr = { { &T_vec_s8, &T_vec_u16, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsr:19", "vsr", CODE_FOR_xfxx_simple, B_UID(825), NULL };
static struct builtin B20_vec_vsr = { { &T_vec_s8, &T_vec_u32, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsr:20", "vsr", CODE_FOR_xfxx_simple, B_UID(826), NULL };
static struct builtin B21_vec_vsr = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsr:21", "vsr", CODE_FOR_xfxx_simple, B_UID(827), NULL };
static struct builtin B22_vec_vsr = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsr:22", "vsr", CODE_FOR_xfxx_simple, B_UID(828), NULL };
static struct builtin B23_vec_vsr = { { &T_vec_u16, &T_vec_u32, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsr:23", "vsr", CODE_FOR_xfxx_simple, B_UID(829), NULL };
static struct builtin B24_vec_vsr = { { &T_vec_u16, &T_vec_u8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsr:24", "vsr", CODE_FOR_xfxx_simple, B_UID(830), NULL };
static struct builtin B25_vec_vsr = { { &T_vec_u32, &T_vec_u16, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsr:25", "vsr", CODE_FOR_xfxx_simple, B_UID(831), NULL };
static struct builtin B26_vec_vsr = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsr:26", "vsr", CODE_FOR_xfxx_simple, B_UID(832), NULL };
static struct builtin B27_vec_vsr = { { &T_vec_u32, &T_vec_u8, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsr:27", "vsr", CODE_FOR_xfxx_simple, B_UID(833), NULL };
static struct builtin B28_vec_vsr = { { &T_vec_u8, &T_vec_u16, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsr:28", "vsr", CODE_FOR_xfxx_simple, B_UID(834), NULL };
static struct builtin B29_vec_vsr = { { &T_vec_u8, &T_vec_u32, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsr:29", "vsr", CODE_FOR_xfxx_simple, B_UID(835), NULL };
static struct builtin B30_vec_vsr = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsr:30", "vsr", CODE_FOR_xfxx_simple, B_UID(836), NULL };
static struct builtin B1_vec_vsro = { { &T_vec_f32, &T_vec_s8, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vsro:1", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(837), NULL };
static struct builtin B2_vec_vsro = { { &T_vec_f32, &T_vec_u8, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 0, "vec_vsro:2", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(838), NULL };
static struct builtin B3_vec_vsro = { { &T_vec_p16, &T_vec_s8, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsro:3", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(839), NULL };
static struct builtin B4_vec_vsro = { { &T_vec_p16, &T_vec_u8, NULL, }, "xx", &T_vec_p16, 2, FALSE, FALSE, 0, "vec_vsro:4", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(840), NULL };
static struct builtin B5_vec_vsro = { { &T_vec_s16, &T_vec_s8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsro:5", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(841), NULL };
static struct builtin B6_vec_vsro = { { &T_vec_s16, &T_vec_u8, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 0, "vec_vsro:6", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(842), NULL };
static struct builtin B7_vec_vsro = { { &T_vec_s32, &T_vec_s8, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsro:7", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(843), NULL };
static struct builtin B8_vec_vsro = { { &T_vec_s32, &T_vec_u8, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsro:8", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(844), NULL };
static struct builtin B9_vec_vsro = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsro:9", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(845), NULL };
static struct builtin B10_vec_vsro = { { &T_vec_s8, &T_vec_u8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 0, "vec_vsro:10", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(846), NULL };
static struct builtin B11_vec_vsro = { { &T_vec_u16, &T_vec_s8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsro:11", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(847), NULL };
static struct builtin B12_vec_vsro = { { &T_vec_u16, &T_vec_u8, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 0, "vec_vsro:12", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(848), NULL };
static struct builtin B13_vec_vsro = { { &T_vec_u32, &T_vec_s8, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsro:13", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(849), NULL };
static struct builtin B14_vec_vsro = { { &T_vec_u32, &T_vec_u8, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsro:14", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(850), NULL };
static struct builtin B15_vec_vsro = { { &T_vec_u8, &T_vec_s8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsro:15", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(851), NULL };
static struct builtin B16_vec_vsro = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 0, "vec_vsro:16", "vsro", CODE_FOR_xfxx_perm_bug, B_UID(852), NULL };
static struct builtin B1_vec_stvx = { { &T_vec_b16, &T_int, &T_vec_b16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:1", "stvx", CODE_FOR_sfxii_store, B_UID(853), NULL };
static struct builtin B2_vec_stvx = { { &T_vec_b32, &T_int, &T_vec_b32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:2", "stvx", CODE_FOR_sfxii_store, B_UID(854), NULL };
static struct builtin B3_vec_stvx = { { &T_vec_b8, &T_int, &T_vec_b8_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:3", "stvx", CODE_FOR_sfxii_store, B_UID(855), NULL };
static struct builtin B4_vec_stvx = { { &T_vec_f32, &T_int, &T_float_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:4", "stvx", CODE_FOR_sfxii_store, B_UID(856), NULL };
static struct builtin B5_vec_stvx = { { &T_vec_f32, &T_int, &T_vec_f32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:5", "stvx", CODE_FOR_sfxii_store, B_UID(857), NULL };
static struct builtin B6_vec_stvx = { { &T_vec_p16, &T_int, &T_vec_p16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:6", "stvx", CODE_FOR_sfxii_store, B_UID(858), NULL };
static struct builtin B7_vec_stvx = { { &T_vec_s16, &T_int, &T_short_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:7", "stvx", CODE_FOR_sfxii_store, B_UID(859), NULL };
static struct builtin B8_vec_stvx = { { &T_vec_s16, &T_int, &T_vec_s16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:8", "stvx", CODE_FOR_sfxii_store, B_UID(860), NULL };
static struct builtin B9_vec_stvx = { { &T_vec_s32, &T_int, &T_int_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:9", "stvx", CODE_FOR_sfxii_store, B_UID(861), NULL };
static struct builtin B10_vec_stvx = { { &T_vec_s32, &T_int, &T_long_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:10", "stvx", CODE_FOR_sfxii_store, B_UID(862), NULL };
static struct builtin B11_vec_stvx = { { &T_vec_s32, &T_int, &T_vec_s32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:11", "stvx", CODE_FOR_sfxii_store, B_UID(863), NULL };
static struct builtin B12_vec_stvx = { { &T_vec_s8, &T_int, &T_signed_char_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:12", "stvx", CODE_FOR_sfxii_store, B_UID(864), NULL };
static struct builtin B13_vec_stvx = { { &T_vec_s8, &T_int, &T_vec_s8_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:13", "stvx", CODE_FOR_sfxii_store, B_UID(865), NULL };
static struct builtin B14_vec_stvx = { { &T_vec_u16, &T_int, &T_unsigned_short_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:14", "stvx", CODE_FOR_sfxii_store, B_UID(866), NULL };
static struct builtin B15_vec_stvx = { { &T_vec_u16, &T_int, &T_vec_u16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:15", "stvx", CODE_FOR_sfxii_store, B_UID(867), NULL };
static struct builtin B16_vec_stvx = { { &T_vec_u32, &T_int, &T_unsigned_int_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:16", "stvx", CODE_FOR_sfxii_store, B_UID(868), NULL };
static struct builtin B17_vec_stvx = { { &T_vec_u32, &T_int, &T_unsigned_long_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:17", "stvx", CODE_FOR_sfxii_store, B_UID(869), NULL };
static struct builtin B18_vec_stvx = { { &T_vec_u32, &T_int, &T_vec_u32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:18", "stvx", CODE_FOR_sfxii_store, B_UID(870), NULL };
static struct builtin B19_vec_stvx = { { &T_vec_u8, &T_int, &T_unsigned_char_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:19", "stvx", CODE_FOR_sfxii_store, B_UID(871), NULL };
static struct builtin B20_vec_stvx = { { &T_vec_u8, &T_int, &T_vec_u8_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvx:20", "stvx", CODE_FOR_sfxii_store, B_UID(872), NULL };
static struct builtin B1_vec_stvewx = { { &T_vec_f32, &T_int, &T_float_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvewx:1", "stvewx", CODE_FOR_sfxii_store, B_UID(873), NULL };
static struct builtin B1_vec_stvehx = { { &T_vec_s16, &T_int, &T_short_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvehx:1", "stvehx", CODE_FOR_sfxii_store, B_UID(874), NULL };
static struct builtin B2_vec_stvewx = { { &T_vec_s32, &T_int, &T_int_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvewx:2", "stvewx", CODE_FOR_sfxii_store, B_UID(875), NULL };
static struct builtin B3_vec_stvewx = { { &T_vec_s32, &T_int, &T_long_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvewx:3", "stvewx", CODE_FOR_sfxii_store, B_UID(876), NULL };
static struct builtin B1_vec_stvebx = { { &T_vec_s8, &T_int, &T_signed_char_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvebx:1", "stvebx", CODE_FOR_sfxii_store, B_UID(877), NULL };
static struct builtin B2_vec_stvehx = { { &T_vec_u16, &T_int, &T_unsigned_short_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvehx:2", "stvehx", CODE_FOR_sfxii_store, B_UID(878), NULL };
static struct builtin B4_vec_stvewx = { { &T_vec_u32, &T_int, &T_unsigned_int_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvewx:4", "stvewx", CODE_FOR_sfxii_store, B_UID(879), NULL };
static struct builtin B5_vec_stvewx = { { &T_vec_u32, &T_int, &T_unsigned_long_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvewx:5", "stvewx", CODE_FOR_sfxii_store, B_UID(880), NULL };
static struct builtin B2_vec_stvebx = { { &T_vec_u8, &T_int, &T_unsigned_char_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvebx:2", "stvebx", CODE_FOR_sfxii_store, B_UID(881), NULL };
static struct builtin B1_vec_stvxl = { { &T_vec_b16, &T_int, &T_vec_b16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:1", "stvxl", CODE_FOR_sfxii_store, B_UID(882), NULL };
static struct builtin B2_vec_stvxl = { { &T_vec_b32, &T_int, &T_vec_b32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:2", "stvxl", CODE_FOR_sfxii_store, B_UID(883), NULL };
static struct builtin B3_vec_stvxl = { { &T_vec_b8, &T_int, &T_vec_b8_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:3", "stvxl", CODE_FOR_sfxii_store, B_UID(884), NULL };
static struct builtin B4_vec_stvxl = { { &T_vec_f32, &T_int, &T_float_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:4", "stvxl", CODE_FOR_sfxii_store, B_UID(885), NULL };
static struct builtin B5_vec_stvxl = { { &T_vec_f32, &T_int, &T_vec_f32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:5", "stvxl", CODE_FOR_sfxii_store, B_UID(886), NULL };
static struct builtin B6_vec_stvxl = { { &T_vec_p16, &T_int, &T_vec_p16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:6", "stvxl", CODE_FOR_sfxii_store, B_UID(887), NULL };
static struct builtin B7_vec_stvxl = { { &T_vec_s16, &T_int, &T_short_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:7", "stvxl", CODE_FOR_sfxii_store, B_UID(888), NULL };
static struct builtin B8_vec_stvxl = { { &T_vec_s16, &T_int, &T_vec_s16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:8", "stvxl", CODE_FOR_sfxii_store, B_UID(889), NULL };
static struct builtin B9_vec_stvxl = { { &T_vec_s32, &T_int, &T_int_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:9", "stvxl", CODE_FOR_sfxii_store, B_UID(890), NULL };
static struct builtin B10_vec_stvxl = { { &T_vec_s32, &T_int, &T_long_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:10", "stvxl", CODE_FOR_sfxii_store, B_UID(891), NULL };
static struct builtin B11_vec_stvxl = { { &T_vec_s32, &T_int, &T_vec_s32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:11", "stvxl", CODE_FOR_sfxii_store, B_UID(892), NULL };
static struct builtin B12_vec_stvxl = { { &T_vec_s8, &T_int, &T_signed_char_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:12", "stvxl", CODE_FOR_sfxii_store, B_UID(893), NULL };
static struct builtin B13_vec_stvxl = { { &T_vec_s8, &T_int, &T_vec_s8_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:13", "stvxl", CODE_FOR_sfxii_store, B_UID(894), NULL };
static struct builtin B14_vec_stvxl = { { &T_vec_u16, &T_int, &T_unsigned_short_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:14", "stvxl", CODE_FOR_sfxii_store, B_UID(895), NULL };
static struct builtin B15_vec_stvxl = { { &T_vec_u16, &T_int, &T_vec_u16_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:15", "stvxl", CODE_FOR_sfxii_store, B_UID(896), NULL };
static struct builtin B16_vec_stvxl = { { &T_vec_u32, &T_int, &T_unsigned_int_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:16", "stvxl", CODE_FOR_sfxii_store, B_UID(897), NULL };
static struct builtin B17_vec_stvxl = { { &T_vec_u32, &T_int, &T_unsigned_long_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:17", "stvxl", CODE_FOR_sfxii_store, B_UID(898), NULL };
static struct builtin B18_vec_stvxl = { { &T_vec_u32, &T_int, &T_vec_u32_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:18", "stvxl", CODE_FOR_sfxii_store, B_UID(899), NULL };
static struct builtin B19_vec_stvxl = { { &T_vec_u8, &T_int, &T_unsigned_char_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:19", "stvxl", CODE_FOR_sfxii_store, B_UID(900), NULL };
static struct builtin B20_vec_stvxl = { { &T_vec_u8, &T_int, &T_vec_u8_ptr, }, "xii", &T_void, 3, FALSE, FALSE, 0, "vec_stvxl:20", "stvxl", CODE_FOR_sfxii_store, B_UID(901), NULL };
static struct builtin B1_vec_vsubuhm = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vsubuhm:1", "vsubuhm", CODE_FOR_xfxx_simple, B_UID(902), NULL };
static struct builtin B2_vec_vsubuhm = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vsubuhm:2", "vsubuhm", CODE_FOR_xfxx_simple, B_UID(903), NULL };
static struct builtin B1_vec_vsubuwm = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vsubuwm:1", "vsubuwm", CODE_FOR_xfxx_simple, B_UID(904), NULL };
static struct builtin B2_vec_vsubuwm = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vsubuwm:2", "vsubuwm", CODE_FOR_xfxx_simple, B_UID(905), NULL };
static struct builtin B1_vec_vsububm = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vsububm:1", "vsububm", CODE_FOR_xfxx_simple, B_UID(906), NULL };
static struct builtin B2_vec_vsububm = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vsububm:2", "vsububm", CODE_FOR_xfxx_simple, B_UID(907), NULL };
static struct builtin B_vec_vsubfp = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vsubfp", "vsubfp", CODE_FOR_xfxx_fp, B_UID(908), NULL };
static struct builtin B3_vec_vsubuhm = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vsubuhm:3", "vsubuhm", CODE_FOR_xfxx_simple, B_UID(909), NULL };
static struct builtin B4_vec_vsubuhm = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vsubuhm:4", "vsubuhm", CODE_FOR_xfxx_simple, B_UID(910), NULL };
static struct builtin B3_vec_vsubuwm = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vsubuwm:3", "vsubuwm", CODE_FOR_xfxx_simple, B_UID(911), NULL };
static struct builtin B4_vec_vsubuwm = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vsubuwm:4", "vsubuwm", CODE_FOR_xfxx_simple, B_UID(912), NULL };
static struct builtin B3_vec_vsububm = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vsububm:3", "vsububm", CODE_FOR_xfxx_simple, B_UID(913), NULL };
static struct builtin B4_vec_vsububm = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vsububm:4", "vsububm", CODE_FOR_xfxx_simple, B_UID(914), NULL };
static struct builtin B5_vec_vsubuhm = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vsubuhm:5", "vsubuhm", CODE_FOR_xfxx_simple, B_UID(915), NULL };
static struct builtin B6_vec_vsubuhm = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vsubuhm:6", "vsubuhm", CODE_FOR_xfxx_simple, B_UID(916), NULL };
static struct builtin B5_vec_vsubuwm = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vsubuwm:5", "vsubuwm", CODE_FOR_xfxx_simple, B_UID(917), NULL };
static struct builtin B6_vec_vsubuwm = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vsubuwm:6", "vsubuwm", CODE_FOR_xfxx_simple, B_UID(918), NULL };
static struct builtin B5_vec_vsububm = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vsububm:5", "vsububm", CODE_FOR_xfxx_simple, B_UID(919), NULL };
static struct builtin B6_vec_vsububm = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vsububm:6", "vsububm", CODE_FOR_xfxx_simple, B_UID(920), NULL };
static struct builtin B_vec_vsubcuw = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsubcuw", "vsubcuw", CODE_FOR_xfxx_simple, B_UID(921), NULL };
static struct builtin B1_vec_vsubshs = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vsubshs:1", "vsubshs", CODE_FOR_xfxx_simple, B_UID(922), NULL };
static struct builtin B1_vec_vsubuhs = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vsubuhs:1", "vsubuhs", CODE_FOR_xfxx_simple, B_UID(923), NULL };
static struct builtin B1_vec_vsubsws = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vsubsws:1", "vsubsws", CODE_FOR_xfxx_simple, B_UID(924), NULL };
static struct builtin B1_vec_vsubuws = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vsubuws:1", "vsubuws", CODE_FOR_xfxx_simple, B_UID(925), NULL };
static struct builtin B1_vec_vsubsbs = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vsubsbs:1", "vsubsbs", CODE_FOR_xfxx_simple, B_UID(926), NULL };
static struct builtin B1_vec_vsububs = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vsububs:1", "vsububs", CODE_FOR_xfxx_simple, B_UID(927), NULL };
static struct builtin B2_vec_vsubshs = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vsubshs:2", "vsubshs", CODE_FOR_xfxx_simple, B_UID(928), NULL };
static struct builtin B3_vec_vsubshs = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vsubshs:3", "vsubshs", CODE_FOR_xfxx_simple, B_UID(929), NULL };
static struct builtin B2_vec_vsubsws = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vsubsws:2", "vsubsws", CODE_FOR_xfxx_simple, B_UID(930), NULL };
static struct builtin B3_vec_vsubsws = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vsubsws:3", "vsubsws", CODE_FOR_xfxx_simple, B_UID(931), NULL };
static struct builtin B2_vec_vsubsbs = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vsubsbs:2", "vsubsbs", CODE_FOR_xfxx_simple, B_UID(932), NULL };
static struct builtin B3_vec_vsubsbs = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vsubsbs:3", "vsubsbs", CODE_FOR_xfxx_simple, B_UID(933), NULL };
static struct builtin B2_vec_vsubuhs = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vsubuhs:2", "vsubuhs", CODE_FOR_xfxx_simple, B_UID(934), NULL };
static struct builtin B3_vec_vsubuhs = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vsubuhs:3", "vsubuhs", CODE_FOR_xfxx_simple, B_UID(935), NULL };
static struct builtin B2_vec_vsubuws = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vsubuws:2", "vsubuws", CODE_FOR_xfxx_simple, B_UID(936), NULL };
static struct builtin B3_vec_vsubuws = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vsubuws:3", "vsubuws", CODE_FOR_xfxx_simple, B_UID(937), NULL };
static struct builtin B2_vec_vsububs = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vsububs:2", "vsububs", CODE_FOR_xfxx_simple, B_UID(938), NULL };
static struct builtin B3_vec_vsububs = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vsububs:3", "vsububs", CODE_FOR_xfxx_simple, B_UID(939), NULL };
static struct builtin B_vec_vsum2sws = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsum2sws", "vsum2sws", CODE_FOR_xfxx_complex, B_UID(940), NULL };
static struct builtin B_vec_vsum4shs = { { &T_vec_s16, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsum4shs", "vsum4shs", CODE_FOR_xfxx_complex, B_UID(941), NULL };
static struct builtin B_vec_vsum4sbs = { { &T_vec_s8, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsum4sbs", "vsum4sbs", CODE_FOR_xfxx_complex, B_UID(942), NULL };
static struct builtin B_vec_vsum4ubs = { { &T_vec_u8, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 0, "vec_vsum4ubs", "vsum4ubs", CODE_FOR_xfxx_complex, B_UID(943), NULL };
static struct builtin B_vec_vsumsws = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 0, "vec_vsumsws", "vsumsws", CODE_FOR_xfxx_complex, B_UID(944), NULL };
static struct builtin B_vec_vrfiz = { { &T_vec_f32, NULL, NULL, }, "x", &T_vec_f32, 1, FALSE, FALSE, 0, "vec_vrfiz", "vrfiz", CODE_FOR_xfx_fp, B_UID(945), NULL };
static struct builtin B1_vec_vupkhsh = { { &T_vec_b16, NULL, NULL, }, "x", &T_vec_b32, 1, FALSE, FALSE, 0, "vec_vupkhsh:1", "vupkhsh", CODE_FOR_xfx_perm, B_UID(946), NULL };
static struct builtin B1_vec_vupkhsb = { { &T_vec_b8, NULL, NULL, }, "x", &T_vec_b16, 1, FALSE, FALSE, 0, "vec_vupkhsb:1", "vupkhsb", CODE_FOR_xfx_perm, B_UID(947), NULL };
static struct builtin B_vec_vupkhpx = { { &T_vec_p16, NULL, NULL, }, "x", &T_vec_u32, 1, FALSE, FALSE, 0, "vec_vupkhpx", "vupkhpx", CODE_FOR_xfx_perm, B_UID(948), NULL };
static struct builtin B2_vec_vupkhsh = { { &T_vec_s16, NULL, NULL, }, "x", &T_vec_s32, 1, FALSE, FALSE, 0, "vec_vupkhsh:2", "vupkhsh", CODE_FOR_xfx_perm, B_UID(949), NULL };
static struct builtin B2_vec_vupkhsb = { { &T_vec_s8, NULL, NULL, }, "x", &T_vec_s16, 1, FALSE, FALSE, 0, "vec_vupkhsb:2", "vupkhsb", CODE_FOR_xfx_perm, B_UID(950), NULL };
static struct builtin B1_vec_vupklsh = { { &T_vec_b16, NULL, NULL, }, "x", &T_vec_b32, 1, FALSE, FALSE, 0, "vec_vupklsh:1", "vupklsh", CODE_FOR_xfx_perm, B_UID(951), NULL };
static struct builtin B1_vec_vupklsb = { { &T_vec_b8, NULL, NULL, }, "x", &T_vec_b16, 1, FALSE, FALSE, 0, "vec_vupklsb:1", "vupklsb", CODE_FOR_xfx_perm, B_UID(952), NULL };
static struct builtin B_vec_vupklpx = { { &T_vec_p16, NULL, NULL, }, "x", &T_vec_u32, 1, FALSE, FALSE, 0, "vec_vupklpx", "vupklpx", CODE_FOR_xfx_perm, B_UID(953), NULL };
static struct builtin B2_vec_vupklsh = { { &T_vec_s16, NULL, NULL, }, "x", &T_vec_s32, 1, FALSE, FALSE, 0, "vec_vupklsh:2", "vupklsh", CODE_FOR_xfx_perm, B_UID(954), NULL };
static struct builtin B2_vec_vupklsb = { { &T_vec_s8, NULL, NULL, }, "x", &T_vec_s16, 1, FALSE, FALSE, 0, "vec_vupklsb:2", "vupklsb", CODE_FOR_xfx_perm, B_UID(955), NULL };
static struct builtin B1_vec_vxor = { { &T_vec_b16, &T_vec_b16, NULL, }, "xx", &T_vec_b16, 2, FALSE, FALSE, 1, "vec_vxor:1", "vxor", CODE_FOR_xfxx_simple, B_UID(956), NULL };
static struct builtin B2_vec_vxor = { { &T_vec_b16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vxor:2", "vxor", CODE_FOR_xfxx_simple, B_UID(957), NULL };
static struct builtin B3_vec_vxor = { { &T_vec_b16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vxor:3", "vxor", CODE_FOR_xfxx_simple, B_UID(958), NULL };
static struct builtin B4_vec_vxor = { { &T_vec_b32, &T_vec_b32, NULL, }, "xx", &T_vec_b32, 2, FALSE, FALSE, 1, "vec_vxor:4", "vxor", CODE_FOR_xfxx_simple, B_UID(959), NULL };
static struct builtin B5_vec_vxor = { { &T_vec_b32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vxor:5", "vxor", CODE_FOR_xfxx_simple, B_UID(960), NULL };
static struct builtin B6_vec_vxor = { { &T_vec_b32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vxor:6", "vxor", CODE_FOR_xfxx_simple, B_UID(961), NULL };
static struct builtin B7_vec_vxor = { { &T_vec_b32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vxor:7", "vxor", CODE_FOR_xfxx_simple, B_UID(962), NULL };
static struct builtin B8_vec_vxor = { { &T_vec_b8, &T_vec_b8, NULL, }, "xx", &T_vec_b8, 2, FALSE, FALSE, 1, "vec_vxor:8", "vxor", CODE_FOR_xfxx_simple, B_UID(963), NULL };
static struct builtin B9_vec_vxor = { { &T_vec_b8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vxor:9", "vxor", CODE_FOR_xfxx_simple, B_UID(964), NULL };
static struct builtin B10_vec_vxor = { { &T_vec_b8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vxor:10", "vxor", CODE_FOR_xfxx_simple, B_UID(965), NULL };
static struct builtin B11_vec_vxor = { { &T_vec_f32, &T_vec_b32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vxor:11", "vxor", CODE_FOR_xfxx_simple, B_UID(966), NULL };
static struct builtin B12_vec_vxor = { { &T_vec_f32, &T_vec_f32, NULL, }, "xx", &T_vec_f32, 2, FALSE, FALSE, 1, "vec_vxor:12", "vxor", CODE_FOR_xfxx_simple, B_UID(967), NULL };
static struct builtin B13_vec_vxor = { { &T_vec_s16, &T_vec_b16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vxor:13", "vxor", CODE_FOR_xfxx_simple, B_UID(968), NULL };
static struct builtin B14_vec_vxor = { { &T_vec_s16, &T_vec_s16, NULL, }, "xx", &T_vec_s16, 2, FALSE, FALSE, 1, "vec_vxor:14", "vxor", CODE_FOR_xfxx_simple, B_UID(969), NULL };
static struct builtin B15_vec_vxor = { { &T_vec_s32, &T_vec_b32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vxor:15", "vxor", CODE_FOR_xfxx_simple, B_UID(970), NULL };
static struct builtin B16_vec_vxor = { { &T_vec_s32, &T_vec_s32, NULL, }, "xx", &T_vec_s32, 2, FALSE, FALSE, 1, "vec_vxor:16", "vxor", CODE_FOR_xfxx_simple, B_UID(971), NULL };
static struct builtin B17_vec_vxor = { { &T_vec_s8, &T_vec_b8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vxor:17", "vxor", CODE_FOR_xfxx_simple, B_UID(972), NULL };
static struct builtin B18_vec_vxor = { { &T_vec_s8, &T_vec_s8, NULL, }, "xx", &T_vec_s8, 2, FALSE, FALSE, 1, "vec_vxor:18", "vxor", CODE_FOR_xfxx_simple, B_UID(973), NULL };
static struct builtin B19_vec_vxor = { { &T_vec_u16, &T_vec_b16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vxor:19", "vxor", CODE_FOR_xfxx_simple, B_UID(974), NULL };
static struct builtin B20_vec_vxor = { { &T_vec_u16, &T_vec_u16, NULL, }, "xx", &T_vec_u16, 2, FALSE, FALSE, 1, "vec_vxor:20", "vxor", CODE_FOR_xfxx_simple, B_UID(975), NULL };
static struct builtin B21_vec_vxor = { { &T_vec_u32, &T_vec_b32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vxor:21", "vxor", CODE_FOR_xfxx_simple, B_UID(976), NULL };
static struct builtin B22_vec_vxor = { { &T_vec_u32, &T_vec_u32, NULL, }, "xx", &T_vec_u32, 2, FALSE, FALSE, 1, "vec_vxor:22", "vxor", CODE_FOR_xfxx_simple, B_UID(977), NULL };
static struct builtin B23_vec_vxor = { { &T_vec_u8, &T_vec_b8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vxor:23", "vxor", CODE_FOR_xfxx_simple, B_UID(978), NULL };
static struct builtin B24_vec_vxor = { { &T_vec_u8, &T_vec_u8, NULL, }, "xx", &T_vec_u8, 2, FALSE, FALSE, 1, "vec_vxor:24", "vxor", CODE_FOR_xfxx_simple, B_UID(979), NULL };
#define LAST_B_UID B_UID(980)

struct builtin *Builtin[] = {
  &B_vec_vabsfp,
  &B_vec_vabssh,
  &B_vec_vabssw,
  &B_vec_vabssb,
  &B_vec_vabsssh,
  &B_vec_vabsssw,
  &B_vec_vabsssb,
  &B1_vec_vadduhm,
  &B2_vec_vadduhm,
  &B1_vec_vadduwm,
  &B2_vec_vadduwm,
  &B1_vec_vaddubm,
  &B2_vec_vaddubm,
  &B_vec_vaddfp,
  &B3_vec_vadduhm,
  &B4_vec_vadduhm,
  &B3_vec_vadduwm,
  &B4_vec_vadduwm,
  &B3_vec_vaddubm,
  &B4_vec_vaddubm,
  &B5_vec_vadduhm,
  &B6_vec_vadduhm,
  &B5_vec_vadduwm,
  &B6_vec_vadduwm,
  &B5_vec_vaddubm,
  &B6_vec_vaddubm,
  &B_vec_vaddcuw,
  &B1_vec_vaddshs,
  &B1_vec_vadduhs,
  &B1_vec_vaddsws,
  &B1_vec_vadduws,
  &B1_vec_vaddsbs,
  &B1_vec_vaddubs,
  &B2_vec_vaddshs,
  &B3_vec_vaddshs,
  &B2_vec_vaddsws,
  &B3_vec_vaddsws,
  &B2_vec_vaddsbs,
  &B3_vec_vaddsbs,
  &B2_vec_vadduhs,
  &B3_vec_vadduhs,
  &B2_vec_vadduws,
  &B3_vec_vadduws,
  &B2_vec_vaddubs,
  &B3_vec_vaddubs,
  &B1_vec_all_eq,
  &B2_vec_all_eq,
  &B3_vec_all_eq,
  &B4_vec_all_eq,
  &B5_vec_all_eq,
  &B6_vec_all_eq,
  &B7_vec_all_eq,
  &B8_vec_all_eq,
  &B9_vec_all_eq,
  &B10_vec_all_eq,
  &B11_vec_all_eq,
  &B12_vec_all_eq,
  &B13_vec_all_eq,
  &B14_vec_all_eq,
  &B15_vec_all_eq,
  &B16_vec_all_eq,
  &B17_vec_all_eq,
  &B18_vec_all_eq,
  &B19_vec_all_eq,
  &B20_vec_all_eq,
  &B21_vec_all_eq,
  &B22_vec_all_eq,
  &B23_vec_all_eq,
  &B1_vec_all_ge,
  &B2_vec_all_ge,
  &B3_vec_all_ge,
  &B4_vec_all_ge,
  &B5_vec_all_ge,
  &B6_vec_all_ge,
  &B7_vec_all_ge,
  &B8_vec_all_ge,
  &B9_vec_all_ge,
  &B10_vec_all_ge,
  &B11_vec_all_ge,
  &B12_vec_all_ge,
  &B13_vec_all_ge,
  &B14_vec_all_ge,
  &B15_vec_all_ge,
  &B16_vec_all_ge,
  &B17_vec_all_ge,
  &B18_vec_all_ge,
  &B19_vec_all_ge,
  &B1_vec_all_gt,
  &B2_vec_all_gt,
  &B3_vec_all_gt,
  &B4_vec_all_gt,
  &B5_vec_all_gt,
  &B6_vec_all_gt,
  &B7_vec_all_gt,
  &B8_vec_all_gt,
  &B9_vec_all_gt,
  &B10_vec_all_gt,
  &B11_vec_all_gt,
  &B12_vec_all_gt,
  &B13_vec_all_gt,
  &B14_vec_all_gt,
  &B15_vec_all_gt,
  &B16_vec_all_gt,
  &B17_vec_all_gt,
  &B18_vec_all_gt,
  &B19_vec_all_gt,
  &B_vec_all_in,
  &B1_vec_all_le,
  &B2_vec_all_le,
  &B3_vec_all_le,
  &B4_vec_all_le,
  &B5_vec_all_le,
  &B6_vec_all_le,
  &B7_vec_all_le,
  &B8_vec_all_le,
  &B9_vec_all_le,
  &B10_vec_all_le,
  &B11_vec_all_le,
  &B12_vec_all_le,
  &B13_vec_all_le,
  &B14_vec_all_le,
  &B15_vec_all_le,
  &B16_vec_all_le,
  &B17_vec_all_le,
  &B18_vec_all_le,
  &B19_vec_all_le,
  &B1_vec_all_lt,
  &B2_vec_all_lt,
  &B3_vec_all_lt,
  &B4_vec_all_lt,
  &B5_vec_all_lt,
  &B6_vec_all_lt,
  &B7_vec_all_lt,
  &B8_vec_all_lt,
  &B9_vec_all_lt,
  &B10_vec_all_lt,
  &B11_vec_all_lt,
  &B12_vec_all_lt,
  &B13_vec_all_lt,
  &B14_vec_all_lt,
  &B15_vec_all_lt,
  &B16_vec_all_lt,
  &B17_vec_all_lt,
  &B18_vec_all_lt,
  &B19_vec_all_lt,
  &B_vec_all_nan,
  &B1_vec_all_ne,
  &B2_vec_all_ne,
  &B3_vec_all_ne,
  &B4_vec_all_ne,
  &B5_vec_all_ne,
  &B6_vec_all_ne,
  &B7_vec_all_ne,
  &B8_vec_all_ne,
  &B9_vec_all_ne,
  &B10_vec_all_ne,
  &B11_vec_all_ne,
  &B12_vec_all_ne,
  &B13_vec_all_ne,
  &B14_vec_all_ne,
  &B15_vec_all_ne,
  &B16_vec_all_ne,
  &B17_vec_all_ne,
  &B18_vec_all_ne,
  &B19_vec_all_ne,
  &B20_vec_all_ne,
  &B21_vec_all_ne,
  &B22_vec_all_ne,
  &B23_vec_all_ne,
  &B_vec_all_nge,
  &B_vec_all_ngt,
  &B_vec_all_nle,
  &B_vec_all_nlt,
  &B_vec_all_numeric,
  &B1_vec_vand,
  &B2_vec_vand,
  &B3_vec_vand,
  &B4_vec_vand,
  &B5_vec_vand,
  &B6_vec_vand,
  &B7_vec_vand,
  &B8_vec_vand,
  &B9_vec_vand,
  &B10_vec_vand,
  &B11_vec_vand,
  &B12_vec_vand,
  &B13_vec_vand,
  &B14_vec_vand,
  &B15_vec_vand,
  &B16_vec_vand,
  &B17_vec_vand,
  &B18_vec_vand,
  &B19_vec_vand,
  &B20_vec_vand,
  &B21_vec_vand,
  &B22_vec_vand,
  &B23_vec_vand,
  &B24_vec_vand,
  &B1_vec_vandc,
  &B2_vec_vandc,
  &B3_vec_vandc,
  &B4_vec_vandc,
  &B5_vec_vandc,
  &B6_vec_vandc,
  &B7_vec_vandc,
  &B8_vec_vandc,
  &B9_vec_vandc,
  &B10_vec_vandc,
  &B11_vec_vandc,
  &B12_vec_vandc,
  &B13_vec_vandc,
  &B14_vec_vandc,
  &B15_vec_vandc,
  &B16_vec_vandc,
  &B17_vec_vandc,
  &B18_vec_vandc,
  &B19_vec_vandc,
  &B20_vec_vandc,
  &B21_vec_vandc,
  &B22_vec_vandc,
  &B23_vec_vandc,
  &B24_vec_vandc,
  &B1_vec_any_eq,
  &B2_vec_any_eq,
  &B3_vec_any_eq,
  &B4_vec_any_eq,
  &B5_vec_any_eq,
  &B6_vec_any_eq,
  &B7_vec_any_eq,
  &B8_vec_any_eq,
  &B9_vec_any_eq,
  &B10_vec_any_eq,
  &B11_vec_any_eq,
  &B12_vec_any_eq,
  &B13_vec_any_eq,
  &B14_vec_any_eq,
  &B15_vec_any_eq,
  &B16_vec_any_eq,
  &B17_vec_any_eq,
  &B18_vec_any_eq,
  &B19_vec_any_eq,
  &B20_vec_any_eq,
  &B21_vec_any_eq,
  &B22_vec_any_eq,
  &B23_vec_any_eq,
  &B1_vec_any_ge,
  &B2_vec_any_ge,
  &B3_vec_any_ge,
  &B4_vec_any_ge,
  &B5_vec_any_ge,
  &B6_vec_any_ge,
  &B7_vec_any_ge,
  &B8_vec_any_ge,
  &B9_vec_any_ge,
  &B10_vec_any_ge,
  &B11_vec_any_ge,
  &B12_vec_any_ge,
  &B13_vec_any_ge,
  &B14_vec_any_ge,
  &B15_vec_any_ge,
  &B16_vec_any_ge,
  &B17_vec_any_ge,
  &B18_vec_any_ge,
  &B19_vec_any_ge,
  &B1_vec_any_gt,
  &B2_vec_any_gt,
  &B3_vec_any_gt,
  &B4_vec_any_gt,
  &B5_vec_any_gt,
  &B6_vec_any_gt,
  &B7_vec_any_gt,
  &B8_vec_any_gt,
  &B9_vec_any_gt,
  &B10_vec_any_gt,
  &B11_vec_any_gt,
  &B12_vec_any_gt,
  &B13_vec_any_gt,
  &B14_vec_any_gt,
  &B15_vec_any_gt,
  &B16_vec_any_gt,
  &B17_vec_any_gt,
  &B18_vec_any_gt,
  &B19_vec_any_gt,
  &B1_vec_any_le,
  &B2_vec_any_le,
  &B3_vec_any_le,
  &B4_vec_any_le,
  &B5_vec_any_le,
  &B6_vec_any_le,
  &B7_vec_any_le,
  &B8_vec_any_le,
  &B9_vec_any_le,
  &B10_vec_any_le,
  &B11_vec_any_le,
  &B12_vec_any_le,
  &B13_vec_any_le,
  &B14_vec_any_le,
  &B15_vec_any_le,
  &B16_vec_any_le,
  &B17_vec_any_le,
  &B18_vec_any_le,
  &B19_vec_any_le,
  &B1_vec_any_lt,
  &B2_vec_any_lt,
  &B3_vec_any_lt,
  &B4_vec_any_lt,
  &B5_vec_any_lt,
  &B6_vec_any_lt,
  &B7_vec_any_lt,
  &B8_vec_any_lt,
  &B9_vec_any_lt,
  &B10_vec_any_lt,
  &B11_vec_any_lt,
  &B12_vec_any_lt,
  &B13_vec_any_lt,
  &B14_vec_any_lt,
  &B15_vec_any_lt,
  &B16_vec_any_lt,
  &B17_vec_any_lt,
  &B18_vec_any_lt,
  &B19_vec_any_lt,
  &B_vec_any_nan,
  &B1_vec_any_ne,
  &B2_vec_any_ne,
  &B3_vec_any_ne,
  &B4_vec_any_ne,
  &B5_vec_any_ne,
  &B6_vec_any_ne,
  &B7_vec_any_ne,
  &B8_vec_any_ne,
  &B9_vec_any_ne,
  &B10_vec_any_ne,
  &B11_vec_any_ne,
  &B12_vec_any_ne,
  &B13_vec_any_ne,
  &B14_vec_any_ne,
  &B15_vec_any_ne,
  &B16_vec_any_ne,
  &B17_vec_any_ne,
  &B18_vec_any_ne,
  &B19_vec_any_ne,
  &B20_vec_any_ne,
  &B21_vec_any_ne,
  &B22_vec_any_ne,
  &B23_vec_any_ne,
  &B_vec_any_nge,
  &B_vec_any_ngt,
  &B_vec_any_nle,
  &B_vec_any_nlt,
  &B_vec_any_numeric,
  &B_vec_any_out,
  &B_vec_vavgsh,
  &B_vec_vavgsw,
  &B_vec_vavgsb,
  &B_vec_vavguh,
  &B_vec_vavguw,
  &B_vec_vavgub,
  &B_vec_vrfip,
  &B_vec_vcmpbfp,
  &B_vec_vcmpeqfp,
  &B1_vec_vcmpequh,
  &B1_vec_vcmpequw,
  &B1_vec_vcmpequb,
  &B2_vec_vcmpequh,
  &B2_vec_vcmpequw,
  &B2_vec_vcmpequb,
  &B_vec_vcmpgefp,
  &B_vec_vcmpgtfp,
  &B_vec_vcmpgtsh,
  &B_vec_vcmpgtsw,
  &B_vec_vcmpgtsb,
  &B_vec_vcmpgtuh,
  &B_vec_vcmpgtuw,
  &B_vec_vcmpgtub,
  &B_vec_vcmplefp,
  &B_vec_vcmpltfp,
  &B_vec_vcmpltsh,
  &B_vec_vcmpltsw,
  &B_vec_vcmpltsb,
  &B_vec_vcmpltuh,
  &B_vec_vcmpltuw,
  &B_vec_vcmpltub,
  &B_vec_vcfsx,
  &B_vec_vcfux,
  &B_vec_vctsxs,
  &B_vec_vctuxs,
  &B_vec_dss,
  &B_vec_dssall,
  &B1_vec_dst,
  &B2_vec_dst,
  &B3_vec_dst,
  &B4_vec_dst,
  &B5_vec_dst,
  &B6_vec_dst,
  &B7_vec_dst,
  &B8_vec_dst,
  &B9_vec_dst,
  &B10_vec_dst,
  &B11_vec_dst,
  &B12_vec_dst,
  &B13_vec_dst,
  &B14_vec_dst,
  &B15_vec_dst,
  &B16_vec_dst,
  &B17_vec_dst,
  &B18_vec_dst,
  &B19_vec_dst,
  &B20_vec_dst,
  &B1_vec_dstst,
  &B2_vec_dstst,
  &B3_vec_dstst,
  &B4_vec_dstst,
  &B5_vec_dstst,
  &B6_vec_dstst,
  &B7_vec_dstst,
  &B8_vec_dstst,
  &B9_vec_dstst,
  &B10_vec_dstst,
  &B11_vec_dstst,
  &B12_vec_dstst,
  &B13_vec_dstst,
  &B14_vec_dstst,
  &B15_vec_dstst,
  &B16_vec_dstst,
  &B17_vec_dstst,
  &B18_vec_dstst,
  &B19_vec_dstst,
  &B20_vec_dstst,
  &B1_vec_dststt,
  &B2_vec_dststt,
  &B3_vec_dststt,
  &B4_vec_dststt,
  &B5_vec_dststt,
  &B6_vec_dststt,
  &B7_vec_dststt,
  &B8_vec_dststt,
  &B9_vec_dststt,
  &B10_vec_dststt,
  &B11_vec_dststt,
  &B12_vec_dststt,
  &B13_vec_dststt,
  &B14_vec_dststt,
  &B15_vec_dststt,
  &B16_vec_dststt,
  &B17_vec_dststt,
  &B18_vec_dststt,
  &B19_vec_dststt,
  &B20_vec_dststt,
  &B1_vec_dstt,
  &B2_vec_dstt,
  &B3_vec_dstt,
  &B4_vec_dstt,
  &B5_vec_dstt,
  &B6_vec_dstt,
  &B7_vec_dstt,
  &B8_vec_dstt,
  &B9_vec_dstt,
  &B10_vec_dstt,
  &B11_vec_dstt,
  &B12_vec_dstt,
  &B13_vec_dstt,
  &B14_vec_dstt,
  &B15_vec_dstt,
  &B16_vec_dstt,
  &B17_vec_dstt,
  &B18_vec_dstt,
  &B19_vec_dstt,
  &B20_vec_dstt,
  &B_vec_vexptefp,
  &B_vec_vrfim,
  &B1_vec_lvx,
  &B2_vec_lvx,
  &B3_vec_lvx,
  &B4_vec_lvx,
  &B5_vec_lvx,
  &B6_vec_lvx,
  &B7_vec_lvx,
  &B8_vec_lvx,
  &B9_vec_lvx,
  &B10_vec_lvx,
  &B11_vec_lvx,
  &B12_vec_lvx,
  &B13_vec_lvx,
  &B14_vec_lvx,
  &B15_vec_lvx,
  &B16_vec_lvx,
  &B17_vec_lvx,
  &B18_vec_lvx,
  &B19_vec_lvx,
  &B20_vec_lvx,
  &B1_vec_lvewx,
  &B2_vec_lvewx,
  &B3_vec_lvewx,
  &B1_vec_lvehx,
  &B1_vec_lvebx,
  &B2_vec_lvebx,
  &B4_vec_lvewx,
  &B5_vec_lvewx,
  &B2_vec_lvehx,
  &B1_vec_lvxl,
  &B2_vec_lvxl,
  &B3_vec_lvxl,
  &B4_vec_lvxl,
  &B5_vec_lvxl,
  &B6_vec_lvxl,
  &B7_vec_lvxl,
  &B8_vec_lvxl,
  &B9_vec_lvxl,
  &B10_vec_lvxl,
  &B11_vec_lvxl,
  &B12_vec_lvxl,
  &B13_vec_lvxl,
  &B14_vec_lvxl,
  &B15_vec_lvxl,
  &B16_vec_lvxl,
  &B17_vec_lvxl,
  &B18_vec_lvxl,
  &B19_vec_lvxl,
  &B20_vec_lvxl,
  &B_vec_vlogefp,
  &B1_vec_lvsl,
  &B2_vec_lvsl,
  &B3_vec_lvsl,
  &B4_vec_lvsl,
  &B5_vec_lvsl,
  &B6_vec_lvsl,
  &B7_vec_lvsl,
  &B8_vec_lvsl,
  &B9_vec_lvsl,
  &B1_vec_lvsr,
  &B2_vec_lvsr,
  &B3_vec_lvsr,
  &B4_vec_lvsr,
  &B5_vec_lvsr,
  &B6_vec_lvsr,
  &B7_vec_lvsr,
  &B8_vec_lvsr,
  &B9_vec_lvsr,
  &B_vec_vmaddfp,
  &B_vec_vmhaddshs,
  &B1_vec_vmaxsh,
  &B1_vec_vmaxuh,
  &B1_vec_vmaxsw,
  &B1_vec_vmaxuw,
  &B1_vec_vmaxsb,
  &B1_vec_vmaxub,
  &B_vec_vmaxfp,
  &B2_vec_vmaxsh,
  &B3_vec_vmaxsh,
  &B2_vec_vmaxsw,
  &B3_vec_vmaxsw,
  &B2_vec_vmaxsb,
  &B3_vec_vmaxsb,
  &B2_vec_vmaxuh,
  &B3_vec_vmaxuh,
  &B2_vec_vmaxuw,
  &B3_vec_vmaxuw,
  &B2_vec_vmaxub,
  &B3_vec_vmaxub,
  &B1_vec_vmrghh,
  &B1_vec_vmrghw,
  &B1_vec_vmrghb,
  &B2_vec_vmrghw,
  &B2_vec_vmrghh,
  &B3_vec_vmrghh,
  &B3_vec_vmrghw,
  &B2_vec_vmrghb,
  &B4_vec_vmrghh,
  &B4_vec_vmrghw,
  &B3_vec_vmrghb,
  &B1_vec_vmrglh,
  &B1_vec_vmrglw,
  &B1_vec_vmrglb,
  &B2_vec_vmrglw,
  &B2_vec_vmrglh,
  &B3_vec_vmrglh,
  &B3_vec_vmrglw,
  &B2_vec_vmrglb,
  &B4_vec_vmrglh,
  &B4_vec_vmrglw,
  &B3_vec_vmrglb,
  &B_vec_mfvscr,
  &B1_vec_vminsh,
  &B1_vec_vminuh,
  &B1_vec_vminsw,
  &B1_vec_vminuw,
  &B1_vec_vminsb,
  &B1_vec_vminub,
  &B_vec_vminfp,
  &B2_vec_vminsh,
  &B3_vec_vminsh,
  &B2_vec_vminsw,
  &B3_vec_vminsw,
  &B2_vec_vminsb,
  &B3_vec_vminsb,
  &B2_vec_vminuh,
  &B3_vec_vminuh,
  &B2_vec_vminuw,
  &B3_vec_vminuw,
  &B2_vec_vminub,
  &B3_vec_vminub,
  &B1_vec_vmladduhm,
  &B2_vec_vmladduhm,
  &B3_vec_vmladduhm,
  &B4_vec_vmladduhm,
  &B_vec_vmhraddshs,
  &B_vec_vmsumshm,
  &B_vec_vmsummbm,
  &B_vec_vmsumuhm,
  &B_vec_vmsumubm,
  &B_vec_vmsumshs,
  &B_vec_vmsumuhs,
  &B1_vec_mtvscr,
  &B2_vec_mtvscr,
  &B3_vec_mtvscr,
  &B4_vec_mtvscr,
  &B5_vec_mtvscr,
  &B6_vec_mtvscr,
  &B7_vec_mtvscr,
  &B8_vec_mtvscr,
  &B9_vec_mtvscr,
  &B10_vec_mtvscr,
  &B_vec_vmulesh,
  &B_vec_vmulesb,
  &B_vec_vmuleuh,
  &B_vec_vmuleub,
  &B_vec_vmulosh,
  &B_vec_vmulosb,
  &B_vec_vmulouh,
  &B_vec_vmuloub,
  &B_vec_vnmsubfp,
  &B1_vec_vnor,
  &B2_vec_vnor,
  &B3_vec_vnor,
  &B4_vec_vnor,
  &B5_vec_vnor,
  &B6_vec_vnor,
  &B7_vec_vnor,
  &B8_vec_vnor,
  &B9_vec_vnor,
  &B10_vec_vnor,
  &B1_vec_vor,
  &B2_vec_vor,
  &B3_vec_vor,
  &B4_vec_vor,
  &B5_vec_vor,
  &B6_vec_vor,
  &B7_vec_vor,
  &B8_vec_vor,
  &B9_vec_vor,
  &B10_vec_vor,
  &B11_vec_vor,
  &B12_vec_vor,
  &B13_vec_vor,
  &B14_vec_vor,
  &B15_vec_vor,
  &B16_vec_vor,
  &B17_vec_vor,
  &B18_vec_vor,
  &B19_vec_vor,
  &B20_vec_vor,
  &B21_vec_vor,
  &B22_vec_vor,
  &B23_vec_vor,
  &B24_vec_vor,
  &B1_vec_vpkuhum,
  &B1_vec_vpkuwum,
  &B2_vec_vpkuhum,
  &B2_vec_vpkuwum,
  &B3_vec_vpkuhum,
  &B3_vec_vpkuwum,
  &B_vec_vpkpx,
  &B_vec_vpkshss,
  &B_vec_vpkswss,
  &B_vec_vpkuhus,
  &B_vec_vpkuwus,
  &B_vec_vpkshus,
  &B_vec_vpkswus,
  &B1_vec_vperm,
  &B2_vec_vperm,
  &B3_vec_vperm,
  &B4_vec_vperm,
  &B5_vec_vperm,
  &B6_vec_vperm,
  &B7_vec_vperm,
  &B8_vec_vperm,
  &B9_vec_vperm,
  &B10_vec_vperm,
  &B11_vec_vperm,
  &B_vec_vrefp,
  &B1_vec_vrlh,
  &B1_vec_vrlw,
  &B1_vec_vrlb,
  &B2_vec_vrlh,
  &B2_vec_vrlw,
  &B2_vec_vrlb,
  &B_vec_vrfin,
  &B_vec_vrsqrtefp,
  &B1_vec_vsel,
  &B2_vec_vsel,
  &B3_vec_vsel,
  &B4_vec_vsel,
  &B5_vec_vsel,
  &B6_vec_vsel,
  &B7_vec_vsel,
  &B8_vec_vsel,
  &B9_vec_vsel,
  &B10_vec_vsel,
  &B11_vec_vsel,
  &B12_vec_vsel,
  &B13_vec_vsel,
  &B14_vec_vsel,
  &B15_vec_vsel,
  &B16_vec_vsel,
  &B17_vec_vsel,
  &B18_vec_vsel,
  &B19_vec_vsel,
  &B20_vec_vsel,
  &B1_vec_vslh,
  &B1_vec_vslw,
  &B1_vec_vslb,
  &B2_vec_vslh,
  &B2_vec_vslw,
  &B2_vec_vslb,
  &B1_vec_vsldoi,
  &B2_vec_vsldoi,
  &B3_vec_vsldoi,
  &B4_vec_vsldoi,
  &B5_vec_vsldoi,
  &B6_vec_vsldoi,
  &B7_vec_vsldoi,
  &B8_vec_vsldoi,
  &B1_vec_vsl,
  &B2_vec_vsl,
  &B3_vec_vsl,
  &B4_vec_vsl,
  &B5_vec_vsl,
  &B6_vec_vsl,
  &B7_vec_vsl,
  &B8_vec_vsl,
  &B9_vec_vsl,
  &B10_vec_vsl,
  &B11_vec_vsl,
  &B12_vec_vsl,
  &B13_vec_vsl,
  &B14_vec_vsl,
  &B15_vec_vsl,
  &B16_vec_vsl,
  &B17_vec_vsl,
  &B18_vec_vsl,
  &B19_vec_vsl,
  &B20_vec_vsl,
  &B21_vec_vsl,
  &B22_vec_vsl,
  &B23_vec_vsl,
  &B24_vec_vsl,
  &B25_vec_vsl,
  &B26_vec_vsl,
  &B27_vec_vsl,
  &B28_vec_vsl,
  &B29_vec_vsl,
  &B30_vec_vsl,
  &B1_vec_vslo,
  &B2_vec_vslo,
  &B3_vec_vslo,
  &B4_vec_vslo,
  &B5_vec_vslo,
  &B6_vec_vslo,
  &B7_vec_vslo,
  &B8_vec_vslo,
  &B9_vec_vslo,
  &B10_vec_vslo,
  &B11_vec_vslo,
  &B12_vec_vslo,
  &B13_vec_vslo,
  &B14_vec_vslo,
  &B15_vec_vslo,
  &B16_vec_vslo,
  &B1_vec_vsplth,
  &B1_vec_vspltw,
  &B1_vec_vspltb,
  &B2_vec_vspltw,
  &B2_vec_vsplth,
  &B3_vec_vsplth,
  &B3_vec_vspltw,
  &B2_vec_vspltb,
  &B4_vec_vsplth,
  &B4_vec_vspltw,
  &B3_vec_vspltb,
  &B_vec_vspltish,
  &B_vec_vspltisw,
  &B_vec_vspltisb,
  &B_vec_splat_u16,
  &B_vec_splat_u32,
  &B_vec_splat_u8,
  &B1_vec_vsrh,
  &B1_vec_vsrw,
  &B1_vec_vsrb,
  &B2_vec_vsrh,
  &B2_vec_vsrw,
  &B2_vec_vsrb,
  &B1_vec_vsrah,
  &B1_vec_vsraw,
  &B1_vec_vsrab,
  &B2_vec_vsrah,
  &B2_vec_vsraw,
  &B2_vec_vsrab,
  &B1_vec_vsr,
  &B2_vec_vsr,
  &B3_vec_vsr,
  &B4_vec_vsr,
  &B5_vec_vsr,
  &B6_vec_vsr,
  &B7_vec_vsr,
  &B8_vec_vsr,
  &B9_vec_vsr,
  &B10_vec_vsr,
  &B11_vec_vsr,
  &B12_vec_vsr,
  &B13_vec_vsr,
  &B14_vec_vsr,
  &B15_vec_vsr,
  &B16_vec_vsr,
  &B17_vec_vsr,
  &B18_vec_vsr,
  &B19_vec_vsr,
  &B20_vec_vsr,
  &B21_vec_vsr,
  &B22_vec_vsr,
  &B23_vec_vsr,
  &B24_vec_vsr,
  &B25_vec_vsr,
  &B26_vec_vsr,
  &B27_vec_vsr,
  &B28_vec_vsr,
  &B29_vec_vsr,
  &B30_vec_vsr,
  &B1_vec_vsro,
  &B2_vec_vsro,
  &B3_vec_vsro,
  &B4_vec_vsro,
  &B5_vec_vsro,
  &B6_vec_vsro,
  &B7_vec_vsro,
  &B8_vec_vsro,
  &B9_vec_vsro,
  &B10_vec_vsro,
  &B11_vec_vsro,
  &B12_vec_vsro,
  &B13_vec_vsro,
  &B14_vec_vsro,
  &B15_vec_vsro,
  &B16_vec_vsro,
  &B1_vec_stvx,
  &B2_vec_stvx,
  &B3_vec_stvx,
  &B4_vec_stvx,
  &B5_vec_stvx,
  &B6_vec_stvx,
  &B7_vec_stvx,
  &B8_vec_stvx,
  &B9_vec_stvx,
  &B10_vec_stvx,
  &B11_vec_stvx,
  &B12_vec_stvx,
  &B13_vec_stvx,
  &B14_vec_stvx,
  &B15_vec_stvx,
  &B16_vec_stvx,
  &B17_vec_stvx,
  &B18_vec_stvx,
  &B19_vec_stvx,
  &B20_vec_stvx,
  &B1_vec_stvewx,
  &B1_vec_stvehx,
  &B2_vec_stvewx,
  &B3_vec_stvewx,
  &B1_vec_stvebx,
  &B2_vec_stvehx,
  &B4_vec_stvewx,
  &B5_vec_stvewx,
  &B2_vec_stvebx,
  &B1_vec_stvxl,
  &B2_vec_stvxl,
  &B3_vec_stvxl,
  &B4_vec_stvxl,
  &B5_vec_stvxl,
  &B6_vec_stvxl,
  &B7_vec_stvxl,
  &B8_vec_stvxl,
  &B9_vec_stvxl,
  &B10_vec_stvxl,
  &B11_vec_stvxl,
  &B12_vec_stvxl,
  &B13_vec_stvxl,
  &B14_vec_stvxl,
  &B15_vec_stvxl,
  &B16_vec_stvxl,
  &B17_vec_stvxl,
  &B18_vec_stvxl,
  &B19_vec_stvxl,
  &B20_vec_stvxl,
  &B1_vec_vsubuhm,
  &B2_vec_vsubuhm,
  &B1_vec_vsubuwm,
  &B2_vec_vsubuwm,
  &B1_vec_vsububm,
  &B2_vec_vsububm,
  &B_vec_vsubfp,
  &B3_vec_vsubuhm,
  &B4_vec_vsubuhm,
  &B3_vec_vsubuwm,
  &B4_vec_vsubuwm,
  &B3_vec_vsububm,
  &B4_vec_vsububm,
  &B5_vec_vsubuhm,
  &B6_vec_vsubuhm,
  &B5_vec_vsubuwm,
  &B6_vec_vsubuwm,
  &B5_vec_vsububm,
  &B6_vec_vsububm,
  &B_vec_vsubcuw,
  &B1_vec_vsubshs,
  &B1_vec_vsubuhs,
  &B1_vec_vsubsws,
  &B1_vec_vsubuws,
  &B1_vec_vsubsbs,
  &B1_vec_vsububs,
  &B2_vec_vsubshs,
  &B3_vec_vsubshs,
  &B2_vec_vsubsws,
  &B3_vec_vsubsws,
  &B2_vec_vsubsbs,
  &B3_vec_vsubsbs,
  &B2_vec_vsubuhs,
  &B3_vec_vsubuhs,
  &B2_vec_vsubuws,
  &B3_vec_vsubuws,
  &B2_vec_vsububs,
  &B3_vec_vsububs,
  &B_vec_vsum2sws,
  &B_vec_vsum4shs,
  &B_vec_vsum4sbs,
  &B_vec_vsum4ubs,
  &B_vec_vsumsws,
  &B_vec_vrfiz,
  &B1_vec_vupkhsh,
  &B1_vec_vupkhsb,
  &B_vec_vupkhpx,
  &B2_vec_vupkhsh,
  &B2_vec_vupkhsb,
  &B1_vec_vupklsh,
  &B1_vec_vupklsb,
  &B_vec_vupklpx,
  &B2_vec_vupklsh,
  &B2_vec_vupklsb,
  &B1_vec_vxor,
  &B2_vec_vxor,
  &B3_vec_vxor,
  &B4_vec_vxor,
  &B5_vec_vxor,
  &B6_vec_vxor,
  &B7_vec_vxor,
  &B8_vec_vxor,
  &B9_vec_vxor,
  &B10_vec_vxor,
  &B11_vec_vxor,
  &B12_vec_vxor,
  &B13_vec_vxor,
  &B14_vec_vxor,
  &B15_vec_vxor,
  &B16_vec_vxor,
  &B17_vec_vxor,
  &B18_vec_vxor,
  &B19_vec_vxor,
  &B20_vec_vxor,
  &B21_vec_vxor,
  &B22_vec_vxor,
  &B23_vec_vxor,
  &B24_vec_vxor,
};

static struct builtin *O_vec_abs[4] = {
  &B_vec_vabsfp,
  &B_vec_vabssh,
  &B_vec_vabssw,
  &B_vec_vabssb,
};
static struct builtin *O_vec_abss[3] = {
  &B_vec_vabsssh,
  &B_vec_vabsssw,
  &B_vec_vabsssb,
};
static struct builtin *O_vec_add[19] = {
  &B1_vec_vadduhm,
  &B2_vec_vadduhm,
  &B1_vec_vadduwm,
  &B2_vec_vadduwm,
  &B1_vec_vaddubm,
  &B2_vec_vaddubm,
  &B_vec_vaddfp,
  &B3_vec_vadduhm,
  &B4_vec_vadduhm,
  &B3_vec_vadduwm,
  &B4_vec_vadduwm,
  &B3_vec_vaddubm,
  &B4_vec_vaddubm,
  &B5_vec_vadduhm,
  &B6_vec_vadduhm,
  &B5_vec_vadduwm,
  &B6_vec_vadduwm,
  &B5_vec_vaddubm,
  &B6_vec_vaddubm,
};
static struct builtin *O_vec_addc[1] = {
  &B_vec_vaddcuw,
};
static struct builtin *O_vec_adds[18] = {
  &B1_vec_vaddshs,
  &B1_vec_vadduhs,
  &B1_vec_vaddsws,
  &B1_vec_vadduws,
  &B1_vec_vaddsbs,
  &B1_vec_vaddubs,
  &B2_vec_vaddshs,
  &B3_vec_vaddshs,
  &B2_vec_vaddsws,
  &B3_vec_vaddsws,
  &B2_vec_vaddsbs,
  &B3_vec_vaddsbs,
  &B2_vec_vadduhs,
  &B3_vec_vadduhs,
  &B2_vec_vadduws,
  &B3_vec_vadduws,
  &B2_vec_vaddubs,
  &B3_vec_vaddubs,
};
static struct builtin *O_vec_all_eq[23] = {
  &B1_vec_all_eq,
  &B2_vec_all_eq,
  &B3_vec_all_eq,
  &B4_vec_all_eq,
  &B5_vec_all_eq,
  &B6_vec_all_eq,
  &B7_vec_all_eq,
  &B8_vec_all_eq,
  &B9_vec_all_eq,
  &B10_vec_all_eq,
  &B11_vec_all_eq,
  &B12_vec_all_eq,
  &B13_vec_all_eq,
  &B14_vec_all_eq,
  &B15_vec_all_eq,
  &B16_vec_all_eq,
  &B17_vec_all_eq,
  &B18_vec_all_eq,
  &B19_vec_all_eq,
  &B20_vec_all_eq,
  &B21_vec_all_eq,
  &B22_vec_all_eq,
  &B23_vec_all_eq,
};
static struct builtin *O_vec_all_ge[19] = {
  &B1_vec_all_ge,
  &B2_vec_all_ge,
  &B3_vec_all_ge,
  &B4_vec_all_ge,
  &B5_vec_all_ge,
  &B6_vec_all_ge,
  &B7_vec_all_ge,
  &B8_vec_all_ge,
  &B9_vec_all_ge,
  &B10_vec_all_ge,
  &B11_vec_all_ge,
  &B12_vec_all_ge,
  &B13_vec_all_ge,
  &B14_vec_all_ge,
  &B15_vec_all_ge,
  &B16_vec_all_ge,
  &B17_vec_all_ge,
  &B18_vec_all_ge,
  &B19_vec_all_ge,
};
static struct builtin *O_vec_all_gt[19] = {
  &B1_vec_all_gt,
  &B2_vec_all_gt,
  &B3_vec_all_gt,
  &B4_vec_all_gt,
  &B5_vec_all_gt,
  &B6_vec_all_gt,
  &B7_vec_all_gt,
  &B8_vec_all_gt,
  &B9_vec_all_gt,
  &B10_vec_all_gt,
  &B11_vec_all_gt,
  &B12_vec_all_gt,
  &B13_vec_all_gt,
  &B14_vec_all_gt,
  &B15_vec_all_gt,
  &B16_vec_all_gt,
  &B17_vec_all_gt,
  &B18_vec_all_gt,
  &B19_vec_all_gt,
};
static struct builtin *O_vec_all_in[1] = {
  &B_vec_all_in,
};
static struct builtin *O_vec_all_le[19] = {
  &B1_vec_all_le,
  &B2_vec_all_le,
  &B3_vec_all_le,
  &B4_vec_all_le,
  &B5_vec_all_le,
  &B6_vec_all_le,
  &B7_vec_all_le,
  &B8_vec_all_le,
  &B9_vec_all_le,
  &B10_vec_all_le,
  &B11_vec_all_le,
  &B12_vec_all_le,
  &B13_vec_all_le,
  &B14_vec_all_le,
  &B15_vec_all_le,
  &B16_vec_all_le,
  &B17_vec_all_le,
  &B18_vec_all_le,
  &B19_vec_all_le,
};
static struct builtin *O_vec_all_lt[19] = {
  &B1_vec_all_lt,
  &B2_vec_all_lt,
  &B3_vec_all_lt,
  &B4_vec_all_lt,
  &B5_vec_all_lt,
  &B6_vec_all_lt,
  &B7_vec_all_lt,
  &B8_vec_all_lt,
  &B9_vec_all_lt,
  &B10_vec_all_lt,
  &B11_vec_all_lt,
  &B12_vec_all_lt,
  &B13_vec_all_lt,
  &B14_vec_all_lt,
  &B15_vec_all_lt,
  &B16_vec_all_lt,
  &B17_vec_all_lt,
  &B18_vec_all_lt,
  &B19_vec_all_lt,
};
static struct builtin *O_vec_all_nan[1] = {
  &B_vec_all_nan,
};
static struct builtin *O_vec_all_ne[23] = {
  &B1_vec_all_ne,
  &B2_vec_all_ne,
  &B3_vec_all_ne,
  &B4_vec_all_ne,
  &B5_vec_all_ne,
  &B6_vec_all_ne,
  &B7_vec_all_ne,
  &B8_vec_all_ne,
  &B9_vec_all_ne,
  &B10_vec_all_ne,
  &B11_vec_all_ne,
  &B12_vec_all_ne,
  &B13_vec_all_ne,
  &B14_vec_all_ne,
  &B15_vec_all_ne,
  &B16_vec_all_ne,
  &B17_vec_all_ne,
  &B18_vec_all_ne,
  &B19_vec_all_ne,
  &B20_vec_all_ne,
  &B21_vec_all_ne,
  &B22_vec_all_ne,
  &B23_vec_all_ne,
};
static struct builtin *O_vec_all_nge[1] = {
  &B_vec_all_nge,
};
static struct builtin *O_vec_all_ngt[1] = {
  &B_vec_all_ngt,
};
static struct builtin *O_vec_all_nle[1] = {
  &B_vec_all_nle,
};
static struct builtin *O_vec_all_nlt[1] = {
  &B_vec_all_nlt,
};
static struct builtin *O_vec_all_numeric[1] = {
  &B_vec_all_numeric,
};
static struct builtin *O_vec_and[24] = {
  &B1_vec_vand,
  &B2_vec_vand,
  &B3_vec_vand,
  &B4_vec_vand,
  &B5_vec_vand,
  &B6_vec_vand,
  &B7_vec_vand,
  &B8_vec_vand,
  &B9_vec_vand,
  &B10_vec_vand,
  &B11_vec_vand,
  &B12_vec_vand,
  &B13_vec_vand,
  &B14_vec_vand,
  &B15_vec_vand,
  &B16_vec_vand,
  &B17_vec_vand,
  &B18_vec_vand,
  &B19_vec_vand,
  &B20_vec_vand,
  &B21_vec_vand,
  &B22_vec_vand,
  &B23_vec_vand,
  &B24_vec_vand,
};
static struct builtin *O_vec_andc[24] = {
  &B1_vec_vandc,
  &B2_vec_vandc,
  &B3_vec_vandc,
  &B4_vec_vandc,
  &B5_vec_vandc,
  &B6_vec_vandc,
  &B7_vec_vandc,
  &B8_vec_vandc,
  &B9_vec_vandc,
  &B10_vec_vandc,
  &B11_vec_vandc,
  &B12_vec_vandc,
  &B13_vec_vandc,
  &B14_vec_vandc,
  &B15_vec_vandc,
  &B16_vec_vandc,
  &B17_vec_vandc,
  &B18_vec_vandc,
  &B19_vec_vandc,
  &B20_vec_vandc,
  &B21_vec_vandc,
  &B22_vec_vandc,
  &B23_vec_vandc,
  &B24_vec_vandc,
};
static struct builtin *O_vec_any_eq[23] = {
  &B1_vec_any_eq,
  &B2_vec_any_eq,
  &B3_vec_any_eq,
  &B4_vec_any_eq,
  &B5_vec_any_eq,
  &B6_vec_any_eq,
  &B7_vec_any_eq,
  &B8_vec_any_eq,
  &B9_vec_any_eq,
  &B10_vec_any_eq,
  &B11_vec_any_eq,
  &B12_vec_any_eq,
  &B13_vec_any_eq,
  &B14_vec_any_eq,
  &B15_vec_any_eq,
  &B16_vec_any_eq,
  &B17_vec_any_eq,
  &B18_vec_any_eq,
  &B19_vec_any_eq,
  &B20_vec_any_eq,
  &B21_vec_any_eq,
  &B22_vec_any_eq,
  &B23_vec_any_eq,
};
static struct builtin *O_vec_any_ge[19] = {
  &B1_vec_any_ge,
  &B2_vec_any_ge,
  &B3_vec_any_ge,
  &B4_vec_any_ge,
  &B5_vec_any_ge,
  &B6_vec_any_ge,
  &B7_vec_any_ge,
  &B8_vec_any_ge,
  &B9_vec_any_ge,
  &B10_vec_any_ge,
  &B11_vec_any_ge,
  &B12_vec_any_ge,
  &B13_vec_any_ge,
  &B14_vec_any_ge,
  &B15_vec_any_ge,
  &B16_vec_any_ge,
  &B17_vec_any_ge,
  &B18_vec_any_ge,
  &B19_vec_any_ge,
};
static struct builtin *O_vec_any_gt[19] = {
  &B1_vec_any_gt,
  &B2_vec_any_gt,
  &B3_vec_any_gt,
  &B4_vec_any_gt,
  &B5_vec_any_gt,
  &B6_vec_any_gt,
  &B7_vec_any_gt,
  &B8_vec_any_gt,
  &B9_vec_any_gt,
  &B10_vec_any_gt,
  &B11_vec_any_gt,
  &B12_vec_any_gt,
  &B13_vec_any_gt,
  &B14_vec_any_gt,
  &B15_vec_any_gt,
  &B16_vec_any_gt,
  &B17_vec_any_gt,
  &B18_vec_any_gt,
  &B19_vec_any_gt,
};
static struct builtin *O_vec_any_le[19] = {
  &B1_vec_any_le,
  &B2_vec_any_le,
  &B3_vec_any_le,
  &B4_vec_any_le,
  &B5_vec_any_le,
  &B6_vec_any_le,
  &B7_vec_any_le,
  &B8_vec_any_le,
  &B9_vec_any_le,
  &B10_vec_any_le,
  &B11_vec_any_le,
  &B12_vec_any_le,
  &B13_vec_any_le,
  &B14_vec_any_le,
  &B15_vec_any_le,
  &B16_vec_any_le,
  &B17_vec_any_le,
  &B18_vec_any_le,
  &B19_vec_any_le,
};
static struct builtin *O_vec_any_lt[19] = {
  &B1_vec_any_lt,
  &B2_vec_any_lt,
  &B3_vec_any_lt,
  &B4_vec_any_lt,
  &B5_vec_any_lt,
  &B6_vec_any_lt,
  &B7_vec_any_lt,
  &B8_vec_any_lt,
  &B9_vec_any_lt,
  &B10_vec_any_lt,
  &B11_vec_any_lt,
  &B12_vec_any_lt,
  &B13_vec_any_lt,
  &B14_vec_any_lt,
  &B15_vec_any_lt,
  &B16_vec_any_lt,
  &B17_vec_any_lt,
  &B18_vec_any_lt,
  &B19_vec_any_lt,
};
static struct builtin *O_vec_any_nan[1] = {
  &B_vec_any_nan,
};
static struct builtin *O_vec_any_ne[23] = {
  &B1_vec_any_ne,
  &B2_vec_any_ne,
  &B3_vec_any_ne,
  &B4_vec_any_ne,
  &B5_vec_any_ne,
  &B6_vec_any_ne,
  &B7_vec_any_ne,
  &B8_vec_any_ne,
  &B9_vec_any_ne,
  &B10_vec_any_ne,
  &B11_vec_any_ne,
  &B12_vec_any_ne,
  &B13_vec_any_ne,
  &B14_vec_any_ne,
  &B15_vec_any_ne,
  &B16_vec_any_ne,
  &B17_vec_any_ne,
  &B18_vec_any_ne,
  &B19_vec_any_ne,
  &B20_vec_any_ne,
  &B21_vec_any_ne,
  &B22_vec_any_ne,
  &B23_vec_any_ne,
};
static struct builtin *O_vec_any_nge[1] = {
  &B_vec_any_nge,
};
static struct builtin *O_vec_any_ngt[1] = {
  &B_vec_any_ngt,
};
static struct builtin *O_vec_any_nle[1] = {
  &B_vec_any_nle,
};
static struct builtin *O_vec_any_nlt[1] = {
  &B_vec_any_nlt,
};
static struct builtin *O_vec_any_numeric[1] = {
  &B_vec_any_numeric,
};
static struct builtin *O_vec_any_out[1] = {
  &B_vec_any_out,
};
static struct builtin *O_vec_avg[6] = {
  &B_vec_vavgsh,
  &B_vec_vavgsw,
  &B_vec_vavgsb,
  &B_vec_vavguh,
  &B_vec_vavguw,
  &B_vec_vavgub,
};
static struct builtin *O_vec_ceil[1] = {
  &B_vec_vrfip,
};
static struct builtin *O_vec_cmpb[1] = {
  &B_vec_vcmpbfp,
};
static struct builtin *O_vec_cmpeq[7] = {
  &B_vec_vcmpeqfp,
  &B1_vec_vcmpequh,
  &B1_vec_vcmpequw,
  &B1_vec_vcmpequb,
  &B2_vec_vcmpequh,
  &B2_vec_vcmpequw,
  &B2_vec_vcmpequb,
};
static struct builtin *O_vec_cmpge[1] = {
  &B_vec_vcmpgefp,
};
static struct builtin *O_vec_cmpgt[7] = {
  &B_vec_vcmpgtfp,
  &B_vec_vcmpgtsh,
  &B_vec_vcmpgtsw,
  &B_vec_vcmpgtsb,
  &B_vec_vcmpgtuh,
  &B_vec_vcmpgtuw,
  &B_vec_vcmpgtub,
};
static struct builtin *O_vec_cmple[1] = {
  &B_vec_vcmplefp,
};
static struct builtin *O_vec_cmplt[7] = {
  &B_vec_vcmpltfp,
  &B_vec_vcmpltsh,
  &B_vec_vcmpltsw,
  &B_vec_vcmpltsb,
  &B_vec_vcmpltuh,
  &B_vec_vcmpltuw,
  &B_vec_vcmpltub,
};
static struct builtin *O_vec_ctf[2] = {
  &B_vec_vcfsx,
  &B_vec_vcfux,
};
static struct builtin *O_vec_cts[1] = {
  &B_vec_vctsxs,
};
static struct builtin *O_vec_ctu[1] = {
  &B_vec_vctuxs,
};
static struct builtin *O_vec_dss[1] = {
  &B_vec_dss,
};
static struct builtin *O_vec_dssall[1] = {
  &B_vec_dssall,
};
static struct builtin *O_vec_dst[20] = {
  &B1_vec_dst,
  &B2_vec_dst,
  &B3_vec_dst,
  &B4_vec_dst,
  &B5_vec_dst,
  &B6_vec_dst,
  &B7_vec_dst,
  &B8_vec_dst,
  &B9_vec_dst,
  &B10_vec_dst,
  &B11_vec_dst,
  &B12_vec_dst,
  &B13_vec_dst,
  &B14_vec_dst,
  &B15_vec_dst,
  &B16_vec_dst,
  &B17_vec_dst,
  &B18_vec_dst,
  &B19_vec_dst,
  &B20_vec_dst,
};
static struct builtin *O_vec_dstst[20] = {
  &B1_vec_dstst,
  &B2_vec_dstst,
  &B3_vec_dstst,
  &B4_vec_dstst,
  &B5_vec_dstst,
  &B6_vec_dstst,
  &B7_vec_dstst,
  &B8_vec_dstst,
  &B9_vec_dstst,
  &B10_vec_dstst,
  &B11_vec_dstst,
  &B12_vec_dstst,
  &B13_vec_dstst,
  &B14_vec_dstst,
  &B15_vec_dstst,
  &B16_vec_dstst,
  &B17_vec_dstst,
  &B18_vec_dstst,
  &B19_vec_dstst,
  &B20_vec_dstst,
};
static struct builtin *O_vec_dststt[20] = {
  &B1_vec_dststt,
  &B2_vec_dststt,
  &B3_vec_dststt,
  &B4_vec_dststt,
  &B5_vec_dststt,
  &B6_vec_dststt,
  &B7_vec_dststt,
  &B8_vec_dststt,
  &B9_vec_dststt,
  &B10_vec_dststt,
  &B11_vec_dststt,
  &B12_vec_dststt,
  &B13_vec_dststt,
  &B14_vec_dststt,
  &B15_vec_dststt,
  &B16_vec_dststt,
  &B17_vec_dststt,
  &B18_vec_dststt,
  &B19_vec_dststt,
  &B20_vec_dststt,
};
static struct builtin *O_vec_dstt[20] = {
  &B1_vec_dstt,
  &B2_vec_dstt,
  &B3_vec_dstt,
  &B4_vec_dstt,
  &B5_vec_dstt,
  &B6_vec_dstt,
  &B7_vec_dstt,
  &B8_vec_dstt,
  &B9_vec_dstt,
  &B10_vec_dstt,
  &B11_vec_dstt,
  &B12_vec_dstt,
  &B13_vec_dstt,
  &B14_vec_dstt,
  &B15_vec_dstt,
  &B16_vec_dstt,
  &B17_vec_dstt,
  &B18_vec_dstt,
  &B19_vec_dstt,
  &B20_vec_dstt,
};
static struct builtin *O_vec_expte[1] = {
  &B_vec_vexptefp,
};
static struct builtin *O_vec_floor[1] = {
  &B_vec_vrfim,
};
static struct builtin *O_vec_ld[20] = {
  &B1_vec_lvx,
  &B2_vec_lvx,
  &B3_vec_lvx,
  &B4_vec_lvx,
  &B5_vec_lvx,
  &B6_vec_lvx,
  &B7_vec_lvx,
  &B8_vec_lvx,
  &B9_vec_lvx,
  &B10_vec_lvx,
  &B11_vec_lvx,
  &B12_vec_lvx,
  &B13_vec_lvx,
  &B14_vec_lvx,
  &B15_vec_lvx,
  &B16_vec_lvx,
  &B17_vec_lvx,
  &B18_vec_lvx,
  &B19_vec_lvx,
  &B20_vec_lvx,
};
static struct builtin *O_vec_lde[9] = {
  &B1_vec_lvewx,
  &B2_vec_lvewx,
  &B3_vec_lvewx,
  &B1_vec_lvehx,
  &B1_vec_lvebx,
  &B2_vec_lvebx,
  &B4_vec_lvewx,
  &B5_vec_lvewx,
  &B2_vec_lvehx,
};
static struct builtin *O_vec_ldl[20] = {
  &B1_vec_lvxl,
  &B2_vec_lvxl,
  &B3_vec_lvxl,
  &B4_vec_lvxl,
  &B5_vec_lvxl,
  &B6_vec_lvxl,
  &B7_vec_lvxl,
  &B8_vec_lvxl,
  &B9_vec_lvxl,
  &B10_vec_lvxl,
  &B11_vec_lvxl,
  &B12_vec_lvxl,
  &B13_vec_lvxl,
  &B14_vec_lvxl,
  &B15_vec_lvxl,
  &B16_vec_lvxl,
  &B17_vec_lvxl,
  &B18_vec_lvxl,
  &B19_vec_lvxl,
  &B20_vec_lvxl,
};
static struct builtin *O_vec_loge[1] = {
  &B_vec_vlogefp,
};
static struct builtin *O_vec_lvebx[2] = {
  &B1_vec_lvebx,
  &B2_vec_lvebx,
};
static struct builtin *O_vec_lvehx[2] = {
  &B1_vec_lvehx,
  &B2_vec_lvehx,
};
static struct builtin *O_vec_lvewx[5] = {
  &B1_vec_lvewx,
  &B2_vec_lvewx,
  &B3_vec_lvewx,
  &B4_vec_lvewx,
  &B5_vec_lvewx,
};
static struct builtin *O_vec_lvsl[9] = {
  &B1_vec_lvsl,
  &B2_vec_lvsl,
  &B3_vec_lvsl,
  &B4_vec_lvsl,
  &B5_vec_lvsl,
  &B6_vec_lvsl,
  &B7_vec_lvsl,
  &B8_vec_lvsl,
  &B9_vec_lvsl,
};
static struct builtin *O_vec_lvsr[9] = {
  &B1_vec_lvsr,
  &B2_vec_lvsr,
  &B3_vec_lvsr,
  &B4_vec_lvsr,
  &B5_vec_lvsr,
  &B6_vec_lvsr,
  &B7_vec_lvsr,
  &B8_vec_lvsr,
  &B9_vec_lvsr,
};
static struct builtin *O_vec_lvx[20] = {
  &B1_vec_lvx,
  &B2_vec_lvx,
  &B3_vec_lvx,
  &B4_vec_lvx,
  &B5_vec_lvx,
  &B6_vec_lvx,
  &B7_vec_lvx,
  &B8_vec_lvx,
  &B9_vec_lvx,
  &B10_vec_lvx,
  &B11_vec_lvx,
  &B12_vec_lvx,
  &B13_vec_lvx,
  &B14_vec_lvx,
  &B15_vec_lvx,
  &B16_vec_lvx,
  &B17_vec_lvx,
  &B18_vec_lvx,
  &B19_vec_lvx,
  &B20_vec_lvx,
};
static struct builtin *O_vec_lvxl[20] = {
  &B1_vec_lvxl,
  &B2_vec_lvxl,
  &B3_vec_lvxl,
  &B4_vec_lvxl,
  &B5_vec_lvxl,
  &B6_vec_lvxl,
  &B7_vec_lvxl,
  &B8_vec_lvxl,
  &B9_vec_lvxl,
  &B10_vec_lvxl,
  &B11_vec_lvxl,
  &B12_vec_lvxl,
  &B13_vec_lvxl,
  &B14_vec_lvxl,
  &B15_vec_lvxl,
  &B16_vec_lvxl,
  &B17_vec_lvxl,
  &B18_vec_lvxl,
  &B19_vec_lvxl,
  &B20_vec_lvxl,
};
static struct builtin *O_vec_madd[1] = {
  &B_vec_vmaddfp,
};
static struct builtin *O_vec_madds[1] = {
  &B_vec_vmhaddshs,
};
static struct builtin *O_vec_max[19] = {
  &B1_vec_vmaxsh,
  &B1_vec_vmaxuh,
  &B1_vec_vmaxsw,
  &B1_vec_vmaxuw,
  &B1_vec_vmaxsb,
  &B1_vec_vmaxub,
  &B_vec_vmaxfp,
  &B2_vec_vmaxsh,
  &B3_vec_vmaxsh,
  &B2_vec_vmaxsw,
  &B3_vec_vmaxsw,
  &B2_vec_vmaxsb,
  &B3_vec_vmaxsb,
  &B2_vec_vmaxuh,
  &B3_vec_vmaxuh,
  &B2_vec_vmaxuw,
  &B3_vec_vmaxuw,
  &B2_vec_vmaxub,
  &B3_vec_vmaxub,
};
static struct builtin *O_vec_mergeh[11] = {
  &B1_vec_vmrghh,
  &B1_vec_vmrghw,
  &B1_vec_vmrghb,
  &B2_vec_vmrghw,
  &B2_vec_vmrghh,
  &B3_vec_vmrghh,
  &B3_vec_vmrghw,
  &B2_vec_vmrghb,
  &B4_vec_vmrghh,
  &B4_vec_vmrghw,
  &B3_vec_vmrghb,
};
static struct builtin *O_vec_mergel[11] = {
  &B1_vec_vmrglh,
  &B1_vec_vmrglw,
  &B1_vec_vmrglb,
  &B2_vec_vmrglw,
  &B2_vec_vmrglh,
  &B3_vec_vmrglh,
  &B3_vec_vmrglw,
  &B2_vec_vmrglb,
  &B4_vec_vmrglh,
  &B4_vec_vmrglw,
  &B3_vec_vmrglb,
};
static struct builtin *O_vec_mfvscr[1] = {
  &B_vec_mfvscr,
};
static struct builtin *O_vec_min[19] = {
  &B1_vec_vminsh,
  &B1_vec_vminuh,
  &B1_vec_vminsw,
  &B1_vec_vminuw,
  &B1_vec_vminsb,
  &B1_vec_vminub,
  &B_vec_vminfp,
  &B2_vec_vminsh,
  &B3_vec_vminsh,
  &B2_vec_vminsw,
  &B3_vec_vminsw,
  &B2_vec_vminsb,
  &B3_vec_vminsb,
  &B2_vec_vminuh,
  &B3_vec_vminuh,
  &B2_vec_vminuw,
  &B3_vec_vminuw,
  &B2_vec_vminub,
  &B3_vec_vminub,
};
static struct builtin *O_vec_mladd[4] = {
  &B1_vec_vmladduhm,
  &B2_vec_vmladduhm,
  &B3_vec_vmladduhm,
  &B4_vec_vmladduhm,
};
static struct builtin *O_vec_mradds[1] = {
  &B_vec_vmhraddshs,
};
static struct builtin *O_vec_msum[4] = {
  &B_vec_vmsumshm,
  &B_vec_vmsummbm,
  &B_vec_vmsumuhm,
  &B_vec_vmsumubm,
};
static struct builtin *O_vec_msums[2] = {
  &B_vec_vmsumshs,
  &B_vec_vmsumuhs,
};
static struct builtin *O_vec_mtvscr[10] = {
  &B1_vec_mtvscr,
  &B2_vec_mtvscr,
  &B3_vec_mtvscr,
  &B4_vec_mtvscr,
  &B5_vec_mtvscr,
  &B6_vec_mtvscr,
  &B7_vec_mtvscr,
  &B8_vec_mtvscr,
  &B9_vec_mtvscr,
  &B10_vec_mtvscr,
};
static struct builtin *O_vec_mule[4] = {
  &B_vec_vmulesh,
  &B_vec_vmulesb,
  &B_vec_vmuleuh,
  &B_vec_vmuleub,
};
static struct builtin *O_vec_mulo[4] = {
  &B_vec_vmulosh,
  &B_vec_vmulosb,
  &B_vec_vmulouh,
  &B_vec_vmuloub,
};
static struct builtin *O_vec_nmsub[1] = {
  &B_vec_vnmsubfp,
};
static struct builtin *O_vec_nor[10] = {
  &B1_vec_vnor,
  &B2_vec_vnor,
  &B3_vec_vnor,
  &B4_vec_vnor,
  &B5_vec_vnor,
  &B6_vec_vnor,
  &B7_vec_vnor,
  &B8_vec_vnor,
  &B9_vec_vnor,
  &B10_vec_vnor,
};
static struct builtin *O_vec_or[24] = {
  &B1_vec_vor,
  &B2_vec_vor,
  &B3_vec_vor,
  &B4_vec_vor,
  &B5_vec_vor,
  &B6_vec_vor,
  &B7_vec_vor,
  &B8_vec_vor,
  &B9_vec_vor,
  &B10_vec_vor,
  &B11_vec_vor,
  &B12_vec_vor,
  &B13_vec_vor,
  &B14_vec_vor,
  &B15_vec_vor,
  &B16_vec_vor,
  &B17_vec_vor,
  &B18_vec_vor,
  &B19_vec_vor,
  &B20_vec_vor,
  &B21_vec_vor,
  &B22_vec_vor,
  &B23_vec_vor,
  &B24_vec_vor,
};
static struct builtin *O_vec_pack[6] = {
  &B1_vec_vpkuhum,
  &B1_vec_vpkuwum,
  &B2_vec_vpkuhum,
  &B2_vec_vpkuwum,
  &B3_vec_vpkuhum,
  &B3_vec_vpkuwum,
};
static struct builtin *O_vec_packpx[1] = {
  &B_vec_vpkpx,
};
static struct builtin *O_vec_packs[4] = {
  &B_vec_vpkshss,
  &B_vec_vpkswss,
  &B_vec_vpkuhus,
  &B_vec_vpkuwus,
};
static struct builtin *O_vec_packsu[4] = {
  &B_vec_vpkshus,
  &B_vec_vpkswus,
  &B_vec_vpkuhus,
  &B_vec_vpkuwus,
};
static struct builtin *O_vec_perm[11] = {
  &B1_vec_vperm,
  &B2_vec_vperm,
  &B3_vec_vperm,
  &B4_vec_vperm,
  &B5_vec_vperm,
  &B6_vec_vperm,
  &B7_vec_vperm,
  &B8_vec_vperm,
  &B9_vec_vperm,
  &B10_vec_vperm,
  &B11_vec_vperm,
};
static struct builtin *O_vec_re[1] = {
  &B_vec_vrefp,
};
static struct builtin *O_vec_rl[6] = {
  &B1_vec_vrlh,
  &B1_vec_vrlw,
  &B1_vec_vrlb,
  &B2_vec_vrlh,
  &B2_vec_vrlw,
  &B2_vec_vrlb,
};
static struct builtin *O_vec_round[1] = {
  &B_vec_vrfin,
};
static struct builtin *O_vec_rsqrte[1] = {
  &B_vec_vrsqrtefp,
};
static struct builtin *O_vec_sel[20] = {
  &B1_vec_vsel,
  &B2_vec_vsel,
  &B3_vec_vsel,
  &B4_vec_vsel,
  &B5_vec_vsel,
  &B6_vec_vsel,
  &B7_vec_vsel,
  &B8_vec_vsel,
  &B9_vec_vsel,
  &B10_vec_vsel,
  &B11_vec_vsel,
  &B12_vec_vsel,
  &B13_vec_vsel,
  &B14_vec_vsel,
  &B15_vec_vsel,
  &B16_vec_vsel,
  &B17_vec_vsel,
  &B18_vec_vsel,
  &B19_vec_vsel,
  &B20_vec_vsel,
};
static struct builtin *O_vec_sl[6] = {
  &B1_vec_vslh,
  &B1_vec_vslw,
  &B1_vec_vslb,
  &B2_vec_vslh,
  &B2_vec_vslw,
  &B2_vec_vslb,
};
static struct builtin *O_vec_sld[8] = {
  &B1_vec_vsldoi,
  &B2_vec_vsldoi,
  &B3_vec_vsldoi,
  &B4_vec_vsldoi,
  &B5_vec_vsldoi,
  &B6_vec_vsldoi,
  &B7_vec_vsldoi,
  &B8_vec_vsldoi,
};
static struct builtin *O_vec_sll[30] = {
  &B1_vec_vsl,
  &B2_vec_vsl,
  &B3_vec_vsl,
  &B4_vec_vsl,
  &B5_vec_vsl,
  &B6_vec_vsl,
  &B7_vec_vsl,
  &B8_vec_vsl,
  &B9_vec_vsl,
  &B10_vec_vsl,
  &B11_vec_vsl,
  &B12_vec_vsl,
  &B13_vec_vsl,
  &B14_vec_vsl,
  &B15_vec_vsl,
  &B16_vec_vsl,
  &B17_vec_vsl,
  &B18_vec_vsl,
  &B19_vec_vsl,
  &B20_vec_vsl,
  &B21_vec_vsl,
  &B22_vec_vsl,
  &B23_vec_vsl,
  &B24_vec_vsl,
  &B25_vec_vsl,
  &B26_vec_vsl,
  &B27_vec_vsl,
  &B28_vec_vsl,
  &B29_vec_vsl,
  &B30_vec_vsl,
};
static struct builtin *O_vec_slo[16] = {
  &B1_vec_vslo,
  &B2_vec_vslo,
  &B3_vec_vslo,
  &B4_vec_vslo,
  &B5_vec_vslo,
  &B6_vec_vslo,
  &B7_vec_vslo,
  &B8_vec_vslo,
  &B9_vec_vslo,
  &B10_vec_vslo,
  &B11_vec_vslo,
  &B12_vec_vslo,
  &B13_vec_vslo,
  &B14_vec_vslo,
  &B15_vec_vslo,
  &B16_vec_vslo,
};
static struct builtin *O_vec_splat[11] = {
  &B1_vec_vsplth,
  &B1_vec_vspltw,
  &B1_vec_vspltb,
  &B2_vec_vspltw,
  &B2_vec_vsplth,
  &B3_vec_vsplth,
  &B3_vec_vspltw,
  &B2_vec_vspltb,
  &B4_vec_vsplth,
  &B4_vec_vspltw,
  &B3_vec_vspltb,
};
static struct builtin *O_vec_splat_s16[1] = {
  &B_vec_vspltish,
};
static struct builtin *O_vec_splat_s32[1] = {
  &B_vec_vspltisw,
};
static struct builtin *O_vec_splat_s8[1] = {
  &B_vec_vspltisb,
};
static struct builtin *O_vec_splat_u16[1] = {
  &B_vec_splat_u16,
};
static struct builtin *O_vec_splat_u32[1] = {
  &B_vec_splat_u32,
};
static struct builtin *O_vec_splat_u8[1] = {
  &B_vec_splat_u8,
};
static struct builtin *O_vec_sr[6] = {
  &B1_vec_vsrh,
  &B1_vec_vsrw,
  &B1_vec_vsrb,
  &B2_vec_vsrh,
  &B2_vec_vsrw,
  &B2_vec_vsrb,
};
static struct builtin *O_vec_sra[6] = {
  &B1_vec_vsrah,
  &B1_vec_vsraw,
  &B1_vec_vsrab,
  &B2_vec_vsrah,
  &B2_vec_vsraw,
  &B2_vec_vsrab,
};
static struct builtin *O_vec_srl[30] = {
  &B1_vec_vsr,
  &B2_vec_vsr,
  &B3_vec_vsr,
  &B4_vec_vsr,
  &B5_vec_vsr,
  &B6_vec_vsr,
  &B7_vec_vsr,
  &B8_vec_vsr,
  &B9_vec_vsr,
  &B10_vec_vsr,
  &B11_vec_vsr,
  &B12_vec_vsr,
  &B13_vec_vsr,
  &B14_vec_vsr,
  &B15_vec_vsr,
  &B16_vec_vsr,
  &B17_vec_vsr,
  &B18_vec_vsr,
  &B19_vec_vsr,
  &B20_vec_vsr,
  &B21_vec_vsr,
  &B22_vec_vsr,
  &B23_vec_vsr,
  &B24_vec_vsr,
  &B25_vec_vsr,
  &B26_vec_vsr,
  &B27_vec_vsr,
  &B28_vec_vsr,
  &B29_vec_vsr,
  &B30_vec_vsr,
};
static struct builtin *O_vec_sro[16] = {
  &B1_vec_vsro,
  &B2_vec_vsro,
  &B3_vec_vsro,
  &B4_vec_vsro,
  &B5_vec_vsro,
  &B6_vec_vsro,
  &B7_vec_vsro,
  &B8_vec_vsro,
  &B9_vec_vsro,
  &B10_vec_vsro,
  &B11_vec_vsro,
  &B12_vec_vsro,
  &B13_vec_vsro,
  &B14_vec_vsro,
  &B15_vec_vsro,
  &B16_vec_vsro,
};
static struct builtin *O_vec_st[20] = {
  &B1_vec_stvx,
  &B2_vec_stvx,
  &B3_vec_stvx,
  &B4_vec_stvx,
  &B5_vec_stvx,
  &B6_vec_stvx,
  &B7_vec_stvx,
  &B8_vec_stvx,
  &B9_vec_stvx,
  &B10_vec_stvx,
  &B11_vec_stvx,
  &B12_vec_stvx,
  &B13_vec_stvx,
  &B14_vec_stvx,
  &B15_vec_stvx,
  &B16_vec_stvx,
  &B17_vec_stvx,
  &B18_vec_stvx,
  &B19_vec_stvx,
  &B20_vec_stvx,
};
static struct builtin *O_vec_ste[9] = {
  &B1_vec_stvewx,
  &B1_vec_stvehx,
  &B2_vec_stvewx,
  &B3_vec_stvewx,
  &B1_vec_stvebx,
  &B2_vec_stvehx,
  &B4_vec_stvewx,
  &B5_vec_stvewx,
  &B2_vec_stvebx,
};
static struct builtin *O_vec_stl[20] = {
  &B1_vec_stvxl,
  &B2_vec_stvxl,
  &B3_vec_stvxl,
  &B4_vec_stvxl,
  &B5_vec_stvxl,
  &B6_vec_stvxl,
  &B7_vec_stvxl,
  &B8_vec_stvxl,
  &B9_vec_stvxl,
  &B10_vec_stvxl,
  &B11_vec_stvxl,
  &B12_vec_stvxl,
  &B13_vec_stvxl,
  &B14_vec_stvxl,
  &B15_vec_stvxl,
  &B16_vec_stvxl,
  &B17_vec_stvxl,
  &B18_vec_stvxl,
  &B19_vec_stvxl,
  &B20_vec_stvxl,
};
static struct builtin *O_vec_stvebx[2] = {
  &B1_vec_stvebx,
  &B2_vec_stvebx,
};
static struct builtin *O_vec_stvehx[2] = {
  &B1_vec_stvehx,
  &B2_vec_stvehx,
};
static struct builtin *O_vec_stvewx[5] = {
  &B1_vec_stvewx,
  &B2_vec_stvewx,
  &B3_vec_stvewx,
  &B4_vec_stvewx,
  &B5_vec_stvewx,
};
static struct builtin *O_vec_stvx[20] = {
  &B1_vec_stvx,
  &B2_vec_stvx,
  &B3_vec_stvx,
  &B4_vec_stvx,
  &B5_vec_stvx,
  &B6_vec_stvx,
  &B7_vec_stvx,
  &B8_vec_stvx,
  &B9_vec_stvx,
  &B10_vec_stvx,
  &B11_vec_stvx,
  &B12_vec_stvx,
  &B13_vec_stvx,
  &B14_vec_stvx,
  &B15_vec_stvx,
  &B16_vec_stvx,
  &B17_vec_stvx,
  &B18_vec_stvx,
  &B19_vec_stvx,
  &B20_vec_stvx,
};
static struct builtin *O_vec_stvxl[20] = {
  &B1_vec_stvxl,
  &B2_vec_stvxl,
  &B3_vec_stvxl,
  &B4_vec_stvxl,
  &B5_vec_stvxl,
  &B6_vec_stvxl,
  &B7_vec_stvxl,
  &B8_vec_stvxl,
  &B9_vec_stvxl,
  &B10_vec_stvxl,
  &B11_vec_stvxl,
  &B12_vec_stvxl,
  &B13_vec_stvxl,
  &B14_vec_stvxl,
  &B15_vec_stvxl,
  &B16_vec_stvxl,
  &B17_vec_stvxl,
  &B18_vec_stvxl,
  &B19_vec_stvxl,
  &B20_vec_stvxl,
};
static struct builtin *O_vec_sub[19] = {
  &B1_vec_vsubuhm,
  &B2_vec_vsubuhm,
  &B1_vec_vsubuwm,
  &B2_vec_vsubuwm,
  &B1_vec_vsububm,
  &B2_vec_vsububm,
  &B_vec_vsubfp,
  &B3_vec_vsubuhm,
  &B4_vec_vsubuhm,
  &B3_vec_vsubuwm,
  &B4_vec_vsubuwm,
  &B3_vec_vsububm,
  &B4_vec_vsububm,
  &B5_vec_vsubuhm,
  &B6_vec_vsubuhm,
  &B5_vec_vsubuwm,
  &B6_vec_vsubuwm,
  &B5_vec_vsububm,
  &B6_vec_vsububm,
};
static struct builtin *O_vec_subc[1] = {
  &B_vec_vsubcuw,
};
static struct builtin *O_vec_subs[18] = {
  &B1_vec_vsubshs,
  &B1_vec_vsubuhs,
  &B1_vec_vsubsws,
  &B1_vec_vsubuws,
  &B1_vec_vsubsbs,
  &B1_vec_vsububs,
  &B2_vec_vsubshs,
  &B3_vec_vsubshs,
  &B2_vec_vsubsws,
  &B3_vec_vsubsws,
  &B2_vec_vsubsbs,
  &B3_vec_vsubsbs,
  &B2_vec_vsubuhs,
  &B3_vec_vsubuhs,
  &B2_vec_vsubuws,
  &B3_vec_vsubuws,
  &B2_vec_vsububs,
  &B3_vec_vsububs,
};
static struct builtin *O_vec_sum2s[1] = {
  &B_vec_vsum2sws,
};
static struct builtin *O_vec_sum4s[3] = {
  &B_vec_vsum4shs,
  &B_vec_vsum4sbs,
  &B_vec_vsum4ubs,
};
static struct builtin *O_vec_sums[1] = {
  &B_vec_vsumsws,
};
static struct builtin *O_vec_trunc[1] = {
  &B_vec_vrfiz,
};
static struct builtin *O_vec_unpackh[5] = {
  &B1_vec_vupkhsh,
  &B1_vec_vupkhsb,
  &B_vec_vupkhpx,
  &B2_vec_vupkhsh,
  &B2_vec_vupkhsb,
};
static struct builtin *O_vec_unpackl[5] = {
  &B1_vec_vupklsh,
  &B1_vec_vupklsb,
  &B_vec_vupklpx,
  &B2_vec_vupklsh,
  &B2_vec_vupklsb,
};
static struct builtin *O_vec_vabsfp[1] = {
  &B_vec_vabsfp,
};
static struct builtin *O_vec_vabssb[1] = {
  &B_vec_vabssb,
};
static struct builtin *O_vec_vabssh[1] = {
  &B_vec_vabssh,
};
static struct builtin *O_vec_vabsssb[1] = {
  &B_vec_vabsssb,
};
static struct builtin *O_vec_vabsssh[1] = {
  &B_vec_vabsssh,
};
static struct builtin *O_vec_vabsssw[1] = {
  &B_vec_vabsssw,
};
static struct builtin *O_vec_vabssw[1] = {
  &B_vec_vabssw,
};
static struct builtin *O_vec_vaddcuw[1] = {
  &B_vec_vaddcuw,
};
static struct builtin *O_vec_vaddfp[1] = {
  &B_vec_vaddfp,
};
static struct builtin *O_vec_vaddsbs[3] = {
  &B1_vec_vaddsbs,
  &B2_vec_vaddsbs,
  &B3_vec_vaddsbs,
};
static struct builtin *O_vec_vaddshs[3] = {
  &B1_vec_vaddshs,
  &B2_vec_vaddshs,
  &B3_vec_vaddshs,
};
static struct builtin *O_vec_vaddsws[3] = {
  &B1_vec_vaddsws,
  &B2_vec_vaddsws,
  &B3_vec_vaddsws,
};
static struct builtin *O_vec_vaddubm[6] = {
  &B1_vec_vaddubm,
  &B2_vec_vaddubm,
  &B3_vec_vaddubm,
  &B4_vec_vaddubm,
  &B5_vec_vaddubm,
  &B6_vec_vaddubm,
};
static struct builtin *O_vec_vaddubs[3] = {
  &B1_vec_vaddubs,
  &B2_vec_vaddubs,
  &B3_vec_vaddubs,
};
static struct builtin *O_vec_vadduhm[6] = {
  &B1_vec_vadduhm,
  &B2_vec_vadduhm,
  &B3_vec_vadduhm,
  &B4_vec_vadduhm,
  &B5_vec_vadduhm,
  &B6_vec_vadduhm,
};
static struct builtin *O_vec_vadduhs[3] = {
  &B1_vec_vadduhs,
  &B2_vec_vadduhs,
  &B3_vec_vadduhs,
};
static struct builtin *O_vec_vadduwm[6] = {
  &B1_vec_vadduwm,
  &B2_vec_vadduwm,
  &B3_vec_vadduwm,
  &B4_vec_vadduwm,
  &B5_vec_vadduwm,
  &B6_vec_vadduwm,
};
static struct builtin *O_vec_vadduws[3] = {
  &B1_vec_vadduws,
  &B2_vec_vadduws,
  &B3_vec_vadduws,
};
static struct builtin *O_vec_vand[24] = {
  &B1_vec_vand,
  &B2_vec_vand,
  &B3_vec_vand,
  &B4_vec_vand,
  &B5_vec_vand,
  &B6_vec_vand,
  &B7_vec_vand,
  &B8_vec_vand,
  &B9_vec_vand,
  &B10_vec_vand,
  &B11_vec_vand,
  &B12_vec_vand,
  &B13_vec_vand,
  &B14_vec_vand,
  &B15_vec_vand,
  &B16_vec_vand,
  &B17_vec_vand,
  &B18_vec_vand,
  &B19_vec_vand,
  &B20_vec_vand,
  &B21_vec_vand,
  &B22_vec_vand,
  &B23_vec_vand,
  &B24_vec_vand,
};
static struct builtin *O_vec_vandc[24] = {
  &B1_vec_vandc,
  &B2_vec_vandc,
  &B3_vec_vandc,
  &B4_vec_vandc,
  &B5_vec_vandc,
  &B6_vec_vandc,
  &B7_vec_vandc,
  &B8_vec_vandc,
  &B9_vec_vandc,
  &B10_vec_vandc,
  &B11_vec_vandc,
  &B12_vec_vandc,
  &B13_vec_vandc,
  &B14_vec_vandc,
  &B15_vec_vandc,
  &B16_vec_vandc,
  &B17_vec_vandc,
  &B18_vec_vandc,
  &B19_vec_vandc,
  &B20_vec_vandc,
  &B21_vec_vandc,
  &B22_vec_vandc,
  &B23_vec_vandc,
  &B24_vec_vandc,
};
static struct builtin *O_vec_vavgsb[1] = {
  &B_vec_vavgsb,
};
static struct builtin *O_vec_vavgsh[1] = {
  &B_vec_vavgsh,
};
static struct builtin *O_vec_vavgsw[1] = {
  &B_vec_vavgsw,
};
static struct builtin *O_vec_vavgub[1] = {
  &B_vec_vavgub,
};
static struct builtin *O_vec_vavguh[1] = {
  &B_vec_vavguh,
};
static struct builtin *O_vec_vavguw[1] = {
  &B_vec_vavguw,
};
static struct builtin *O_vec_vcfsx[1] = {
  &B_vec_vcfsx,
};
static struct builtin *O_vec_vcfux[1] = {
  &B_vec_vcfux,
};
static struct builtin *O_vec_vcmpbfp[1] = {
  &B_vec_vcmpbfp,
};
static struct builtin *O_vec_vcmpeqfp[1] = {
  &B_vec_vcmpeqfp,
};
static struct builtin *O_vec_vcmpequb[2] = {
  &B1_vec_vcmpequb,
  &B2_vec_vcmpequb,
};
static struct builtin *O_vec_vcmpequh[2] = {
  &B1_vec_vcmpequh,
  &B2_vec_vcmpequh,
};
static struct builtin *O_vec_vcmpequw[2] = {
  &B1_vec_vcmpequw,
  &B2_vec_vcmpequw,
};
static struct builtin *O_vec_vcmpgefp[1] = {
  &B_vec_vcmpgefp,
};
static struct builtin *O_vec_vcmpgtfp[1] = {
  &B_vec_vcmpgtfp,
};
static struct builtin *O_vec_vcmpgtsb[1] = {
  &B_vec_vcmpgtsb,
};
static struct builtin *O_vec_vcmpgtsh[1] = {
  &B_vec_vcmpgtsh,
};
static struct builtin *O_vec_vcmpgtsw[1] = {
  &B_vec_vcmpgtsw,
};
static struct builtin *O_vec_vcmpgtub[1] = {
  &B_vec_vcmpgtub,
};
static struct builtin *O_vec_vcmpgtuh[1] = {
  &B_vec_vcmpgtuh,
};
static struct builtin *O_vec_vcmpgtuw[1] = {
  &B_vec_vcmpgtuw,
};
static struct builtin *O_vec_vcmplefp[1] = {
  &B_vec_vcmplefp,
};
static struct builtin *O_vec_vcmpltfp[1] = {
  &B_vec_vcmpltfp,
};
static struct builtin *O_vec_vcmpltsb[1] = {
  &B_vec_vcmpltsb,
};
static struct builtin *O_vec_vcmpltsh[1] = {
  &B_vec_vcmpltsh,
};
static struct builtin *O_vec_vcmpltsw[1] = {
  &B_vec_vcmpltsw,
};
static struct builtin *O_vec_vcmpltub[1] = {
  &B_vec_vcmpltub,
};
static struct builtin *O_vec_vcmpltuh[1] = {
  &B_vec_vcmpltuh,
};
static struct builtin *O_vec_vcmpltuw[1] = {
  &B_vec_vcmpltuw,
};
static struct builtin *O_vec_vctsxs[1] = {
  &B_vec_vctsxs,
};
static struct builtin *O_vec_vctuxs[1] = {
  &B_vec_vctuxs,
};
static struct builtin *O_vec_vexptefp[1] = {
  &B_vec_vexptefp,
};
static struct builtin *O_vec_vlogefp[1] = {
  &B_vec_vlogefp,
};
static struct builtin *O_vec_vmaddfp[1] = {
  &B_vec_vmaddfp,
};
static struct builtin *O_vec_vmaxfp[1] = {
  &B_vec_vmaxfp,
};
static struct builtin *O_vec_vmaxsb[3] = {
  &B1_vec_vmaxsb,
  &B2_vec_vmaxsb,
  &B3_vec_vmaxsb,
};
static struct builtin *O_vec_vmaxsh[3] = {
  &B1_vec_vmaxsh,
  &B2_vec_vmaxsh,
  &B3_vec_vmaxsh,
};
static struct builtin *O_vec_vmaxsw[3] = {
  &B1_vec_vmaxsw,
  &B2_vec_vmaxsw,
  &B3_vec_vmaxsw,
};
static struct builtin *O_vec_vmaxub[3] = {
  &B1_vec_vmaxub,
  &B2_vec_vmaxub,
  &B3_vec_vmaxub,
};
static struct builtin *O_vec_vmaxuh[3] = {
  &B1_vec_vmaxuh,
  &B2_vec_vmaxuh,
  &B3_vec_vmaxuh,
};
static struct builtin *O_vec_vmaxuw[3] = {
  &B1_vec_vmaxuw,
  &B2_vec_vmaxuw,
  &B3_vec_vmaxuw,
};
static struct builtin *O_vec_vmhaddshs[1] = {
  &B_vec_vmhaddshs,
};
static struct builtin *O_vec_vmhraddshs[1] = {
  &B_vec_vmhraddshs,
};
static struct builtin *O_vec_vminfp[1] = {
  &B_vec_vminfp,
};
static struct builtin *O_vec_vminsb[3] = {
  &B1_vec_vminsb,
  &B2_vec_vminsb,
  &B3_vec_vminsb,
};
static struct builtin *O_vec_vminsh[3] = {
  &B1_vec_vminsh,
  &B2_vec_vminsh,
  &B3_vec_vminsh,
};
static struct builtin *O_vec_vminsw[3] = {
  &B1_vec_vminsw,
  &B2_vec_vminsw,
  &B3_vec_vminsw,
};
static struct builtin *O_vec_vminub[3] = {
  &B1_vec_vminub,
  &B2_vec_vminub,
  &B3_vec_vminub,
};
static struct builtin *O_vec_vminuh[3] = {
  &B1_vec_vminuh,
  &B2_vec_vminuh,
  &B3_vec_vminuh,
};
static struct builtin *O_vec_vminuw[3] = {
  &B1_vec_vminuw,
  &B2_vec_vminuw,
  &B3_vec_vminuw,
};
static struct builtin *O_vec_vmladduhm[4] = {
  &B1_vec_vmladduhm,
  &B2_vec_vmladduhm,
  &B3_vec_vmladduhm,
  &B4_vec_vmladduhm,
};
static struct builtin *O_vec_vmrghb[3] = {
  &B1_vec_vmrghb,
  &B2_vec_vmrghb,
  &B3_vec_vmrghb,
};
static struct builtin *O_vec_vmrghh[4] = {
  &B1_vec_vmrghh,
  &B2_vec_vmrghh,
  &B3_vec_vmrghh,
  &B4_vec_vmrghh,
};
static struct builtin *O_vec_vmrghw[4] = {
  &B1_vec_vmrghw,
  &B2_vec_vmrghw,
  &B3_vec_vmrghw,
  &B4_vec_vmrghw,
};
static struct builtin *O_vec_vmrglb[3] = {
  &B1_vec_vmrglb,
  &B2_vec_vmrglb,
  &B3_vec_vmrglb,
};
static struct builtin *O_vec_vmrglh[4] = {
  &B1_vec_vmrglh,
  &B2_vec_vmrglh,
  &B3_vec_vmrglh,
  &B4_vec_vmrglh,
};
static struct builtin *O_vec_vmrglw[4] = {
  &B1_vec_vmrglw,
  &B2_vec_vmrglw,
  &B3_vec_vmrglw,
  &B4_vec_vmrglw,
};
static struct builtin *O_vec_vmsummbm[1] = {
  &B_vec_vmsummbm,
};
static struct builtin *O_vec_vmsumshm[1] = {
  &B_vec_vmsumshm,
};
static struct builtin *O_vec_vmsumshs[1] = {
  &B_vec_vmsumshs,
};
static struct builtin *O_vec_vmsumubm[1] = {
  &B_vec_vmsumubm,
};
static struct builtin *O_vec_vmsumuhm[1] = {
  &B_vec_vmsumuhm,
};
static struct builtin *O_vec_vmsumuhs[1] = {
  &B_vec_vmsumuhs,
};
static struct builtin *O_vec_vmulesb[1] = {
  &B_vec_vmulesb,
};
static struct builtin *O_vec_vmulesh[1] = {
  &B_vec_vmulesh,
};
static struct builtin *O_vec_vmuleub[1] = {
  &B_vec_vmuleub,
};
static struct builtin *O_vec_vmuleuh[1] = {
  &B_vec_vmuleuh,
};
static struct builtin *O_vec_vmulosb[1] = {
  &B_vec_vmulosb,
};
static struct builtin *O_vec_vmulosh[1] = {
  &B_vec_vmulosh,
};
static struct builtin *O_vec_vmuloub[1] = {
  &B_vec_vmuloub,
};
static struct builtin *O_vec_vmulouh[1] = {
  &B_vec_vmulouh,
};
static struct builtin *O_vec_vnmsubfp[1] = {
  &B_vec_vnmsubfp,
};
static struct builtin *O_vec_vnor[10] = {
  &B1_vec_vnor,
  &B2_vec_vnor,
  &B3_vec_vnor,
  &B4_vec_vnor,
  &B5_vec_vnor,
  &B6_vec_vnor,
  &B7_vec_vnor,
  &B8_vec_vnor,
  &B9_vec_vnor,
  &B10_vec_vnor,
};
static struct builtin *O_vec_vor[24] = {
  &B1_vec_vor,
  &B2_vec_vor,
  &B3_vec_vor,
  &B4_vec_vor,
  &B5_vec_vor,
  &B6_vec_vor,
  &B7_vec_vor,
  &B8_vec_vor,
  &B9_vec_vor,
  &B10_vec_vor,
  &B11_vec_vor,
  &B12_vec_vor,
  &B13_vec_vor,
  &B14_vec_vor,
  &B15_vec_vor,
  &B16_vec_vor,
  &B17_vec_vor,
  &B18_vec_vor,
  &B19_vec_vor,
  &B20_vec_vor,
  &B21_vec_vor,
  &B22_vec_vor,
  &B23_vec_vor,
  &B24_vec_vor,
};
static struct builtin *O_vec_vperm[11] = {
  &B1_vec_vperm,
  &B2_vec_vperm,
  &B3_vec_vperm,
  &B4_vec_vperm,
  &B5_vec_vperm,
  &B6_vec_vperm,
  &B7_vec_vperm,
  &B8_vec_vperm,
  &B9_vec_vperm,
  &B10_vec_vperm,
  &B11_vec_vperm,
};
static struct builtin *O_vec_vpkpx[1] = {
  &B_vec_vpkpx,
};
static struct builtin *O_vec_vpkshss[1] = {
  &B_vec_vpkshss,
};
static struct builtin *O_vec_vpkshus[1] = {
  &B_vec_vpkshus,
};
static struct builtin *O_vec_vpkswss[1] = {
  &B_vec_vpkswss,
};
static struct builtin *O_vec_vpkswus[1] = {
  &B_vec_vpkswus,
};
static struct builtin *O_vec_vpkuhum[3] = {
  &B1_vec_vpkuhum,
  &B2_vec_vpkuhum,
  &B3_vec_vpkuhum,
};
static struct builtin *O_vec_vpkuhus[1] = {
  &B_vec_vpkuhus,
};
static struct builtin *O_vec_vpkuwum[3] = {
  &B1_vec_vpkuwum,
  &B2_vec_vpkuwum,
  &B3_vec_vpkuwum,
};
static struct builtin *O_vec_vpkuwus[1] = {
  &B_vec_vpkuwus,
};
static struct builtin *O_vec_vrefp[1] = {
  &B_vec_vrefp,
};
static struct builtin *O_vec_vrfim[1] = {
  &B_vec_vrfim,
};
static struct builtin *O_vec_vrfin[1] = {
  &B_vec_vrfin,
};
static struct builtin *O_vec_vrfip[1] = {
  &B_vec_vrfip,
};
static struct builtin *O_vec_vrfiz[1] = {
  &B_vec_vrfiz,
};
static struct builtin *O_vec_vrlb[2] = {
  &B1_vec_vrlb,
  &B2_vec_vrlb,
};
static struct builtin *O_vec_vrlh[2] = {
  &B1_vec_vrlh,
  &B2_vec_vrlh,
};
static struct builtin *O_vec_vrlw[2] = {
  &B1_vec_vrlw,
  &B2_vec_vrlw,
};
static struct builtin *O_vec_vrsqrtefp[1] = {
  &B_vec_vrsqrtefp,
};
static struct builtin *O_vec_vsel[20] = {
  &B1_vec_vsel,
  &B2_vec_vsel,
  &B3_vec_vsel,
  &B4_vec_vsel,
  &B5_vec_vsel,
  &B6_vec_vsel,
  &B7_vec_vsel,
  &B8_vec_vsel,
  &B9_vec_vsel,
  &B10_vec_vsel,
  &B11_vec_vsel,
  &B12_vec_vsel,
  &B13_vec_vsel,
  &B14_vec_vsel,
  &B15_vec_vsel,
  &B16_vec_vsel,
  &B17_vec_vsel,
  &B18_vec_vsel,
  &B19_vec_vsel,
  &B20_vec_vsel,
};
static struct builtin *O_vec_vsl[30] = {
  &B1_vec_vsl,
  &B2_vec_vsl,
  &B3_vec_vsl,
  &B4_vec_vsl,
  &B5_vec_vsl,
  &B6_vec_vsl,
  &B7_vec_vsl,
  &B8_vec_vsl,
  &B9_vec_vsl,
  &B10_vec_vsl,
  &B11_vec_vsl,
  &B12_vec_vsl,
  &B13_vec_vsl,
  &B14_vec_vsl,
  &B15_vec_vsl,
  &B16_vec_vsl,
  &B17_vec_vsl,
  &B18_vec_vsl,
  &B19_vec_vsl,
  &B20_vec_vsl,
  &B21_vec_vsl,
  &B22_vec_vsl,
  &B23_vec_vsl,
  &B24_vec_vsl,
  &B25_vec_vsl,
  &B26_vec_vsl,
  &B27_vec_vsl,
  &B28_vec_vsl,
  &B29_vec_vsl,
  &B30_vec_vsl,
};
static struct builtin *O_vec_vslb[2] = {
  &B1_vec_vslb,
  &B2_vec_vslb,
};
static struct builtin *O_vec_vsldoi[8] = {
  &B1_vec_vsldoi,
  &B2_vec_vsldoi,
  &B3_vec_vsldoi,
  &B4_vec_vsldoi,
  &B5_vec_vsldoi,
  &B6_vec_vsldoi,
  &B7_vec_vsldoi,
  &B8_vec_vsldoi,
};
static struct builtin *O_vec_vslh[2] = {
  &B1_vec_vslh,
  &B2_vec_vslh,
};
static struct builtin *O_vec_vslo[16] = {
  &B1_vec_vslo,
  &B2_vec_vslo,
  &B3_vec_vslo,
  &B4_vec_vslo,
  &B5_vec_vslo,
  &B6_vec_vslo,
  &B7_vec_vslo,
  &B8_vec_vslo,
  &B9_vec_vslo,
  &B10_vec_vslo,
  &B11_vec_vslo,
  &B12_vec_vslo,
  &B13_vec_vslo,
  &B14_vec_vslo,
  &B15_vec_vslo,
  &B16_vec_vslo,
};
static struct builtin *O_vec_vslw[2] = {
  &B1_vec_vslw,
  &B2_vec_vslw,
};
static struct builtin *O_vec_vspltb[3] = {
  &B1_vec_vspltb,
  &B2_vec_vspltb,
  &B3_vec_vspltb,
};
static struct builtin *O_vec_vsplth[4] = {
  &B1_vec_vsplth,
  &B2_vec_vsplth,
  &B3_vec_vsplth,
  &B4_vec_vsplth,
};
static struct builtin *O_vec_vspltisb[1] = {
  &B_vec_vspltisb,
};
static struct builtin *O_vec_vspltish[1] = {
  &B_vec_vspltish,
};
static struct builtin *O_vec_vspltisw[1] = {
  &B_vec_vspltisw,
};
static struct builtin *O_vec_vspltw[4] = {
  &B1_vec_vspltw,
  &B2_vec_vspltw,
  &B3_vec_vspltw,
  &B4_vec_vspltw,
};
static struct builtin *O_vec_vsr[30] = {
  &B1_vec_vsr,
  &B2_vec_vsr,
  &B3_vec_vsr,
  &B4_vec_vsr,
  &B5_vec_vsr,
  &B6_vec_vsr,
  &B7_vec_vsr,
  &B8_vec_vsr,
  &B9_vec_vsr,
  &B10_vec_vsr,
  &B11_vec_vsr,
  &B12_vec_vsr,
  &B13_vec_vsr,
  &B14_vec_vsr,
  &B15_vec_vsr,
  &B16_vec_vsr,
  &B17_vec_vsr,
  &B18_vec_vsr,
  &B19_vec_vsr,
  &B20_vec_vsr,
  &B21_vec_vsr,
  &B22_vec_vsr,
  &B23_vec_vsr,
  &B24_vec_vsr,
  &B25_vec_vsr,
  &B26_vec_vsr,
  &B27_vec_vsr,
  &B28_vec_vsr,
  &B29_vec_vsr,
  &B30_vec_vsr,
};
static struct builtin *O_vec_vsrab[2] = {
  &B1_vec_vsrab,
  &B2_vec_vsrab,
};
static struct builtin *O_vec_vsrah[2] = {
  &B1_vec_vsrah,
  &B2_vec_vsrah,
};
static struct builtin *O_vec_vsraw[2] = {
  &B1_vec_vsraw,
  &B2_vec_vsraw,
};
static struct builtin *O_vec_vsrb[2] = {
  &B1_vec_vsrb,
  &B2_vec_vsrb,
};
static struct builtin *O_vec_vsrh[2] = {
  &B1_vec_vsrh,
  &B2_vec_vsrh,
};
static struct builtin *O_vec_vsro[16] = {
  &B1_vec_vsro,
  &B2_vec_vsro,
  &B3_vec_vsro,
  &B4_vec_vsro,
  &B5_vec_vsro,
  &B6_vec_vsro,
  &B7_vec_vsro,
  &B8_vec_vsro,
  &B9_vec_vsro,
  &B10_vec_vsro,
  &B11_vec_vsro,
  &B12_vec_vsro,
  &B13_vec_vsro,
  &B14_vec_vsro,
  &B15_vec_vsro,
  &B16_vec_vsro,
};
static struct builtin *O_vec_vsrw[2] = {
  &B1_vec_vsrw,
  &B2_vec_vsrw,
};
static struct builtin *O_vec_vsubcuw[1] = {
  &B_vec_vsubcuw,
};
static struct builtin *O_vec_vsubfp[1] = {
  &B_vec_vsubfp,
};
static struct builtin *O_vec_vsubsbs[3] = {
  &B1_vec_vsubsbs,
  &B2_vec_vsubsbs,
  &B3_vec_vsubsbs,
};
static struct builtin *O_vec_vsubshs[3] = {
  &B1_vec_vsubshs,
  &B2_vec_vsubshs,
  &B3_vec_vsubshs,
};
static struct builtin *O_vec_vsubsws[3] = {
  &B1_vec_vsubsws,
  &B2_vec_vsubsws,
  &B3_vec_vsubsws,
};
static struct builtin *O_vec_vsububm[6] = {
  &B1_vec_vsububm,
  &B2_vec_vsububm,
  &B3_vec_vsububm,
  &B4_vec_vsububm,
  &B5_vec_vsububm,
  &B6_vec_vsububm,
};
static struct builtin *O_vec_vsububs[3] = {
  &B1_vec_vsububs,
  &B2_vec_vsububs,
  &B3_vec_vsububs,
};
static struct builtin *O_vec_vsubuhm[6] = {
  &B1_vec_vsubuhm,
  &B2_vec_vsubuhm,
  &B3_vec_vsubuhm,
  &B4_vec_vsubuhm,
  &B5_vec_vsubuhm,
  &B6_vec_vsubuhm,
};
static struct builtin *O_vec_vsubuhs[3] = {
  &B1_vec_vsubuhs,
  &B2_vec_vsubuhs,
  &B3_vec_vsubuhs,
};
static struct builtin *O_vec_vsubuwm[6] = {
  &B1_vec_vsubuwm,
  &B2_vec_vsubuwm,
  &B3_vec_vsubuwm,
  &B4_vec_vsubuwm,
  &B5_vec_vsubuwm,
  &B6_vec_vsubuwm,
};
static struct builtin *O_vec_vsubuws[3] = {
  &B1_vec_vsubuws,
  &B2_vec_vsubuws,
  &B3_vec_vsubuws,
};
static struct builtin *O_vec_vsum2sws[1] = {
  &B_vec_vsum2sws,
};
static struct builtin *O_vec_vsum4sbs[1] = {
  &B_vec_vsum4sbs,
};
static struct builtin *O_vec_vsum4shs[1] = {
  &B_vec_vsum4shs,
};
static struct builtin *O_vec_vsum4ubs[1] = {
  &B_vec_vsum4ubs,
};
static struct builtin *O_vec_vsumsws[1] = {
  &B_vec_vsumsws,
};
static struct builtin *O_vec_vupkhpx[1] = {
  &B_vec_vupkhpx,
};
static struct builtin *O_vec_vupkhsb[2] = {
  &B1_vec_vupkhsb,
  &B2_vec_vupkhsb,
};
static struct builtin *O_vec_vupkhsh[2] = {
  &B1_vec_vupkhsh,
  &B2_vec_vupkhsh,
};
static struct builtin *O_vec_vupklpx[1] = {
  &B_vec_vupklpx,
};
static struct builtin *O_vec_vupklsb[2] = {
  &B1_vec_vupklsb,
  &B2_vec_vupklsb,
};
static struct builtin *O_vec_vupklsh[2] = {
  &B1_vec_vupklsh,
  &B2_vec_vupklsh,
};
static struct builtin *O_vec_vxor[24] = {
  &B1_vec_vxor,
  &B2_vec_vxor,
  &B3_vec_vxor,
  &B4_vec_vxor,
  &B5_vec_vxor,
  &B6_vec_vxor,
  &B7_vec_vxor,
  &B8_vec_vxor,
  &B9_vec_vxor,
  &B10_vec_vxor,
  &B11_vec_vxor,
  &B12_vec_vxor,
  &B13_vec_vxor,
  &B14_vec_vxor,
  &B15_vec_vxor,
  &B16_vec_vxor,
  &B17_vec_vxor,
  &B18_vec_vxor,
  &B19_vec_vxor,
  &B20_vec_vxor,
  &B21_vec_vxor,
  &B22_vec_vxor,
  &B23_vec_vxor,
  &B24_vec_vxor,
};
static struct builtin *O_vec_xor[24] = {
  &B1_vec_vxor,
  &B2_vec_vxor,
  &B3_vec_vxor,
  &B4_vec_vxor,
  &B5_vec_vxor,
  &B6_vec_vxor,
  &B7_vec_vxor,
  &B8_vec_vxor,
  &B9_vec_vxor,
  &B10_vec_vxor,
  &B11_vec_vxor,
  &B12_vec_vxor,
  &B13_vec_vxor,
  &B14_vec_vxor,
  &B15_vec_vxor,
  &B16_vec_vxor,
  &B17_vec_vxor,
  &B18_vec_vxor,
  &B19_vec_vxor,
  &B20_vec_vxor,
  &B21_vec_vxor,
  &B22_vec_vxor,
  &B23_vec_vxor,
  &B24_vec_vxor,
};

struct overloadx Overload[] = {
  { "vec_abs", 4, 1, O_vec_abs, O_UID(0), NULL },
  { "vec_abss", 3, 1, O_vec_abss, O_UID(1), NULL },
  { "vec_add", 19, 2, O_vec_add, O_UID(2), NULL },
  { "vec_addc", 1, 2, O_vec_addc, O_UID(3), NULL },
  { "vec_adds", 18, 2, O_vec_adds, O_UID(4), NULL },
  { "vec_all_eq", 23, 2, O_vec_all_eq, O_UID(5), NULL },
  { "vec_all_ge", 19, 2, O_vec_all_ge, O_UID(6), NULL },
  { "vec_all_gt", 19, 2, O_vec_all_gt, O_UID(7), NULL },
  { "vec_all_in", 1, 2, O_vec_all_in, O_UID(8), NULL },
  { "vec_all_le", 19, 2, O_vec_all_le, O_UID(9), NULL },
  { "vec_all_lt", 19, 2, O_vec_all_lt, O_UID(10), NULL },
  { "vec_all_nan", 1, 1, O_vec_all_nan, O_UID(11), NULL },
  { "vec_all_ne", 23, 2, O_vec_all_ne, O_UID(12), NULL },
  { "vec_all_nge", 1, 2, O_vec_all_nge, O_UID(13), NULL },
  { "vec_all_ngt", 1, 2, O_vec_all_ngt, O_UID(14), NULL },
  { "vec_all_nle", 1, 2, O_vec_all_nle, O_UID(15), NULL },
  { "vec_all_nlt", 1, 2, O_vec_all_nlt, O_UID(16), NULL },
  { "vec_all_numeric", 1, 1, O_vec_all_numeric, O_UID(17), NULL },
  { "vec_and", 24, 2, O_vec_and, O_UID(18), NULL },
  { "vec_andc", 24, 2, O_vec_andc, O_UID(19), NULL },
  { "vec_any_eq", 23, 2, O_vec_any_eq, O_UID(20), NULL },
  { "vec_any_ge", 19, 2, O_vec_any_ge, O_UID(21), NULL },
  { "vec_any_gt", 19, 2, O_vec_any_gt, O_UID(22), NULL },
  { "vec_any_le", 19, 2, O_vec_any_le, O_UID(23), NULL },
  { "vec_any_lt", 19, 2, O_vec_any_lt, O_UID(24), NULL },
  { "vec_any_nan", 1, 1, O_vec_any_nan, O_UID(25), NULL },
  { "vec_any_ne", 23, 2, O_vec_any_ne, O_UID(26), NULL },
  { "vec_any_nge", 1, 2, O_vec_any_nge, O_UID(27), NULL },
  { "vec_any_ngt", 1, 2, O_vec_any_ngt, O_UID(28), NULL },
  { "vec_any_nle", 1, 2, O_vec_any_nle, O_UID(29), NULL },
  { "vec_any_nlt", 1, 2, O_vec_any_nlt, O_UID(30), NULL },
  { "vec_any_numeric", 1, 1, O_vec_any_numeric, O_UID(31), NULL },
  { "vec_any_out", 1, 2, O_vec_any_out, O_UID(32), NULL },
  { "vec_avg", 6, 2, O_vec_avg, O_UID(33), NULL },
  { "vec_ceil", 1, 1, O_vec_ceil, O_UID(34), NULL },
  { "vec_cmpb", 1, 2, O_vec_cmpb, O_UID(35), NULL },
  { "vec_cmpeq", 7, 2, O_vec_cmpeq, O_UID(36), NULL },
  { "vec_cmpge", 1, 2, O_vec_cmpge, O_UID(37), NULL },
  { "vec_cmpgt", 7, 2, O_vec_cmpgt, O_UID(38), NULL },
  { "vec_cmple", 1, 2, O_vec_cmple, O_UID(39), NULL },
  { "vec_cmplt", 7, 2, O_vec_cmplt, O_UID(40), NULL },
  { "vec_ctf", 2, 2, O_vec_ctf, O_UID(41), NULL },
  { "vec_cts", 1, 2, O_vec_cts, O_UID(42), NULL },
  { "vec_ctu", 1, 2, O_vec_ctu, O_UID(43), NULL },
  { "vec_dss", 1, 1, O_vec_dss, O_UID(44), NULL },
  { "vec_dssall", 1, 0, O_vec_dssall, O_UID(45), NULL },
  { "vec_dst", 20, 3, O_vec_dst, O_UID(46), NULL },
  { "vec_dstst", 20, 3, O_vec_dstst, O_UID(47), NULL },
  { "vec_dststt", 20, 3, O_vec_dststt, O_UID(48), NULL },
  { "vec_dstt", 20, 3, O_vec_dstt, O_UID(49), NULL },
  { "vec_expte", 1, 1, O_vec_expte, O_UID(50), NULL },
  { "vec_floor", 1, 1, O_vec_floor, O_UID(51), NULL },
  { "vec_ld", 20, 2, O_vec_ld, O_UID(52), NULL },
  { "vec_lde", 9, 2, O_vec_lde, O_UID(53), NULL },
  { "vec_ldl", 20, 2, O_vec_ldl, O_UID(54), NULL },
  { "vec_loge", 1, 1, O_vec_loge, O_UID(55), NULL },
  { "vec_lvebx", 2, 2, O_vec_lvebx, O_UID(56), NULL },
  { "vec_lvehx", 2, 2, O_vec_lvehx, O_UID(57), NULL },
  { "vec_lvewx", 5, 2, O_vec_lvewx, O_UID(58), NULL },
  { "vec_lvsl", 9, 2, O_vec_lvsl, O_UID(59), NULL },
  { "vec_lvsr", 9, 2, O_vec_lvsr, O_UID(60), NULL },
  { "vec_lvx", 20, 2, O_vec_lvx, O_UID(61), NULL },
  { "vec_lvxl", 20, 2, O_vec_lvxl, O_UID(62), NULL },
  { "vec_madd", 1, 3, O_vec_madd, O_UID(63), NULL },
  { "vec_madds", 1, 3, O_vec_madds, O_UID(64), NULL },
  { "vec_max", 19, 2, O_vec_max, O_UID(65), NULL },
  { "vec_mergeh", 11, 2, O_vec_mergeh, O_UID(66), NULL },
  { "vec_mergel", 11, 2, O_vec_mergel, O_UID(67), NULL },
  { "vec_mfvscr", 1, 0, O_vec_mfvscr, O_UID(68), NULL },
  { "vec_min", 19, 2, O_vec_min, O_UID(69), NULL },
  { "vec_mladd", 4, 3, O_vec_mladd, O_UID(70), NULL },
  { "vec_mradds", 1, 3, O_vec_mradds, O_UID(71), NULL },
  { "vec_msum", 4, 3, O_vec_msum, O_UID(72), NULL },
  { "vec_msums", 2, 3, O_vec_msums, O_UID(73), NULL },
  { "vec_mtvscr", 10, 1, O_vec_mtvscr, O_UID(74), NULL },
  { "vec_mule", 4, 2, O_vec_mule, O_UID(75), NULL },
  { "vec_mulo", 4, 2, O_vec_mulo, O_UID(76), NULL },
  { "vec_nmsub", 1, 3, O_vec_nmsub, O_UID(77), NULL },
  { "vec_nor", 10, 2, O_vec_nor, O_UID(78), NULL },
  { "vec_or", 24, 2, O_vec_or, O_UID(79), NULL },
  { "vec_pack", 6, 2, O_vec_pack, O_UID(80), NULL },
  { "vec_packpx", 1, 2, O_vec_packpx, O_UID(81), NULL },
  { "vec_packs", 4, 2, O_vec_packs, O_UID(82), NULL },
  { "vec_packsu", 4, 2, O_vec_packsu, O_UID(83), NULL },
  { "vec_perm", 11, 3, O_vec_perm, O_UID(84), NULL },
  { "vec_re", 1, 1, O_vec_re, O_UID(85), NULL },
  { "vec_rl", 6, 2, O_vec_rl, O_UID(86), NULL },
  { "vec_round", 1, 1, O_vec_round, O_UID(87), NULL },
  { "vec_rsqrte", 1, 1, O_vec_rsqrte, O_UID(88), NULL },
  { "vec_sel", 20, 3, O_vec_sel, O_UID(89), NULL },
  { "vec_sl", 6, 2, O_vec_sl, O_UID(90), NULL },
  { "vec_sld", 8, 3, O_vec_sld, O_UID(91), NULL },
  { "vec_sll", 30, 2, O_vec_sll, O_UID(92), NULL },
  { "vec_slo", 16, 2, O_vec_slo, O_UID(93), NULL },
  { "vec_splat", 11, 2, O_vec_splat, O_UID(94), NULL },
  { "vec_splat_s16", 1, 1, O_vec_splat_s16, O_UID(95), NULL },
  { "vec_splat_s32", 1, 1, O_vec_splat_s32, O_UID(96), NULL },
  { "vec_splat_s8", 1, 1, O_vec_splat_s8, O_UID(97), NULL },
  { "vec_splat_u16", 1, 1, O_vec_splat_u16, O_UID(98), NULL },
  { "vec_splat_u32", 1, 1, O_vec_splat_u32, O_UID(99), NULL },
  { "vec_splat_u8", 1, 1, O_vec_splat_u8, O_UID(100), NULL },
  { "vec_sr", 6, 2, O_vec_sr, O_UID(101), NULL },
  { "vec_sra", 6, 2, O_vec_sra, O_UID(102), NULL },
  { "vec_srl", 30, 2, O_vec_srl, O_UID(103), NULL },
  { "vec_sro", 16, 2, O_vec_sro, O_UID(104), NULL },
  { "vec_st", 20, 3, O_vec_st, O_UID(105), NULL },
  { "vec_ste", 9, 3, O_vec_ste, O_UID(106), NULL },
  { "vec_stl", 20, 3, O_vec_stl, O_UID(107), NULL },
  { "vec_stvebx", 2, 3, O_vec_stvebx, O_UID(108), NULL },
  { "vec_stvehx", 2, 3, O_vec_stvehx, O_UID(109), NULL },
  { "vec_stvewx", 5, 3, O_vec_stvewx, O_UID(110), NULL },
  { "vec_stvx", 20, 3, O_vec_stvx, O_UID(111), NULL },
  { "vec_stvxl", 20, 3, O_vec_stvxl, O_UID(112), NULL },
  { "vec_sub", 19, 2, O_vec_sub, O_UID(113), NULL },
  { "vec_subc", 1, 2, O_vec_subc, O_UID(114), NULL },
  { "vec_subs", 18, 2, O_vec_subs, O_UID(115), NULL },
  { "vec_sum2s", 1, 2, O_vec_sum2s, O_UID(116), NULL },
  { "vec_sum4s", 3, 2, O_vec_sum4s, O_UID(117), NULL },
  { "vec_sums", 1, 2, O_vec_sums, O_UID(118), NULL },
  { "vec_trunc", 1, 1, O_vec_trunc, O_UID(119), NULL },
  { "vec_unpackh", 5, 1, O_vec_unpackh, O_UID(120), NULL },
  { "vec_unpackl", 5, 1, O_vec_unpackl, O_UID(121), NULL },
  { "vec_vabsfp", 1, 1, O_vec_vabsfp, O_UID(122), NULL },
  { "vec_vabssb", 1, 1, O_vec_vabssb, O_UID(123), NULL },
  { "vec_vabssh", 1, 1, O_vec_vabssh, O_UID(124), NULL },
  { "vec_vabsssb", 1, 1, O_vec_vabsssb, O_UID(125), NULL },
  { "vec_vabsssh", 1, 1, O_vec_vabsssh, O_UID(126), NULL },
  { "vec_vabsssw", 1, 1, O_vec_vabsssw, O_UID(127), NULL },
  { "vec_vabssw", 1, 1, O_vec_vabssw, O_UID(128), NULL },
  { "vec_vaddcuw", 1, 2, O_vec_vaddcuw, O_UID(129), NULL },
  { "vec_vaddfp", 1, 2, O_vec_vaddfp, O_UID(130), NULL },
  { "vec_vaddsbs", 3, 2, O_vec_vaddsbs, O_UID(131), NULL },
  { "vec_vaddshs", 3, 2, O_vec_vaddshs, O_UID(132), NULL },
  { "vec_vaddsws", 3, 2, O_vec_vaddsws, O_UID(133), NULL },
  { "vec_vaddubm", 6, 2, O_vec_vaddubm, O_UID(134), NULL },
  { "vec_vaddubs", 3, 2, O_vec_vaddubs, O_UID(135), NULL },
  { "vec_vadduhm", 6, 2, O_vec_vadduhm, O_UID(136), NULL },
  { "vec_vadduhs", 3, 2, O_vec_vadduhs, O_UID(137), NULL },
  { "vec_vadduwm", 6, 2, O_vec_vadduwm, O_UID(138), NULL },
  { "vec_vadduws", 3, 2, O_vec_vadduws, O_UID(139), NULL },
  { "vec_vand", 24, 2, O_vec_vand, O_UID(140), NULL },
  { "vec_vandc", 24, 2, O_vec_vandc, O_UID(141), NULL },
  { "vec_vavgsb", 1, 2, O_vec_vavgsb, O_UID(142), NULL },
  { "vec_vavgsh", 1, 2, O_vec_vavgsh, O_UID(143), NULL },
  { "vec_vavgsw", 1, 2, O_vec_vavgsw, O_UID(144), NULL },
  { "vec_vavgub", 1, 2, O_vec_vavgub, O_UID(145), NULL },
  { "vec_vavguh", 1, 2, O_vec_vavguh, O_UID(146), NULL },
  { "vec_vavguw", 1, 2, O_vec_vavguw, O_UID(147), NULL },
  { "vec_vcfsx", 1, 2, O_vec_vcfsx, O_UID(148), NULL },
  { "vec_vcfux", 1, 2, O_vec_vcfux, O_UID(149), NULL },
  { "vec_vcmpbfp", 1, 2, O_vec_vcmpbfp, O_UID(150), NULL },
  { "vec_vcmpeqfp", 1, 2, O_vec_vcmpeqfp, O_UID(151), NULL },
  { "vec_vcmpequb", 2, 2, O_vec_vcmpequb, O_UID(152), NULL },
  { "vec_vcmpequh", 2, 2, O_vec_vcmpequh, O_UID(153), NULL },
  { "vec_vcmpequw", 2, 2, O_vec_vcmpequw, O_UID(154), NULL },
  { "vec_vcmpgefp", 1, 2, O_vec_vcmpgefp, O_UID(155), NULL },
  { "vec_vcmpgtfp", 1, 2, O_vec_vcmpgtfp, O_UID(156), NULL },
  { "vec_vcmpgtsb", 1, 2, O_vec_vcmpgtsb, O_UID(157), NULL },
  { "vec_vcmpgtsh", 1, 2, O_vec_vcmpgtsh, O_UID(158), NULL },
  { "vec_vcmpgtsw", 1, 2, O_vec_vcmpgtsw, O_UID(159), NULL },
  { "vec_vcmpgtub", 1, 2, O_vec_vcmpgtub, O_UID(160), NULL },
  { "vec_vcmpgtuh", 1, 2, O_vec_vcmpgtuh, O_UID(161), NULL },
  { "vec_vcmpgtuw", 1, 2, O_vec_vcmpgtuw, O_UID(162), NULL },
  { "vec_vcmplefp", 1, 2, O_vec_vcmplefp, O_UID(163), NULL },
  { "vec_vcmpltfp", 1, 2, O_vec_vcmpltfp, O_UID(164), NULL },
  { "vec_vcmpltsb", 1, 2, O_vec_vcmpltsb, O_UID(165), NULL },
  { "vec_vcmpltsh", 1, 2, O_vec_vcmpltsh, O_UID(166), NULL },
  { "vec_vcmpltsw", 1, 2, O_vec_vcmpltsw, O_UID(167), NULL },
  { "vec_vcmpltub", 1, 2, O_vec_vcmpltub, O_UID(168), NULL },
  { "vec_vcmpltuh", 1, 2, O_vec_vcmpltuh, O_UID(169), NULL },
  { "vec_vcmpltuw", 1, 2, O_vec_vcmpltuw, O_UID(170), NULL },
  { "vec_vctsxs", 1, 2, O_vec_vctsxs, O_UID(171), NULL },
  { "vec_vctuxs", 1, 2, O_vec_vctuxs, O_UID(172), NULL },
  { "vec_vexptefp", 1, 1, O_vec_vexptefp, O_UID(173), NULL },
  { "vec_vlogefp", 1, 1, O_vec_vlogefp, O_UID(174), NULL },
  { "vec_vmaddfp", 1, 3, O_vec_vmaddfp, O_UID(175), NULL },
  { "vec_vmaxfp", 1, 2, O_vec_vmaxfp, O_UID(176), NULL },
  { "vec_vmaxsb", 3, 2, O_vec_vmaxsb, O_UID(177), NULL },
  { "vec_vmaxsh", 3, 2, O_vec_vmaxsh, O_UID(178), NULL },
  { "vec_vmaxsw", 3, 2, O_vec_vmaxsw, O_UID(179), NULL },
  { "vec_vmaxub", 3, 2, O_vec_vmaxub, O_UID(180), NULL },
  { "vec_vmaxuh", 3, 2, O_vec_vmaxuh, O_UID(181), NULL },
  { "vec_vmaxuw", 3, 2, O_vec_vmaxuw, O_UID(182), NULL },
  { "vec_vmhaddshs", 1, 3, O_vec_vmhaddshs, O_UID(183), NULL },
  { "vec_vmhraddshs", 1, 3, O_vec_vmhraddshs, O_UID(184), NULL },
  { "vec_vminfp", 1, 2, O_vec_vminfp, O_UID(185), NULL },
  { "vec_vminsb", 3, 2, O_vec_vminsb, O_UID(186), NULL },
  { "vec_vminsh", 3, 2, O_vec_vminsh, O_UID(187), NULL },
  { "vec_vminsw", 3, 2, O_vec_vminsw, O_UID(188), NULL },
  { "vec_vminub", 3, 2, O_vec_vminub, O_UID(189), NULL },
  { "vec_vminuh", 3, 2, O_vec_vminuh, O_UID(190), NULL },
  { "vec_vminuw", 3, 2, O_vec_vminuw, O_UID(191), NULL },
  { "vec_vmladduhm", 4, 3, O_vec_vmladduhm, O_UID(192), NULL },
  { "vec_vmrghb", 3, 2, O_vec_vmrghb, O_UID(193), NULL },
  { "vec_vmrghh", 4, 2, O_vec_vmrghh, O_UID(194), NULL },
  { "vec_vmrghw", 4, 2, O_vec_vmrghw, O_UID(195), NULL },
  { "vec_vmrglb", 3, 2, O_vec_vmrglb, O_UID(196), NULL },
  { "vec_vmrglh", 4, 2, O_vec_vmrglh, O_UID(197), NULL },
  { "vec_vmrglw", 4, 2, O_vec_vmrglw, O_UID(198), NULL },
  { "vec_vmsummbm", 1, 3, O_vec_vmsummbm, O_UID(199), NULL },
  { "vec_vmsumshm", 1, 3, O_vec_vmsumshm, O_UID(200), NULL },
  { "vec_vmsumshs", 1, 3, O_vec_vmsumshs, O_UID(201), NULL },
  { "vec_vmsumubm", 1, 3, O_vec_vmsumubm, O_UID(202), NULL },
  { "vec_vmsumuhm", 1, 3, O_vec_vmsumuhm, O_UID(203), NULL },
  { "vec_vmsumuhs", 1, 3, O_vec_vmsumuhs, O_UID(204), NULL },
  { "vec_vmulesb", 1, 2, O_vec_vmulesb, O_UID(205), NULL },
  { "vec_vmulesh", 1, 2, O_vec_vmulesh, O_UID(206), NULL },
  { "vec_vmuleub", 1, 2, O_vec_vmuleub, O_UID(207), NULL },
  { "vec_vmuleuh", 1, 2, O_vec_vmuleuh, O_UID(208), NULL },
  { "vec_vmulosb", 1, 2, O_vec_vmulosb, O_UID(209), NULL },
  { "vec_vmulosh", 1, 2, O_vec_vmulosh, O_UID(210), NULL },
  { "vec_vmuloub", 1, 2, O_vec_vmuloub, O_UID(211), NULL },
  { "vec_vmulouh", 1, 2, O_vec_vmulouh, O_UID(212), NULL },
  { "vec_vnmsubfp", 1, 3, O_vec_vnmsubfp, O_UID(213), NULL },
  { "vec_vnor", 10, 2, O_vec_vnor, O_UID(214), NULL },
  { "vec_vor", 24, 2, O_vec_vor, O_UID(215), NULL },
  { "vec_vperm", 11, 3, O_vec_vperm, O_UID(216), NULL },
  { "vec_vpkpx", 1, 2, O_vec_vpkpx, O_UID(217), NULL },
  { "vec_vpkshss", 1, 2, O_vec_vpkshss, O_UID(218), NULL },
  { "vec_vpkshus", 1, 2, O_vec_vpkshus, O_UID(219), NULL },
  { "vec_vpkswss", 1, 2, O_vec_vpkswss, O_UID(220), NULL },
  { "vec_vpkswus", 1, 2, O_vec_vpkswus, O_UID(221), NULL },
  { "vec_vpkuhum", 3, 2, O_vec_vpkuhum, O_UID(222), NULL },
  { "vec_vpkuhus", 1, 2, O_vec_vpkuhus, O_UID(223), NULL },
  { "vec_vpkuwum", 3, 2, O_vec_vpkuwum, O_UID(224), NULL },
  { "vec_vpkuwus", 1, 2, O_vec_vpkuwus, O_UID(225), NULL },
  { "vec_vrefp", 1, 1, O_vec_vrefp, O_UID(226), NULL },
  { "vec_vrfim", 1, 1, O_vec_vrfim, O_UID(227), NULL },
  { "vec_vrfin", 1, 1, O_vec_vrfin, O_UID(228), NULL },
  { "vec_vrfip", 1, 1, O_vec_vrfip, O_UID(229), NULL },
  { "vec_vrfiz", 1, 1, O_vec_vrfiz, O_UID(230), NULL },
  { "vec_vrlb", 2, 2, O_vec_vrlb, O_UID(231), NULL },
  { "vec_vrlh", 2, 2, O_vec_vrlh, O_UID(232), NULL },
  { "vec_vrlw", 2, 2, O_vec_vrlw, O_UID(233), NULL },
  { "vec_vrsqrtefp", 1, 1, O_vec_vrsqrtefp, O_UID(234), NULL },
  { "vec_vsel", 20, 3, O_vec_vsel, O_UID(235), NULL },
  { "vec_vsl", 30, 2, O_vec_vsl, O_UID(236), NULL },
  { "vec_vslb", 2, 2, O_vec_vslb, O_UID(237), NULL },
  { "vec_vsldoi", 8, 3, O_vec_vsldoi, O_UID(238), NULL },
  { "vec_vslh", 2, 2, O_vec_vslh, O_UID(239), NULL },
  { "vec_vslo", 16, 2, O_vec_vslo, O_UID(240), NULL },
  { "vec_vslw", 2, 2, O_vec_vslw, O_UID(241), NULL },
  { "vec_vspltb", 3, 2, O_vec_vspltb, O_UID(242), NULL },
  { "vec_vsplth", 4, 2, O_vec_vsplth, O_UID(243), NULL },
  { "vec_vspltisb", 1, 1, O_vec_vspltisb, O_UID(244), NULL },
  { "vec_vspltish", 1, 1, O_vec_vspltish, O_UID(245), NULL },
  { "vec_vspltisw", 1, 1, O_vec_vspltisw, O_UID(246), NULL },
  { "vec_vspltw", 4, 2, O_vec_vspltw, O_UID(247), NULL },
  { "vec_vsr", 30, 2, O_vec_vsr, O_UID(248), NULL },
  { "vec_vsrab", 2, 2, O_vec_vsrab, O_UID(249), NULL },
  { "vec_vsrah", 2, 2, O_vec_vsrah, O_UID(250), NULL },
  { "vec_vsraw", 2, 2, O_vec_vsraw, O_UID(251), NULL },
  { "vec_vsrb", 2, 2, O_vec_vsrb, O_UID(252), NULL },
  { "vec_vsrh", 2, 2, O_vec_vsrh, O_UID(253), NULL },
  { "vec_vsro", 16, 2, O_vec_vsro, O_UID(254), NULL },
  { "vec_vsrw", 2, 2, O_vec_vsrw, O_UID(255), NULL },
  { "vec_vsubcuw", 1, 2, O_vec_vsubcuw, O_UID(256), NULL },
  { "vec_vsubfp", 1, 2, O_vec_vsubfp, O_UID(257), NULL },
  { "vec_vsubsbs", 3, 2, O_vec_vsubsbs, O_UID(258), NULL },
  { "vec_vsubshs", 3, 2, O_vec_vsubshs, O_UID(259), NULL },
  { "vec_vsubsws", 3, 2, O_vec_vsubsws, O_UID(260), NULL },
  { "vec_vsububm", 6, 2, O_vec_vsububm, O_UID(261), NULL },
  { "vec_vsububs", 3, 2, O_vec_vsububs, O_UID(262), NULL },
  { "vec_vsubuhm", 6, 2, O_vec_vsubuhm, O_UID(263), NULL },
  { "vec_vsubuhs", 3, 2, O_vec_vsubuhs, O_UID(264), NULL },
  { "vec_vsubuwm", 6, 2, O_vec_vsubuwm, O_UID(265), NULL },
  { "vec_vsubuws", 3, 2, O_vec_vsubuws, O_UID(266), NULL },
  { "vec_vsum2sws", 1, 2, O_vec_vsum2sws, O_UID(267), NULL },
  { "vec_vsum4sbs", 1, 2, O_vec_vsum4sbs, O_UID(268), NULL },
  { "vec_vsum4shs", 1, 2, O_vec_vsum4shs, O_UID(269), NULL },
  { "vec_vsum4ubs", 1, 2, O_vec_vsum4ubs, O_UID(270), NULL },
  { "vec_vsumsws", 1, 2, O_vec_vsumsws, O_UID(271), NULL },
  { "vec_vupkhpx", 1, 1, O_vec_vupkhpx, O_UID(272), NULL },
  { "vec_vupkhsb", 2, 1, O_vec_vupkhsb, O_UID(273), NULL },
  { "vec_vupkhsh", 2, 1, O_vec_vupkhsh, O_UID(274), NULL },
  { "vec_vupklpx", 1, 1, O_vec_vupklpx, O_UID(275), NULL },
  { "vec_vupklsb", 2, 1, O_vec_vupklsb, O_UID(276), NULL },
  { "vec_vupklsh", 2, 1, O_vec_vupklsh, O_UID(277), NULL },
  { "vec_vxor", 24, 2, O_vec_vxor, O_UID(278), NULL },
  { "vec_xor", 24, 2, O_vec_xor, O_UID(279), NULL },
  { NULL, 0, 0, NULL, 0, NULL }
};
#define LAST_O_UID O_UID(280)
