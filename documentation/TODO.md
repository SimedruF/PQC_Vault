# TODO pentru 8 august 2026

## Starea de la care continuăm

Pașii de securitate 1–6 sunt aplicați. Proiectul folosește în prezent:

- format portabil de autentificare V5 (`PQCUSR05`) cu ML-KEM-768, scrypt și AES-256-GCM;
- arhive `PQCENC02` și bază de date `PQCDB002`, autentificate cu AES-GCM;
- nonce-uri GCM independente și date de antet autentificate;
- ștergerea explicită din memorie a parolelor, cheilor și bufferelor decriptate;
- scrieri atomice pentru utilizatori, arhive, baza de date, setări și extrageri;
- rollback al stării din memorie când o salvare eșuează;
- 11 teste CTest funcționale, inclusiv simularea întreruperii scrierilor,
  schimbarea tranzacțională a parolei master, backup/restore autentificat,
  validarea centralizată a căilor, tranzacțiile interne ale arhivelor,
  validarea formatelor și limitele arhivelor.

Comanda de verificare a punctului de plecare:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

## Prioritate critică

### 1. Schimbarea tranzacțională a parolei master — finalizat 8 august 2026

- [x] Tratarea bazei de date, fișierului utilizatorului și tuturor arhivelor ca o singură operație logică.
- [x] Pregătirea și verificarea tuturor fișierelor noi înainte de publicarea primului fișier.
- [x] Adăugarea unui jurnal de tranzacție sau a unui mecanism sigur de rollback pentru cazul în care aplicația se oprește între două înlocuiri.
- [x] Păstrarea parolei vechi funcționale pentru toate fișierele dacă oricare etapă eșuează.
- [x] Teste cu eroare simulată înaintea fiecărei etape: bază de date, utilizator și fiecare arhivă.

Implementarea pregătește și autentifică toate înlocuitoarele, păstrează originalele
într-un jurnal privat și recuperează automat orice tranzacție incompletă la următoarea
inițializare a `PasswordManager`. Un jurnal marcat finalizat este doar curățat, iar
unul rămas în starea pregătită restaurează toate originalele.

### 2. Backup și recuperare controlată — finalizat 8 august 2026

- [x] Implementarea reală a `EncryptedDatabase::exportBackup()` și `importBackup()`.
- [x] Definirea formatului `PQCBKP01`, criptat, versionat și autentificat.
- [x] Verificarea completă a backup-ului înainte de înlocuirea datelor locale.
- [x] Cheie de recuperare opțională de 256 de biți, generată pentru export și păstrată numai de utilizator.
- [x] Afișarea clară în UI că backup-ul nu poate fi restaurat fără cheia sa.
- [x] Teste pentru parolă greșită, backup modificat, backup trunchiat și întreruperea importului.

Cheia de recuperare trebuie să fie opțională, generată aleator și păstrată numai de utilizator; nu trebuie introdusă o parolă universală sau o cale ascunsă de acces.

Implementarea curentă acoperă baza de credențiale. Un pachet care include și
contul plus arhivele poate fi construit ulterior dacă este necesar.

### 3. Validarea centralizată a numelor și căilor — finalizat 8 august 2026

- [x] Crearea unui validator comun pentru numele utilizatorilor și arhivelor.
- [x] Respingerea valorilor goale, `..`, separatorilor `/` și `\\`, caracterelor de control și numelor excesiv de lungi.
- [x] Verificarea prin cale canonică a faptului că fișierele utilizatorilor rămân în `users/`, iar arhivele în `archives/`.
- [x] Protejarea extragerii împotriva numelor de fișiere care ar putea ieși din directorul ales.
- [x] Teste pentru path traversal, căi absolute, Unicode problematic și coliziuni de nume.

Validarea este aplicată în backend la creare, încărcare și deserializare, iar
interfața reutilizează aceleași reguli. Căile sunt comparate pe componente după
canonizare, inclusiv împotriva escape-ului prin symlink. Regulile și limitele
sunt descrise în `PATH_VALIDATION.md`.

## Prioritate ridicată

### 4. Tranzacții complete pentru modificările interne — finalizat 8 august 2026

- [x] Extinderea testelor de rollback pentru înlocuirea și ștergerea fișierelor din arhivă.
- [x] Verificarea fluxului `RepairArchive()` la lipsă de spațiu sau eroare de permisiuni.
- [x] Confirmarea faptului că nu rămân fișiere `*.tmp.*` după nicio cale de eroare.
- [x] Adăugarea unui test pentru sincronizarea a două instanțe care încearcă să salveze același fișier; dacă este necesar, introducerea unui lock per arhivă.

