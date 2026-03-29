# Face Bench Checklist

Checklist curto para validar a frente de faces do NodeBot em bancada real.

## Como usar

- Faça um boot limpo do firmware
- Observe cada cenário por alguns segundos
- Marque `OK`, `Apertado`, `Aberto demais`, `Brusco`, `Lento` ou `Bug`
- Se possível, grave vídeo curto da sequência completa

### Atalho por serial

Use o serial para forçar cenários de bancada:

- `facebench idle`
- `facebench engaged`
- `facebench listen`
- `facebench think`
- `facebench happy`
- `facebench glee`
- `facebench sad`
- `facebench alert`
- `facebench music`
- `facebench sleepy`
- `facebench unimp`
- `facebench sleep`

## Escala rápida

- `OK`: leitura boa e coerente
- `Apertado`: olhos parecem menores do que deveriam
- `Aberto demais`: perdeu personalidade da expressão
- `Brusco`: transição visual seca
- `Lento`: transição demora mais do que deveria
- `Bug`: semi-blink, olho preso, assimetria estranha ou travamento

## Sequência principal

### 1. Idle parado

- Estado esperado: `IDLE_NEUTRAL`
- Observar:
  - olhos claramente abertos
  - idle estável sem cair para leitura de semi-fechado
  - microefeitos sem “amassar” demais a expressão
- Marcação: `_____`
- Notas: `____________________________________________________________`

### 2. Atenção/presença

- Estados esperados: `ENGAGED_NEUT`, `ENGAGED_AWE`, `ENGAGED_FOCUS`
- Observar:
  - transição natural saindo do idle
  - engaged não parece mais fechado que idle sem motivo
  - `AWE` continua social/curioso sem parecer bug visual
- Marcação: `_____`
- Notas: `____________________________________________________________`

### 3. Wake + fala

- Estados esperados: `TALK_LISTEN`, `TALK_THINK`, retorno para `ENGAGED_NEUT`
- Observar:
  - `LISTEN` mais atento e aberto
  - `THINK` mais focado sem virar semi-squint feio
  - volta ao engaged sem salto estranho
- Marcação: `_____`
- Notas: `____________________________________________________________`

### 4. Interação positiva

- Estados esperados: `IDLE_HAPPY`, `ENGAGED_HAPPY`, `GLEE`, `MUSIC`
- Observar:
  - felicidade legível
  - não parece olho espremido
  - `MUSIC` combina com a família feliz
- Marcação: `_____`
- Notas: `____________________________________________________________`

### 5. Estados pesados

- Estados esperados: `IDLE_SAD_U`, `IDLE_SAD_D`, `SYSTEM_ALERT`, `SLEEPY`, `UNIMP`
- Observar:
  - tristeza/preocupação continuam expressivas
  - leitura pesada sem parecer travamento
  - `SYSTEM_ALERT` se destaca sem “assustar” demais
- Marcação: `_____`
- Notas: `____________________________________________________________`

## Sequência mais importante para gravar

- `IDLE -> ENGAGED -> TALK_LISTEN -> TALK_THINK -> ENGAGED_NEUT -> IDLE`

## Se aparecer bug

- Anote:
  - estado aproximado
  - se aconteceu parado ou em transição
  - se foi nos dois olhos ou em um só
  - se ficou preso ou se recuperou sozinho

- Modelo de anotação:
  - `Estado: __________________`
  - `Sintoma: _________________`
  - `Parado/transição: ________`
  - `Recuperou sozinho: _______`
