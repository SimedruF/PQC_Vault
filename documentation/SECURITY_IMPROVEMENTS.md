# Îmbunătățiri de Securitate PQC Wallet - Versiunea 2.0

> Notă istorică: acest document descrie formatul V2. Implementarea curentă scrie
> formatul V4 cu ML-KEM-768 și nonce-uri AES-GCM independente; V2/V3 cu Kyber
> sunt păstrate numai pentru autentificare și migrare automată.

## Problema identificată
Versiunea anterioară a PQC Wallet avea următoarele vulnerabilități:
- Cheia secretă Kyber era stocată în clar în fișierul .enc
- Parola era criptată doar cu XOR simplu
- Fișierele puteau fi decriptate de oricine cu acces la ele
- Nu exista autentificare sau verificare de integritate

## Soluțiile implementate

### 1. Protecție la nivel de sistem de fișiere
```bash
# Permisiuni restrictive pentru directorul users
chmod 700 users/

# Permisiuni restrictive pentru fișierele .enc
chmod 600 users/*.enc
```

### 2. Criptare multi-nivel îmbunătățită

#### Structura nouă a fișierului (Versiunea 2):
```
[Version: uint32_t] - Versiunea formatului (2)
[Salt: 32 bytes] - Salt aleator pentru derivarea cheii
[IV: 16 bytes] - Vector de inițializare pentru AES
[Ciphertext: variable] - Kyber768 KEM ciphertext
[Public Key: variable] - Kyber768 cheia publică
[Encrypted Secret Key: variable] - Cheia secretă criptată cu AES-256-GCM
[Encrypted Password: variable] - Parola dublu criptată
[Auth Tags: 32 bytes] - Tag-uri de autentificare (2x16 bytes)
```

#### Algoritmul de criptare îmbunătățit:

1. **Derivarea cheii din parolă**:
   ```
   Scrypt(password, salt, N=32768, r=8, p=1) → derived_key (256-bit)
   ```

2. **Protecția cheii secrete Kyber**:
   ```
   AES-256-GCM(secret_key, derived_key, IV) → encrypted_secret_key + tag1
   ```

3. **Criptarea dublă a parolei**:
   ```
   XOR(password, kyber_shared_secret) → temp_encrypted
   AES-256-GCM(temp_encrypted, derived_key, IV) → final_encrypted + tag2
   ```

### 3. Verificare de integritate și autentificare
- Folosește AES-GCM care oferă atât criptare, cât și autentificare
- Tag-uri de autentificare separate pentru cheia secretă și parolă
- Verificarea integrității eșuează dacă fișierul a fost modificat

### 4. Parametri de securitate îmbunătățiți

#### Scrypt pentru derivarea cheii:
- **N=32768**: Factor de cost (memorie și timp)
- **r=8**: Dimensiunea blocului
- **p=1**: Factor de paralelizare
- **Salt=32 bytes**: Salt aleator unic pentru fiecare utilizator

#### AES-256-GCM:
- **Cheie=256 bits**: Derivată din parolă cu Scrypt
- **IV=128 bits**: Vector de inițializare aleator
- **Tag=128 bits**: Tag de autentificare pentru verificarea integrității

### 5. Compatibilitate retroactivă
- Detectarea automată a formatului vechi (versiunea 1)
- Funcție `VerifyPasswordLegacy()` pentru fișierele vechi
- Unealta de migrare pentru actualizarea utilizatorilor existenți

## Beneficiile de securitate

### Înainte (Versiunea 1):
❌ Cheia secretă în clar
❌ Criptare XOR simplă
❌ Fără verificare de integritate
❌ Fără protecție împotriva modificării fișierelor
❌ Vulnerabil la atacuri offline

### După (Versiunea 2):
✅ Cheia secretă criptată cu AES-256-GCM
✅ Criptare dublă (Kyber + AES)
✅ Verificare de integritate cu tag-uri de autentificare
✅ Protecție împotriva modificării fișierelor
✅ Parametri Scrypt puternici împotriva atacurilor de forță brută
✅ Permisiuni restrictive la nivel de sistem de fișiere

## Testarea securității

### Încercare de decriptare cu vechile unelte:
```bash
$ ./extract_password
Aborted (core dumped)

$ ./analyze_user
terminate called after throwing an instance of 'std::bad_alloc'
```

### Verificarea permisiunilor:
```bash
$ ls -la users/
drwx------  2 simedruf simedruf 4096 Jul 13 11:45 .
-rw-------  1 simedruf simedruf 4828 Jul 13 11:45 simedruf.enc
```

## Migrarea utilizatorilor existenți

Pentru a migra utilizatorii existenți la noul format securizat:

```bash
cd /home/simedruf/Projects/PQCWallet
g++ -std=c++17 -I. migrate_security.cpp src/PasswordManager.cpp -loqs -lssl -lcrypto -o migrate_security
./migrate_security
```

