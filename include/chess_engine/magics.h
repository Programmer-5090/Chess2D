#ifndef MAGICS_H
#define MAGICS_H

/*
* Black magic bitboards by Volker Annuss
*/

#include <cstdint>

namespace Chess {
	inline constexpr int lookupTableSize = 88507;
	extern uint64_t lookupTable[lookupTableSize];

	struct RawMagicEntry {
		uint64_t factor;
		int position;
	};

	struct MagicEntry {
		uint64_t* ptr;
		uint64_t mask;
		uint64_t magic;
		int shift;
	};

	extern MagicEntry bishopMagics[64];
	extern MagicEntry rookMagics[64];
	void initialize_magics();

	inline constexpr RawMagicEntry bishopMagicsRaw[64] =
	{
		{ 0x107ac08050500bffull,  66157 },
		{ 0x7fffdfdfd823fffdull,  71730 },
		{ 0x0400c00fe8000200ull,  37781 },
		{ 0x103f802004000000ull,  21015 },
		{ 0xc03fe00100000000ull,  47590 },
		{ 0x24c00bffff400000ull,    835 },
		{ 0x0808101f40007f04ull,  23592 },
		{ 0x100808201ec00080ull,  30599 },
		{ 0xffa2feffbfefb7ffull,  68776 },
		{ 0x083e3ee040080801ull,  19959 },
		{ 0x040180bff7e80080ull,  21783 },
		{ 0x0440007fe0031000ull,  64836 },
		{ 0x2010007ffc000000ull,  23417 },
		{ 0x1079ffe000ff8000ull,  66724 },
		{ 0x7f83ffdfc03fff80ull,  74542 },
		{ 0x080614080fa00040ull,  67266 },
		{ 0x7ffe7fff817fcff9ull,  26575 },
		{ 0x7ffebfffa01027fdull,  67543 },
		{ 0x20018000c00f3c01ull,  24409 },
		{ 0x407e0001000ffb8aull,  30779 },
		{ 0x201fe000fff80010ull,  17384 },
		{ 0xffdfefffde39ffefull,  18778 },
		{ 0x7ffff800203fbfffull,  65109 },
		{ 0x7ff7fbfff8203fffull,  20184 },
		{ 0x000000fe04004070ull,  38240 },
		{ 0x7fff7f9fffc0eff9ull,  16459 },
		{ 0x7ffeff7f7f01f7fdull,  17432 },
		{ 0x3f6efbbf9efbffffull,  81040 },
		{ 0x0410008f01003ffdull,  84946 },
		{ 0x20002038001c8010ull,  18276 },
		{ 0x087ff038000fc001ull,   8512 },
		{ 0x00080c0c00083007ull,  78544 },
		{ 0x00000080fc82c040ull,  19974 },
		{ 0x000000407e416020ull,  23850 },
		{ 0x00600203f8008020ull,  11056 },
		{ 0xd003fefe04404080ull,  68019 },
		{ 0x100020801800304aull,  85965 },
		{ 0x7fbffe700bffe800ull,  80524 },
		{ 0x107ff00fe4000f90ull,  38221 },
		{ 0x7f8fffcff1d007f8ull,  64647 },
		{ 0x0000004100f88080ull,  61320 },
		{ 0x00000020807c4040ull,  67281 },
		{ 0x00000041018700c0ull,  79076 },
		{ 0x0010000080fc4080ull,  17115 },
		{ 0x1000003c80180030ull,  50718 },
		{ 0x2006001cf00c0018ull,  24659 },
		{ 0xffffffbfeff80fdcull,  38291 },
		{ 0x000000101003f812ull,  30605 },
		{ 0x0800001f40808200ull,  37759 },
		{ 0x084000101f3fd208ull,   4639 },
		{ 0x080000000f808081ull,  21759 },
		{ 0x0004000008003f80ull,  67799 },
		{ 0x08000001001fe040ull,  22841 },
		{ 0x085f7d8000200a00ull,  66689 },
		{ 0xfffffeffbfeff81dull,  62548 },
		{ 0xffbfffefefdff70full,  66597 },
		{ 0x100000101ec10082ull,  86749 },
		{ 0x7fbaffffefe0c02full,  69558 },
		{ 0x7f83fffffff07f7full,  61589 },
		{ 0xfff1fffffff7ffc1ull,  62533 },
		{ 0x0878040000ffe01full,  64387 },
		{ 0x005d00000120200aull,  26581 },
		{ 0x0840800080200fdaull,  76355 },
		{ 0x100000c05f582008ull,  11140 }
	};

