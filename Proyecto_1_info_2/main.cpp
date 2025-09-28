#include <iostream>
#include <fstream>
#include <cstring> //usamos cstring ya que incluimos memcpy, con el fin de evitar errores en algunos compiladores


typedef unsigned long long usize;

static unsigned char* readFile(const char* path, usize& outLen){
    outLen = 0;
    std::ifstream f(path, std::ios::binary);
    if(!f.good()) return nullptr;
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if(sz <= 0){ f.close(); return nullptr; }
    f.seekg(0, std::ios::beg);
    unsigned char* buf = new (std::nothrow) unsigned char[(usize)sz];
    if(!buf){ f.close(); return nullptr; }
    f.read(reinterpret_cast<char*>(buf), sz);
    if(!f){ delete[] buf; f.close(); return nullptr; }
    f.close();
    outLen = (usize)sz;
    return buf;
}

static bool writeFile(const char* path, const unsigned char* buf, usize len){
    std::ofstream f(path, std::ios::binary);
    if(!f.good()) return false;
    f.write(reinterpret_cast<const char*>(buf), (std::streamsize)len);
    bool ok = (bool)f;
    f.close();
    return ok;
}

// bit rotations and xor
static inline unsigned char ROL8(unsigned char b, int n){ n &= 7; if(!n) return b; return (unsigned char)(((b << n) | (b >> (8 - n))) & 0xFF); }
static inline unsigned char ROR8(unsigned char b, int n){ n &= 7; if(!n) return b; return (unsigned char)(((b >> n) | (b << (8 - n))) & 0xFF); }
static inline unsigned char XOR8(unsigned char b, unsigned char K){ return (unsigned char)(b ^ K); }

static void applyXorThenRotate(const unsigned char* in, usize len, unsigned char K, int n, bool useROL, unsigned char* out){
    for(usize i=0;i<len;++i){
        unsigned char v = XOR8(in[i], K);
        out[i] = useROL ? ROL8(v,n) : ROR8(v,n);
    }
}

// ---------------- RLE implementations ----------------
static bool rle_decompress_ascii(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen){
    *outBuf = nullptr; *outLen = 0;
    if(inLen == 0) return false;
    usize cap = inLen * 8 + 1024; if(cap < 4096) cap = 4096;
    unsigned char* out = new (std::nothrow) unsigned char[cap]; if(!out) return false;
    usize o = 0, i = 0;
    while(i < inLen){
        usize num = 0, digits=0;
        while(i < inLen && in[i] >= '0' && in[i] <= '9'){ num = num*10 + (in[i]-'0'); ++i; ++digits; if(digits>12){ delete[] out; return false; } }
        if(i >= inLen){ delete[] out; return false; }
        unsigned char sym = in[i++];
        if(num == 0){ delete[] out; return false; }
        if(o + num > cap){
            usize newCap = (o + num)*2 + 1024;
            unsigned char* tmp = new (std::nothrow) unsigned char[newCap]; if(!tmp){ delete[] out; return false; }
            memcpy(tmp, out, o); delete[] out; out = tmp; cap = newCap;
        }
        for(usize k=0;k<num;++k) out[o++] = sym;
    }
    *outBuf = out; *outLen = o; return true;
}

static bool rle_decompress_bin_le(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen){
    *outBuf=nullptr; *outLen=0;
    if(inLen < 2 || (inLen % 2)!=0) return false;
    usize cap = inLen * 16 + 1024; if(cap < 4096) cap = 4096;
    unsigned char* out = new (std::nothrow) unsigned char[cap]; if(!out) return false;
    usize o=0;
    for(usize i=0;i<inLen;i+=2){
        unsigned char len = in[i]; unsigned char sym = in[i+1];
        if(len==0){ delete[] out; return false; }
        if(o + len > cap){
            usize newCap = (o + len)*2 + 1024;
            unsigned char* tmp = new (std::nothrow) unsigned char[newCap]; if(!tmp){ delete[] out; return false; }
            memcpy(tmp, out, o); delete[] out; out = tmp; cap = newCap;
        }
        for(unsigned int k=0;k<len;++k) out[o++] = sym;
    }
    *outBuf = out; *outLen = o; return true;
}

