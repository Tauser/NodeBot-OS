# E08 - Bring-up: IMU

- Status: ✅ Critérios atendidos
- Complexidade: Baixa
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P2
- Risco: BAIXO
- Depende de: E07 (I2C validado)

## Bring-up: IMU

Complexidade: Baixa
Depende de: E07 (I2C validado)
Grupo: Bring-up HW
HW Real: SIM
ID: E08
Prioridade: P2
Risco: BAIXO
Status: ✅ Critérios atendidos

## Objetivo

IMU lendo aceleração e giroscópio. Detecção básica de orientação. Base para IMUService na E27.

## Critérios de pronto

- O que NÃO entra
    
    
    | accel_z | em repouso entre 950–1050mg |
    | --- | --- |
- Giroscópio em repouso: < 2 dps de drift em todos os eixos
- Inclinar 45°: pitch/roll calculados corretos ± 5°

## Testes mínimos

- Repouso plano: accel_z ~1000mg, giroscópio < 2 dps
- Inclinar lentamente 45°: pitch ou roll muda de 0→~45°
- Agitar: magnitude de aceleração > 2000mg

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, I2C mesmo barramento da bateria, IMU [MPU-6050 endereço 0x68 OU ICM-42688 endereço 0x68].
Tarefa: imu_driver.h + imu_driver.c.
Funções: imu_init(), imu_get_accel_mg(int16_t *x, *y, *z), imu_get_gyro_dps(int16_t *x, *y, *z).
Config: ±4g acelerômetro, ±500dps giroscópio, ODR 100Hz.
Incluir: calcular magnitude √(x²+y²+z²) em repouso — deve ser ~1000mg.
Saída: driver + teste que imprime aceleração e giroscópio a cada 100ms.
```

## ◎ Prompt de Revisão

```
IMU driver da E08.
Verificar: (1) endereço I2C correto? (2) sem conflito com MAX17048/bq25185? (3) magnitude em repouso entre 950–1050mg? (4) timeout em operações I2C?
Listar problemas.
```

## ✎ Prompt de Correção

```
IMU com problema: [sintoma]
Contexto: E08, IMU, I2C.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E08 concluída. IMU lendo aceleração e giroscópio.
Próxima: E09 (bring-up de touch capacitivo).
Mostre como ler fita de cobre como sensor capacitivo ou resistivo no ESP32-S3 — qual é a melhor abordagem com ADC ou touch peripheral?
```


