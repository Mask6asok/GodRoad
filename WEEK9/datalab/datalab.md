一些简单的位运算，浮点数这一块还没搞懂，以后再补上

# bitXor

只允许使用 ~ & 来实现异或，按照逻辑运算可得

```c
/* 
 * bitXor - x^y using only ~ and & 
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y)
{
    int a = (~x) & y;
    int b = (~y) & x;
    return ~((~a) & (~b));
}
```

# tmin

返回负数，即为0x80000000，移位即可

```c
/* 
 * tmin - return minimum two's complement integer  0x80000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void)
{
    return 1 << 31;
}
```

# isTmax

判断是否为最大正数0x7FFFFFFF，利用低31位全为1的性质，乘2加1即可变为0，同时判断0xFFFFFFFF的特殊情况

```c
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,0x7fffffff
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
int isTmax(int x)
{
    return !~(x + 1 + x) & !!(x + 1);
}
```

# allOddBits

判断是否为0xAAAAAAAA，即偶数位全为1，此时与0x55555555或运算后，即可变成0xFFFFFFFF，取反即为0，其余数不满足

```c
/* 
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   where bits are numbered from 0 (least significant) to 31 (most significant)
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x)
{
    return !~(x | (0x55 + (0x55 << 8) + (0x55 << 16) + (0x55 << 24)));
}
```

# negate

取反加1

```c
/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x)
{
    return ~x + 1;
}
```

# isAsciiDigit

判断是否为0-9的ascii字符，合法字符为0b110000到0b111001，可以发现左移四位后为0b11，且第四位画卡诺图可得$!b_3|!b_2!b_1$

```c
/* 
 * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
 *   Example: isAsciiDigit(0x35) = 1.
 *            isAsciiDigit(0x3a) = 0.
 *            isAsciiDigit(0x05) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int isAsciiDigit(int x)
{
    return !((x >> 4) ^ 3) & ((!((x >> 3) & 1)) | (!((x >> 2) & 1)) & (!((x >> 1) & 1)));
}
```

# conditional

实现三目运算符，根据x的值选择y或者z，输出可以使用这样的式子 output = a&y | b&z，当x != 0时，a = 0xFFFFFFFF，b = 0；当x == 0时，a = 0，b = 0xFFFFFFFF，于是可得a = !x + ~1 + 1，b = ~!x + 1

```c
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z)
{
    int a = !x + ~1 + 1;
    int b = ~!x + 1;
    return a & y | b & z;
}
```

# isLessOrEqual

判断小于等于，首先根据符号位来看，当两者符号位不一致时，若x符号位为1且y符号位为0，则y肯定比x大，否则y肯定比x小；当两者符号位一致时，看y-x的符号位即可判断大小

```c
/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y)
{
    int xs = (x >> 31) & 1; // 符号位
    int ys = (y >> 31) & 1;
    int s = !(((y + ~x + 1) >> 31) & 1); // 符号相同，相减取符号位，1则y>x
    return (xs & !ys) | (s & !(xs ^ ys));
}
```

# logicalNeg

实现!功能，非0则0，为0则1，首先取符号位，若符号位为1，则直接判0，否则加上最大正数，看结果的符号位若为1，则说明该数不为0

```c
/* 
 * logicalNeg - implement the ! operator, using all of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x)
{
    int xs = (x >> 31) & 1;
    int TMAX = ~(1 << 31);
    int xp = x + TMAX;
    int xps = (xp >> 31) & 1;
    return (xs ^ 1) & (xps ^ 1);
}
```

# howManyBits

统计数字的二进制补码表示一共有多少位，二分依次计数

```c
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
int howManyBits(int x)
{
    x = ((x >> 31) & ~x | (~!(x >> 31) + 1) & x);
    int y = x >> 16;
    int h16 = !!y << 4;
    x = x >> h16;
    y = x >> 8;
    int h8 = !!y << 3;
    x = x >> h8;
    y = x >> 4;
    int h4 = !!y << 2;
    x = x >> h4;
    y = x >> 2;
    int h2 = !!y << 1;
    x = x >> h2;
    y = x >> 1;
    int h1 = !!y << 0;
    x = x >> h1;
    return h16 + h8 + h4 + h2 + h1 + 1 + x;
}
```

# TODO