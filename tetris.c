#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>  // Para usleep()
#include "acl345.c"
#include <pthread.h>
#include "globals.h"
#include <sys/mman.h>
#include <fcntl.h>

// Dimensões da tela
#define SCREEN_X 320
#define SCREEN_Y 240

#define ROWS 31
#define COLS 14
#define TRUE 1
#define FALSE 0


#define PRETO   0
#define LARANJA 38
#define VERDE   56
#define AMARELO 63
#define ROXO    467
#define CIANO   504
#define BRANCO  511
#define INCOLOR 510
#define ROSA 342


// Declarações das funções assembly
extern void open_devmem();
extern void bg_block(int posicao, int cor);
extern void poligono(int cor, int tamanho, int forma, int endereco);
extern void bg_color(int cor);
extern void set_sprite(int offset, int px, int py, int sp);
extern void close_devmen();
extern int acess_btn();
extern void acess_display();

// Definições de endereços e registradores
char Table[ROWS][COLS] = {0};
int score = 0;
int linesCompleted = 0; // Contador de linhas completas
char GameOn = TRUE;
int fallDelay = 80000; // Tempo de atraso para a peça cair (em microssegundos)
time_t lastUpdate; // Temporizador para controlar a queda da peça

extern int offset_x;
static accelX ; // Variável global para armazenar o valor do eixo X do acelerômetro

// Estrutura para representar uma peça
typedef struct { 
    char **array;
    int width, row, col;
    int color; // Adiciona cor à estrutura da forma
} Shape;
Shape current;

// Array de peças
const Shape ShapesArray[7] = {
    {(char *[]){(char []){0,1,1},(char []){1,1,0}, (char []){0,0,0}}, 3}, // S_shape     
    {(char *[]){(char []){1,1,0},(char []){0,1,1}, (char []){0,0,0}}, 3}, // Z_shape     
    {(char *[]){(char []){0,1,0},(char []){1,1,1}, (char []){0,0,0}}, 3}, // T_shape     
    {(char *[]){(char []){0,0,1},(char []){1,1,1}, (char []){0,0,0}}, 3}, // L_shape     
    {(char *[]){(char []){1,0,0},(char []){1,1,1}, (char []){0,0,0}}, 3}, // ML_shape    
    {(char *[]){(char []){1,1},(char []){1,1}}, 2}, // SQ_shape
    {(char *[]){(char []){0,0,0,0}, (char []){1,1,1,1}, (char []){0,0,0,0}, (char []){0,0,0,0}}, 4} // R_shape
};


/**
 * Função bg_desable para desabilitar o background
 */
void bg_desable(){
    int num_colunas = 80;
    int num_linhas = 4800;
    int c;
    bg_color(PRETO);
    for (c=0; c < 16; c++) {
        poligono(0, 0, 0, c);
    }
    for(c=0; c < 4800; c++){
        bg_block(c, 510);
    }
    
}


/**
 * Função DrawShape para desenhar uma peça
 * Recebe uma peça e desenha na tela
 * Percorre a matriz da peça e desenha um quadrado para cada valor 1
 */
void DrawShape(Shape shape) {
    int leftBorderX = 110;  // Coord. x da borda lateral esquerda
    int topBorderY = 18;    // Coord. y da borda superior
    int num_colunas = 80;   // Número de colunas na tela

    for (int i = 0; i < shape.width; i++) {
        for (int j = 0; j < shape.width; j++) {
            // Verifica se o bloco está preenchido na matriz da peça
            if (shape.array[i][j]) {
                // Calcula a posição linear do bloco (quadrado)
                int posicao = (topBorderY + shape.row + i) * num_colunas + (leftBorderX + shape.col + j);

                // Chama a função assembly para desenhar o bloco com a cor especificada em `shape.color`
                bg_block(posicao, shape.color);
            }
        }
    }
}


// Array de cores para as peças
const int ShapeColors[7] = {
    LARANJA,    // S_shape
    VERDE,  // Z_shape
    CIANO,   // T_shape
    BRANCO, // L_shape
    ROXO,   // ML_shape
    AMARELO, // SQ_shape
    ROSA    // R_shape
};

/**
 * Função GetShapeColor para obter a cor de uma peça
 * Recebe o índice da peça e retorna a cor correspondente
 * Se o índice for inválido, retorna a cor padrão
 */
