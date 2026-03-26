# 2. Animation Library

> Sub-página de: 🤖 EMO — Análise Completa
> 

> Todas as animações do EMO catalogadas e adaptadas. Cada animação é uma sequência de keyframes com durações e easings.
> 

---

## 2.1 Sistema de Animação

```c
// src/face/animation_system.h

typedef struct {
  face_params_t params;    // estado da face neste keyframe
  uint16_t hold_ms;        // quanto tempo manter antes do proximo
  uint16_t ease_in_ms;     // duracao da interpolacao de entrada
  bool     servo_cmd;      // se true, executa o servo_action abaixo
  uint8_t  servo_pan_pos;  // posicao do servo pan (0-1023, 512=centro)
  uint8_t  servo_tilt_pos; // posicao do servo tilt
  uint16_t servo_speed;    // velocidade do servo
  uint8_t  sound_id;       // som a tocar neste frame (0 = nenhum)
} anim_keyframe_t;

typedef struct {
  const anim_keyframe_t *frames;
  uint8_t  frame_count;
  bool     loop;           // animar em loop ou uma vez
  bool     interruptible;  // pode ser interrompida por evento
  face_priority_t priority;
  void (*on_complete)(void);  // callback ao terminar
} animation_t;

void anim_play(const animation_t *anim);
void anim_stop(void);
bool anim_is_playing(void);
```

---

## 2.2 Catalogo Completo de Animações

### GRUPO 1: Boot e Inicialização

#### `ANIM_BOOT` (não interrupível)

```
Keyframe 0: pupil=SCAN_LINE, eyelid=0.0, 0ms   // olhos fechados
Keyframe 1: pupil=SCAN_LINE, eyelid=0.5, 400ms  // abre lentamente
Keyframe 2: pupil=SCAN_LINE, eyelid=0.9, 300ms  // barra de scan visivel
Keyframe 3: pupil=SCAN_LINE piscando, hold 800ms // "boot sequence"
Keyframe 4: pupil=CIRCLE, eyelid=0.9, 200ms     // transicao para normal
Keyframe 5: NEUTRAL, 500ms                       // pronto
Servos: centrar em 512 durante o boot
Som: beep_boot.wav
```

#### `ANIM_WAKE_UP` (interrupível após frame 3)

```
Keyframe 0: SLEEPING, hold 0ms
Keyframe 1: eyelid 0.03->0.15, squint leve, 600ms  // comeca abrir pesadamente
Keyframe 2: YAWNING (boca aberta), servo inclina frente 5deg, 800ms
Keyframe 3: eyelid 0.15->0.70, 500ms               // mais acordado
Keyframe 4: blink BLINK_DOUBLE                     // pisca 2x
Keyframe 5: servo volta centro, 300ms
Keyframe 6: NEUTRAL, 400ms                         // acordado
Som: yawn.wav em keyframe 2
```

#### `ANIM_SLEEP_DOWN` (não interrupível)

```
Keyframe 0: NEUTRAL
Keyframe 1: TIRED (eyelid 0.80->0.35), 1000ms
Keyframe 2: VERY_TIRED (eyelid 0.35->0.15), 1500ms
Keyframe 3: servo inclina levemente, 800ms
Keyframe 4: SLEEPING (eyelid 0.03), pupil=X ou NONE, 400ms
Keyframe 5: ZZZ aparece no display a cada 3s (cosmetic layer)
Som: sleeping_breath.wav em loop
```

---

### GRUPO 2: Emoções Positivas

