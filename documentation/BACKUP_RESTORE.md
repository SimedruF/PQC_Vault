# Backup și restore pentru baza de credențiale

## Stare

Implementat și verificat la 8 august 2026.

Formatul `PQCBKP01` este un container separat de `PQCDB002`. El permite exportul
bazei de credențiale sub o cheie de backup independentă de parola master curentă.
Nu este o copie în clar și nu reutilizează cheia fișierului live.

## Format PQCBKP01

Toate valorile numerice din antet sunt big-endian:

```text
[magic: 8]             "PQCBKP01"
[version: u32]         1
[KDF: u32]             scrypt
[N: u64]               32768
[r: u32]               8
[p: u32]               1
[salt size: u32]       32
[nonce size: u32]      12
[tag size: u32]        16
[ciphertext size: u64]
[salt: 32]
[nonce: 12]
[ciphertext]
[AES-GCM tag: 16]
```

Antetul complet este AAD pentru AES-256-GCM. Orice modificare a versiunii,
parametrilor KDF, lungimilor, saltului, nonce-ului sau conținutului face ca
autentificarea să eșueze.

Payload-ul autentificat conține:

- versiunea schemei de backup;
- momentul exportului;
- reprezentarea validată a bazei de credențiale.

## Export

1. Baza trebuie să fie încărcată și autentificată.
2. Conținutul este serializat într-un envelope versionat.
3. Este generat un salt și un nonce nou.
4. Payload-ul este criptat cu scrypt și AES-256-GCM.
5. Rezultatul este decriptat în memorie și comparat cu sursa.
6. Backup-ul este publicat prin scriere atomică.

Dacă exportul este întrerupt, un backup existent rămâne neschimbat.

## Restore

1. Fișierul este citit cu o limită strictă de dimensiune.
2. Magic bytes, versiunea, parametrii și lungimile sunt validate.
3. AES-GCM autentifică integral backup-ul înaintea interpretării payload-ului.
4. Envelope-ul și fiecare înregistrare a bazei sunt validate structural.
5. Datele sunt recriptate cu parola master a sesiunii curente în format `PQCDB002`.
6. Noul container este decriptat și comparat cu payload-ul importat.
7. Baza live este înlocuită atomic.
8. Starea din memorie este schimbată numai după succesul scrierii.

Parola greșită, un backup modificat sau trunchiat și o eroare de scriere nu
modifică baza live.

## Cheia de recuperare

Dialogul de export poate genera o cheie aleatorie de 256 de biți în format:

```text
PQC-RK1-<64 caractere hexazecimale>
```

Cheia este afișată numai în dialog și este ștearsă din buffer la confirmare,
anulare sau închiderea ferestrei. Utilizatorul trebuie să o păstreze offline.
Aplicația nu stochează o copie și nu conține o parolă universală de recuperare.

Se poate utiliza și o parolă aleasă manual, de minimum 12 caractere, însă o
cheie generată oferă entropie semnificativ mai bună.

## Utilizare în ImGui

În fereastra `Database Manager`:

- `Database → Export Backup` deschide dialogul de export;
- `Database → Import Backup` deschide dialogul de restore;
- restore necesită o confirmare explicită că înregistrările curente vor fi
  înlocuite.

După import, lista de credențiale este reîncărcată automat.

## Testare

Testul `database_backup_security_test` verifică:

- magic bytes `PQCBKP01` și absența câmpurilor sensibile în clar;
- generarea cheii de recuperare;
- păstrarea backup-ului anterior la export întrerupt;
- respingerea parolei greșite;
- respingerea tag-ului modificat și a fișierului trunchiat;
- păstrarea exactă a bazei live la import eșuat;
- actualizarea simultană a discului și a stării din memorie la restore valid;
- redeschiderea bazei restaurate cu parola master curentă.

Rezultatul curent este `7/7` teste CTest trecute.

## Limită de scop

`PQCBKP01` acoperă baza de credențiale administrată de `EncryptedDatabase`. Nu
include fișierul de autentificare al utilizatorului și nici arhivele `PQCENC02`.
Un pachet complet al contului poate fi adăugat ulterior peste același model de
container și tranzacție multi-fișier.

Setarea existentă „Automatic backup” nu pornește încă exporturi automate. Pentru
a evita stocarea nesigură a cheii de backup, automatizarea trebuie legată ulterior
de un keychain/credential vault al sistemului de operare.
