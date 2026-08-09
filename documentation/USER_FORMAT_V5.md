# Formatul portabil al fișierelor utilizatorilor V5

Fișierele noi din users/ folosesc formatul V5. Toate numerele din V5 sunt
unsigned și codificate big-endian, independent de arhitectura procesorului și
de dimensiunea tipului size_t.

## Antet

| Offset | Dimensiune | Câmp |
|---:|---:|---|
| 0 | 8 | magic ASCII PQCUSR05 |
| 8 | 4 | versiune, valoarea 5 |
| 12 | 8 | lungimea totală a fișierului |
| 20 | 4 | număr componente, valoarea 9 |

După antet urmează exact nouă componente. Fiecare este codificată ca o lungime
unsigned de 64 biți big-endian, urmată de numărul declarat de octeți.

Ordinea componentelor este:

1. saltul scrypt;
2. nonce-ul AES-GCM pentru cheia secretă ML-KEM;
3. nonce-ul AES-GCM pentru parola protejată;
4. ciphertext-ul ML-KEM-768;
5. cheia publică ML-KEM-768;
6. cheia secretă ML-KEM criptată;
7. parola protejată și criptată;
8. tagul GCM al cheii secrete;
9. tagul GCM al parolei.

## Limite și parsare

- fișierul complet: maximum 64 MiB;
- fiecare componentă: maximum 16 MiB și minimum un octet;
- numărul de componente trebuie să fie exact nouă;
- lungimea totală declarată trebuie să coincidă exact cu dimensiunea fișierului;
- toate componentele trebuie să se termine exact la sfârșitul declarat;
- dimensiunile criptografice fixe sunt verificate înainte de derivarea sau
  folosirea cheilor.

Parserul respinge fișiere trunchiate, lungimi care produc depășiri, componente
supradimensionate și orice octet suplimentar la final.

## Criptografie

V5 păstrează modelul criptografic introdus de V4:

- ML-KEM-768;
- scrypt cu N=32768, r=8 și p=1;
- AES-256-GCM;
- salt de 32 octeți;
- două nonce-uri independente de 12 octeți;
- două taguri GCM de 16 octeți.

Schimbarea V4→V5 privește reprezentarea portabilă a containerului. La migrare
se generează totuși un set criptografic nou, inclusiv salt, nonce-uri și chei
ML-KEM noi.

## Compatibilitate și migrare

Citirea V1–V4 rămâne disponibilă:

- V1–V3 folosesc Kyber și formatele native istorice;
- V4 folosește ML-KEM-768, dar versiunea și lungimile sunt native-endian;
- V5 este singurul format produs de codul curent.

Un fișier vechi este migrat numai după verificarea completă a parolei. V5 este
construit în memorie și publicat prin AtomicFile::Write(). Dacă publicarea
eșuează, autentificarea poate continua, dar fișierul vechi rămâne identic și
migrarea va fi reîncercată la o autentificare ulterioară. O parolă greșită sau
un fișier invalid nu declanșează nicio scriere.

## Testare

Testul password_manager_gcm verifică acum:

- crearea și autentificarea V5;
- antetul, lungimea totală și nonce-urile independente;
- compatibilitatea V1, V2, V3 și V4;
- migrarea V4 întreruptă și reluarea ei ulterioară;
- absența migrării după o parolă greșită;
- fișiere trunchiate, lungimi malițioase, lungime totală inconsistentă și date
  suplimentare la final;
- păstrarea V5 după schimbarea tranzacțională a parolei.

