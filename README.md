# RAPL Energy Readers

Simple programs that read CPU energy usage from Intel RAPL (Running Average Power Limit) on Linux, written in different languages to compare how they behave.

Each reader:
- opens `/sys/class/powercap/intel-rapl:0/energy_uj` once (the CPU package energy counter, in microjoules)
- reads it as fast as possible for 60 seconds
- records a nanosecond timestamp (monotonic clock) with every reading
- writes the results to a CSV file: `sample,time_ns,energy_uj`

## Readers

| Language | Folder | Output file |
|----------|--------|-------------|
| C        | `C_Reader/`    | `energy.csv` |
| Java     | `Java_Reader/` | `energy_java.csv` |

## Requirements

- Linux on an Intel (or recent AMD) CPU with RAPL support
- Root access, because `energy_uj` is only readable by root
- `gcc` for C, JDK 11+ for Java

## Running

**C**
```bash
cd C_Reader
gcc -O2 -o energyreader energyreader.c
sudo ./energyreader
```

**Java**
```bash
cd Java_Reader
javac -d bin src/energyreader/EnergyReader.java
sudo $(which java) -Xmx2g -cp bin energyreader.EnergyReader
```

## Notes

- RAPL updates about every 1 ms, so many consecutive readings repeat the same value. That's expected.
- The readers busy-poll one CPU
