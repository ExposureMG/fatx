La longue liste de warnings est utile, mais trop stricte par défaut augmente la friction. Tu as déjà -Werror conditionnel en Debug/CI (add_compile_options($<...:-Werror>)). C’est bien. Je suggère:
Conserver l’actuel en Debug/CI, mais assouplir certains flags au runtime (ex.: -Wconversion) si ça bloque des plateformes.

[API et types]
Pour device/I/O:
Utilise std::span<const std::byte> et std::span<std::byte> pour buffer I/O.
Introduis des types forts (Strong typedef) pour streamptr/filesize afin d’éviter confusion d’unités.
Envisage std::expected<T,E> (ou tl::expected) pour coder les erreurs au lieu de int, avec un enum pour les codes.

[Tests]
Actuellement enable_testing() + 
test.sh
 qui génère des add_test() dynamiques à partir d’un script. Original et fonctionnel.
Propose d’ajouter GoogleTest en complément pour des tests unitaires ciblés (conversion clusters, cache, sérialisation/desérialisation des headers) et garder 
test.sh
 pour les tests end-to-end.
Intégrer aux CI avec matrice gcc/clang Debug/Release et -fsanitize activable via -DSANITIZE=ON.

[CI et analyse statique]
GitHub Actions:
Build matrix ubuntu-latest: gcc-13/clang-18, Debug/Release.
Etapes: configure, build, ctest, upload artefacts (binaire fatx et pages man).
clang-tidy (profils modernes C++23) + cppcheck.
Option: codecov si tu ajoutes une couverture (gtest).

[Sécurité et validation]
Chemins et noms: centraliser la validation (taille name_size, caractères interdits) et s’y référer en 
fatx_create()
, 
fatx_rename()
, etc. aujourd’hui la logique est dispersée dans 
src/fatx.cpp
 (ex.: 
fatx_create()
 lignes 186–201, 209–219).
Pour unrm en export local, normaliser les chemins pour éviter .. et symlinks surprises.

[Documentation et UX]
doxygen_add_docs(doc src) est configuré. Ajoute des diagrammes (Graphviz) et un “How-to” + “Troubleshooting” dans 
README
.
CLI: offrir --json optionnel pour fsck.fatx, label.fatx, unrm.fatx afin d’être scriptable. Uniformiser --dry-run, --yes/--no, --verbose, --quiet entre alias.