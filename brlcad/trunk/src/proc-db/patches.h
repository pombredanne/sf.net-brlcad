/*                       P A T C H E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file patches.h
 *
 */

pt patches[PATCH_COUNT] = {
    {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}},
    {{4, 17, 18, 19}, {8, 20, 21, 22}, {12, 23, 24, 25}, {16, 26, 27, 28}},
    {{19, 29, 30, 31}, {22, 32, 33, 34}, {25, 35, 36, 37}, {28, 38, 39, 40}},
    {{31, 41, 42, 1}, {34, 43, 44, 5}, {37, 45, 46, 9}, {40, 47, 48, 13}},
    {{13, 14, 15, 16}, {49, 50, 51, 52}, {53, 54, 55, 56}, {57, 58, 59, 60}},
    {{16, 26, 27, 28}, {52, 61, 62, 63}, {56, 64, 65, 66}, {60, 67, 68, 69}},
    {{28, 38, 39, 40}, {63, 70, 71, 72}, {66, 73, 74, 75}, {69, 76, 77, 78}},
    {{40, 47, 48, 13}, {72, 79, 80, 49}, {75, 81, 82, 53}, {78, 83, 84, 57}},
    {{57, 58, 59, 60}, {85, 86, 87, 88}, {89, 90, 91, 92}, {93, 94, 95, 96}},
    {{60, 67, 68, 69}, {88, 97, 98, 99}, {92, 100, 101, 102}, {96, 103, 104, 105}},
    {{69, 76, 77, 78}, {99, 106, 107, 108}, {102, 109, 110, 111}, {105, 112, 113, 114}},
    {{78, 83, 84, 57}, {108, 115, 116, 85}, {111, 117, 118, 89}, {114, 119, 120, 93}},
    {{121, 122, 123, 124}, {125, 126, 127, 128}, {129, 130, 131, 132}, {133, 134, 135, 136}},
    {{124, 137, 138, 121}, {128, 139, 140, 125}, {132, 141, 142, 129}, {136, 143, 144, 133}},
    {{133, 134, 135, 136}, {145, 146, 147, 148}, {149, 150, 151, 152}, {69, 153, 154, 155}},
    {{136, 143, 144, 133}, {148, 156, 157, 145}, {152, 158, 159, 149}, {155, 160, 161, 69}},
    {{162, 163, 164, 165}, {166, 167, 168, 169}, {170, 171, 172, 173}, {174, 175, 176, 177}},
    {{165, 178, 179, 162}, {169, 180, 181, 166}, {173, 182, 183, 170}, {177, 184, 185, 174}},
    {{174, 175, 176, 177}, {186, 187, 188, 189}, {190, 191, 192, 193}, {194, 195, 196, 197}},
    {{177, 184, 185, 174}, {189, 198, 199, 186}, {193, 200, 201, 190}, {197, 202, 203, 194}},
    {{204, 204, 204, 204}, {207, 208, 209, 210}, {211, 211, 211, 211}, {212, 213, 214, 215}},
    {{204, 204, 204, 204}, {210, 217, 218, 219}, {211, 211, 211, 211}, {215, 220, 221, 222}},
    {{204, 204, 204, 204}, {219, 224, 225, 226}, {211, 211, 211, 211}, {222, 227, 228, 229}},
    {{204, 204, 204, 204}, {226, 230, 231, 207}, {211, 211, 211, 211}, {229, 232, 233, 212}},
    {{212, 213, 214, 215}, {234, 235, 236, 237}, {238, 239, 240, 241}, {242, 243, 244, 245}},
    {{215, 220, 221, 222}, {237, 246, 247, 248}, {241, 249, 250, 251}, {245, 252, 253, 254}},
    {{222, 227, 228, 229}, {248, 255, 256, 257}, {251, 258, 259, 260}, {254, 261, 262, 263}},
    {{229, 232, 233, 212}, {257, 264, 265, 234}, {260, 266, 267, 238}, {263, 268, 269, 242}},
    {{270, 270, 270, 270}, {279, 280, 281, 282}, {275, 276, 277, 278}, {271, 272, 273, 274}},
    {{270, 270, 270, 270}, {282, 289, 290, 291}, {278, 286, 287, 288}, {274, 283, 284, 285}},
    {{270, 270, 270, 270}, {291, 298, 299, 300}, {288, 295, 296, 297}, {285, 292, 293, 294}},
    {{270, 270, 270, 270}, {300, 305, 306, 279}, {297, 303, 304, 275}, {294, 301, 302, 271}}
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