Salvarea folosește un lock exclusiv per arhivă și o revizie SHA-256 pentru a
refuza instanțele rămase în urmă. Adăugarea, înlocuirea, ștergerea și repararea
își restaurează starea în memorie dacă scrierea atomică eșuează. Detaliile sunt
în `ARCHIVE_TRANSACTIONS.md`.

### 5. Format portabil pentru fișierele utilizatorilor — finalizat 8 august 2026

- [x] Înlocuirea valorilor numerice scrise în endian-ul nativ în formatul V4 cu o codificare explicită, independentă de platformă.
- [x] Introducerea unui format V5 cu magic bytes, versiune, lungime totală și limite stricte pentru toate componentele.
- [x] Migrare V4 → V5 numai după autentificare reușită și prin scriere atomică.
- [x] Testarea fișierelor trunchiate, a lungimilor malițioase și a datelor suplimentare la final.

V5 folosește magic-ul `PQCUSR05`, numere big-endian, lungime totală și exact
nouă componente delimitate. V1–V4 rămân disponibile numai pentru citire și sunt
migrate după autentificare reușită. Formatul este descris în
`USER_FORMAT_V5.md`.

### 6. Testare automată de securitate — implementare finalizată 8 august 2026

- [x] Configurații separate Debug/Release cu AddressSanitizer și UndefinedBehaviorSanitizer pe Linux.
- [x] Testarea build-ului și a înlocuirii atomice pe Windows.
- [x] Adăugarea testelor de fuzzing pentru parsarea formatelor V1–V5, `PQCENC01/02` și `PQCDB002`.
- [x] Teste cu arhive mari, fișiere goale și limite maxime admise.
- [x] Integrarea testelor în CI și blocarea publicării când unul eșuează.
- [ ] După publicarea repository-ului: confirmarea primei rulări Windows și configurarea `Security gate` ca required status check în GitHub.

Preseturile CMake rulează toate cele 11 teste cu ASan/UBSan în Debug și Release.
Validatorul structural comun este folosit atât de aplicație, cât și de harness-ul
libFuzzer. O campanie locală de 20.000 de execuții a trecut fără constatări.
Workflow-ul `Security CI` adaugă joburi Linux, fuzzing și Windows, plus verificarea
agregată `Security gate`. Jobul Windows compilează în Release și rulează inclusiv
testul de înlocuire atomică; executarea lui efectivă trebuie confirmată de prima
rulare GitHub Actions, deoarece nu poate fi simulată integral pe gazda Linux locală.
Detaliile și comenzile sunt în `SECURITY_TESTING.md`.

## Curățenie și optimizări ulterioare

- [x] Arhivarea surselor necompilate `PasswordManager_old.cpp` și `PasswordManagerSecure.cpp` în `documentation/legacy/`, după confirmarea că nu au referințe și nu sunt necesare build-ului.
- [ ] Actualizarea documentației vechi care încă descrie Kyber, AES-CTR sau HMAC pentru arhive, deși implementarea curentă folosește ML-KEM și AES-GCM.
- [ ] Repararea linkurilor din `documentation/INDEX.md` care indică documente inexistente.
- [ ] Înlocuirea mesajelor foarte detaliate de diagnostic cu logging configurabil, fără informații sensibile.
- [ ] Implementarea opțională a drag-and-drop-ului marcat TODO în `ArchiveWindow.cpp`.
- [ ] Analizarea criptării în streaming pentru arhive mari, pentru a evita păstrarea simultană în RAM a arhivei serializate și a rezultatului criptat.

## Ordine recomandată pentru mâine

1. ~~Schimbarea tranzacțională a parolei master și testele de întrerupere.~~ Finalizat.
2. ~~Formatul și fluxul real de backup/restore.~~ Finalizat pentru baza de credențiale.
3. ~~Validarea numelor și căilor.~~ Finalizat.
4. ~~Rularea testelor existente plus sanitizers.~~ Finalizat.
5. Curățarea surselor vechi și actualizarea documentației.

## Criteriu de încheiere a zilei

- toate testele existente și cele noi trec;
- nicio eroare simulată nu produce un amestec de fișiere criptate cu parole diferite;
- importul nu modifică datele existente înainte ca backup-ul să fie autentificat integral;
- niciun nume furnizat de utilizator nu poate scrie sau extrage în afara directoarelor permise;
- documentația descrie exact formatele și algoritmii implementați.
