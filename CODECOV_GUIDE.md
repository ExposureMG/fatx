# Guide d'utilisation de Codecov pour FATX

## Configuration

Codecov est maintenant intégré au projet FATX avec les fonctionnalités suivantes :

### 1. Workflow GitHub Actions
Le fichier `.github/workflows/ci.yml` configure une matrice de compilation qui :
- Génère automatiquement des rapports de couverture avec `lcov` pour les builds Debug
- Téléverse les rapports vers Codecov avec différents flags selon le compilateur et le type de build
- Effectue des analyses statiques avec `cppcheck` et `clang-tidy`

### 2. CMake Configuration
- **Flags de couverture** : Ajout automatique de `-fprofile-arcs` et `-ftest-coverage` pour GCC/Clang en mode Debug
- **Cible `coverage`** : Génère un rapport HTML avec `genhtml` (nécessite `lcov`)
- **Tests unitaires** : Intégration de GoogleTest pour des tests ciblés

### 3. Fichier de configuration Codecov
Le fichier `.codecov.yml` définit :
- **Objectif de couverture** : 80% global, 75% pour les PRs
- **Fichiers ignorés** : Tests, fichiers générés, fichiers système
- **Annotations GitHub** : Commentaires automatiques sur les PRs

## Utilisation locale

### Générer un rapport de couverture

```bash
# Build en mode Debug
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# Exécuter les tests (génère les fichiers .gcda/.gcno)
ctest

# Générer le rapport de couverture
make coverage

# Le rapport HTML sera dans build/coverage-report/
```

### Nettoyer les rapports

```bash
make coverage-clean
```

## Utilisation CI/CD

### Workflow automatique
1. **Push/PR** sur les branches `main`, `master`, `develop`
2. **Matrice de compilation** : 5 configurations différentes
3. **Upload automatique** vers Codecov avec flags spécifiques
4. **Commentaires automatiques** sur les PRs avec comparaison de couverture

### Flags utilisés
- `gcc-11-Debug`
- `gcc-11-Release`
- `clang-14-Debug`
- `clang-14-Release`
- `gcc-11-Debug-sanitizer` (avec AddressSanitizer)

## Tests unitaires

### Structure des tests
```
tests/
└── test_validation.cpp  # Tests pour le système de validation FATX
```

### Ajouter de nouveaux tests

1. Créer un fichier `tests/test_nouvelle_fonctionnalité.cpp`
2. Ajouter le fichier à la cible `fatx_tests` dans `CMakeLists.txt`
3. Les tests seront automatiquement découverts par GoogleTest

Exemple :
```cpp
#include <gtest/gtest.h>

// Test fixture
class MonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configuration
    }
};

// Test case
TEST_F(MonTest, MonFonctionnalité) {
    // Test logic
    EXPECT_EQ(resultat_attendu, fonction_testée());
}
```

## Interprétation des résultats

### Rapports de couverture
- **Lignes couvertes** : Code exécuté pendant les tests
- **Branches couvertes** : Conditions testées (if/else, switch)
- **Fonctions couvertes** : Fonctions appelées pendant les tests

### Objectifs recommandés
- **Global** : > 80% de couverture
- **Nouvelles fonctionnalités** : > 90% de couverture
- **Code critique** : 100% de couverture

### Amélioration de la couverture
1. **Identifier le code non testé** dans les rapports HTML
2. **Ajouter des tests unitaires** pour les fonctions manquantes
3. **Créer des tests d'intégration** pour les chemins complexes
4. **Utiliser des mocks** pour isoler les unités de code

## Intégration avec l'IDE

### Visual Studio Code
- Extension "Coverage Gutters" pour voir la couverture en temps réel
- Configuration automatique des chemins d'include

### CLion
- Support natif des rapports de couverture LCOV
- Intégration avec GoogleTest

## Bonnes pratiques

1. **Tests avant le code** : Écrire les tests avant d'implémenter les fonctionnalités
2. **Couverture équilibrée** : Éviter la sur-ingénierie des tests
3. **Tests maintenables** : Utiliser des données de test réalistes
4. **Intégration continue** : S'assurer que tous les tests passent avant les merges

## Dépannage

### Problèmes courants

**Erreur : "lcov not found"**
```bash
sudo apt-get install lcov
```

**Erreur : "No coverage data found"**
- Vérifier que les tests sont exécutés avant la génération du rapport
- S'assurer que la compilation est en mode Debug avec les flags de couverture

**Erreur : "Token Codecov manquant"**
- Ajouter `CODECOV_TOKEN` aux secrets du repository GitHub
- Le token peut être obtenu sur https://codecov.io

### Debugging
```bash
# Exécuter un test spécifique avec coverage
cd build
ctest -R nom_du_test -V

# Voir les fichiers de couverture générés
find . -name "*.gcno" -o -name "*.gcda"
```