int GetShapeColor(int shapeIndex) {
    if (shapeIndex >= 0 && shapeIndex < 7) {
        return ShapeColors[shapeIndex];
    }
    return BRANCO; // Cor padrão
}

/**
 * Função CopyShape para copiar uma peça
 * Aloca memória para a nova peça e copia os valores da peça original
 * Retorna a nova peça
 */
Shape CopyShape(Shape shape){
    Shape new_shape = shape;
    new_shape.array = (char**)malloc(new_shape.width * sizeof(char*));
    for(int i = 0; i < new_shape.width; i++){
        new_shape.array[i] = (char*)malloc(new_shape.width * sizeof(char));
        for(int j = 0; j < new_shape.width; j++) {
            new_shape.array[i][j] = shape.array[i][j];
        }
    }
    return new_shape;
}

/**
 * Função DeleteShape para deletar uma peça
 * Percorre a matriz da peça e libera a memória alocada
 */
void DeleteShape(Shape shape) {
    for(int i = 0; i < shape.width; i++) {
        free(shape.array[i]);
    }
    free(shape.array);
}

/**
 * Função CheckPosition para verificar se a posição da peça é válida
 * Percorre a matriz da peça e verifica se a posição é válida
 * Retorna TRUE se a posição for válida e FALSE caso contrário 
 */
int CheckPosition(Shape shape) { 
    char **array = shape.array;
    for(int i = 0; i < shape.width; i++) {
        for(int j = 0; j < shape.width; j++) {
            if(array[i][j]) {
                if(shape.col + j < 0 || shape.col + j >= COLS || shape.row + i >= ROWS) {
                    return FALSE; // Fora dos limites
                }
                if(shape.row + i >= 0 && Table[shape.row + i][shape.col + j]) {
                    return FALSE; // Colisão com outra peça
                }
            }
        }
    }
    return TRUE; // Posição válida
}

/**
 * Função GetNewShape para obter uma nova peça
 * Gera um número aleatório entre 0 e 6 e copia a peça
 * correspondente para a variável global current
 * A função também centraliza a peça na matriz
 * 
 */
void GetNewShape() { 
    int shapeIndex = rand() % 7; // Gera um número aleatório entre 0 e 6
    current = CopyShape(ShapesArray[shapeIndex]);
    current.col = COLS / 2 - (current.width / 2); // Centraliza a peça na matriz
    current.row = 0; // Começa na linha 0
    current.color = GetShapeColor(shapeIndex); 
    if (!CheckPosition(current)) {
        GameOn = FALSE; // Finaliza o jogo
    }
}

/**
 * Função writeTable para escrever a peça atual na matriz
 * Percorre a matriz da peça atual e escreve os valores na matriz
 * principal
 * 
 */
void WriteToTable() {
    for(int i = 0; i < current.width; i++) {
        for(int j = 0; j < current.width; j++) {
            if(current.array[i][j])
                Table[current.row + i][current.col + j] = current.array[i][j];
        }
    }
}

/**
 * Função para verificar se uma linha foi completada
 * Percorre a matriz e verifica se uma linha foi completada
 * Se uma linha foi completada, ela é removida e as linhas acima
 * são movidas para baixo
 * A função também incrementa a pontuação
 * 
 */
void Check_lines() { 
    int count = 0;
    for(int i = 0; i < ROWS; i++) {
        int sum = 0;
        for(int j = 0; j < COLS; j++) {
            sum += Table[i][j];
        }
        if(sum == COLS) {
            count++;
            linesCompleted++;
            for(int k = i; k >= 1; k--)
                for(int l = 0; l < COLS; l++)
                    Table[k][l] = Table[k - 1][l];
            for(int l = 0; l < COLS; l++)
                Table[0][l] = 0;
        }
    }
    score += 100 * count; // Adiciona 100 pontos por linha completa
    if (linesCompleted % 5 == 0) {
        fallDelay = fallDelay > 1000 ? fallDelay - 1000 : fallDelay; // Reduz o delay até um mínimo de 1ms
    }

}

/**
 * Função ClearCenterArea para limpar a área central
 * Percorre a área central da tela e apaga os blocos
 * preenchidos, usando a função bg_block() da biblioteca de vídeo
 */