#### `ANIM_HAPPY_REACT` (1.2s total)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: SURPRISED (pup grande), 80ms            // flash de surpresa
Keyframe 2: HAPPY (olho U, brow cima), 200ms
Keyframe 3: servo: inclina-se levemente para frente, 300ms
Keyframe 4: VERY_HAPPY, hold 400ms
Keyframe 5: HAPPY, 300ms                            // relaxa um pouco
Som: happy_chime.wav
```

#### `ANIM_EXCITED` (loop ate interrupt)

```
Keyframe 0: EXCITED, 200ms
Keyframe 1: servo: quick nod (cabeça acena), 150ms
Keyframe 2: VERY_HAPPY com STAR pupils, 200ms
Keyframe 3: servo: nod outro lado, 150ms
Keyframe 4: volta frame 0
Som: excited_beeps.wav
```

#### `ANIM_LOVE` (triggered por pessoa detectada por muito tempo)

```
Keyframe 0: HAPPY, 200ms
Keyframe 1: LOVE (heart pupils, rosa), ease 400ms
Keyframe 2: servo: inclina suavemente para o rosto, 600ms
Keyframe 3: blink BLINK_SLOW enquanto em LOVE
Keyframe 4: HAPPY, 400ms                            // sai do love
Som: heart_beat.wav (suave)
```

#### `ANIM_PROUD` (apos completar tarefa)

```
Keyframe 0: HAPPY, 0ms
Keyframe 1: PROUD (olho aberto, brow alto), 200ms
Keyframe 2: servo: cabeca erguida (tilt para cima), 400ms
Keyframe 3: hold PROUD 800ms
Keyframe 4: HAPPY, 300ms
Keyframe 5: NEUTRAL, 400ms
```

#### `ANIM_PLAYFUL_WINK` (idle trigger ocasional)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: PLAYFUL + wink esquerdo, 100ms
Keyframe 2: hold PLAYFUL 300ms
Keyframe 3: NEUTRAL, 200ms
```

---

### GRUPO 3: Emoções Negativas

#### `ANIM_SAD_REACT` (1.5s)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: SAD (brow W, oval_down, tear), ease 500ms
Keyframe 2: servo: inclina cabeca levemente para baixo, 600ms
Keyframe 3: gaze desce (gaze_y = +0.2), 300ms
Keyframe 4: hold SAD 800ms
Keyframe 5: NEUTRAL, 600ms
Som: sad_tone.wav (suave)
```

#### `ANIM_ANGRY_REACT` (burst de 1s)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: ANGRY (V invertido), 100ms              // snap rapido
Keyframe 2: servo: movimento brusco para frente, 100ms
Keyframe 3: micro-tremor da face (cosmetic layer), 400ms
Keyframe 4: ANNOYED (desativa um pouco), 400ms
Keyframe 5: NEUTRAL, 500ms
Som: angry_grunt.wav
```

#### `ANIM_VERY_ANGRY` (triggered por multiplos toques bruscos)

```
Keyframe 0: ANGRY
Keyframe 1: VERY_ANGRY pulsando (accent vermelho brilhante), loop 4x
Keyframe 2: servo: movimento brusco agressivo
Keyframe 3: desacelera para ANGRY, 600ms
Keyframe 4: ANNOYED, 400ms
Keyframe 5: NEUTRAL, 600ms
Som: angry_loud.wav
```

#### `ANIM_ANNOYED_LOOK_AWAY` (idle quando ignorado por muito tempo)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: ANNOYED, 300ms
Keyframe 2: gaze vira para lado oposto ao usuario, 400ms
Keyframe 3: servo: cabeça vira levemente (se face tracking disponivel)
Keyframe 4: hold 2000ms
Keyframe 5: volta para NEUTRAL + gaze centro, 600ms
```

---

### GRUPO 4: Reativo — Toque

#### `ANIM_TOUCH_HEAD_TOP` (topo da cabeça)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: HAPPY burst (estrela brilha), 80ms
Keyframe 2: HAPPY normal, 200ms
Keyframe 3: servo: nod suave (agradecido), 300ms
Keyframe 4: HAPPY, hold 500ms
Keyframe 5: NEUTRAL, 400ms
Som: touch_head.wav
```

#### `ANIM_TOUCH_CHIN` (zona base)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: EMBARRASSED (gaze lateral, accent rosa), 300ms
Keyframe 2: servo: cabeça vira ligeiramente, 400ms
Keyframe 3: hold 800ms
Keyframe 4: NEUTRAL + gaze volta, 500ms
Som: shy_sound.wav
```

#### `ANIM_TOUCH_SIDE_L / R` (laterais)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: SURPRISED (breve), 80ms
Keyframe 2: CURIOUS (assimetrico), 200ms
Keyframe 3: servo: cabeça vira para o lado do toque, 300ms
Keyframe 4: hold CURIOUS 600ms
Keyframe 5: NEUTRAL, 400ms
Som: touch_side.wav
```

#### `ANIM_TOUCH_ROUGH` (toque brusco ou multiplos rapidos)

