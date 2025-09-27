#include <iostream>
#include <fstream>
#include <cstring>
#include <new>
#include <ctime>

using namespace std;
typedef unsigned long usize;

// ----------------- utilidades -----------------
static unsigned char rotl(unsigned char v, int n) {
    return (unsigned char)((v << n) | (v >> (8 - n)));
}
static unsigned char rotr(unsigned char v, int n) {
    return (unsigned char)((v >> n) | (v << (8 - n)));
}

// ----------------- desencriptado -----------------
static void desencriptar(const unsigned char* in, usize len, unsigned char* out, int rot, unsigned char key) {
    for (usize i = 0; i < len; i++) {
        unsigned char v = in[i];
        v ^= key;
        v = rotr(v, rot);
        out[i] = v;
    }
}

// ----------------- RLE ASCII -----------------
static bool rle_decompress_ascii(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen) {
    *outBuf = nullptr; *outLen = 0;
    usize cap = inLen * 8 + 1024;
    if (cap < 4096) cap = 4096;
    unsigned char* out = new (nothrow) unsigned char[cap];
    if (!out) return false;

    usize o = 0;
    for (usize i = 0; i < inLen;) {
        // leer número
        usize num = 0;
        while (i < inLen && in[i] >= '0' && in[i] <= '9') {
            num = num * 10 + (in[i] - '0');
            i++;
        }
        if (i >= inLen) break;
        unsigned char sym = in[i++];
        if (num == 0) { delete[] out; return false; }
        if (o + num > cap) {
            usize newCap = (o + num) * 2 + 1024;
            unsigned char* tmp = new (nothrow) unsigned char[newCap];
            if (!tmp) { delete[] out; return false; }
            for (usize k = 0; k < o; ++k) tmp[k] = out[k];
            delete[] out; out = tmp; cap = newCap;
        }
        for (usize k = 0; k < num; ++k) out[o++] = sym;
    }
    *outBuf = out; *outLen = o;
    return true;
}

// ----------------- RLE BIN LE (clásico) -----------------
static bool rle_decompress_bin_le(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen) {
    *outBuf = nullptr; *outLen = 0;
    if (inLen < 2) return false;
    if (inLen % 2 != 0) return false;

    usize cap = inLen * 16 + 1024;
    if (cap < 4096) cap = 4096;
    unsigned char* out = new (nothrow) unsigned char[cap];
    if (!out) return false;

    usize o = 0;
    for (usize i = 0; i < inLen; i += 2) {
        unsigned char len = in[i];
        unsigned char sym = in[i + 1];
        if (len == 0) { delete[] out; return false; }
        if (o + len > cap) {
            usize newCap = (o + len) * 2 + 1024;
            unsigned char* tmp = new (nothrow) unsigned char[newCap];
            if (!tmp) { delete[] out; return false; }
            for (usize k = 0; k < o; ++k) tmp[k] = out[k];
            delete[] out; out = tmp; cap = newCap;
        }
        for (unsigned int k = 0; k < len; ++k) out[o++] = sym;
    }
    *outBuf = out; *outLen = o;
    return true;
}

// ----------------- RLE BIN TRIPLET (dataset) -----------------
// formato: [basura][len][sym]
static bool rle_decompress_bin_triplet(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen) {
    *outBuf = nullptr; *outLen = 0;
    if (inLen < 3) return false;
    if (inLen % 3 != 0) return false;

    usize cap = inLen * 16 + 1024;
    if (cap < 4096) cap = 4096;
    unsigned char* out = new (nothrow) unsigned char[cap];
    if (!out) return false;

    usize o = 0;
    for (usize i = 0; i < inLen; i += 3) {
        unsigned char len = in[i + 1];
        unsigned char sym = in[i + 2];
        if (len == 0) { delete[] out; return false; }
        if (o + len > cap) {
            usize newCap = (o + len) * 2 + 1024;
            unsigned char* tmp = new (nothrow) unsigned char[newCap];
            if (!tmp) { delete[] out; return false; }
            for (usize k = 0; k < o; ++k) tmp[k] = out[k];
            delete[] out; out = tmp; cap = newCap;
        }
        for (unsigned int k = 0; k < len; ++k) out[o++] = sym;
    }
    *outBuf = out; *outLen = o;
    return true;
}
