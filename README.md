# Xpatch specification (draft)

Author: Rodrigo Arias Mallo <rodarima@gmail.com>

**NOTE: This specification is a draft and it is still being revised.**

The xpatch format defines a set of operations to apply to a file or directory to
transform it in another. The only operations are deletion and addition of
information.

Following the patch(1) format to change lines of text, the xpatch format extends
this format to also allow changes of binary content.

It includes a notation to describe changes in a way that it can be easily read
by humans.

## General format

A xpatch is composed by a list of changes. Each has a header and a list of
hunks. Here is a simple example:

```diff
--- a.bin
+++ b.bin
@@ u8,u32 -0x1897d8,1 +0x1897d8,1 @@
- 0x1b7358 # 1799000
+ 0x1d4c00 # 1920000
@@ u8,u32 -0x189ca8,1 +0x189ca8,1 @@
- 0x1b7358 # 1799000
+ 0x1d4c00 # 1920000
```

The header:

    --- a.bin
    +++ b.bin

Defines the source file the patch is applied to, and which is the resulting
file. Each hunk is prefixed with a control line:

    @@ u8,u32 -0x1897d8,1 +0x1897d8,1 @@

Which is composed of three elements separated by white spaces: the type
declaration, the deletion operation and the addition operation.

The optional type declaration:

    u8,u32

Has two components separated by a comma. The first one identifies the unit of
the addressing value. In this example, it will use a byte or 8 bits. The next
value defines the type of the hunk differences. In this examples, the
differences will be described as unsigned integers of 32 bits in little endian
(the default).

If the type declaration is missing, the hunk is interpreted as a normal patch,
and the addresses refer to lines.

The deletion operation is prefixed with the '-' symbol:

    -0x1897d8,1

It is composed of the starting address, which increases by the width specified
in the type declaration. The next value specifies the number of elements that
will be removed. In this example a single u32 integer. The deletion operation
must match the data stored in the file for the hunk to success, otherwise it is
rejected and the patch will fail to apply.

The addition operation adds in the given address, the number of elements of the
same type specified:

    +0x1897d8,1

In the example, only one element of unsigned integer of 32 bits is added.

When the number of elements of the deletion and addition sizes are equal, the
hunk doesn't alter the file size. If all hunks keep the property, the file will
keep the same size.

The hunk control line is finished by the suffix @@.

After the control line, the deletion data begins:

    - 0x1b7358 # 1799000

The deletion lines must begin with the character '-' followed by space. It must
be followed by the same amount of elements specified in the hunk control line
for the deletion operation, in this case only one integer is removed.

The integer is specified in hexadecimal notation by using the 0x prefix. After
the number, a comment may be added by prefixing it with #.

Multiple constants may appear in a single deletion line if separated by
white spaces (by default).

After the deletion data comes the addition data, which follows the same syntax,
but begins with the symbols + and space:

    + 0x1d4c00 # 1920000

## Data format

The numbers are by default interpreted as little endian values. The following
types are defined:

- Signed integers: i8, i16, i24, i32, i64
- Unsigned integers: u8, u16, u24, u32, u64
- Floating point: f32, f64

The following bases are recognized for integers:

- Decimal:      1234 (must not start by 0).
- Octal:        0123 (must start by 0)
- Hexadecimal:  0x1234
- Binary:       0b1011

Signed integers and floating point number are allowed to use negative values by
using the - symbol as a prefix, without any white space allowed. Here is an
example of negative values:

    @@ u8,u32 -0x1897d8,1 +0x1897d8,1 @@
    - -1799000
    + -1920000

Constants can use the symbol _ to aid humans to group numbers in arbitrary
positions. These are typically used to group decimal places each three digits or
hexadecimal each four:

    1_000_000
    0x4000_1200

The symbol _ can only be surrounded by digits. The values `_1`, `1_` or `0x_1`
are all invalid.

Floating point numbers can use decimals with a dot:

    1.234
    0.0 and 0. is the same as just 0

## Extended format

By default, constants must be separated by white spaces. But a custom format may
be specified in the hunk control line to modify the default parsing format by
following the printf notation. For example:

```diff
@@ u8,u8,%2x -0x189ca8,4 +0x189ca8,4 @@
- 0146     # mov r1, r0
- 6846     # mov r0, sp
+ 4ff2bafc # bl 0x254526
```

Specifies that unsigned integers of 8 bits are given as pairs of hexadecimal
digits. Notice that there are no spaces among the constants. The value:

    - 0146

Is parsed as 0x01 and 0x46 unsigned integers of 8 bits in little endian.

On the other hand, the following hunk is invalid:

```diff
@@ u8,u8,%x -0x189ca8,4 +0x189ca8,4 @@
- 0146     # mov r1, r0
- 6846     # mov r0, sp
+ 4ff2bafc # bl 0x254526
```

As the %x doesn't define a length, so the constants are separated by white
space, but the values don't fit inside a unsigned integer of 8 bits.

The extended format specifier is constructed as follows:

- Mandatory percent sign (%) 
- Number of digits (optional)
- Base

The available bases are d=decimal, x=hexadecimal, o=octal and b=binary.

[We may want to include base64 and others here]

When the number of digits is not specified, the format is applied to all digits
until a white space is found.

Examples:

- `%2x`: Read two hexadecimal digits
- `%3o`: Three octal digits
- `%4d`: Decimal numbers of 4 digits
- `%1b`: One binary digit.
- `%8b`: Eight binary digits.

The binary format is useful to write sequences of just bits interpreted as
little endian numbers of the defined width:

```diff
@@ u8,u8,%b -0x8004,2 +0x8004,2 @@
- 0011_1001 0010_1010
+ 1111_0011 0111_0011
```

Changes two bytes at address 0x8004. The original bytes are 0b00111001 and
0b00101010. Notice how avoiding the 0b prefix and using _ makes it more
readable.
