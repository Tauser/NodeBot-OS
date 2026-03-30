# E07 - Bring-up: Bateria e Power Path

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P1
- Risco: ALTO
- Depende de: E02

## Bring-up: Bateria e Power Path

Complexidade: Média
Depende de: E02
Grupo: Bring-up HW
HW Real: SIM
ID: E07
Prioridade: P1
Risco: ALTO
Status: 🔲 Não iniciada

## Objetivo

Fuel gauge lendo SOC% e tensão. bq25185 reportando status de carga. TPS61088 com tensão de saída medida. Power path validado.

## Critérios de pronto

- SOC% do MAX17048 bate com voltímetro ± 5%
- bq25185 muda de CHARGING para FULL corretamente
- TPS61088 saída entre 4.8–5.2V em carga de 0–500mA

## Testes mínimos

- I2C scanner: verificar que MAX17048 (0x36) e bq25185 (0x6B) respondem
- Conectar USB: bq25185 deve reportar CHARGING
- Desconectar USB: deve reportar DISCHARGING com SOC caindo

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, I2C (SDA/SCL conforme hal_init.h), MAX17048 (0x36), bq25185 (0x6B).
Tarefa: max17048_driver.h/.c + bq25185_driver.h/.c.
MAX17048: init_quickstart(), get_soc_percent() → float, get_voltage_mv() → uint16_t.
bq25185: init(), get_charge_status() → enum {CHARGING, FULL, DISCHARGING, FAULT}.
Ambos: retornar esp_err_t em todas as funções, checar I2C ACK.
Saída: drivers + teste que imprime SOC%, tensão e status a cada 2s.
```

## ◎ Prompt de Revisão

```
Drivers de bateria da E07.
Verificar: (1) endereços I2C corretos (MAX17048=0x36, bq25185=0x6B)? (2) quickstart do MAX17048 chamado no init? (3) timeout em todas as operações I2C? (4) SOC% bate com voltímetro ±5%?
Listar problemas.
```

## ✎ Prompt de Correção

```
Bateria com problema: [sintoma — ex: SOC sempre 100% independente da carga]
Contexto: E07, MAX17048, I2C, ESP32-S3.
Diagnosticar. Causa provável + fix.
```

## → Prompt de Continuidade

```
E07 concluída. Fuel gauge OK, status de carga OK.
Próxima: E08 (bring-up do IMU).
Mostre como ler aceleração e giroscópio de um MPU-6050 ou ICM-42688 via I2C no ESP32-S3 com ESP-IDF.
```


