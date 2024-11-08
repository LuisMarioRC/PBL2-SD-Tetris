#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "globals.h"
/**
 * Definições de endereços base
 */
#define HPS_PHYS_BASE 0xFF000000
#define HPS_SPAN 0x01000000   
#define SYSMGR_BASE 0xFFD08000
#define I2C0_BASE 0xFFC04000 

/**
 * Definições de registradores do módulo SYSMGR
 */
#define SYSMGR_I2C0USEFPGA (SYSMGR_BASE + 0x704)
#define SYSMGR_GENERALIO7 (SYSMGR_BASE + 0x49C)
#define SYSMGR_GENERALIO8 (SYSMGR_BASE + 0x4A0)

/**
 * Definições de registradores do módulo I2C
 */
#define I2C0_CON (I2C0_BASE + 0x00) 
#define I2C0_TAR (I2C0_BASE + 0x04)
#define I2C0_DATA_CMD (I2C0_BASE + 0x10)
#define I2C0_FS_SCL_HCNT (I2C0_BASE + 0x1C)
#define I2C0_FS_SCL_LCNT (I2C0_BASE + 0x20)
#define I2C0_ENABLE (I2C0_BASE + 0x6C) 
#define I2C0_RXFLR (I2C0_BASE + 0x78)   
#define I2C0_TXFLR (I2C0_BASE + 0x74)
#define I2C0_CLR_INTR (I2C0_BASE + 0x40)



/**
 * Definições de registradores do acelerômetro ADXL345
 */
#define ADXL345_ADDR 0x53   
#define ADXL345_DEVID 0x00 
#define ADXL345_POWER_CTL 0x2D
#define ADXL345_DATA_FORMAT 0x31
#define ADXL345_DATAX0 0x32 
#define ADXL345_DATAX1 0x33 
#define ADXL345_INT_ENABLE 0x2E
#define ADXL345_INT_MAP 0x2F
#define ADXL345_INT_SOURCE 0x30
#define ADXL345_BW_RATE 0x2C

#define AMOSTRAS_CALIBRACAO 100
#define FILTRO_MOVIMENTO 0.50

/**
 * Variáveis globais
 * base_hps - ponteiro para o endereço base do HPS
 * mg_por_lsb - miligraus por LSB
 * offset_x - offset do eixo X
 * fd - descritor do arquivo
 */
volatile uint32_t *base_hps;
int mg_por_lsb = 4; 
int offset_x = 0;
int fd;



/**
 * Função escrever_registro - escreve um valor em um registrador do HPS
 */
void escrever_registro(uint32_t endereco, uint32_t valor) {
   *(volatile uint32_t*)(base_hps + (endereco - HPS_PHYS_BASE) / 4) = valor;
}



/**
 * Função ler_registro - lê um registrador do HPS
 */
uint32_t ler_registro(uint32_t endereco) {
   return *(volatile uint32_t*)(base_hps + (endereco - HPS_PHYS_BASE) / 4);
}

/**
 * Função inicializar_i2c - inicializa o módulo I2C
 * @return void
 */
void inicializar_i2c() {
   escrever_registro(SYSMGR_I2C0USEFPGA, 0);
   escrever_registro(SYSMGR_GENERALIO7, 1);
   escrever_registro(SYSMGR_GENERALIO8, 1);




   escrever_registro(I2C0_ENABLE, 0x0);
   escrever_registro(I2C0_CON, 0x65);
   escrever_registro(I2C0_TAR, ADXL345_ADDR);


   escrever_registro(I2C0_FS_SCL_HCNT, 60 + 30);
   escrever_registro(I2C0_FS_SCL_LCNT, 130 + 30);
  
   escrever_registro(I2C0_ENABLE, 0x1);


   usleep(10000);
}

/**
 * Função ADXL345_REG_READ - lê um registrador do acelerômetro ADXL345
 * @param address - endereço do registrador a ser lido
 * @param value - ponteiro para a variável que armazenará o valor lido
 */
void ADXL345_REG_READ(uint8_t address, uint8_t *value) {
   // Send reg address (+0x400 to send START signal)
   escrever_registro(I2C0_DATA_CMD, address + 0x400);
   // Send read signal
   escrever_registro(I2C0_DATA_CMD, 0x100);
   // Read the response (first wait until RX buffer contains data)
   while (ler_registro(I2C0_RXFLR) == 0) {}
   *value = ler_registro(I2C0_DATA_CMD) & 0xFF;
}

/**
 * Função ADXL345_REG_WRITE - escreve um valor em um registrador do acelerômetro ADXL345
 * @param address - endereço do registrador a ser escrito
 * @param value - valor a ser escrito no registrador
 */
void ADXL345_REG_WRITE(uint8_t address, uint8_t value) {
   // Send reg address (+0x400 to send START signal)
   escrever_registro(I2C0_DATA_CMD, address + 0x400);
   // Send value
   escrever_registro(I2C0_DATA_CMD, value);
}

