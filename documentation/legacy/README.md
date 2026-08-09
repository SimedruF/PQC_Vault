# Surse istorice necompilate

Fișierele din acest director sunt păstrate exclusiv pentru referință istorică.
Nu sunt incluse de CMake și nu trebuie copiate înapoi în `src/`:

- `PasswordManager_old.cpp.txt` folosește vechiul format Kyber/XOR;
- `PasswordManagerSecure.cpp.txt` este o versiune intermediară, anterioară
  formatului portabil V5 și corecțiilor tranzacționale curente.

Implementarea activă este `src/PasswordManager.cpp`. Compatibilitatea V1–V4 și
migrarea spre V5 sunt implementate și testate acolo.
