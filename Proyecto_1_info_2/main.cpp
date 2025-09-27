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

// ----------------- LZ78 LE -----------------
struct LZDictEntry { unsigned short prefix; unsigned char c; };

static bool lz78_decompress_le(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen) {
    *outBuf = nullptr; *outLen = 0;
    if (inLen < 3) return false;
    if (inLen % 3 != 0) return false;

    usize cap = inLen * 32 + 1024;
    if (cap < 8192) cap = 8192;
    unsigned char* out = new (nothrow) unsigned char[cap];
    if (!out) return false;

    usize o = 0;
    usize dictCap = inLen / 3 + 4;
    LZDictEntry* dict = new (nothrow) LZDictEntry[dictCap];
    if (!dict) { delete[] out; return false; }
    usize dictSize = 1; // índice 0 reservado

    for (usize i = 0; i < inLen; i += 3) {
        unsigned short prefix = (unsigned short)(in[i] | (in[i + 1] << 8));
        unsigned char c = in[i + 2];

        // reconstruir cadena del prefijo
        unsigned char tmp[65536];
        usize tlen = 0;
        unsigned short p = prefix;
        while (p != 0 && p < dictSize && tlen < sizeof(tmp) - 1) {
            tmp[tlen++] = dict[p].c;
            p = dict[p].prefix;
        }
        // escribir cadena prefijo invertida
        for (usize j = 0; j < tlen; j++) {
            if (o >= cap) {
                usize newCap = cap * 2 + 1024;
                unsigned char* tmp2 = new (nothrow) unsigned char[newCap];
                if (!tmp2) { delete[] out; delete[] dict; return false; }
                for (usize k = 0; k < o; k++) tmp2[k] = out[k];
                delete[] out; out = tmp2; cap = newCap;
            }
            out[o++] = tmp[tlen - 1 - j];
        }
        // añadir el caracter final
        if (o >= cap) {
            usize newCap = cap * 2 + 1024;
            unsigned char* tmp2 = new (nothrow) unsigned char[newCap];
            if (!tmp2) { delete[] out; delete[] dict; return false; }
            for (usize k = 0; k < o; k++) tmp2[k] = out[k];
            delete[] out; out = tmp2; cap = newCap;
        }
        out[o++] = c;

        if (dictSize < dictCap) {
            dict[dictSize].prefix = prefix;
            dict[dictSize].c = c;
            dictSize++;
        }
    }
    delete[] dict;
    *outBuf = out; *outLen = o;
    return true;
}

// ----------------- try_all_decompress -----------------
static bool try_all_decompress(const unsigned char* in, usize inLen,
                               unsigned char** out, usize* outLen, int* which) {
    *which = 0; *out = nullptr; *outLen = 0;
    unsigned char* buf = nullptr; usize blen = 0;

    if (rle_decompress_bin_triplet(in, inLen, &buf, &blen)) {
        *out = buf; *outLen = blen; *which = 4; return true;
    }
    if (rle_decompress_bin_le(in, inLen, &buf, &blen)) {
        *out = buf; *outLen = blen; *which = 2; return true;
    }
    if (lz78_decompress_le(in, inLen, &buf, &blen)) {
        *out = buf; *outLen = blen; *which = 3; return true;
    }
    if (rle_decompress_ascii(in, inLen, &buf, &blen)) {
        *out = buf; *outLen = blen; *which = 1; return true;
    }

    return false;
}

static const char* methodNameReport(int which) {
    if (which == 1) return "RLE_ASCII";
    if (which == 2) return "RLE_BIN_LE";
    if (which == 3) return "LZ78_LE";
    if (which == 4) return "RLE_BIN_TRIPLET (dataset)";
    return "UNKNOWN";
}

// ----------------- main -----------------
int main() {
    int n;
    cout << "Cantidad maxima de casos a evaluar (n): ";
    cin >> n;

    for (int caso = 1; caso <= n; caso++) {
        char fname[64], fpista[64];
        sprintf(fname, "Encriptado%d.txt", caso);
        sprintf(fpista, "pista%d.txt", caso);

        ifstream fin(fname, ios::binary);
        if (!fin) { cout << "- Saltando " << fname << endl; continue; }
        fin.seekg(0, ios::end); usize len = fin.tellg(); fin.seekg(0);
        unsigned char* encBuf = new unsigned char[len];
        fin.read((char*)encBuf, len); fin.close();

        ifstream fp(fpista, ios::binary);
        if (!fp) { cout << "- No existe " << fpista << endl; delete[] encBuf; continue; }
        fp.seekg(0, ios::end); usize plen = fp.tellg(); fp.seekg(0);
        unsigned char* pistaBuf = new unsigned char[plen + 1];
        fp.read((char*)pistaBuf, plen); pistaBuf[plen] = 0; fp.close();

        cout << "\n=== Procesando caso " << caso << " (" << len << " bytes) ===" << endl;

        bool found = false;
        for (int rot = 1; rot <= 7 && !found; rot++) {
            for (int K = 0; K < 256 && !found; K++) {
                unsigned char* decBuf = new unsigned char[len];
                desencriptar(encBuf, len, decBuf, rot, (unsigned char)K);

                unsigned char* plain = nullptr; usize plainLen = 0; int which = 0;
                if (try_all_decompress(decBuf, len, &plain, &plainLen, &which)) {
                    // buscar pista
                    bool match = false;
                    for (usize i = 0; i + plen <= plainLen; i++) {
                        usize j = 0;
                        while (j < plen && plain[i + j] == pistaBuf[j]) j++;
                        if (j == plen) { match = true; break; }
                    }
                    if (match) {
                        cout << "\n** " << fname << " **" << endl;
                        cout << "Compresion: " << methodNameReport(which) << endl;
                        cout << "Rotacion (n): " << rot << endl;
                        cout << "Key= 0x" << hex << K << dec << endl;
                        cout << "Mensaje desencriptado:\n";
                        for (usize i = 0; i < plainLen; i++) cout << (char)plain[i];
                        cout << "\n";
                        found = true;
                        delete[] plain;
                    } else {
                        delete[] plain;
                    }
                }
                delete[] decBuf;
            }
        }
        if (!found) {
            cout << "No se encontro combinacion valida en " << fname << endl;
        }
        delete[] encBuf;
        delete[] pistaBuf;
    }
    return 0;
}

