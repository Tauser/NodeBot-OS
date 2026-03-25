#pragma once

/*
 * face_debug — ferramenta de calibração de olho EMO via serial.
 *
 * Uso standalone: NÃO deve rodar simultaneamente com FaceEngine, pois ambos
 * escrevem no display sem sincronização. Para calibrar, pause face_engine
 * (não inicie face_engine_start_task) e chame face_debug_start_task().
 *
 * Protocolo serial (UART0 / USB-CDC, uma linha por comando):
 *   e=<0.0-1.0>  s=<0.0-1.0>  b=<-1.0..1.0>  p=<0-4>  c=<RGB565>
 *   Exemplo: e=0.80 s=0.40 b=-0.50 p=0 c=65535
 *   Campos omitidos mantêm o valor anterior.
 *
 * Pupil IDs:
 *   0 = círculo          1 = círculo pequeno
 *   2 = estrela (5 pts)  3 = coração   4 = oval vertical
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ─── Constantes de geometria (ajuste para calibrar posição na tela) ─── */
#define FACE_DBG_EYE_CX       120
#define FACE_DBG_EYE_CY       160
#define FACE_DBG_EYE_W         56
#define FACE_DBG_EYE_H_MAX     56
#define FACE_DBG_PUPIL_R_MIN   10
#define FACE_DBG_PUPIL_R_MAX   22
#define FACE_DBG_BROW_OFFSET   10   /* px acima do topo do olho */
#define FACE_DBG_BROW_W        44
#define FACE_DBG_BROW_THICK     4

/* ─── API ────────────────────────────────────────────────────────────── */

/*
 * Registra o sprite alvo (lgfx::LGFX_Sprite* passado como void* para
 * compatibilidade C). Deve ser chamado antes de face_debug_draw().
 */
void face_debug_set_sprite(void *sprite);

/*
 * Limpa o sprite (preto) e desenha um olho EMO-style completo.
 *   eyelid:     0.0-1.0   abertura vertical (0=fechado, 1=totalmente aberto)
 *   squint:     0.0-1.0   pálpebra inferior sobe em diagonal lado interno
 *   brow_angle: -1.0..+1.0  neg=externo(esq) sobe / pos=interno(dir) sobe
 *   pupil:      0-4  (veja tabela no topo)
 *   color:      RGB565 — cor do olho e da sobrancelha
 *
 * Depois de desenhar, empurra o sprite para o display automaticamente.
 */
void face_debug_draw(float eyelid, float squint, float brow_angle,
                     int pupil, uint16_t color);

/*
 * Inicia task FreeRTOS de leitura serial (Core 0, prioridade 3).
 * Se sprite == NULL, cria um sprite 240×320 RGB565 em PSRAM automaticamente.
 * Lê linhas do stdin e redesenha a cada comando recebido.
 */
void face_debug_start_task(void *sprite);

#ifdef __cplusplus
}
#endif
