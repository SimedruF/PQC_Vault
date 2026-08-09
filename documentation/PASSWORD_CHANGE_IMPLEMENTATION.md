# Schimbarea tranzacțională a parolei master

## Stare

Implementat și verificat la 8 august 2026.

Schimbarea parolei coordonează într-o singură tranzacție logică:

- fișierul V4 al utilizatorului;
- baza de date `PQCDB002`;
- toate arhivele `PQCENC02` ale utilizatorului.

## Flux

1. Parola veche este autentificată.
2. Noul fișier de utilizator este construit cu ML-KEM-768 și AES-256-GCM și
   este verificat criptografic în memorie.
3. Baza de date este criptată cu parola nouă, apoi decriptată și comparată cu
   sursa înainte de publicare.
4. Fiecare arhivă este încărcată cu parola veche, criptată cu parola nouă și
   decriptată din nou pentru verificarea exactă a conținutului.
5. Originalele și înlocuitoarele validate sunt scrise în directorul privat
   `.pqcwallet_transactions/`.
6. Un jurnal persistent este sincronizat înainte să fie modificat primul fișier real.
7. Destinațiile sunt înlocuite individual prin scrieri atomice, iar progresul
   este înregistrat după fiecare fișier.
8. După publicarea tuturor fișierelor, jurnalul este marcat `COMMITTED` și curățat.

## Recuperare după întrerupere

La inițializarea `PasswordManager` sunt procesate jurnalele rămase:

- `PREPARED`: toate fișierele originale sunt restaurate;
- `COMMITTED`: fișierele noi sunt păstrate și jurnalul rezidual este curățat.

Astfel, o oprire între două înlocuiri nu lasă utilizatorul cu un amestec de
fișiere criptate sub parole diferite.

## Integrarea UI

`WalletWindow` apelează o singură operație coordonată. Baza de date nu mai este
re-criptată separat înaintea contului. După succes, instanța veche a ferestrei de
arhivă este distrusă, deoarece aceasta încă reținea parola anterioară și ar fi
putut rescrie accidental arhiva.

## Testare

Testul `master_password_transaction_test` acoperă un utilizator, o bază de date
și două arhive:

- eroare înaintea publicării fiecărei destinații;
- verificarea faptului că parola veche rămâne valabilă peste tot după rollback;
- verificarea faptului că parola nouă nu este publicată parțial;
- simularea opririi după înlocuirea a două destinații;
- recuperarea automată la următoarea inițializare;
- commit complet și respingerea parolei vechi de către toate componentele.

Comanda de verificare:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Rezultatul curent este `6/6` teste trecute.

## Limită rămasă

Această tranzacție protejează schimbarea parolei, dar nu este un mecanism de
recuperare a unei parole uitate. Backup-ul criptat și cheia opțională de
recuperare rămân activități separate în `TODO.md`.