void ClearCenterArea() {
    int leftBorderX = 110;  // Coord. x da borda lateral esquerda
    int topBorderY = 17;    // Coord. y da borda superior
    int num_colunas = 80;   // Número de colunas na tela
    int num_linhas = 60;    // Número de linhas na tela

    // Define o tamanho da área a ser apagada (3x3)
    for (int i = 1; i <= 31; i++) {  // Altera para 31 para apagar mais 2 blocos para baixo
        for (int j = 0; j <= 13; j++) {
            int posicao = (topBorderY + i) * num_colunas + (leftBorderX + j);
            bg_block(posicao, 510);  // Cor de fundo
        }
    }
}

/**
 * Função PrintTable para desenhar a matriz na tela
 * Percorre a matriz e desenha os blocos preenchidos quando chegar ao fim
 * desenha a próxima peça no topo da tela
 * usando a função bg_block() da biblioteca de vídeo em assembly
 */
void PrintTable() {
    ClearCenterArea();
    int leftBorderX = 110;  // Coord. x da borda lateral esquerda
    int topBorderY = 18;    // Coord. y da borda superior
    int num_colunas = 80;   // Número de colunas na tela
    int num_linhas = 60;    // Número de linhas na tela

    // Desenha apenas as bordas do tabuleiro (com 31 linhas e 14 colunas de blocos)
    for (int i = 0; i < 31; i++) {  // Itera pelas linhas
        for (int j = 0; j < 14; j++) {  // Itera pelas colunas
            // Calcular a posição do bloco (quadrado)
            int posicao = (topBorderY + i) * num_colunas + (leftBorderX + j);

            // Desenha as bordas:
            // Desenha a linha superior (i == 0)
            // Desenha a linha inferior (i == 30)
            // Desenha a coluna esquerda (j == 0)
            // Desenha a coluna direita (j == 13)
            if (Table[i][j]) {
                bg_block(posicao, BRANCO); // Blocos preenchidos
            } 
        }
    }
    DrawShape(current);
}

/**
 * Função table para desenhar o tabuleiro inicial na tela
 * Desenha as bordas do tabuleiro na tela
 * Usa a função bg_block() da biblioteca de vídeo em assembly
 */
void table() {
    int leftBorderX = 109;  // Nova coordenada x da borda lateral esquerda para expandir a matriz
    int topBorderY = 17;    // Nova coordenada y da borda superior para expandir a matriz
    int num_colunas = 80;   // Número de colunas na tela
    int num_linhas = 60;    // Número de linhas na tela

    // Desenha bordas do tabuleiro com um centro expandido (33 linhas e 16 colunas de blocos)
    for (int i = 0; i < 33; i++) {  // Itera pelas linhas, aumentando o centro em 1 linha em cada direção
        for (int j = 0; j < 16; j++) {  // Itera pelas colunas, aumentando o centro em 1 coluna em cada direção
            // Calcula a posição do bloco (quadrado)
            int posicao = (topBorderY + i) * num_colunas + (leftBorderX + j);

            // Desenha as bordas expandidas:
            // Desenha a linha superior (i == 0)
            // Desenha a linha inferior (i == 32)
            // Desenha a coluna esquerda (j == 0)
            // Desenha a coluna direita (j == 15)
            if (i == 0 || i == 32 || j == 0 || j == 15) {
                bg_block(posicao, ROSA);  // Cor das bordas
            }
        }
    }
}



/**
 * Função para ler o acelerômetro
 * A função lê o valor do eixo X do acelerômetro e armazena na variável global
 * accelX. O valor é lido a cada 100ms
 */
void* ReadAccelerometer(void* arg) {
    while (GameOn) {
        accelX = get_direcao_movimento(); // Atualiza o valor do eixo X
        printf("%f",accelX);
        usleep(100000); // Ajuste o intervalo de leitura conforme necessário
    }
    return NULL;
}


extern void escrever_registro();

/**
 * Função RotateShape para rotacionar uma peça
 * Recebe uma peça e retorna a peça rotacionada 90 graus
 * A função aloca memória para a nova peça e copia os valores
 * da peça original, rotacionando 90 graus
 * Retorna a peça rotacionada
 */
Shape RotateShape(Shape shape) {
    Shape rotated;
    rotated.width = shape.width;
    rotated.array = (char **)malloc(rotated.width * sizeof(char *));
    for (int i = 0; i < rotated.width; i++) {
        rotated.array[i] = (char *)malloc(rotated.width * sizeof(char));
        for (int j = 0; j < rotated.width; j++) {
            rotated.array[i][j] = shape.array[shape.width - j - 1][i]; // Rotação 90 graus
        }
    }
    rotated.row = shape.row; // Mantém a linha atual
    rotated.col = shape.col; // Mantém a coluna atual
    rotated.color = shape.color; // Mantém a cor da peça original
    usleep(100000); 
    return rotated;
}