/**
 * Função escrever_i2c - escreve um valor em um registrador do acelerômetro ADXL345
 * @param endereco_reg - endereço do registrador a ser escrito
 * @param valor - valor a ser escrito no registrador
 */
void escrever_i2c(uint8_t endereco_reg, uint8_t valor) {
   ADXL345_REG_WRITE(endereco_reg, valor);
}



/**
 * Função ler_i2c - lê um registrador do acelerômetro ADXL345
 * @param endereco_reg - endereço do registrador a ser lido
 * @return valor do registrador
 */
uint8_t ler_i2c(uint8_t endereco_reg) {
   uint8_t valor;
   ADXL345_REG_READ(endereco_reg, &valor);
   return valor;
}

/**
 * Função inicializar_adxl345 - inicializa o acelerômetro ADXL345
 * @return void
 */
void inicializar_adxl345() {
   escrever_i2c(ADXL345_POWER_CTL, 0x00);
   usleep(10000);
   escrever_i2c(ADXL345_POWER_CTL, 0x08);
   escrever_i2c(ADXL345_DATA_FORMAT, 0x00); // ±2g
   escrever_i2c(ADXL345_BW_RATE, 0x0A);   // 100 Hz
   escrever_i2c(ADXL345_INT_ENABLE, 0x80);
   escrever_i2c(ADXL345_INT_MAP, 0x00);
}

uint8_t ler_adxl345_devid() {
   return ler_i2c(ADXL345_DEVID);
}

/**
 * Função ler_aceleracao_x - lê a aceleração no eixo X
 * @param x - ponteiro para a variável que armazenará a aceleração
 * @return void
 */
void ler_aceleracao_x(int16_t *x) {
   while(!dados_prontos());
   *x = (ler_i2c(ADXL345_DATAX1) << 8) | ler_i2c(ADXL345_DATAX0);
   ler_i2c(ADXL345_INT_SOURCE);
}


int dados_prontos() {
   return (ler_i2c(ADXL345_INT_SOURCE) & 0x80) != 0;
}

/**
 * Função verificar_status_i2c - verifica se o módulo I2C está habilitado
 * @return void
 */
void verificar_status_i2c() {
   uint32_t status = ler_registro(I2C0_ENABLE);
   printf("Status I2C: 0x%08X\n", status);
   if (status & 0x1) {
       printf("I2C esta habilitado\n");
   } else {
       printf("I2C nao esta habilitado\n");
   }
}

/**
 * Função calibrar_acelerometro - calibra o offset do eixo X
 * @param offset_x - ponteiro para a variável que armazenará o offset do eixo X
 * @return void
 */
void calibrar_acelerometro(int16_t *offset_x) {
   int32_t soma_x = 0;
   int16_t x;
   int i;
 
   for (i = 0; i < AMOSTRAS_CALIBRACAO; i++) {
       ler_aceleracao_x(&x);
       soma_x += x;
   }
   *offset_x = soma_x / AMOSTRAS_CALIBRACAO;
   printf("Calibracao completa. Offset: X=%d \n", *offset_x);
}

/**
 * Função configura_acelerometro - inicializa o acelerômetro ADXL345
 * e calibra o offset do eixo X
 */
int configura_acelerometro(){
   uint8_t devid;

   fd = open("/dev/mem", O_RDWR | O_SYNC);
   if (fd < 0) {
       printf("Erro ao abrir /dev/mem\n");
       return -1;
   }

   base_hps = mmap(NULL, HPS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_PHYS_BASE);
   if (base_hps == MAP_FAILED) {
       printf("Erro ao mapear memoria\n");
       close(fd);
       return -1;
   }

   inicializar_i2c();
   verificar_status_i2c();

   int i;
   for (i = 0; i < 5; i++) {
       devid = ler_adxl345_devid();
       printf("Tentativa %d - ADXL345 DEVID: 0x%02X\n", i+1, devid);
       if (devid == 0xE5) {
           break;
       }
       usleep(100000);
   }

   inicializar_adxl345();
   printf("ADXL345 inicializado\n");

   printf("Calibrando acelerometro...\n");
   calibrar_acelerometro(&offset_x);
   printf("Calibrado! Iniciando leituras de aceleracao...\n");

   return 0;
}

/**
 * Função get_direcao_movimento - retorna a direção do movimento do acelerômetro
 * 1 - Direita
 * -1 - Esquerda
 * 0 - Sem movimento
 */
int get_direcao_movimento(){
   int16_t x_bruto;
   float x_g;

   ler_aceleracao_x(&x_bruto);

   x_g = (x_bruto - offset_x) * (mg_por_lsb / 1000.0);
   //printf("X lido: %d, X lapidado: %.2f g\n", x_bruto, x_g);

      if (x_bruto > 50) {
         //printf("Direita\n");
         return 1;
      } else if (x_bruto < -50) {
         //printf("Esquerda\n");
         return -1;
      } else {
         //printf("Sem movimento\n");
         return 0;
      }
   return 0; // sem movimento
}

int desmapear(){
   munmap((void*)base_hps, HPS_SPAN);
   close(fd);
   return 0;
}