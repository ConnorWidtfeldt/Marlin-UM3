#!/usr/bin/env python3
"""Reproduce fixtures from selected shipped ARM instructions, without running the application.

Requires pinned optional requirements-recovery.txt. Mapped code runs only in
Unicorn, with instruction limits. No target entry point or dynamic linker runs.
"""

import argparse
import hashlib
import json
import struct
import unicorn
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_FPEXC,
    UC_ARM_REG_SP,
    UC_ARM_REG_LR,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_S0,
    UC_ARM_REG_PC,
)


class Replay:
    def __init__(self, artifact):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
        with artifact.open("rb") as f:
            elf = ELFFile(f)
            segments = [
                (s["p_vaddr"], s["p_memsz"], s.data())
                for s in elf.iter_segments()
                if s["p_type"] == "PT_LOAD"
            ]
        start = min(a for a, n, b in segments) & ~4095
        end = (max(a + n for a, n, b in segments) + 4095) & ~4095
        self.uc.mem_map(start, end - start)
        for address, size, data in segments:
            self.uc.mem_write(address, data)
        self.uc.mem_map(0x200000, 0x10000)
        self.uc.mem_map(0x300000, 0x10000)
        self.uc.mem_map(0x400000, 4096)
        self.uc.reg_write(UC_ARM_REG_C1_C0_2, 0xF00000)
        self.uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)

    def call(self, address, args=(), sample=None):
        self.uc.reg_write(UC_ARM_REG_SP, 0x20FFF0)
        self.uc.reg_write(UC_ARM_REG_LR, 0x400001)
        for r, v in zip(
            [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args
        ):
            self.uc.reg_write(r, v & 0xFFFFFFFF)
        if sample is not None:
            self.uc.reg_write(
                UC_ARM_REG_S0, struct.unpack("<I", struct.pack("<f", sample))[0]
            )
        self.uc.emu_start(address | 1, 0x400000, count=1000000)
        if self.uc.reg_read(UC_ARM_REG_PC) != 0x400000:
            raise RuntimeError("Instruction limit did not reach return")
        return self.uc.reg_read(UC_ARM_REG_R0)

    def float_at(self, address, value):
        self.uc.mem_write(address, struct.pack("<f", value))


EXPECTED = "bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee"


def generate(simulator):
    if unicorn.__version__ != "2.1.4":
        raise ValueError("Fixture replay requires the pinned Unicorn 2.1.4")
    if hashlib.sha256(simulator.read_bytes()).hexdigest() != EXPECTED:
        raise ValueError("Unrecognized stock simulator SHA256")
    cases = []
    configs = [
        (
            "hotend_ramp",
            dict(
                target=210,
                kff=0.57,
                kp=9,
                ki=0.2,
                kd=40,
                ki_max=100,
                kpcf=0.06,
                functional_range=25,
                min=5,
                max=365,
            ),
            [20, 20, 180] + list(range(181, 216)) + [210] * 12,
        ),
        (
            "sample_rejection",
            dict(
                target=200,
                kff=0.57,
                kp=9,
                ki=0.2,
                kd=40,
                ki_max=100,
                kpcf=0.06,
                functional_range=25,
                min=5,
                max=365,
            ),
            [
                20,
                20,
                30,
                31,
                196,
                197,
                198,
                199,
                200,
                201,
                202,
                203,
                204,
                204,
                210,
                210,
                205,
                200,
                195,
            ],
        ),
        (
            "bed_ramp",
            dict(
                target=60,
                kff=2.5,
                kp=125,
                ki=0,
                kd=0,
                ki_max=60,
                kpcf=0,
                functional_range=25,
                min=5,
                max=200,
            ),
            [20, 20] + list(range(21, 66)) + [60] * 8,
        ),
        (
            "integral_clamp",
            dict(
                target=100,
                kff=0,
                kp=0,
                ki=2,
                kd=0,
                ki_max=7,
                kpcf=0,
                functional_range=25,
                min=5,
                max=365,
            ),
            [99] * 16 + [101] * 16,
        ),
        (
            "target_off",
            dict(
                target=0,
                kff=0.57,
                kp=9,
                ki=0.2,
                kd=40,
                ki_max=100,
                kpcf=0.06,
                functional_range=25,
                min=5,
                max=365,
            ),
            [20, 20, 19, 18, 17, 16, 15, 10, 5, 0],
        ),
        (
            "control_bounds",
            dict(
                target=200,
                kff=0,
                kp=1,
                ki=0,
                kd=0,
                ki_max=100,
                kpcf=0,
                functional_range=25,
                min=190,
                max=205,
            ),
            [185, 185, 186, 190, 195, 200, 205, 206, 207, 210, 210],
        ),
    ]
    for name, settings, temps in configs:
        r = Replay(simulator)
        obj = 0x300000
        r.call(0x35D10, [obj])
        f32 = lambda value: struct.unpack("<f", struct.pack("<f", value))[0]
        fields = {
            0: settings["kff"],
            4: settings["kp"],
            8: f32(settings["ki"]) * 0.032768,
            12: f32(settings["kd"]) / 0.032768,
            16: settings["ki_max"],
            20: settings["kpcf"],
            32: settings["functional_range"],
            36: settings["min"],
            40: settings["max"],
            48: settings["target"],
        }
        for off, value in fields.items():
            r.float_at(obj + off, value)
        samples = []
        for i, t in enumerate(temps):
            fan = [0, 32, 128, 255][(i // 7) % 4]
            r.uc.mem_write(0x580C8, bytes([fan]))
            samples.append(
                {"temperature": t, "fan": fan, "output": r.call(0x35DAC, [obj], t)}
            )
        cases.append({"name": name, "settings": settings, "samples": samples})
    conversions = []
    for kind, addr, count in [("hotend", 0x4046C, 13), ("bed", 0x403F8, 29)]:
        r = Replay(simulator)
        knots = [
            struct.unpack("<hh", r.uc.mem_read(addr + 4 * i, 4))[0]
            for i in range(count)
        ]
        values = sorted(
            set(
                knots
                + [int((a + b) / 2) for a, b in zip(knots, knots[1:])]
                + [-32768, 32767]
            )
        )
        for raw in values:
            r.call(0x34A80, [raw, addr, count])
            celsius = struct.unpack(
                "<f",
                struct.pack(
                    "<I",
                    r.uc.reg_read(UC_ARM_REG_S0),
                ),
            )[0]
            conversions.append({"kind": kind, "raw_sum": raw, "celsius": celsius})
    power = []
    for temp in [0, 20, 60, 115]:
        for remaining in [25, 100, 175]:
            r = Replay(simulator)
            obj = 0x300000
            r.uc.mem_write(obj, bytes([255]))
            for off, value in {
                4: remaining,
                8: 24,
                12: temp,
                16: 3.678255,
                20: 0.012285,
            }.items():
                r.float_at(obj + off, value)
            power.append(
                {
                    "temperature": temp,
                    "remaining_watts": remaining,
                    "requested": 255,
                    "output": r.call(0x36778, [obj]),
                }
            )

    removals = []
    for initial in [False, True]:
        for request, temp, target in [
            (True, 20, 0),
            (True, 70, 0),
            (True, 70.01, 0),
            (True, 20, 1),
            (False, 200, 210),
            (False, 20, 0),
        ]:
            r = Replay(simulator)
            r.uc.mem_write(0x5756C, bytes([initial]))
            r.float_at(0x58A10, temp)
            r.float_at(0x58844 + 48, target)
            r.call(0x3443C, [0, int(request)])
            removals.append(
                {
                    "initial_allowed": initial,
                    "requested": request,
                    "temperature": temp,
                    "target": target,
                    "allowed": bool(r.uc.mem_read(0x5756C, 1)[0]),
                }
            )
    watchdogs = []
    sequences = [
        ("deadline_exact", [(100, 20, 255), (30100, 21, 255), (30101, 21, 255)]),
        (
            "progress",
            [
                (100, 20, 255),
                (20000, 21.001, 255),
                (50000, 21.5, 255),
                (50001, 21.5, 255),
            ],
        ),
        (
            "partial_output_reset",
            [
                (100, 20, 255),
                (30099, 20, 254),
                (60000, 20, 255),
                (90000, 20, 255),
                (90001, 20, 255),
            ],
        ),
        (
            "zero_sentinel",
            [(0, 20, 255), (30001, 20, 255), (60001, 20, 255), (60002, 20, 255)],
        ),
        ("clock_wrap", [(4294967200, 20, 255), (29904, 20, 255), (29905, 20, 255)]),
    ]
    for name, sequence in sequences:
        r = Replay(simulator)
        state = {"now": 0, "disabled": False, "stop": None}

        def boundary(uc, address, size, data):
            if address == 0x39D34:
                uc.reg_write(UC_ARM_REG_R0, state["now"])
            elif address == 0x35288:
                state["disabled"] = True
            elif address == 0x27168:
                state["stop"] = uc.reg_read(UC_ARM_REG_R0) & 255
            else:
                return
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

        r.uc.hook_add(UC_HOOK_CODE, boundary)
        samples = []
        for now, temp, pwm in sequence:
            state.update(now=now, disabled=False, stop=None)
            r.uc.mem_write(0x58A48, bytes([pwm]))
            r.float_at(0x58A10, temp)
            r.call(0x344CC, [0])
            assert state["disabled"] == (state["stop"] is not None)
            samples.append(
                {
                    "temperature": temp,
                    "now_ms": now,
                    "pwm": pwm,
                    "fault": state["disabled"],
                    "stop_reason": state["stop"],
                }
            )
        watchdogs.append({"name": name, "samples": samples})

    result = {
        "schema_version": 1,
        "simulator_sha256": EXPECTED,
        "authority": "controlled function replay of shipped ARM instructions; not physical AVR execution",
        "engine": "Unicorn 2.1.4",
        "functions": {
            "pid_constructor": "0x35d10",
            "pid_update": "0x35dac",
            "analog2temp_sub": "0x34a80",
            "bed_budget": "0x36778",
            "removal_interlock": "0x3443c",
            "heating_watchdog": "0x344cc",
        },
        "notes": [
            "Watchdog replay mocks only the clock, heater-disable and stop call boundaries.",
            "ELF load segments mapped without running its entry point or dynamic linker.",
            "Only selected functions run; debug serial output disabled.",
            "Control ranges, coefficients and fan byte supplied as explicit fixture inputs.",
            "ARM double is 64-bit; AVR double is 32-bit, so parity is bounded by architecture.",
        ],
        "pid_cases": cases,
        "conversion_cases": conversions,
        "bed_budget_cases": power,
        "removal_cases": removals,
        "watchdog_cases": watchdogs,
    }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--output", type=Path)
    mode.add_argument("--check", type=Path)
    args = parser.parse_args()
    result = generate(args.simulator)
    if args.check:
        if result != json.loads(args.check.read_text()):
            raise SystemExit("Stock instruction replay differs from recorded fixtures")
        print("Stock instruction replay matches all recorded fixtures")
    else:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
        print("Wrote binary-derived fixtures to", args.output)


if __name__ == "__main__":
    main()