static bool rle_decompress_bin_triplet(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen){
    *outBuf=nullptr; *outLen=0;
    if(inLen < 3 || (inLen % 3)!=0) return false;
    usize cap = inLen * 16 + 1024; if(cap < 4096) cap = 4096;
    unsigned char* out = new (std::nothrow) unsigned char[cap]; if(!out) return false;
    usize o=0;
    for(usize i=0;i<inLen;i+=3){
        unsigned char len = in[i+1]; unsigned char sym = in[i+2];
        if(len==0){ delete[] out; return false; }
        if(o + len > cap){
            usize newCap = (o + len)*2 + 1024;
            unsigned char* tmp = new (std::nothrow) unsigned char[newCap]; if(!tmp){ delete[] out; return false; }
            memcpy(tmp, out, o); delete[] out; out = tmp; cap = newCap;
        }
        for(unsigned int k=0;k<len;++k) out[o++] = sym;
    }
    *outBuf = out; *outLen = o; return true;
}

// ---------------- LZ78: attempt both LE and BE ----------------
static bool lz78_decompress_triplet_endian(const unsigned char* in, usize inLen, bool littleEndian,
                                           unsigned char** outBuf, usize* outLen){
    *outBuf = nullptr; *outLen = 0;
    if(inLen < 3 || (inLen % 3) != 0) return false;
    const usize MAX_ENTRIES = 65535ULL;
    usize tripCount = inLen / 3;
    usize dictCap = tripCount + 4;
    if(dictCap > MAX_ENTRIES) dictCap = (size_t)MAX_ENTRIES;

    unsigned short* parent = new (std::nothrow) unsigned short[dictCap + 1];
    unsigned char* ch = new (std::nothrow) unsigned char[dictCap + 1];
    if(!parent || !ch){ if(parent) delete[] parent; if(ch) delete[] ch; return false; }

    usize cap = inLen * 32 + 1024; if(cap < 8192) cap = 8192;
    unsigned char* out = new (std::nothrow) unsigned char[cap];
    if(!out){ delete[] parent; delete[] ch; return false; }

    unsigned char* stack = new (std::nothrow) unsigned char[65537];
    if(!stack){ delete[] parent; delete[] ch; delete[] out; return false; }

    usize o = 0;
    unsigned short nextIdx = 1;
    for(usize i=0;i+2<inLen;i+=3){
        unsigned short idx;
        if(littleEndian) idx = (unsigned short)(in[i] | (in[i+1] << 8));
        else idx = (unsigned short)((in[i] << 8) | in[i+1]);
        unsigned char c = in[i+2];

        // idx must be < nextIdx (0 allowed)
        if(idx >= nextIdx && nextIdx != 0){
            delete[] parent; delete[] ch; delete[] out; delete[] stack;
            return false;
        }

        // reconstruct prefix
        usize sp=0; unsigned short t=idx;
        while(t != 0){
            if(t >= nextIdx){ delete[] parent; delete[] ch; delete[] out; delete[] stack; return false; }
            stack[sp++] = ch[t];
            t = parent[t];
            if(sp > 65536){ delete[] parent; delete[] ch; delete[] out; delete[] stack; return false; }
        }

        if(o + sp + 1 > cap){
            usize newCap = (o + sp + 1) * 2 + 1024;
            unsigned char* tmp = new (std::nothrow) unsigned char[newCap];
            if(!tmp){ delete[] parent; delete[] ch; delete[] out; delete[] stack; return false; }
            memcpy(tmp, out, o);
            delete[] out; out = tmp; cap = newCap;
        }

        for(usize k=0;k<sp;++k) out[o++] = stack[sp - 1 - k];
        out[o++] = c;

        if(nextIdx <= dictCap){
            parent[nextIdx] = idx;
            ch[nextIdx] = c;
            ++nextIdx;
        } else {
            delete[] parent; delete[] ch; delete[] out; delete[] stack;
            return false;
        }
    }

    delete[] parent; delete[] ch; delete[] stack;
    *outBuf = out; *outLen = o; return true;
}

