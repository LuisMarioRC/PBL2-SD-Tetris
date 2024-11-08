.global open_devmem
.type open_devmem, %function
.global close_devmen
.type close_devmen, %function
.global bg_block
.type bg_block, %function
.global poligono
.type poligono, %function
.global check_fifo
.type check_fifo, %function
.global bg_color
.type bg_color, %function
.global set_sprite
.type set_sprite, %function
.global acess_btn
.type acess_btn, %function
.global acess_display
.type acess_display, %function

@ Constantes
.equ data_a, 0x80
.equ data_b, 0x70
.equ wrreg, 0xc0

open_devmem:
    @ Abrir /dev/mem

    @reservando na pilha os resgitradores que vai usar
    sub sp, sp, #28
    str r1, [sp, #24]
    str r2, [sp, #20]
    str r3, [sp, #16]
    str r4, [sp, #12]
    str r5, [sp, #8]
    str r7, [sp, #4]
    str r0, [sp, #0]


    ldr r0, =devmem
    mov r1, #2              @ Leitura e escrita
    mov r2, #0              @ Modo de arquivo
    mov r7, #5              @ syscall: open
    svc 0                   @ Chamada de sistema

    mov r4, r0              @ fd
    mov r0, #0              @ Endereço base
    mov r1, #4096           @ Tamanho da página
    mov r2, #3              @ Leitura e escrita
    mov r3, #1              @ Mapeamento compartilhado
    ldr r5, =fpga_bridge   @ Endereço do bridge FPGA
    ldr r5, [r5]            @ Offset base
    mov r7, #192            @ syscall: mmap2
    svc 0                  @ Chamada de sistema

    ldr r1, =mapped_address  @ Salva o endereço mapeado
    str r0, [r1]

    @carregando os registradores que foram salvos na pilha
    ldr r1, [sp, #24]
    ldr r2, [sp, #20]
    ldr r3, [sp, #16]
    ldr r4, [sp, #12]
    ldr r5, [sp, #8]
    ldr r7, [sp, #4]
    ldr r0, [sp, #0]

    add sp, sp, #28

    bx lr

@-----------------------------------------------------------------------
close_devmen:
    @ Fechar /dev/mem

    @reservando na pilha os resgitradores que vai usar
    sub sp, sp, #12
    str r0, [sp, #8]
    str r1, [sp, #4]
    str r7, [sp, #0]

    ldr r0, =mapped_address  @ Endereço mapeado
    ldr r0, [r0]
    mov r1, #4096 @ tamanho da página mapeada
    mov r7, #91   @ system call: munmap
    svc 0
    
    ldr r0, =devmem  @ /dev/mem
    ldr r0, [r0]
    mov r7, #6   @ system call: close
    svc 0
    
    ldr r0, [sp, #8]
    ldr r1, [sp, #4]
    ldr r7, [sp, #0]

    add sp, sp, #12

    bx lr

@------------------------------------------------------------------------
bg_block:
    @ r0-> parametro posição  ( (linhas * 80) +  coluna)
    @ r1-> parametro cor

    @reservando na pilha os resgitradores que vai usar
    sub sp, sp, #28  @reservando 28 bytes na pilha
    str lr, [sp, #24]
    str r1, [sp, #20]
    str r2, [sp, #16]
    str r4, [sp, #12]
    str r5, [sp, #8]
    str r6, [sp, #4]
    str r0, [sp, #0]


    ldr r4, =mapped_address  @ Endereço mapeado
    ldr r4, [r4]

    @DATAA
    mov r2, #2      @opcode
    lsl r0, #4       @deslocamento
    add r2, r2, r0    @opcode + posição

    mov r5, #0
    mov r6, #1

    str r5, [r4, #0xc0]   @desabilitando a escrita no wrreg
    str r2, [r4, #0x80]   @passando o opcode e a posição para dataA
    str r1, [r4, #0x70]   @passando a cor para dataB
    bl check_fifo         @verifica se o fifo está cheio entes de ativar o wrreg
    str r6, [r4, #0xc0]   @ativando a escrita no wrreg
    str r5, [r4, #0xc0]   @desativando a escrita no wrreg

    @carregando os registradores que foram salvos na pilha
    ldr lr, [sp, #24]
    ldr r1, [sp, #20]
    ldr r2, [sp, #16]
    ldr r4, [sp, #12]
    ldr r5, [sp, #8]
    ldr r6, [sp, #4]
    ldr r0, [sp, #0]

    add sp, sp, #24  @liberando a pilha

    bx lr

@------------------------------------------------------------------------
check_fifo: @função que verifica se o fifo está cheio
  sub sp, sp, #8    @reservando 8 bytes na pilha
  str r0, [sp, #4]
  str r1, [sp, #0]
  
  ldr r0, =mapped_address   @ Endereço mapeado
  ldr r0, [r0]
checking:                   @label para verificar se o fifo está cheio
  ldr r1, [r0, #0xb0]       @lendo o registrador de status do fifo
  CMP r1, #1                @comparando o status do fifo com 1
  beq checking              @se o fifo estiver cheio, volta para o label checking

  ldr r0, [sp, #4]
  ldr r1, [sp, #0]
  add sp, sp, #8 
  
  bx lr

@--------------------------------------------------------------------------

poligono:  @função que desenha um poligono
  @ r0 -> cor    (rgb)
  @ r1 -> tamanho (0 a 15)
  @ r2 -> forma   (0 -> quadrado, 1 -> triângulo)
  @ r3 -> endereço  (0 a 15)
  @ r4 -> posiçãoX  
  @ r5 -> posiçãoy

    sub sp, sp, #40       @reservando 40 bytes na pilha
    str lr, [sp, #36]
    str r11, [sp, #32] 
    str r7, [sp, #28] 
    str r6, [sp, #24] 
    str r5, [sp, #20]
    str r4, [sp, #16]
    str r3, [sp, #12] 
    str r2, [sp, #8]
    str r1, [sp, #4]
    str r0, [sp, #0]
    ldr r4, [sp, #40]
    ldr r5, [sp, #44]

  ldr r11, =mapped_address        @ Endereço mapeado
  ldr r11, [r11]

  @desabilitando a escrição no wrreg
  mov r9, #0
  str r9, [r11, #wrreg]

  @DATAA
  mov r8, #3  @opcode
  lsl r3, #4
  add r3, r3, r8    @opcode + endereço

  str r3, [r11, #data_a]    @passando o opcode e o endereço para dataA

  @DATAB
  mov r6, r4 @x
  mov r7, r5 @y

  @deslocamento
  lsl r2, #31
  lsl r0, #22
  lsl r1, #18
  lsl r6, #9      
  
  @calculando o data B
  add r2, r2, r0
  add r2, r2, r1
  add r2, r2, r6
  add r2, r2, r7

  str r2, [r11, #data_b]    @passando o dataB

  mov r9, #1                @habilitando a escrita no wrreg 
  str r9, [r11, #wrreg]
  mov r9, #0                @desabilitando a escrita no wrreg
  str r9, [r11, #wrreg]

    ldr lr, [sp, #36]
    ldr r11, [sp, #32] 
    ldr r7, [sp, #28] 
    ldr r6, [sp, #24] 
    ldr r9, [sp, #20]
    ldr r8, [sp, #16]
    ldr r3, [sp, #12] 
    ldr r2, [sp, #8]
    ldr r1, [sp, #4]
    ldr r0, [sp, #0]

    add sp, sp, #40        @liberando a pilha

  bx lr

@---------------------------------------------------------------------
bg_color:   @função que seta a cor de fundo de background
    @ r0 : cor

    sub sp, sp, #16       @reservando 16 bytes na pilha
    str lr, [sp, #12]
    str r4, [sp, #8]
    str r1, [sp, #4]
    str r0, [sp, #0]


    ldr r4, =mapped_address         @ Endereço mapeado
    ldr r4, [r4]

    mov r1, r0                      @ Carrega a cor no reg
    str r1, [r4, #0x70]               @ Passa a cor para o registrador dataB

    mov r1, #0                          @ Configura opcode WBR + registrador 0 (background)
    str r1, [r4, #0x80]               @ Passa o opcode e o registrador para dataA

    mov r1, #1                         @ Sinal de início de escrita no WRREG
    bl check_fifo                      @checa de a pilha está cheia
    str r1, [r4, #0xc0]                @ Ativa a escrita no registrador WRREG
    mov r1, #0                          @ Desativa o sinal de escrita
    str r1, [r4, #0xc0]                @ Confirma a finalização da escrita

    ldr lr, [sp, #12]
    ldr r4, [sp, #8]
    ldr r1, [sp, #4]
    ldr r0, [sp, #0]
    add sp, sp, #16                   @liberando a pilha

    bx lr                             @retorna 

@----------------------------------------------------------------
set_sprite:          @função que seta o sprite
    @ r0 -> offeset (Sprite)
    @ r1 -> px      (posição x)
    @ r2 -> py      (posição y)
    @ r3 -> sp      (1 -> habilita, 0 -> desabilita)
    @ r4 -> registrador  (1 a 15)

    sub sp, sp, #44         @reservando 44 bytes na pilha
    str lr, [sp, #40]
    str r10, [sp, #36] 
    str r9, [sp, #32] 
    str r7, [sp, #28] 
    str r6, [sp, #24] 
    str r5, [sp, #20]
    str r4, [sp, #16]
    str r3, [sp, #12] 
    str r2, [sp, #8]
    str r1, [sp, #4]
    str r0, [sp, #0]

    ldr r4, [sp, #44]      @carregando o registrador r4 da pilha

    ldr r10, =mapped_address    @ Endereço mapeado
    ldr r10, [r10]

    @DATAA
    mov r9, #0 @opcode

    lsl r4, #4    @deslocamento

    add r4, r4, r9    @opcode + registrador


    @DATAB
    mov sp, r3   @sp
    mov r6, r0   @offset 

    @deslocamento
    lsl sp, #29
    lsl r1, #19
    lsl r2, #9

    mov r5, #0  @zera o registrador

    @calculando o data B
    add r5, sp, r1
    add r5, r5, r2
    add r5, r5, r6

    mov r7, #0                @desabilitando a escrita no wrreg
    str r7, [r10, #wrreg]

    str r4, [r10, #data_a]  @passando o opcode e o registrador para dataA
    str r5, [r10, #data_b]   @passando o dataB

    mov r7, #1              @habilitando a escrita no wrreg
    str r7, [r10, #wrreg]
    mov r7, #0              @desabilitando a escrita no wrreg
    str r7, [r10, #wrreg]    

    ldr lr, [sp, #40]
    ldr r10, [sp, #36] 
    ldr r9, [sp, #32] 
    ldr r7, [sp, #28] 
    ldr r6, [sp, #24] 
    ldr r5, [sp, #20]
    ldr r4, [sp, #16]
    ldr r3, [sp, #12] 
    ldr r2, [sp, #8]
    ldr r1, [sp, #4]
    ldr r0, [sp, #0]
    add sp, sp, #44       @liberando a pilha

    bx lr 
@--------------------------------------------------------------------------
acess_btn:    @função que acessa os botões

    ldr r11, =mapped_address    @ Endereço mapeado
    ldr r11, [r11]

    ldr r0, [r11, #0x0]  @endereço base botões

    bx lr

@-------------------------------------------------------------------------
acess_display:    @função que acessa o display

    ldr r11, =mapped_address    @ Endereço mapeado
    ldr r11, [r11]

    ldr r0, [r11, #0x10]      @endereço base display 1
    ldr r0, [r11, #0x20]      @endereço base display 2
    ldr r0, [r11, #0x30]      @endereço base display 3
    ldr r0, [r11, #0x40]      @endereço base display 4
    ldr r0, [r11, #0x50]      @endereço base display 5
    ldr r0, [r11, #0x60]      @endereço base display 6

    bx lr

.section .data
mapped_address: .space 4
devmem: .asciz "/dev/mem"
fpga_bridge: .word 0xff200             @ Endereço do bridge FPGA