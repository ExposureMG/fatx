[Standard C++ et CMake]
Dans 
CMakeLists.txt
: 
set(CMAKE_CXX_STANDARD 26)
 est OK, mais préfère target_compile_features(fatx PRIVATE cxx_std_23) pour mieux lier la contrainte au binaire et éviter les effets globaux.
Eviter include_directories(src)/link_directories(...). Préfère des portées cibles:
target_include_directories(fatx PRIVATE src)
Conserve target_link_libraries(fatx PRIVATE Boost::program_options FUSE3::fuse3).
La longue liste de warnings est utile, mais trop stricte par défaut augmente la friction. Tu as déjà -Werror conditionnel en Debug/CI (add_compile_options($<...:-Werror>)). C’est bien. Je suggère:
Conserver l’actuel en Debug/CI, mais assouplir certains flags au runtime (ex.: -Wconversion) si ça bloque des plateformes.

[FUSE callbacks]
Les signatures dans 
src/fatx.cpp
 sont conformes FUSE3 (ex. 
fatx_readdir()
 prend fuse_readdir_flags, getattr() prend fuse_file_info*, etc.). Bon point.
Assure-toi que chaque callback:
Renseigne bien errno via codes négatifs (tu le fais: ex. -ENOENT).
Verrouille en lecture/écriture les zones critiques. Exemples:
fatx_getattr()
 utilise f->mux_E.lock_shared()/unlock_shared() — ok.
fatx_truncate()
 utilise lock()/unlock() — ok mais voir note thread-safety ci-dessous.
[Thread-safety et verrous]
Dans 
src/fatx.cpp
, l’usage direct lock()/unlock() sur f->mux_E est correct mais fragile.
Recommande d’utiliser std::shared_lock et std::unique_lock (RAII) pour tous les accès, p.ex. 
fatx_getattr()
 et 
fatx_truncate()
.
Centraliser des helpers RAII si entry expose plusieurs mutex (lecture/écriture, FAT/cache, etc.).
Dans 
src/fatx.hpp
 (parties non affichées mais référencées dans ta note), applique systématiquement [[nodiscard]] aux méthodes retournant un statut (ex.: entry::find(), opérations retournant int).
Ajoute noexcept sur les accesseurs et helpers qui ne peuvent pas lancer d’exception (ex.: conversions dans clsarithm).
[Logging]
Aujourd’hui, dbglog(...) et console::write(...) sont omniprésents (
src/fatx.cpp
), avec format type fmt/std::format.
Recommande d’introduire spdlog:
Niveaux: trace/debug/info/warn/error.
Horodatage, thread-safe, rotation fichiers.
Mapper les flags DBG_* de 
CMakeLists.txt
 vers des niveaux spdlog (et un --verbose en CLI).
Conserver console::write(...) comme couche d’adaptation si tu veux limiter le diff initial.

[API et types]
Pour device/I/O:
Utilise std::span<const std::byte> et std::span<std::byte> pour buffer I/O.
Introduis des types forts (Strong typedef) pour streamptr/filesize afin d’éviter confusion d’unités.
Envisage std::expected<T,E> (ou tl::expected) pour coder les erreurs au lieu de int, avec un enum pour les codes.
[Statfs values]
Dans 
fatx_statfs()
 (
src/fatx.cpp
):
sfs->f_bsize = 1 et sfs->f_frsize = 1 sont atypiques et peuvent perturber certains outils. Préfère:
f_bsize = blksize (ou 
fatx_context::get()->par.clus_size
)
f_frsize = fatx_context::get()->par.clus_size
Les totaux/free en bytes semblent calculés. Vérifie cohérence avec bsize/frsize (sinon les utilitaires calculent mal l’espace).
[Structure des sources]
Tu as déjà un découpage logique dans 
CMakeLists.txt
 (ajout de 
device.cpp
, 
frontend.cpp
, 
partition.cpp
, 
entry.cpp
, 
diskmap.cpp
, 
utils.cpp
). Bon début.
Pousse le découpage:
Séparer les callbacks FUSE dans src/fuse_ops.cpp (isoler l’interface système).
Garder 
fatx_context
 minimal dans 
fatx.hpp
 et déplacer les blocs volumineux en headers dédiés pour accélérer la recompilation.
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
Points concrets à corriger rapidement
[Include FUSE3] dans 
src/fatx.cpp
: remplacer #include <fuse.h> par #include <fuse3/fuse.h> pour coller à find_package(FUSE3 ...).
[statfs] dans 
fatx_statfs()
:
Renseigner f_bsize/f_frsize avec des valeurs cohérentes (cluster size).
[RAII locks] remplacer lock()/unlock() par std::unique_lock/std::shared_lock dans getattr(), truncate(), chmod(), utimens(), etc.
[CMake cible] lier le standard avec target_compile_features et déplacer les include/link au niveau cible.
Plan proposé
Étape 1: CMake + FUSE3 header + statfs fixes.
Étape 2: RAII locks + attributs [[nodiscard]]/noexcept sur les fonctions sûres.
Étape 3: Introduire spdlog (derrière console::write() au début) + mapping des flags de debug.
Étape 4: Ajout GoogleTest sur 2–3 domaines critiques, intégration CI + sanitizers.
Étape 5: Refactoring fuse_ops.cpp et durcissement API device avec span et types forts.
TODO en cours
[in_progress] Moderniser CMake (standard via target features, FUSE3, options) [cmake-modernize]
[pending] Thread-safety (RAII locks, [[nodiscard]], noexcept) [thread-safety]
[pending] Logging structuré (spdlog) [logging-spdlog]
[pending] Scinder sources (incl. fuse_ops.cpp) [split-sources]
[pending] Tests unitaires (GoogleTest) [unittests-gtest]
[pending] CI + clang-tidy + sanitizers [ci-setup]
[pending] API durcie (span, types forts) [api-hardening]
[pending] Docs (How-to, Troubleshooting, Doxygen) [docs-update]
Suite
Veux-tu que je prépare un PR initial focalisé sur:

Mise à jour 
CMakeLists.txt
 (standard via target_compile_features, includes/links par cible, conservation de ta matrice de warnings),
Migration #include <fuse3/fuse.h>,
Correction 
fatx_statfs()
 pour f_bsize/f_frsize,
Remplacement de quelques lock()/unlock() par RAII sur 3–4 points sensibles,
Squelette GoogleTest minimal branché dans ctest ?
Dis-moi si tu veux prioriser un autre aspect (ex.: logging spdlog d’abord).

Résumé: analyse effectuée et plan proposé; prêt à implémenter un PR étape 1.