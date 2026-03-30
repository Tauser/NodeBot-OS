# Etapas Criticas — Referencia Rapida

Estas etapas tem risco CRITICO ou ALTO. Nao pule os testes minimos.

---

## E04 — Bring-up Servos SCS0009 🔴 CRITICO

**Por que critico:** Servo sem protecao de corrente e risco fisico (LiPo + stall = incendio).

**Protecao obrigatoria no driver:**
```c
// No scs0009_driver.c — chamar em cada read loop
if (scs0009_get_current_ma(id) > 900) {
    scs0009_set_torque_enable(id, false);
    log_write(FATAL, "servo", "overcurrent detected");
}
```

**Teste obrigatorio:** Bloquear servo mecanicamente por 3s -> verificar que para em <200ms.

---

## E11 — Validacao de Consumo 🟠 ALTO

**Perfis a medir:**

| Perfil                 | Esperado     |
|------------------------|--------------|
| Idle (so CPU)          | ~50 mA       |
| Display ativo 20fps    | ~105 mA      |
| 1 servo em movimento   | ~155-173 mA  |
| WiFi scan              | +60-80 mA    |
| Tudo simultaneo        | ~255 mA      |
| Pico maximo            | ~805 mA      |

**Brownout threshold ESP32-S3:** 2.97V — configurar no menuconfig.

---

## E23 — MotionSafetyService 🔴 CRITICO

**Task de maior prioridade da aplicacao: P22, Core 1, 5ms**

```c
// Checklist obrigatorio para a task:
// [ ] Sem delay() bloqueante
// [ ] Sem malloc()
// [ ] Sem I/O de SD
// [ ] Heartbeat verificado a cada tick
// [ ] Fail-safe = parado (nao = ultima posicao)
// [ ] Limite de corrente com debounce (80ms, nao 1 ciclo)
```

**Testes obrigatorios:**
- Bloquear servo mecanicamente -> detectado em <100ms
- Parar de enviar heartbeat por 600ms -> servos param
- Core 1 sob carga maxima -> P22 ainda roda a cada 5ms

---

## E25 — Brownout Handler 🟠 ALTO

**Brownout handler tem apenas ~5ms:**

```c
// Apenas isto no handler:
// 1. log_flush_now()            <- max 3ms
// 2. nvs_set_u8("unclean", 1)
// 3. nvs_commit()               <- max 2ms
// NAO fazer: I2C, SD, malloc, printf
```

**Boot loop detection:**
- Incrementar `boot_count` no inicio do boot
- Decrementar apos 60s estavel
- Se `boot_count >= 3` -> ativar SafeMode

---

## E33 — BehaviorEngine 🟠 ALTO

**Regras do BehaviorLoop (100ms, Core 1, P12):**

```c
// Ordem OBRIGATORIA:
// 1. motion_safety_feed_heartbeat()  <- SEMPRE PRIMEIRO
// 2. state_vector_tick(100)          <- decay de 100ms
// 3. processar eventos do EventBus   <- sem bloqueio
// 4. behavior_tree_evaluate()        <- <10ms
// 5. publicar comandos via EventBus  <- nao chamar direto
```

**BT minima:**
```
if safety_mode       -> face_safe
elif battery < 10%   -> face_tired + sleep mode
elif wake_word < 5s  -> engage
elif touch < 2s      -> react_touch
else                 -> idle_behavior_tick()
```

---

## E39 — OTA com Rollback 🟠 ALTO

**Janela de seguranca obrigatoria:**
```
battery_pct > 30%
AND servos parados
AND nao durante conversacao ativa
```

**Rollback config (sdkconfig):**
```
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=n
```

**Novo firmware tem 60s para chamar:**
```c
ota_manager_mark_stable();  // chamar do BehaviorLoop apos 60s
```

---

## Checklist de Seguranca Geral

- [ ] E04: protecao de corrente no driver (>900mA -> para)
- [ ] E11: consumo medido e documentado com valores reais
- [ ] E23: MotionSafety em P22, sem delay, sem malloc
- [ ] E23: heartbeat verificado a cada 5ms
- [ ] E24: bateria critica para servos automaticamente
- [ ] E25: brownout handler salva em <5ms
- [ ] E25: LED de privacidade nao desabilitavel por codigo
- [ ] E33: BehaviorLoop alimenta heartbeat antes de tudo
- [ ] E39: assinatura verificada ANTES do swap de particao