	inline constexpr RawMagicEntry rookMagicsRaw[64] =
	{
		{ 0x80280013ff84ffffull,  10890 },
		{ 0x5ffbfefdfef67fffull,  56054 },
		{ 0xffeffaffeffdffffull,  67495 },
		{ 0x003000900300008aull,  72797 },
		{ 0x0030018003500030ull,  17179 },
		{ 0x0020012120a00020ull,  63978 },
		{ 0x0030006000c00030ull,  56650 },
		{ 0xffa8008dff09fff8ull,  15929 },
		{ 0x7fbff7fbfbeafffcull,  55905 },
		{ 0x0000140081050002ull,  26301 },
		{ 0x0000180043800048ull,  78100 },
		{ 0x7fffe800021fffb8ull,  86245 },
		{ 0xffffcffe7fcfffafull,  75228 },
		{ 0x00001800c0180060ull,  31661 },
		{ 0xffffe7ff8fbfffe8ull,  38053 },
		{ 0x0000180030620018ull,  37433 },
		{ 0x00300018010c0003ull,  74747 },
		{ 0x0003000c0085ffffull,  53847 },
		{ 0xfffdfff7fbfefff7ull,  70952 },
		{ 0x7fc1ffdffc001fffull,  49447 },
		{ 0xfffeffdffdffdfffull,  62629 },
		{ 0x7c108007befff81full,  58996 },
		{ 0x20408007bfe00810ull,  36009 },
		{ 0x0400800558604100ull,  21230 },
		{ 0x0040200010080008ull,  51882 },
		{ 0x0010020008040004ull,  11841 },
		{ 0xfffdfefff7fbfff7ull,  25794 },
		{ 0xfebf7dfff8fefff9ull,  49689 },
		{ 0xc00000ffe001ffe0ull,  63400 },
		{ 0x2008208007004007ull,  33958 },
		{ 0xbffbfafffb683f7full,  21991 },
		{ 0x0807f67ffa102040ull,  45618 },
		{ 0x200008e800300030ull,  70134 },
		{ 0x0000008780180018ull,  75944 },
		{ 0x0000010300180018ull,  68392 },
		{ 0x4000008180180018ull,  66472 },
		{ 0x008080310005fffaull,  23236 },
		{ 0x4000188100060006ull,  19067 },
		{ 0xffffff7fffbfbfffull,      0 },
		{ 0x0000802000200040ull,  43566 },
		{ 0x20000202ec002800ull,  29810 },
		{ 0xfffff9ff7cfff3ffull,  65558 },
		{ 0x000000404b801800ull,  77684 },
		{ 0x2000002fe03fd000ull,  73350 },
		{ 0xffffff6ffe7fcffdull,  61765 },
		{ 0xbff7efffbfc00fffull,  49282 },
		{ 0x000000100800a804ull,  78840 },
		{ 0xfffbffefa7ffa7feull,  82904 },
		{ 0x0000052800140028ull,  24594 },
		{ 0x00000085008a0014ull,   9513 },
		{ 0x8000002b00408028ull,  29012 },
		{ 0x4000002040790028ull,  27684 },
		{ 0x7800002010288028ull,  27901 },
		{ 0x0000001800e08018ull,  61477 },
		{ 0x1890000810580050ull,  25719 },
		{ 0x2003d80000500028ull,  50020 },
		{ 0xfffff37eefefdfbeull,  41547 },
		{ 0x40000280090013c1ull,   4750 },
		{ 0xbf7ffeffbffaf71full,   6014 },
		{ 0xfffdffff777b7d6eull,  41529 },
		{ 0xeeffffeff0080bfeull,  84192 },
		{ 0xafe0000fff780402ull,  33433 },
		{ 0xee73fffbffbb77feull,   8555 },
		{ 0x0002000308482882ull,   1009 }
	};
}  // namespace Chess
#endif // MAGICS_H
