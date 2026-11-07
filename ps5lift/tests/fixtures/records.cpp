struct IntPair {
    int a;
    int b;
};

struct LongPair {
    long a;
    long b;
};

struct FloatTriple {
    float x;
    float y;
    float z;
};

struct FloatInt {
    float f;
    int i;
};

struct DoubleLong {
    double d;
    long l;
};

struct Chars {
    char c[3];
};

struct Shorts {
    short s[3];
};

struct NestedOdd {
    int a;
    struct {
        float x;
        float y;
    } pair;
};

struct WithUnion {
    union {
        double d;
        long l;
    } u;
    float f;
};

struct Floats {
    float v[4];
};

struct FloatsDouble {
    float a;
    float b;
    double c;
};

union IntOrFloat {
    int i;
    float f;
};

struct Wide {
    __int128 v;
};

struct Large {
    long a;
    long b;
    long c;
};

struct LargeFloats {
    double v[3];
};

struct [[gnu::packed]] Packed {
    char c;
    int i;
};

struct Bits {
    int a : 3;
    int b : 5;
};

struct HoldsFunction {
    void (*f)(void);
};

struct PaddedEightbyte {
    alignas(16) long a;
};

#define RECORD(type)                        \
    extern "C" void take_##type(type value); \
    extern "C" type give_##type(void);       \
    extern "C" void use_##type(type* value) { \
        take_##type(*value);                  \
        *value = give_##type();               \
    }

RECORD(IntPair)
RECORD(LongPair)
RECORD(FloatTriple)
RECORD(FloatInt)
RECORD(DoubleLong)
RECORD(Chars)
RECORD(Shorts)
RECORD(NestedOdd)
RECORD(WithUnion)
RECORD(Floats)
RECORD(FloatsDouble)
RECORD(IntOrFloat)
RECORD(Wide)
RECORD(Large)
RECORD(LargeFloats)
RECORD(Packed)
RECORD(Bits)
RECORD(HoldsFunction)
RECORD(PaddedEightbyte)
