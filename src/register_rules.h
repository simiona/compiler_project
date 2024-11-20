#ifndef REGISTER_RULRES
#define REGISTER_RULRES

#include <set>
#include <array>
const std::set<int> allocateRegs{9, 10, 11, 12, 13, 14, 15};//caller-save
const int XXna = 28;
const int XXnb = 27;
const std::array<int, 8> paramRegs = {0, 1, 2, 3, 4, 5, 6, 7};
const int XXn1 = 16;
const int XXn2 = 17;
const int XXn3 = 18;
const int XXn4 = 19;
const int FPR = 29;
const int RETR = 0;
const int LNR = 30;
const int INT_LENGTH = 4;
#endif