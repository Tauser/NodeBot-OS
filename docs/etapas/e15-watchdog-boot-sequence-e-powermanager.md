# E15 - Watchdog, Boot Sequence e PowerManager

- Status: ✅ Critérios atendidos
- Complexidade: Média
- Grupo: Runtime
- HW Real: SIM
- Prioridade: P1
- Risco: ALTO
- Depende de: 

## Watchdog, Boot Sequence e PowerManager

Complexidade: Média
Grupo: Runtime
HW Real: SIM
ID: E15
Prioridade: P1
Risco: ALTO
Status: ✅ Critérios atendidos

## Objetivo

Watchdog de hardware e de task configurados. Boot sequence inicializando componentes em ordem com log de cada passo. PowerManager com 4 modos de energia.

## Critérios de pronto

- HW WDT: parar de alimentar → reset em ≤ 8s
- Boot loop: 3 boots sem estabilizar → logar aviso
- PowerManager: forçar LOW_POWER via debug → modo muda

## Testes mínimos

- Comentar wdt_feed() em uma task crítica: verificar reset em ≤ 3s
- Forçar falha em um passo do boot: verificar que log indica o passo e motivo
- Simular 3 boots com falha: verificar boot_count = 3 em NVS

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, FreeRTOS, ESP-IDF. E15 — Watchdog e boot sequence.
Tarefa A: watchdog_manager.h + watchdog_manager.c.
HW WDT: timeout 8s. Task WDT: registrar tasks críticas (Safety P22, FaceRender P20, AudioIO P18) com timeout 3s cada.
Interface: wdt_init(), wdt_register_task(task_handle, timeout_ms), wdt_feed(task_handle).
Tarefa B: boot_sequence.c com função app_boot() que inicializa em ordem:
  1. hal_init → 2. config_manager_init → 3. storage_manager_init → 4. log_manager_init → 5. drivers → 6. event_bus_init → 7. power_manager_init.
Cada passo: log INFO "[STEP N] init_ok" ou FATAL "[STEP N] init_fail: err=X".
Saída: ambos os arquivos.
```

## ◎ Prompt de Revisão

```
Watchdog e boot sequence da E15.
Verificar: (1) HW WDT timeout ≥ 8s? (2) boot sequence alimenta WDT entre passos demorados? (3) boot_count em NVS incrementado antes de inicializar (não depois)? (4) PowerManager expõe modo via getter (não global mutável)?
Listar problemas.
```

## ✎ Prompt de Correção

```
Watchdog com problema: [sintoma]
Contexto: E15.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E15 concluída. Runtime completo: EventBus, Config, Storage, Log, Watchdog, Boot.
Runtime está pronto. Próxima fase: Face.
Próxima: E16 (Face loop básico — framebuffer, DMA, 20fps).
Mostre como implementar double-buffering com ST7789 e PSRAM no ESP32-S3: dois framebuffers de 150KB em PSRAM e DMA swap via SPI.
```


