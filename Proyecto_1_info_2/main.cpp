#include <iostream>
#include <fstream>
using namespace std;

// Funciones de desencriptación
char rotarIzquierda(char c, int n) { return (c << n) | (c >> (8 - n)); }
char rotarDerecha(char c, int n) { return (c >> n) | (c << (8 - n)); }
char aplicarXOR(char c, char clave) { return c ^ clave; }
char desencriptarChar(char c, int n, char clave) { return rotarDerecha(aplicarXOR(c, clave), n); }

// Función para encontrar n y K usando la pista
bool encontrarParametros(const char* mensajeEnc, const char* pista, int lenPista, int &nCorrecto, char &KCorrecto) {
    for (int n = 1; n <= 7; n++) {
        for (int K = 0; K <= 255; K++) {
            bool coincide = true;
            for (int i = 0; i < lenPista; i++) {
                char desen = desencriptarChar(mensajeEnc[i], n, (char)K);
                if (desen != pista[i]) {
                    coincide = false;
                    break;
                }
            }
            if (coincide) {
                nCorrecto = n;
                KCorrecto = (char)K;
                return true;
            }
        }
    }
    return false;
}

int main() {
    // Leer archivo Encriptado1.txt
    ifstream fEnc("C:\\Users\\LENOVO\\OneDrive\\Desktop\\Desafio_1\\Encriptado1.txt", ios::binary);
    if (!fEnc) { cout << "No se pudo abrir Encriptado1.txt\n"; return 1; }
    fEnc.seekg(0, ios::end);
    int lenEnc = fEnc.tellg();
    fEnc.seekg(0, ios::beg);
    char* mensajeEnc = new char[lenEnc];
    fEnc.read(mensajeEnc, lenEnc);
    fEnc.close();

    // Leer archivo pista1.txt
    ifstream fPista("C:\\Users\\LENOVO\\OneDrive\\Desktop\\Desafio_1\\pista1.txt", ios::binary);
    if (!fPista) { cout << "No se pudo abrir pista1.txt\n"; delete[] mensajeEnc; return 1; }
    fPista.seekg(0, ios::end);
    int lenPista = fPista.tellg();
    fPista.seekg(0, ios::beg);
    char* pista = new char[lenPista];
    fPista.read(pista, lenPista);
    fPista.close();

    // Buscar n y K
    int nCorrecto;
    char KCorrecto;
    if (encontrarParametros(mensajeEnc, pista, lenPista, nCorrecto, KCorrecto)) {
        cout << "Parámetros encontrados:\n";
        cout << "n (rotación bits) = " << nCorrecto << "\n";
        cout << "K (clave XOR) = " << (int)KCorrecto << "\n";
    } else {
        cout << "No se encontraron parámetros que coincidan con la pista.\n";
    }

    delete[] mensajeEnc;
    delete[] pista;
    return 0;
}
