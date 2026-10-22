"""Read-only YF-RS485-640 serial capture. Never writes sensor configuration."""
import argparse
import datetime
import json
import math
import pathlib
import sys
import time


def crc16(data):
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def read_request(address, function, start, count):
    if not 1 <= address <= 247 or function not in (0x03, 0x65):
        raise ValueError("Only addressed read requests (0x03/0x65) are allowed")
    maximum = 125 if function == 0x03 else 1024
    if not 0 <= start <= 65535 or not 1 <= count <= maximum or start + count > 65536:
        raise ValueError("Invalid register span")
    body = bytes((address, function, start >> 8, start & 255, count >> 8, count & 255))
    return body + crc16(body).to_bytes(2, "little")


def _serial_factory(**kwargs):
    try:
        import serial
    except ImportError as error:
        raise RuntimeError("pyserial is required for hardware capture: python -m pip install pyserial==3.5") from error
    return serial.Serial(**kwargs)


def capture(port, baud, stopbits, seconds, output, *, serial_factory=None, clock=time.monotonic_ns):
    if not port or baud not in (9600, 115200) or stopbits not in (1, 2):
        raise ValueError("Explicit port, confirmed baud 9600/115200 and stopbits 1/2 required")
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("Capture duration must be finite and positive")
    request = read_request(1, 0x65, 0, 0x0311)
    target = pathlib.Path(output)
    target.mkdir(parents=True, exist_ok=False)
    metadata = {
        "port": port, "baud": baud, "stopbits": stopbits, "seconds_requested": seconds,
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "hardware_validated": False, "timestamps": "host reception monotonic_ns, not exposure",
        "status": "starting", "rx_bytes": 0, "requests": 0,
    }
    device = None
    start_ns = clock()
    stop_ns = start_ns + int(seconds * 1_000_000_000)
    wire_seconds = 1576 * (9 + stopbits) / baud
    request_period = int(max(0.25, wire_seconds + 0.03) * 1_000_000_000)
    response_timeout = int(max(0.4, wire_seconds + 0.25) * 1_000_000_000)
    next_request = start_ns
    deadline = start_ns
    pending = False
    response = bytearray()
    try:
        device = (serial_factory or _serial_factory)(
            port=port, baudrate=baud, bytesize=8, parity="N", stopbits=stopbits,
            timeout=0.02, write_timeout=0.2)
        with (target / "traffic.jsonl").open("x", encoding="utf-8") as traffic:
            def record(direction, chunk):
                traffic.write(json.dumps({"monotonic_ns": clock(), "direction": direction,
                                          "hex": bytes(chunk).hex()}) + "\n")
                traffic.flush()
            while clock() < stop_ns:
                now = clock()
                if pending and now >= deadline:
                    pending = False
                if not pending and now >= next_request:
                    written = device.write(request)
                    if written != len(request):
                        raise OSError("Short RS485 request write")
                    record("tx", request)
                    metadata["requests"] += 1
                    next_request, deadline = now + request_period, now + response_timeout
                    pending = True
                    response.clear()
                chunk = device.read(512)
                if chunk:
                    record("rx", chunk)
                    metadata["rx_bytes"] += len(chunk)
                    # This controls polling only, never claims temperatures are valid.
                    if len(response) < 2048:
                        response.extend(chunk[:2048 - len(response)])
                    if len(response) >= 5 and response[0] == 1:
                        if response[1] == 0xE5:
                            pending = False
                        elif response[1] == 0x65:
                            size = int.from_bytes(response[2:4], "big") + 6
                            if 6 <= size <= 2048 and len(response) >= size:
                                pending = False
        metadata["status"] = "captured"
        return metadata
    except BaseException:
        metadata["status"] = "error"
        raise
    finally:
        try:
            if device is not None:
                device.close()
        finally:
            metadata["elapsed_ns"] = max(0, clock() - start_ns)
            (target / "metadata.json").write_text(
                json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, choices=(9600, 115200))
    parser.add_argument("--stopbits", type=int, choices=(1, 2))
    parser.add_argument("--seconds", type=float)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args(argv)
    if args.list_ports:
        try:
            from serial.tools import list_ports
        except ImportError:
            parser.error("Install pyserial==3.5 to enumerate hardware ports")
        for port in list_ports.comports():
            print(f"{port.device}: {port.description}")
        return 0
    if any(value is None for value in (args.port, args.baud, args.stopbits, args.seconds, args.output)):
        parser.error("--port --baud --stopbits --seconds --output must all be explicit")
    try:
        result = capture(args.port, args.baud, args.stopbits, args.seconds, args.output)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Capture failed: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    print(f"Captured {result['rx_bytes']} raw bytes; hardware_validated=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