// ---------------- try_all_decompress (RLE first, then LZ78 LE, then BE) ----------------
static bool try_all_decompress(const unsigned char* in, usize inLen, unsigned char** out, usize* outLen, int* which){
    *which = 0; *out = nullptr; *outLen = 0;
    unsigned char* buf = nullptr; usize blen = 0;

    // RLE triplet first
    if(rle_decompress_bin_triplet(in,inLen,&buf,&blen)){ *out=buf; *outLen=blen; *which=4; return true; }
    // RLE bin classic
    if(rle_decompress_bin_le(in,inLen,&buf,&blen)){ *out=buf; *outLen=blen; *which=2; return true; }
    // RLE ASCII
    if(rle_decompress_ascii(in,inLen,&buf,&blen)){ *out=buf; *outLen=blen; *which=1; return true; }
    // LZ78 LE
    if(lz78_decompress_triplet_endian(in,inLen,true,&buf,&blen)){ *out=buf; *outLen=blen; *which=3; return true; }
    // LZ78 BE
    if(lz78_decompress_triplet_endian(in,inLen,false,&buf,&blen)){ *out=buf; *outLen=blen; *which=5; return true; }

    return false;
}

// ---------------- helpers for case-insensitive search ----------------
static inline unsigned char tolower_ascii_uc(unsigned char c){
    if(c >= 'A' && c <= 'Z') return (unsigned char)(c + 32);
    return c;
}
static long find_substr_ci(const unsigned char* hay, usize hayLen, const char* needleLower, usize needleLen){
    if(needleLen == 0 || needleLen > hayLen) return -1;
    for(usize i=0;i + needleLen <= hayLen; ++i){
        usize j=0;
        while(j<needleLen && tolower_ascii_uc(hay[i+j]) == (unsigned char)needleLower[j]) ++j;
        if(j==needleLen) return (long)i;
    }
    return -1;
}

