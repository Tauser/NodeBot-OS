# E01 - Toolchain e Estrutura de Projeto

- Status: ✅ Critérios atendidos
- Complexidade: Baixa
- Grupo: Fundação
- HW Real: SIM
- Prioridade: P1
- Risco: BAIXO
- Depende de: —

## Toolchain e Estrutura de Projeto

Complexidade: Baixa
Depende de: —
Grupo: Fundação
HW Real: SIM
ID: E01
Prioridade: P1
Risco: BAIXO
Status: ✅ Critérios atendidos

## Objetivo

ESP-IDF v5.x instalado, estrutura de pastas criada, CMake configurado, blink estruturado rodando no hardware.

## Por que agora

Retrabalho de estrutura depois é o mais caro de todo o projeto. Convenções e organização agora evitam refactoring futuro.

## O que implementar

- Instalar ESP-IDF v5.x e validar hello_world no HW real
- Criar pastas: src/{drivers,core,services,face,audio,behavior,models,policies,diagnostics}, assets, config, tests, tools
- CMakeLists.txt raiz + por componente (cada pasta = componente ESP-IDF)
- Blink estruturado no main.c usando a hierarquia real
- README de onboarding (toolchain, build, flash, serial)
- config/nvs_defaults.h com 10 parâmetros críticos e defaults seguros
- .gitignore e primeiro commit

## O que NÃO entra

- Drivers reais de qualquer periférico
- Lógica de produto ou serviços
- Dependências além do blink

## Critérios de pronto

- Compilação limpa; zero warnings relevantes
- README funciona em ambiente limpo (< 30 min para compilar e flashar)
- Estrutura de pastas espelha a arquitetura do Plano Mestre

## Testes mínimos

- Build clean: 0 errors, 0 warnings
- Onboarding: seguir README do zero, cronometrar
- Blink estruturado piscando

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, ESP-IDF v5.x, projeto de robô desktop.
Tarefa: gere a estrutura completa de CMake para firmware embarcado modular.
Pastas obrigatórias: src/drivers, src/core, src/services, src/face, src/audio, src/behavior, src/models, src/policies, src/diagnostics, assets, config, tests, tools.
Cada pasta = componente CMake independente com CMakeLists.txt próprio.
Saída: CMakeLists.txt raiz + modelo por componente + main.c com blink no LED GPIO48.
```

## ◎ Prompt de Revisão

```
Estrutura da E01 pronta.
Verificar: (1) build sem warnings? (2) cada pasta tem CMakeLists.txt? (3) main.c sem lógica de produto? (4) nvs_defaults.h tem ≥10 campos? (5) README testado?
Listar problemas com severidade.
```

## ✎ Prompt de Correção

```
[Descreva o problema exato]
Contexto: E01, ESP32-S3, estrutura de projeto.
Corrija o item específico. Não altere o resto da estrutura.
Saída: correção + 1 parágrafo explicando o que mudou.
```

## → Prompt de Continuidade

```
E01 concluída. Build limpo, estrutura criada, blink rodando.
Próxima: E02 (pinagem e configuração de periféricos).
Mostre o template de hal_init.h para documentar pinagem de todos os periféricos do robô desktop (display ST7789, servos SCS0009, INMP441, MAX98357A, microSD, IMU, touch, WS2812, bateria).
```


