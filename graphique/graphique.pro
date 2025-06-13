QT += core gui charts sql

CONFIG += c++11

SOURCES += main.cpp

# Ajoutez les configurations de base de données ODBC
LIBS += -lodbc
