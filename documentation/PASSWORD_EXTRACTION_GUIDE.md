# Extragerea parolei din fișierul simedruf.enc

## Răspuns scurt
Parola din fișierul `users/simedruf.enc` este: **`Criptaretare2@24`**

## Cum funcționează sistemul de parole din PQC Wallet

### Structura fișierului .enc

Fișierul `simedruf.enc` conține următoarele componente (în această ordine):

1. **Ciphertext** (1088 bytes) - Kyber768 KEM ciphertext
2. **Public Key** (1184 bytes) - Cheia publică Kyber768
3. **Secret Key** (2400 bytes) - Cheia secretă Kyber768  
4. **Encrypted Password** (16 bytes) - Parola criptată cu XOR

### Algoritmul de criptare

1. **Generarea cheilor**: Se folosește algoritmul post-cuantic Kyber768 pentru a genera o pereche de chei (publică/secretă)
2. **Encapsularea**: Se generează un secret partajat folosind cheia publică
3. **Criptarea parolei**: Parola este criptată folosind XOR cu secretul partajat
4. **Salvarea**: Toate componentele sunt salvate în fișierul .enc

### Procesul de decriptare

```cpp
// 1. Încarcă datele din fișier
EncryptedPasswordData data = LoadEncryptedData("users/simedruf.enc");

// 2. Inițializează Kyber KEM
OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);

// 3. Decapsulează pentru a obține secretul partajat
uint8_t shared_secret[32];
OQS_KEM_decaps(kem, shared_secret, data.ciphertext, data.secret_key);

// 4. Decriptează parola folosind XOR
string password = XORDecrypt(data.encrypted_password, shared_secret);
```

## Metode de extragere a parolei

### Metoda 1: Folosind utilitarul creat

```bash
cd /home/simedruf/Projects/PQCWallet
g++ -std=c++17 extract_password.cpp -loqs -lssl -lcrypto -o extract_password
./extract_password users/simedruf.enc
```

### Metoda 2: Folosind aplicația PQC Wallet

Aplicația PQC Wallet face exact același proces în funcția `PasswordManager::VerifyPassword()` din fișierul `src/PasswordManager.cpp`.

### Metoda 3: Analiza manuală

Poți folosi utilitarul de analiză pentru a vedea structura fișierului:

```bash
g++ -std=c++17 analyze_user.cpp -o analyze_user
./analyze_user
```

## Securitatea sistemului

### Puncte forte:
- **Post-Quantum Security**: Folosește Kyber768, rezistent la atacurile cuantice
- **Separarea componentelor**: Cheia secretă, ciphertext-ul și parola criptată sunt separate
- **Algoritm standardizat**: Kyber768 este standardul NIST pentru criptografia post-cuantică

### Puncte slabe (pentru securitate):
- **Cheia secretă în același fișier**: Pentru decriptare, cheia secretă este în același fișier cu datele criptate
- **XOR simplu**: Parola este criptată doar cu XOR (deși cu o cheie forte)
- **Fără salt/IV**: Nu se folosește salt sau IV suplimentar

## Structura în hex

```
Offset 0x0000: [8 bytes] - Dimensiunea ciphertext (1088 bytes = 0x440)
Offset 0x0008: [1088 bytes] - Ciphertext Kyber768
Offset 0x0448: [8 bytes] - Dimensiunea cheii publice (1184 bytes)
Offset 0x0450: [1184 bytes] - Cheia publică Kyber768
Offset 0x08F0: [8 bytes] - Dimensiunea cheii secrete (2400 bytes)
Offset 0x08F8: [2400 bytes] - Cheia secretă Kyber768
Offset 0x1258: [8 bytes] - Dimensiunea parolei criptate (16 bytes)
Offset 0x1260: [16 bytes] - Parola criptată cu XOR
```

## Concluzie

Fișierul `simedruf.enc` conține parola `Criptaretare2@24` criptată folosind algoritmi post-cuantici. Deși sistemul folosește criptografie avansată, faptul că toate componentele necesare pentru decriptare sunt în același fișier înseamnă că oricine are acces la fișier poate extrage parola folosind metodele de mai sus.
