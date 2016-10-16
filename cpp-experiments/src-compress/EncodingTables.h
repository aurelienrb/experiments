#pragma once

// Table used to encode a symbol name.
// 5 bits (32 values)
struct SYMBOL_NAME_CODES {
	static const int BIT_WIDTH = 5;
	enum ENUM {
		LETTER_A,
		LETTER_B,
		LETTER_C,
		LETTER_D,
		LETTER_E,
		LETTER_F,
		LETTER_G,
		LETTER_H,
		LETTER_I,
		LETTER_J,
		LETTER_K,
		LETTER_L,
		LETTER_M,
		LETTER_N,
		LETTER_O,
		LETTER_P,
		LETTER_Q,
		LETTER_R,
		LETTER_S,
		LETTER_T,
		LETTER_U,
		LETTER_V,
		LETTER_W,
		LETTER_X,
		LETTER_Y,
		LETTER_Z,
		UNDERSCORE,
		CASE_INVERSE_ONCE,
		CASE_INVERSE_PERMANENT,
		DIGITS_2BITS,  // [0..3]
		DIGITS_6BITS,  // [4..67]
		DIGITS_10BITS, // [68..1091]
	};
};

