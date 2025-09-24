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