// ---------------- read and normalize pista ----------------
static char* read_and_normalize_pista(const char* path, usize& outLen){
    usize plen=0;
    unsigned char* raw = readFile(path, plen);
    if(!raw) return nullptr;
    char* clue = new (std::nothrow) char[plen + 1];
    if(!clue){ delete[] raw; return nullptr; }
    usize j=0;
    for(usize i=0;i<plen;++i){
        unsigned char c = raw[i];
        if(c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        unsigned char lc = tolower_ascii_uc(c);
        clue[j++] = (char)lc;
    }
    clue[j] = '\0';
    delete[] raw;
    outLen = j;
    return clue;
}

// ---------------- recover brute-force ----------------
static bool recover(const unsigned char* cipher, usize clen, const char* clueLower, usize clueLen,
                    unsigned char** plainOut, usize* plainLen, int* out_n, int* out_K, int* out_method, bool debug=false){
    unsigned char* tmp = new (std::nothrow) unsigned char[clen];
    if(!tmp) return false;

    for(int n=1;n<=7;++n){
        for(int K=0; K<=255; ++K){
            // decrypt attempt: XOR then ROR (inverse of ROL then XOR)
            applyXorThenRotate(cipher, clen, (unsigned char)K, n, /*useROL=*/false, tmp);
            unsigned char* dec=nullptr; usize dlen=0; int which=0;
            if(try_all_decompress(tmp, clen, &dec, &dlen, &which)){
                if(find_substr_ci(dec, dlen, clueLower, clueLen) >= 0){
                    *plainOut = dec; *plainLen = dlen; *out_n = n; *out_K = K; *out_method = which;
                    delete[] tmp; return true;
                }
                delete[] dec;
            }
            // also try XOR then ROL (in case encrypted with ROR first)
            applyXorThenRotate(cipher, clen, (unsigned char)K, n, /*useROL=*/true, tmp);
            dec=nullptr; dlen=0; which=0;
            if(try_all_decompress(tmp, clen, &dec, &dlen, &which)){
                if(find_substr_ci(dec, dlen, clueLower, clueLen) >= 0){
                    *plainOut = dec; *plainLen = dlen; *out_n = n; *out_K = K; *out_method = which;
                    delete[] tmp; return true;
                }
                delete[] dec;
            }
        }
    }

    delete[] tmp;
    return false;
}

int main(){
    unsigned int n=0;
    std::cout << "Cantidad maxima de casos a evaluar (n): ";
    if(!(std::cin >> n)) return 1;
    std::cin.ignore(1000,'\n');

    const usize SHOW_LIMIT = 1200;

    for(unsigned int X=1; X<=n; ++X){
        char encName[64], pistaName[64], outName[64];
        std::snprintf(encName, sizeof(encName), "Encriptado%u.txt", X);
        std::snprintf(pistaName, sizeof(pistaName), "pista%u.txt", X);
        std::snprintf(outName, sizeof(outName), "salida%u.txt", X);

        usize clen=0;
        unsigned char* cipher = readFile(encName, clen);
        if(!cipher){ std::cerr << "- Saltando caso " << X << " (no existe " << encName << "...)\n"; continue; }

        usize clueLen=0;
        char* clueLower = read_and_normalize_pista(pistaName, clueLen);
        if(!clueLower){ std::cerr << "- Saltando caso " << X << " (no existe" << pistaName << ")\n"; delete[] cipher; continue; }
        if(clueLen == 0){ std::cerr << "- Pista vacia en " << pistaName << " (saltando)\n"; delete[] cipher; delete[] clueLower; continue; }

        std::cout << "\n================================================= Procesando caso " << X << " (" << (unsigned long long)clen << " bytes) ========================================================\n";

        unsigned char* plain = nullptr; usize plainLen = 0; int nrot=0, K=0, method=0;
        bool ok = recover(cipher, clen, clueLower, clueLen, &plain, &plainLen, &nrot, &K, &method, /*debug=*/true);
        delete[] cipher;

        if(!ok){
            std::cout << "No se encontro combinacion valida en " << encName << "\n";
            delete[] clueLower;
            continue;
        }

        const char* methodName = "UNKNOWN";
        if(method == 1) methodName = "RLE_ASCII";
        else if(method == 2) methodName = "RLE_BIN_LE";
        else if(method == 3) methodName = "LZ78_TRIPLET_LE";
        else if(method == 4) methodName = "RLE_BIN_TRIPLET (dataset)";
        else if(method == 5) methodName = "LZ78_TRIPLET_BE";

        std::cout << "\n** " << encName << " **\n";
        std::cout << "Compresion: " << methodName << "\n";
        std::cout << "\nRotacion (n): " << nrot << "\n";
        const char* H = "0123456789ABCDEF";
        std::cout << "\nKey= 0x" << H[(K>>4)&0xF] << H[K&0xF] << "  (dec=" << K << ")\n";

        if(writeFile(outName, plain, plainLen)) std::cout << "\nEscrito: " << outName << " (" << (unsigned long long)plainLen << " bytes)\n";
        else std::cerr << "No pude escribir " << outName << "\n";

        std::cout << "\n---------------------------------------------------- Mensaje desencriptado (primeros " << SHOW_LIMIT << " chars) ----------------------------------------\n\n";
        usize shown=0;
        for(usize i=0;i<plainLen && shown < SHOW_LIMIT; ++i){
            unsigned char c = plain[i];
            if(c >= 32 && c <= 126){ std::cout << (char)c; ++shown; }
            else { std::cout << '.'; ++shown; }
        }
        if(plainLen > SHOW_LIMIT) std::cout << "\n... (mensaje truncado, total " << (unsigned long long)plainLen << " bytes)\n";
        else std::cout << "\n\n------------------------------------------------------ Fin del mensaje ------------------------------------------------------------------\n";

        delete[] plain;
        delete[] clueLower;
    }

    return 0;
}