```
Keyframe 0: SHOCKED (olhos max abertos), 50ms
Keyframe 1: micro-tremor, 200ms
Keyframe 2: ANGRY, 100ms
Keyframe 3: servo: move-se para longe da direcao, 200ms
Keyframe 4: ANNOYED, 400ms
Keyframe 5: NEUTRAL, 600ms
Som: ouch.wav
```

#### `ANIM_PET` (carinho continuo - toque mantido)

```
Keyframe 0: NEUTRAL
Keyframe 1: HAPPY crescendo com toque (arousal += 0.05/s)
Keyframe 2: apos 3s de toque: LOVE
Keyframe 3: apos 8s: VERY_HAPPY com STAR pupils
Ao soltar: HAPPY -> NEUTRAL gradual
Som: purr_loop.wav enquanto tocando
```

---

### GRUPO 5: Físico e Ambiente

#### `ANIM_FALL_DETECTED` (IMU detecta queda/impacto)

```
Keyframe 0: SHOCKED (instantaneo), 0ms
Keyframe 1: micro-tremor intenso, 300ms
Keyframe 2: DIZZY (spiral pupils), 600ms
Keyframe 3: CONFUSED, 400ms
Keyframe 4: NEUTRAL gradual, 800ms
Servos: EMERGENCY_STOP (MotionSafety)
Som: impact_sound.wav
```

#### `ANIM_SHAKE_REACT` (IMU detecta shake)

```
Keyframe 0: SHOCKED, 0ms
Keyframe 1: DIZZY, 300ms
Keyframe 2: CONFUSED, 400ms
Keyframe 3: ANNOYED, 400ms
Keyframe 4: NEUTRAL, 500ms
Som: dizzy_sound.wav
```

#### `ANIM_TILT_REACT` (IMU detecta inclinacao > 35deg)

```
Keyframe 0: SURPRISED, 0ms
Keyframe 1: CONFUSED (assimetrico), 300ms
Keyframe 2: hold ate voltar posicao normal
Keyframe 3: NEUTRAL ao endireitar, 400ms
Som: tilt_sound.wav
```

