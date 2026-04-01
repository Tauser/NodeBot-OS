#!/usr/bin/env python3
"""
jig_test.py — E41 NodeBot Factory Jig
======================================
Conecta ao ESP32 via serial, executa 9 testes de periféricos e gera JSON.

Uso:
    python jig_test.py --port COM3
    python jig_test.py --port /dev/ttyUSB0 --baud 115200 --timeout 5

Saída:
    JSON em stdout  →  {serial_id, pass_count, fail_count, details, timestamp}
    Log humano em stderr

Critério de PASS por item:
    DISPLAY   "DISPLAY_OK ..."
    SERVO     "SERVO_OK ..."
    MIC       "MIC_OK rms=N"    N > 50
    SPEAKER   "SPEAKER_OK"
    SD        "SD_OK ..."
    BATTERY   "BATTERY_OK pct=N"  N >= 0
    IMU       "IMU_OK ..."
    LED       "LED_OK"
    WIFI      "WIFI_OK networks=N"  N > 0
"""

import argparse
import json
import re
import sys
import time
from datetime import datetime, timezone

try:
    import serial
except ImportError:
    print("Erro: instale pyserial — pip install pyserial", file=sys.stderr)
    sys.exit(1)


# ── Configuração ──────────────────────────────────────────────────────────────

TESTS = [
    "DISPLAY",
    "SERVO",
    "MIC",
    "SPEAKER",
    "SD",
    "BATTERY",
    "IMU",
    "LED",
    "WIFI",
]

BOOT_TIMEOUT   = 10.0   # s para aguardar BOOT_OK após reset
TEST_TIMEOUT   =  5.0   # s por teste
INTER_TEST_MS  = 100    # ms entre testes


# ── Validadores ───────────────────────────────────────────────────────────────

def _validate_mic(response: str) -> tuple[bool, str]:
    """MIC_OK rms=N  onde N > 50"""
    m = re.search(r"rms=(\d+(?:\.\d+)?)", response)
    if not m:
        return False, "resposta sem campo rms"
    rms = float(m.group(1))
    if rms > 50:
        return True, response.strip()
    return False, f"rms={rms:.0f} abaixo do mínimo (50)"


def _validate_battery(response: str) -> tuple[bool, str]:
    """BATTERY_OK pct=N  onde N >= 0 e N <= 100"""
    m = re.search(r"pct=(\d+(?:\.\d+)?)", response)
    if not m:
        return False, "resposta sem campo pct"
    pct = float(m.group(1))
    if 0.0 <= pct <= 100.0:
        return True, response.strip()
    return False, f"pct={pct:.1f} fora do intervalo válido"


def _validate_wifi(response: str) -> tuple[bool, str]:
    """WIFI_OK networks=N  onde N > 0"""
    m = re.search(r"networks=(\d+)", response)
    if not m:
        return False, "resposta sem campo networks"
    n = int(m.group(1))
    if n > 0:
        return True, response.strip()
    return False, f"networks={n} — nenhuma rede encontrada"


VALIDATORS = {
    "MIC":     _validate_mic,
    "BATTERY": _validate_battery,
    "WIFI":    _validate_wifi,
}


def validate(test_name: str, response: str) -> tuple[bool, str]:
    """Retorna (pass, detalhe)."""
    ok_prefix = f"{test_name}_OK"
    fail_prefix = f"{test_name}_FAIL"

    if response.startswith(fail_prefix):
        reason = response[len(fail_prefix):].lstrip(": ").strip()
        return False, reason or "FAIL sem mensagem"

    if not response.startswith(ok_prefix):
        return False, f"resposta inesperada: {response!r}"

    # Validação adicional por teste
    if test_name in VALIDATORS:
        return VALIDATORS[test_name](response)

    return True, response.strip()


# ── Serial helpers ────────────────────────────────────────────────────────────

def readline_timeout(ser: serial.Serial, timeout_s: float) -> str:
    """Lê uma linha; retorna '' se timeout."""
    deadline = time.monotonic() + timeout_s
    buf = b""
    while time.monotonic() < deadline:
        ch = ser.read(1)
        if ch:
            buf += ch
            if ch in (b"\n", b"\r"):
                return buf.decode(errors="replace").strip()
    return ""


def wait_boot_ok(ser: serial.Serial, timeout_s: float) -> bool:
    """Aguarda 'BOOT_OK' no serial. Descarta outras linhas."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        line = readline_timeout(ser, deadline - time.monotonic())
        print(f"[boot] {line}", file=sys.stderr)
        if line == "BOOT_OK":
            return True
    return False


# ── Runner ────────────────────────────────────────────────────────────────────

def run_jig(port: str, baud: int, test_timeout: float) -> dict:
    timestamp = datetime.now(timezone.utc).isoformat()
    details = []
    pass_count = 0
    fail_count = 0

    print(f"[jig] abrindo {port} @ {baud}", file=sys.stderr)

    with serial.Serial(port, baud, timeout=0.1) as ser:
        ser.reset_input_buffer()

        print(f"[jig] aguardando BOOT_OK ({BOOT_TIMEOUT:.0f}s)…", file=sys.stderr)
        if not wait_boot_ok(ser, BOOT_TIMEOUT):
            print("[jig] BOOT_OK não recebido — abortando", file=sys.stderr)
            sys.exit(2)

        print("[jig] boot OK — iniciando testes", file=sys.stderr)

        for test in TESTS:
            cmd = f"TEST_{test}\n"
            ser.write(cmd.encode())
            ser.flush()

            response = readline_timeout(ser, test_timeout)
            passed, detail = validate(test, response)

            status = "PASS" if passed else "FAIL"
            print(f"[{status}] {test:10s} — {detail}", file=sys.stderr)

            details.append({
                "test":   test,
                "result": status,
                "message": detail,
            })

            if passed:
                pass_count += 1
            else:
                fail_count += 1

            time.sleep(INTER_TEST_MS / 1000.0)

    # serial_id: lido do NVS via campo BOOT_OK não está disponível aqui,
    # usa timestamp como identificador de sessão
    serial_id = f"JIG-{datetime.now().strftime('%Y%m%d%H%M%S')}"

    return {
        "serial_id":   serial_id,
        "pass_count":  pass_count,
        "fail_count":  fail_count,
        "details":     details,
        "timestamp":   timestamp,
    }


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="NodeBot factory jig tester")
    parser.add_argument("--port",    required=True,          help="Porta serial (ex: COM3, /dev/ttyUSB0)")
    parser.add_argument("--baud",    type=int, default=115200, help="Baud rate (padrão: 115200)")
    parser.add_argument("--timeout", type=float, default=TEST_TIMEOUT, help="Timeout por teste em segundos")
    args = parser.parse_args()

    result = run_jig(args.port, args.baud, args.timeout)

    # JSON vai para stdout (para integração com automação)
    print(json.dumps(result, indent=2, ensure_ascii=False))

    overall = "PASS" if result["fail_count"] == 0 else "FAIL"
    print(
        f"\n[jig] resultado final: {overall} "
        f"({result['pass_count']}/{len(TESTS)} pass)",
        file=sys.stderr,
    )

    sys.exit(0 if result["fail_count"] == 0 else 1)


if __name__ == "__main__":
    main()
