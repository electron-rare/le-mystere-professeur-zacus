#!/bin/bash
# Script pour générer le rapport de couverture avec gcov/lcov

BUILD_DIR=.pio/build
COVERAGE_DIR=coverage

# Nettoyage
rm -rf $COVERAGE_DIR
mkdir $COVERAGE_DIR

# Compilation avec les flags de couverture
gcov_flags="-fprofile-arcs -ftest-coverage"

# Compilation (adapter selon votre environnement PlatformIO)
pio run -e esp32s3 --project-option="build_flags=$gcov_flags"

# Génération des fichiers gcov
gcov -o $BUILD_DIR $BUILD_DIR/**/*.gcda $BUILD_DIR/**/*.gcno

# Génération du rapport lcov
lcov --capture --directory $BUILD_DIR --output-file $COVERAGE_DIR/coverage.info
lcov --list $COVERAGE_DIR/coverage.info

# Génération du rapport HTML
genhtml $COVERAGE_DIR/coverage.info --output-directory $COVERAGE_DIR/html

echo "Rapport de couverture généré dans $COVERAGE_DIR/html"
