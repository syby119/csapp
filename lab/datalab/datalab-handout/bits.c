/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31.


EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implement floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants. You can use any arithmetic,
logical, or comparison operations on int or unsigned data.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
//1
/* 
 * bitXor - x^y using only ~ and & 
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y) {
  return (~(~x & ~y)) & (~(x & y));                                    // 7
}

/* 
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
  return 1 << 31;                                                      // 1
}

//2
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
int isTmax(int x) {
  int _x = ~x;                                                         // 1

  int cond1 = !!_x;                                                    // 2
  int cond2 = !(_x ^ (x + 1));                                         // 3

  return cond1 & cond2;                                                // 1
}

/* 
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   where bits are numbered from 0 (least significant) to 31 (most significant)
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x) {
  int t = (0xAA << 8) + 0xAA;                                          // 2
  int c = (t << 16) + t;                                               // 2

  return !~((x & c) + ~c);                                             // 5
}

/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
  return 1 + ~x;                                                       // 2
}

//3
/* 
 * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
 *   Example: isAsciiDigit(0x35) = 1.
 *            isAsciiDigit(0x3a) = 0.
 *            isAsciiDigit(0x05) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int isAsciiDigit(int x) {
  int cond0 = !(x & ~0x3F);                                            // 3
  int cond1 = (x + 0x10) >> 6;                                         // 2
  int cond2 = !(x & 0x8);                                              // 2
  int cond3 = (!(x & 0x4)) & (!(x & 0x2));                             // 5

  return cond0 & cond1 & (cond2 | cond3);                              // 3
}

/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
  int t = (!x) + ~0x0;                                                 // 3
  return (y & t) | (z & ~t);                                           // 4
}

/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y) {
  int xNotNegative = 1 + (x >> 31);                                    // 2
  int yNotNegative = 1 + (y >> 31);                                    // 2

  int case1 = xNotNegative & !yNotNegative;                            // 2
  int case2 = yNotNegative & !xNotNegative;                            // 2

  int t = ~(x + ((~y) + 1)) + 1;                                       // 5
  int tNotNegative = 1 + (t >> 31);                                    // 2

  return (!case1) & (case2 | ((!case2) & tNotNegative));               // 5
}

//4
/* 
 * logicalNeg - implement the ! operator, using all of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
  return 1 + ~(~(x | (1 + ~x)) >> 31);                                 // 7
}

/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
  int cond;
  int t;

  int count = 0;

  // if (x < 0) {
  //     x = ~x;
  // }
  cond = 1 + ~(x >> 31);                                               // 3
  t = (!cond) + ~0x0;                                                  // 3
  x = ((t & ~x) | (x & ~t));                                           // 5

  // if ((x >> 16) > 0) {
  //     count += 16;
  //     x = x >> 16;
  // }
  cond = x >> 16;                                                      // 1
  t = (!cond) + ~0x0;                                                  // 3
  count = count + (16 & t);                                            // 2
  x = ((cond & t) | (x & ~t));                                         // 4

  // if ((x >> 8) > 0) {
  //     count += 8;
  //     x = x >> 8;
  // }
  cond = x >> 8;                                                       // 1
  t = (!cond) + ~0x0;                                                  // 3
  count = count + (8 & t);                                             // 2
  x = ((cond & t) | (x & ~t));                                         // 4

  // if ((x >> 4) > 0) {
  //     count += 4;
  //     x = x >> 4;
  // }
  cond = x >> 4;                                                       // 1
  t = (!cond) + ~0x0;                                                  // 3
  count = count + (4 & t);                                             // 2
  x = ((cond & t) | (x & ~t));                                         // 4

  // if ((x >> 2) > 0) {
  //     count += 2;
  //     x = x >> 2;
  // }
  cond = x >> 2;                                                       // 1
  t = (!cond) + ~0x0;                                                  // 3
  count = count + (2 & t);                                             // 2
  x = ((cond & t) | (x & ~t));                                         // 4

  // if ((x >> 1) > 0) {
  //     count += 1;
  //     x = x >> 1;
  // }
  cond = x >> 1;                                                       // 1
  t = (!cond) + ~0x0;                                                  // 3
  count = count + (1 & t);                                             // 2
  x = ((cond & t) | (x & ~t));                                         // 4

  return count + x + 1;                                                // 2
}

//float
/* 
 * floatScale2 - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale2(unsigned uf) {
  unsigned mask1 = 1 << 31;                                            // 1
  unsigned mask2 = 0xFF << 23;                                         // 1

  unsigned s = uf & mask1;                                             // 2
  unsigned e = uf & mask2;                                             // 2
  unsigned m = (uf << 9) >> 9;                                         // 2

  // case 1: special value
  if (e == mask2) {                                                    // 2
    return uf;
  }

  // case 2: denormalized value
  if (e) {                                                             // 1
    e = e + (1 << 23);                                                 // 2
    if (e == mask2) {                                                  // 1
        m = 0;
    }

    return s | e | m;                                                  // 2
  }

  // case 3: normalized value
  return s | e | (m << 1);                                             // 3
}

/* 
 * floatFloat2Int - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int floatFloat2Int(unsigned uf) {
  unsigned mask1 = 1 << 31;                                            // 1
  unsigned mask2 = 0xFF << 23;                                         // 1

  unsigned s = uf & mask1;                                             // 2
  unsigned e = uf & mask2;                                             // 2
  unsigned m = (uf << 9) >> 9;                                         // 2

  int E = (e >> 23) - 0x7F;                                            // 2

  unsigned t;
  
  // special value
  if (e == mask2) {                                                    // 1
      return mask1;
  }

  // normalized value + samll denormalized value
  if (E < 0) {                                                         // 1
    return 0;
  } 
  
  // big denormalized value
  if (E > 30) {                                                        // 1
    return mask1;
  }

  // median denormalized value
  t = (1 << 23) | m;                                                   // 2

  if (E < 23) {                                                        // 1
    t = t >> (23 - E);                                                 // 2
  } else {
    t = t << (E - 23);                                                 // 2
  }

  if (s) {                                                             // 1
    return -t;                                                         // 1
  } else {
    return t;
  }
}

/* 
 * floatPower2 - Return bit-level equivalent of the expression 2.0^x
 *   (2.0 raised to the power x) for any 32-bit integer x.
 *
 *   The unsigned value that is returned should have the identical bit
 *   representation as the single-precision floating-point number 2.0^x.
 *   If the result is too small to be represented as a denorm, return
 *   0. If too large, return +INF.
 * 
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. Also if, while 
 *   Max ops: 30 
 *   Rating: 4
 */
unsigned floatPower2(int x) {
  // too big
  if (x > 127) {
    return 0xFF << 23;
  }

  // too small to be represented by denorm
  if (x < -126) {
      return 0;
  }

  return (x + 127) << 23;
}
