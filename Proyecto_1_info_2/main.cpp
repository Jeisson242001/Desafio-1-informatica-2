#include <iostream>
#include <fstream>
#include <cstdint>
#include <limits>

// ---------------- tipo tamaño ----------------
typedef unsigned long long usize;

// ---------------- utilidades C-string ----------------
static usize cstr_len(const char* s){
    if(!s) return 0;
    const char* p = s;
    while(*p) ++p;
    return (usize)(p - s);
}
static void utoa10(unsigned int v, char* buf){
    char tmp[32]; int k=0;
    if (v == 0) { buf[0]='0'; buf[1]='\0'; return; }
    while (v > 0 && k < 31) { tmp[k++] = (char)('0' + (v % 10)); v /= 10; }
    for (int i = 0; i < k; ++i) buf[i] = tmp[k-1-i];
    buf[k] = '\0';
}

// ---------------- construir nombres sin overflow ----------------
static bool make_name_safe(const char* base, const char* pref, unsigned int num, const char* suf, char* out, usize outSize){
    usize baseL = cstr_len(base);
    char numbuf[32]; utoa10(num, numbuf);
    usize total = baseL + cstr_len(pref) + cstr_len(numbuf) + cstr_len(suf) + 1;
    if (total > outSize) return false;
    char* p = out;
    const char* q = base; while(*q) *p++ = *q++;
    q = pref; while(*q) *p++ = *q++;
    q = numbuf; while(*q) *p++ = *q++;
    q = suf; while(*q) *p++ = *q++;
    *p = '\0';
    return true;
}

// ---------------- I/O binaria ----------------
static unsigned char* readFile(const char* path, usize& outLen) {
    outLen = 0;
    std::ifstream f(path, std::ios::binary);
    if(!f.good()) return nullptr;
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if(sz < 0){ f.close(); return nullptr; }
    if(sz == 0){ f.close(); outLen=0; unsigned char* b = new (std::nothrow) unsigned char[1]; if(b) b[0]=0; return b; }
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

// ---------------- rotaciones y XOR ----------------
static inline unsigned char ROL8(unsigned char b, int n){ n &= 7; if(!n) return b; return (unsigned char)(((b << n) | (b >> (8 - n))) & 0xFF); }
static inline unsigned char ROR8(unsigned char b, int n){ n &= 7; if(!n) return b; return (unsigned char)(((b >> n) | (b << (8 - n))) & 0xFF); }
static inline unsigned char XOR8(unsigned char b, unsigned char K){ return (unsigned char)(b ^ K); }
// Aplica XOR then rotate (useROL=true => ROL, false => ROR)
static void applyXorThenRotate(const unsigned char* in, usize len, unsigned char K, int n, bool useROL, unsigned char* out){
    for(usize i=0;i<len;++i){
        unsigned char v = XOR8(in[i], K);
        out[i] = useROL ? ROL8(v,n) : ROR8(v,n);
    }
}


// ---------------- búsqueda naive substring ----------------
static long find_substr(const unsigned char* hay, usize hayLen, const char* needle){
    usize nLen = cstr_len(needle);
    if(nLen==0 || nLen>hayLen) return -1;
    for(usize i=0; i + nLen <= hayLen; ++i){
        usize j=0; while(j<nLen && hay[i+j] == (unsigned char)needle[j]) ++j;
        if(j==nLen) return (long)i;
    }
    return -1;
}

// ---------------- RLE ASCII (ej: 11W1B12W...) ----------------
static bool rle_decompress_ascii(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen){
    *outBuf=nullptr; *outLen=0;
    usize cap = inLen * 16 + 1024; if(cap < 4096) cap = 4096;
    unsigned char* out = new (std::nothrow) unsigned char[cap]; if(!out) return false;
    usize o=0, i=0;
    while(i<inLen){
        if(in[i]<'0' || in[i]>'9'){ delete[] out; return false; }
        usize len=0, digits=0;
        while(i<inLen && in[i]>='0' && in[i]<='9'){
            len = len*10 + (usize)(in[i]-'0'); ++i; ++digits; if(digits>10){ delete[] out; return false; }
        }
        if(i>=inLen){ delete[] out; return false; }
        unsigned char sym = in[i++]; // símbolo
        if(len==0){ delete[] out; return false; }
        if(o+len>cap){
            usize newCap = (o+len)*2 + 1024; unsigned char* tmp = new (std::nothrow) unsigned char[newCap]; if(!tmp){ delete[] out; return false; }
            for(usize k=0;k<o;++k) tmp[k]=out[k]; delete[] out; out=tmp; cap=newCap;
        }
        for(usize k=0;k<len;++k) out[o++]=sym;
    }
    *outBuf=out; *outLen=o; return true;
}


// ---------------- RLE binario (uint16 LE len + símbolo) ----------------
static bool rle_decompress_bin_le(const unsigned char* in, usize inLen, unsigned char** outBuf, usize* outLen){
    *outBuf=nullptr; *outLen=0; if(inLen<3) return false;
    usize cap = inLen * 32 + 1024; if(cap < 4096) cap = 4096;
    unsigned char* out = new (std::nothrow) unsigned char[cap]; if(!out) return false;
    usize o=0, i=0;
    while(i+2<inLen){
        unsigned short len = (unsigned short)(in[i] | (in[i+1] << 8)); i+=2;
        unsigned char sym = in[i++]; if(len==0){ delete[] out; return false; }
        if(o+len>cap){
            usize newCap = (o+len)*2 + 1024; unsigned char* tmp = new (std::nothrow) unsigned char[newCap]; if(!tmp){ delete[] out; return false; }
            for(usize k=0;k<o;++k) tmp[k]=out[k]; delete[] out; out=tmp; cap=newCap;
        }
        for(unsigned short k=0;k<len;++k) out[o++]=sym;
    }
    if(i!=inLen){ delete[] out; return false; }
    *outBuf=out; *outLen=o; return true;
}

