# Testare automată de securitate

## Acoperire

Suita conține 11 teste CTest. Pe lângă fluxurile criptografice și tranzacționale
existente, sunt verificate acum:

- parsarea structurală a formatelor de utilizator V1–V5;
- containerele `PQCENC01`, `PQCENC02` și `PQCDB002` trunchiate, supradimensionate
  sau cu date suplimentare;
- arhive cu fișiere goale și cu un fișier reprezentativ de 8 MiB;
- limite de 64 MiB pentru fișierul utilizatorului, 16 MiB per componentă,
  512 MiB per intrare de arhivă și 1 GiB per container criptat;
- scrierea și înlocuirea atomică, inclusiv erorile simulate înainte de publicare.

`FormatValidation` este folosit de fluxurile reale de încărcare înainte de
operațiile criptografice costisitoare și de același harness libFuzzer. Astfel,
fuzzing-ul nu testează o copie simplificată a parserului.

## Teste normale

Din rădăcina proiectului:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

## AddressSanitizer și UndefinedBehaviorSanitizer

Debug:

```bash
cmake --preset linux-sanitized-debug
cmake --build --preset linux-sanitized-debug
ctest --preset linux-sanitized-debug
```

Release:

```bash
cmake --preset linux-sanitized-release
cmake --build --preset linux-sanitized-release
ctest --preset linux-sanitized-release
```

Preseturile activează oprirea imediată la prima constatare ASan/UBSan și
detectarea scurgerilor de memorie.

## Fuzzing local

Este necesar Clang cu libFuzzer. Se folosește un corpus de lucru temporar pentru
ca libFuzzer să nu scrie cazurile descoperite peste corpusul inițial din proiect:

```bash
cmake --preset linux-fuzz
cmake --build --preset linux-fuzz
fuzz_work_corpus="$(mktemp -d)"
cp -R fuzz/corpus/. "$fuzz_work_corpus/"
build/linux-fuzz/format_parser_fuzz "$fuzz_work_corpus" \
  -runs=20000 -max_len=1048576 -timeout=10
```

Pentru o campanie mai lungă se elimină `-runs=20000` și se oprește manual
procesul. Orice crash este o eroare de securitate care trebuie rezolvată înainte
de publicare.

## CI și Windows

`.github/workflows/security-ci.yml` rulează la fiecare push și pull request:

1. toate testele Linux în Debug și Release cu ASan/UBSan;
2. 20.000 de execuții libFuzzer;
3. build Release Windows cu dependențe statice și toate testele CTest, inclusiv
   `atomic_file_integrity` care exercită `MoveFileExW`;
4. jobul agregat `Security gate`, care eșuează dacă oricare dintre celelalte
   joburi nu reușește.

În setările repository-ului GitHub, `Security gate` trebuie configurat ca
required status check pentru ramura protejată. Fără această regulă externă,
workflow-ul semnalează eroarea, dar GitHub poate permite în continuare merge-ul.

Prima execuție Windows trebuie urmărită în GitHub Actions; mediul Linux local nu
poate valida apelurile Win32 reale.