Instrumentul de migrare va:
1. Verifica parola cu formatul vechi
2. Crea o copie de backup
3. Șterge fișierul vechi
4. Creează utilizatorul cu noul format securizat
5. Verifică că migrarea a reușit
6. Șterge backup-ul în caz de succes

## Recomandări suplimentare

### Pentru securitate maximă:
1. **Criptarea discului**: Folosiți FileVault (macOS), BitLocker (Windows), sau LUKS (Linux)
2. **Backup-uri criptate**: Stocați backup-urile într-un mod criptat separat
3. **Parole puternice**: Folosiți parole complexe și unice
4. **Actualizări regulate**: Mențineți sistemul și aplicația actualizate

### Pentru dezvoltatori:
1. **Audit de securitate**: Efectuați audituri de securitate regulate
2. **Testare de penetrare**: Testați rezistența la atacuri
3. **Logging de securitate**: Implementați logging pentru încercările de acces
4. **Rate limiting**: Implementați limitări pentru încercările de logare

## Concluzie

Noua implementare transformă PQC Wallet într-un sistem cu adevărat sigur, unde:
- Fișierele .enc nu mai pot fi decriptate fără parola corectă
- Atacurile offline sunt semnificativ mai dificile datorită Scrypt
- Integritatea datelor este garantată prin tag-urile de autentificare
- Permisiunile restrictive oferă protecție suplimentară la nivel de SO

Această implementare respectă standardele moderne de securitate și oferă protecție robustă împotriva atacurilor post-cuantice.

## 🛡️ Ghid de Utilizare - Securitate Post-Cuantică

### Cum Funcționează Protecția
PQC Wallet folosește algoritmi rezistenți la calculatoarele cuantice:

1. **Autentificare**: Parola este protejată cu criptare post-cuantică
2. **Stocare Fișiere**: Arhivele folosesc criptare hibridă (clasică + post-cuantică)  
3. **Protecție Date**: Toate datele sensibile sunt criptate în mai multe straturi

### Algoritmii de Criptare Utilizați

#### Protecția Parolei
```
Parola Utilizator → Scrypt → AES-256-GCM + Kyber768
```
- **Scrypt**: Funcție de derivare a cheii rezistentă la atacuri hardware
- **AES-256-GCM**: Criptare autentificată 256-bit (vulnerabilă la quantum dar încă puternică)
- **Kyber768**: Mecanism de încapsulare a cheii post-cuantică selectat de NIST

#### Criptarea Arhivelor
```
Fișiere Arhivă → AES-256-CTR → Kyber768 KEM → Stocare Securizată
```
- **AES-256-CTR**: Criptare rapidă pentru date mari
- **Kyber768**: Protejează cheile AES împotriva atacurilor cuantice
- **Autentificare**: HMAC-SHA256 pentru integritatea datelor

#### Specificații Detaliate ale Algoritmilor

**Kyber768 (Post-Cuantic)**:
- Nivel de Securitate: 192-bit (echivalent AES-192)
- Dimensiune Cheie: 2400 bytes (publică), 2400 bytes (secretă)
- Dimensiune Ciphertext: 1088 bytes
- Bazat pe: Problema Module Learning With Errors (M-LWE)
- Rezistență Cuantică: ✅ Dovedit sigur împotriva calculatoarelor cuantice

**Scrypt (Derivarea Cheii)**:
- Parametri: N=32768, r=8, p=1
- Output: Cheie derivată de 256-bit
- Cost Memorie: ~32MB (previne atacurile ASIC)
- Cost Timp: Dificultate configurabilă

**AES-256-GCM (Simetric)**:
- Dimensiune Cheie: 256 bits
- Dimensiune Bloc: 128 bits
- Autentificare: AEAD încorporat (Authenticated Encryption)
- Dimensiune IV: 128 bits (generat aleator)

### Niveluri de Securitate

| Componentă | Securitate Clasică | Securitate Cuantică |
|------------|-------------------|---------------------|
| Kyber768 | 192-bit | 192-bit ✅ |
| AES-256 | 256-bit | 128-bit ⚠️ |
| Scrypt | Configurabil | Memory-hard ✅ |
| HMAC-SHA256 | 256-bit | 128-bit ⚠️ |

**Legendă**:
- ✅ = Rezistent la quantum
- ⚠️ = Vulnerabil la quantum dar calculațional impracticabil

### De Ce Post-Cuantică?

Calculatoarele cuantice viitoare vor sparge criptarea actuală:
- **RSA**: Vulnerabil la algoritmul lui Shor
- **ECC**: Vulnerabil la algoritmul lui Shor
- **AES**: Puterea înjumătățită de algoritmul lui Grover
- **Kyber**: Imun la atacurile cuantice cunoscute

PQC Wallet protejează datele dumneavoastră astăzi și în viitorul cuantic.
