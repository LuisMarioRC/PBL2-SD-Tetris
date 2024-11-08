#ifndef ACL345_H
#define ACL345_H

extern int mg_por_lsb; // Declare como extern
extern int offset_x;   // Declare como extern

// Prototipos das funções
void inicializar_i2c();
void ler_aceleracao_x();

#endif
