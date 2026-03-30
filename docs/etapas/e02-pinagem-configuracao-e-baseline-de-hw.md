# E02 - Pinagem, Configuração e Baseline de HW

- Status: ✅ Critérios atendidos
- Complexidade: Baixa
- Grupo: Fundação
- HW Real: SIM
- Prioridade: P1
- Risco: BAIXO
- Depende de: E01

## Pinagem, Configuração e Baseline de HW

Complexidade: Baixa
Depende de: E01
Grupo: Fundação
HW Real: SIM
ID: E02
Prioridade: P1
Risco: BAIXO
Status: ✅ Critérios atendidos

## Objetivo

Documentar e congelar a pinagem de todos os periféricos no hal_init.h. Nenhum driver será escrito sem a pinagem definida aqui.

## Por que agora

Pinagem mudada depois de drivers escritos implica refactoring de todos os drivers. Congela uma vez agora.

## O que implementar

- Mapear todos os GPIOs do ESP32-S3 para cada periférico
- Escrever hal_init.h com #defines de todos os pinos
- Verificar conflitos de GPIO (ST7789 em SPI dedicado, SD onboard em SDMMC, I2C bateria/IMU/câmera)
- Documentar configurações de protocolo: SPI freq, I2C addr, UART baud, I2S format
- Criar hardware_[notes.md](http://notes.md) com observações de timing, conflitos e limitações físicas
- Revisar se algum GPIO tem conflito com boot strapping pins (GPIO 0, 3, 45, 46)

## O que NÃO entra

- Escrever qualquer driver — apenas mapear pinos
- Testar qualquer periférico nesta etapa

## Critérios de pronto

- hal_init.h congelado e commitado
- Sem conflitos de GPIO pendentes
- Qualquer pessoa lê o arquivo e entende a pinagem em < 5 minutos

## Testes mínimos

- Revisão de datasheet: cruzar cada GPIO com tabela do ESP32-S3 Technical Reference
- Verificar ausência de boot strapping pins em uso errado

---

## ▶ Prompt Principal

```
Placa: Freenove ESP32-S3-WROOM CAM N16R8, ESP-IDF v5.x.

Gere src/platform/hal_init.h com #define de pinos agrupados 
por protocolo para:
- SPI: ST7789 (CS/DC/RST), MOSI/MISO/CLK
- SDMMC: microSD onboard (SD_DATA=40, SD_CLK=39, SD_CMD=38)
- UART: servo TX/RX
- I2S: microfone INMP441 (WS/SCK/SD) + speaker MAX98357A (BCLK/LRC/DIN)
- I2C: SDA/SCL compartilhado (IMU + MAX17048 + bq25185)
- RMT: WS2812 onboard (GPIO48)
- CAMERA: OV2640 onboard com pinagem fixa da placa
- TOUCH: 1 pino nativo ESP32-S3 (fita de cobre)

Restrições:
- Não usar GPIO 0, 3, 45, 46 (strapping)
- Não redefinir pinos da OV2640 (fixos na placa)
- Comentar no topo: "PINAGEM CONGELADA v1.0"

Saída: hal_init.h + lista de conflitos identificados.
```

## ◎ Prompt de Revisão

```
hal_init.h da E02.
Verificar: (1) todos os periféricos mapeados? (2) boot strapping pins (0,3,45,46) usados incorretamente? (3) display está em SPI e microSD onboard está em SDMMC dedicado? (4) I2C tem endereços únicos (MAX17048=0x36, bq25185=0x6B, IMU=0x68)? (5) hardware_notes.md documentado?
Listar conflitos encontrados.
```

## ✎ Prompt de Correção

```
Conflito de GPIO identificado: [descreva o conflito]
Contexto: E02, ESP32-S3, hal_init.h.
Propor solução sem alterar outros pinos já definidos.
Saída: pino alternativo + justificativa.
```

## → Prompt de Continuidade

```
E02 concluída. Pinagem congelada em hal_init.h, sem conflitos.
Próxima: E03 (bring-up do display ST7789).
Mostre o driver mínimo para ST7789 via SPI no ESP32-S3 com ESP-IDF, inicialização, fill_color() e draw_pixel(). Usar os pinos do hal_init.h.
```