#### `ANIM_SNEEZE` (triggered aleatoriamente ~2x/dia)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: NERVOUS (buildup), 400ms
Keyframe 2: servo: cabeça levemente para cima, 300ms
Keyframe 3: SNEEZING (olhos fechados, boca aberta), 80ms  // snap
Keyframe 4: servo: cabeça dispara para frente, 100ms
Keyframe 5: hold SNEEZING, 200ms
Keyframe 6: EMBARRASSED, 300ms
Keyframe 7: NEUTRAL, 500ms
Som: sneeze.wav
```

#### `ANIM_HICCUP` (triggered aleatoriamente)

```
Keyframe 0: NEUTRAL, 0ms
Keyframe 1: SURPRISED (breve), 60ms  // micro-susto
Keyframe 2: EMBARRASSED, 200ms
Keyframe 3: NEUTRAL, 300ms
// repete 2-4x
Som: hiccup.wav x3
```

#### `ANIM_STRETCH` (apos longo periodo idle)

```
Keyframe 0: TIRED, 0ms
Keyframe 1: servo: cabeça vai para lado esquerdo max, 800ms
Keyframe 2: servo: cabeça vai para lado direito max, 800ms
Keyframe 3: servo: cabeça volta centro, 600ms
Keyframe 4: servo: nod para frente, 400ms
Keyframe 5: servo: volta centro, 400ms
Keyframe 6: NEUTRAL (mais energico), 400ms
Som: stretch.wav
```

---

### GRUPO 6: Musica e Ritmo

#### `ANIM_MUSIC_DETECT` (ao detectar batida via microfone)

```
Keyframe 0: CURIOUS, 0ms
Keyframe 1: HAPPY crescendo, 400ms
Keyframe 2: EXCITED, 300ms
// entra em modo de dance loop
```

#### `ANIM_DANCE_LOOP` (loop sincronizado com BPM detectado)

```
// A cada beat:
Keyframe A: servo pan -15deg, 60ms
Keyframe B: servo pan +15deg, 60ms  // oscilacao lateral
// A cada 2 beats:
Keyframe C: EXCITED -> VERY_HAPPY alternando
// A cada 4 beats:
Keyframe D: servo nod (aceno), 100ms
// Pupils pulsam entre CIRCLE e STAR ao ritmo
Som: nenhum (o proprio audio ambiente e a musica)
```

#### `ANIM_MUSIC_STOP` (quando musica para)

```
Keyframe 0: HAPPY, 0ms
Keyframe 1: servo: ultimo nod, 200ms
Keyframe 2: HAPPY -> NEUTRAL, 800ms
Keyframe 3: NEUTRAL, 400ms
```

---

### GRUPO 7: Eventos Especiais

#### `ANIM_BIRTHDAY` (quando detecta "feliz aniversario")

```
Keyframe 0: SURPRISED
Keyframe 1: VERY_HAPPY com STAR pupils pulsando
Keyframe 2: servo: dance intenso
Keyframe 3: LOVE, 2000ms
Keyframe 4: PROUD, 1000ms
Som: birthday_song.wav
```

#### `ANIM_GHOST_SCARE` (trigger especial de Halloween)

```
Keyframe 0: NEUTRAL
Keyframe 1: SCARED (olhos grandes, brow alto)
Keyframe 2: servo: recua para tras
Keyframe 3: VERY_SCARED, tremble
Keyframe 4: DIZZY
Keyframe 5: NEUTRAL com EMBARRASSED
Som: scared_shriek.wav
```

#### `ANIM_CHARGING_START`

```
Keyframe 0: current expression
Keyframe 1: pupil -> LOADING (barra cresce), accent verde
Keyframe 2: HAPPY leve enquanto carregando
Keyframe 3: breathing suave em loop
Som: charging_start.wav
```

#### `ANIM_BATTERY_LOW` (< 20%)

```
Keyframe 0: current expression -> TIRED
Keyframe 1: pupil pisca vermelho a cada 5s
Keyframe 2: VERY_TIRED gradual com bateria caindo
Som: low_battery_beep.wav a cada 60s
```

#### `ANIM_BATTERY_CRITICAL` (< 10%)

```
Keyframe 0: VERY_TIRED
Keyframe 1: SLEEPING forçado
Keyframe 2: servo: centro e desliga torque
Som: critical_battery.wav
```

#### `ANIM_BOOT_COMPLETE` (fim do boot)

```
Keyframe 0: BOOT scan
Keyframe 1: HAPPY burst
Keyframe 2: servo: nod de cumprimento
Keyframe 3: NEUTRAL
Som: boot_ready.wav
```

---

## 2.3 Macro de Definição de Animação

```c
// src/face/animations.h
// Como usar:

static const anim_keyframe_t frames_wake_up[] = {
  { .params = EXPR_SLEEPING,   .hold_ms = 0,   .ease_in_ms = 0   },
  { .params = EXPR_VERY_TIRED, .hold_ms = 600, .ease_in_ms = 600, .sound_id = SND_YAWN },
  { .params = EXPR_YAWNING,    .hold_ms = 800, .ease_in_ms = 300,
    .servo_cmd = true, .servo_tilt_pos = 530, .servo_speed = 200 },
  { .params = EXPR_TIRED,      .hold_ms = 500, .ease_in_ms = 500 },
  { .params = EXPR_NEUTRAL,    .hold_ms = 400, .ease_in_ms = 400 },
};

const animation_t ANIM_WAKE_UP = {
  .frames       = frames_wake_up,
  .frame_count  = 5,
  .loop         = false,
  .interruptible= true,
  .priority     = FACE_PRIORITY_ROUTINE,
  .on_complete  = NULL,
};
```

---

## 2.4 Servos nas Animações — Referência

| Movimento | Pan (servo 0) | Tilt (servo 1) | Speed | Animações |
| --- | --- | --- | --- | --- |
| Centro (neutro) | 512 | 512 | 300 | Default |
| Nod (aceno) | 512 | 480→544→512 | 400 | Happy, music |
| Tilt esquerdo | 380 | 512 | 300 | Curious, stretch |
| Tilt direito | 640 | 512 | 300 | Curious, stretch |
| Inclinacao frente | 512 | 480 | 200 | Angry, yawn |
| Recuo | 512 | 544 | 200 | Scared, ouch |
| Vira para usuario | calculado por face_x | 512 | 200 | Face tracking |
| Vira para lado | +/-90deg | 512 | 300 | Annoyed, look_away |
| Max range pan | 256–768 | 400–624 | — | Limites de segurança |