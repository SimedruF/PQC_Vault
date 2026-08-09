# Tranzacțiile interne ale arhivelor

Modificările unei arhive sunt publicate ca o singură înlocuire atomică și nu
mai pot suprascrie în tăcere o versiune salvată de altă instanță.

## Protocolul de salvare

Pentru fiecare fișier de arhivă, SaveArchive() execută următoarele operații:

1. obține lockul exclusiv al arhivei;
2. calculează revizia SHA-256 a fișierului curent;
3. compară revizia cu versiunea încărcată de instanță;
4. serializează și criptează candidatul în memorie;
5. publică rezultatul prin AtomicFile::Write();
6. reține noua revizie numai după publicarea reușită.

Dacă revizia diferă, salvarea este refuzată. Instanța trebuie să apeleze
ReloadArchive() și apoi să reaplice operația. Acest model optimist previne
lost-update fără a combina automat două modificări care ar putea fi
incompatibile.

## Lockul per arhivă

Lockul este păstrat în fișierul cu sufixul .lock de lângă arhivă. Pe Linux este
folosit flock(), iar pe Windows un handle deschis fără partajare. Așteptarea are
o limită de cinci secunde.

Fișierul de lock conține markerul PQCLOCK1, are permisiuni private pe platformele
POSIX și rămâne intenționat pe disc. Ștergerea lui după fiecare operație ar crea
o cursă în care două procese ar putea bloca inode-uri diferite.

## Rollback în memorie

- Adăugare sau înlocuire: intrarea anterioară este păstrată până după commit.
- Ștergere: nodul scos din map este păstrat până după commit.
- Reparare: dimensiunile și hash-urile anterioare sunt înregistrate într-un
  jurnal undo; intrările eliminate nu sunt curățate până la commit.
- Resetare și schimbarea parolei arhivei păstrează, de asemenea, starea veche
  până la salvarea reușită.

La orice eșec, fișierul criptat rămâne nemodificat și starea obiectului este
restaurată fără reîncărcare de pe disc. RepairArchive() nu mai inventează date
zero pentru o intrare deteriorată; repară numai metadatele care pot fi derivate
din datele autentificate.

## Testare

Testul archive_transaction verifică:

- rollbackul unei înlocuiri și al unei ștergeri;
- repararea eșuată când scrierea temporară nu poate fi pornită, modelând lipsa
  spațiului sau o eroare de permisiuni;
- absența fișierelor cu nume .tmp. după fiecare eroare simulată;
- două instanțe care pornesc de la aceeași revizie: exact una poate salva, iar
  cealaltă trebuie să facă reload înainte de retry;
- păstrarea tuturor intrărilor după reload și retry.

