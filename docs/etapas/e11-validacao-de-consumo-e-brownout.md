# E11 - Validação de Consumo e Brownout

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P1
- Risco: ALTO
- Depende de: 

## Validação de Consumo e Brownout

Complexidade: Média
Grupo: Bring-up HW
HW Real: SIM
ID: E11
Prioridade: P1
Risco: ALTO
Status: 🔲 Não iniciada

## Objetivo

Medir consumo real em 5 perfis. Confirmar que o sistema não sofre brownout em pico de carga. Documentar os números que guiarão o PowerManager.

## Perfis a medir

| Perfil | Esperado |
| --- | --- |
| Idle (só CPU) | ~50 mA |
| Display ativo 20fps | ~105 mA |
| 1 servo em movimento | ~155–173 mA |
| WiFi scan | +60–80 mA extra |
| Tudo simultâneo | ~255 mA |
| Pico máximo | ~805 mA |

## Critérios de pronto

- Todos os 5 perfis medidos e documentados
- Brownout testado e resultado registrado (ocorreu ou não)
- Capacitor de hold-up instalado se queda de tensão > 200mV em pico

## Testes mínimos

- Medir cada perfil com multímetro série na linha da bateria
- Medir tensão 3.3V com osciloscópio durante movimento de servo + WiFi simultâneos
- Forçar reset intencional para ver se brownout detector do ESP32-S3 está configurado

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, todos os periféricos da E03–E10 disponíveis.
Ajude-me a planejar a medição de consumo para 5 perfis:
1. Idle (só CPU rodando, display off)
2. Display ativo com fill a 20fps
3. 1 servo em movimento contínuo
4. WiFi scan ativo
5. Tudo simultâneo (display + servo + WiFi)
Para cada perfil: o que ligar, o que medir, como registrar.
Incluir: como detectar brownout (osciloscópio na VCC, threshold no menuconfig).
Saída: procedimento de teste + template de tabela para documentar.
```

## ◎ Prompt de Revisão

```
Resultados de consumo da E11: [cole os valores medidos]
Verificar: (1) pico máximo documentado? (2) tensão mínima em pico > 3.0V? (3) capacitor de hold-up recomendado se necessário? (4) autonomia calculada para 3 perfis com LiPo 3Ah?
Listar inconsistências ou riscos nos números.
```

## ✎ Prompt de Correção

```
Brownout ocorrendo em [PERFIL].
Contexto: E11, ESP32-S3, LiPo 3Ah.
Propor solução: capacitor de hold-up (valor, posicionamento) ou limitação de software.
Saída: solução com especificação elétrica.
```

## → Prompt de Continuidade

```
E11 concluída. Consumo medido, brownout avaliado, tabela documentada.
Bring-up de hardware COMPLETO. Pronto para o runtime.
Próxima: E12 (EventBus — o coração do desacoplamento do sistema).
Mostre a implementação do EventBus com pool de eventos pré-alocados (sem malloc), filas FreeRTOS por prioridade e interface de pub/sub.
```


