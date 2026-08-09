# Validarea numelor și căilor

Implementarea centrală se află în src/PathSecurity.h și
src/PathSecurity.cpp. Regulile sunt aplicate în backend; verificările din
interfața ImGui sunt numai pentru feedback imediat și nu reprezintă limita de
securitate.

## Nume acceptate

Limitele sunt măsurate în octeți UTF-8:

- utilizator: maximum 64;
- arhivă: maximum 128;
- fișier stocat într-o arhivă: maximum 255.

Sunt respinse:

- valorile goale, punctul singular și orice apariție dublă de punct;
- separatorii slash și backslash și caracterele neportabile pentru nume;
- caracterele ASCII de control și secvențele UTF-8 invalide;
- caracterele Unicode invizibile, controalele bidirecționale, separatorii
  Unicode asemănători cu slash/backslash și formele cu combining marks;
- spațiile de la început/sfârșit, punctul final și numele rezervate de Windows
  precum CON, NUL, COM1 sau LPT1.

Unicode UTF-8 vizibil este permis. Formele cu combining marks sunt respinse
deoarece proiectul nu depinde de o bibliotecă de normalizare NFC; astfel sunt
evitate nume vizual identice cu reprezentări diferite.

Coliziunile ASCII fără diferențiere între majuscule și minuscule sunt respinse
pentru utilizatori, arhive și fișierele din arhive. Comportamentul rămâne astfel
același pe Linux și Windows.

## Izolarea căilor

ResolveContainedPath():

1. acceptă numai o cale relativă;
2. respinge componentele punct și punct-punct;
3. canonizează directorul permis și destinația cu
   std::filesystem::weakly_canonical;
4. compară componentele de cale, nu prefixe textuale;
5. respinge un director-rădăcină care este symlink și orice symlink intern care
   ar conduce în afara rădăcinii.

UserFilePath(), UserDatabasePath() și ArchiveFilePath() folosesc această
funcție pentru a păstra fișierele contului și bazei de date în users/, iar
arhivele în archives/.

## Extragerea

Numele intern este verificat atât la adăugare, cât și la deserializarea unei
arhive. Dacă destinația aleasă este un director, numele este adăugat numai prin
ResolveExtractionPath(), după verificarea canonică a încadrării. Dacă
utilizatorul alege explicit un nume complet de destinație, părintele acelei căi
devine directorul permis pentru operația respectivă.

Fișierul este publicat în continuare prin AtomicFile::Write().

## Testare

Testul path_validation_security acoperă:

- traversal relativ și căi absolute;
- escape prin symlink;
- separatori și caractere de control;
- UTF-8 invalid, control bidirecțional și forme Unicode descompuse;
- limite de lungime și coliziuni de majuscule/minuscule;
- respingerea numelor periculoase inclusiv la deserializarea arhivelor vechi;
- extragerea efectivă numai în directorul selectat.