/**
 * Função para alternar o estado de pausa do jogo
 * A função alterna o estado de pausa
 */
int isPaused = 0;

void TogglePause() {
    isPaused = !isPaused;  // Alterna o estado de pausa
    if (isPaused) {
        printf("Jogo pausado\n");
    } else {
        printf("Jogo despausado\n");
    }
}

/**
 * Função para pausar o jogo
 * A função pausa o jogo
 */
void *PauseGame(void *arg) {
    int btn;
    while (1) {
        btn = button();
        if (btn == 2) {  // Pressione o botão 2 para alternar o estado de pausa
            TogglePause();
            usleep(200000);  // Evita múltiplos registros da mesma pressão
        }
        usleep(100000);  // Pequeno atraso para não sobrecarregar a CPU
    }
}

/**
 * Função button para ler o estado do botão
 * A função lê o estado do botão e retorna o valor do botão pressionado
 * Retorna 1 se o botão 1 foi pressionado
 * Retorna 2 se o botão 2 foi pressionado
 * Retorna NULL se nenhum botão foi pressionado
 */
int button() {
    switch (acess_btn())
    {
    case 0b1110:
        return 1;
    case 0b1101:
        return 2;
    default:
        return NULL;
    }
}


/**
 * Função principal do jogo Tetris
 * Inicializa o vídeo, abre o dispositivo de botão e configura o acelerômetro
 * Cria uma thread para ler o acelerômetro
 * Gera a primeira peça e entra em um loop principal
 * O loop principal controla a queda da peça, a movimentação horizontal
 * baseada no acelerômetro e a rotação da peça
 * O loop também verifica se o jogo acabou, pausa o jogo e atualiza a pontuação
 * 
 */

int main() {
    open_devmem();
    bg_desable();
    configura_acelerometro();
    
    pthread_t accelThread;   // Thread do acelerômetro
    pthread_t pauseThread;   // Thread de pausa

    pthread_create(&accelThread, NULL, ReadAccelerometer, NULL);  // Cria a thread do acelerômetro
    pthread_create(&pauseThread, NULL, PauseGame, NULL);          // Cria a thread para alternar pausa

    srand(time(NULL));
    GetNewShape();  // Gera a primeira forma
    table();

    while (GameOn) {
        lastUpdate = time(NULL);

        while (GameOn) {
            // Verifica se o jogo está pausado e pula a lógica do jogo enquanto estiver pausado
            if (isPaused) {
                usleep(100000);  // Reduz o uso da CPU enquanto o jogo está pausado
                continue;
            }

            Shape temp = CopyShape(current);  // Cria uma cópia da peça atual
            int btn = button();

            // Verifica se o botão 0 está pressionado para rotacionar a peça
            if (btn == 1) { 
                Shape rotated = RotateShape(current);
                if (CheckPosition(rotated)) {
                    DeleteShape(current);
                    current = rotated;
                } else {
                    DeleteShape(rotated);
                }
            }

            // Movimentação horizontal baseada no acelerômetro
            if (accelX == -1) { // Limite negativo para mover à esquerda
                temp.col--;
                if (CheckPosition(temp)) {
                    current.col--;
                }
            } else if (accelX == 1) { // Limite positivo para mover à direita
                temp.col++;
                if (CheckPosition(temp)) {
                    current.col++;
                }
            }

            // Exemplo de exibição de pontuação e linhas (comentado)
            char scoreText[20];
            sprintf(scoreText, "Score: %d\n", score);
            printf(scoreText);

            char linesText[30];
            sprintf(linesText, "Linhas: %d\n", linesCompleted);
            printf(linesText);

            usleep(fallDelay);  // Aguarda o tempo de queda da peça
            if (time(NULL) - lastUpdate >= 1) {  // Atualiza a cada segundo
                current.row++;
                if (!CheckPosition(current)) {
                    current.row--;
                    WriteToTable();
                    Check_lines();
                    GetNewShape();
                }
                PrintTable();
            }
        }
    }
    // Encerra o jogo
    pthread_cancel(accelThread);
    pthread_cancel(pauseThread);
    // Encerra a thread do acelerômetro
    pthread_join(accelThread, NULL);
    pthread_join(pauseThread, NULL);
    return 0;
}