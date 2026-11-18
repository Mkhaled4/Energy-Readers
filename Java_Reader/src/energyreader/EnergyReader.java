package energyreader;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;

public class EnergyReader {

    private static final String RAPL_PATH   = "/sys/class/powercap/intel-rapl:0/energy_uj";
    private static final long   DURATION_NS = 60_000_000_000L;   // 60 s
    private static final int    MAX_SAMPLES = 50_000_000;        // safety cap on the buffer

    private final FileChannel channel;
    private final ByteBuffer buf = ByteBuffer.allocateDirect(32);

    public EnergyReader(String path) throws IOException {
        channel = FileChannel.open(Paths.get(path), StandardOpenOption.READ);
    }

    // Same idea as pread(fd, buf, n, 0): read from offset 0 without reopening
    public long read() throws IOException {
        buf.clear();
        int n = channel.read(buf, 0);
        if (n <= 0) {
            throw new IOException("empty read from energy_uj");
        }
        long value = 0;
        for (int i = 0; i < n; i++) {
            byte b = buf.get(i);
            if (b < '0' || b > '9') break;           // stop at '\n'
            value = value * 10 + (b - '0');
        }
        return value;
    }

    public void close() throws IOException {
        channel.close();
    }

    static void writeCsv(String path, long[] times, long[] energy, int count) throws IOException {
        try (BufferedWriter w = new BufferedWriter(new FileWriter(path), 1 << 20)) {
            w.write("sample,time_ns,energy_uj\n");
            StringBuilder sb = new StringBuilder(64);
            for (int i = 0; i < count; i++) {
                sb.setLength(0);
                sb.append(i + 1).append(',')
                  .append(times[i]).append(',')
                  .append(energy[i]).append('\n');
                w.append(sb);
            }
        }
    }

    public static void main(String[] args) {
        long[] times  = new long[MAX_SAMPLES];
        long[] energy = new long[MAX_SAMPLES];

        EnergyReader reader;
        try {
            reader = new EnergyReader(RAPL_PATH);
        } catch (IOException e) {
            System.err.println("open energy_uj (try sudo): " + e);
            System.exit(1);
            return;
        }

        long start  = System.nanoTime();
        long target = start + DURATION_NS;
        long now    = start;
        int i = 0;

        try {
            do {
                long e = reader.read();
                now = System.nanoTime();
                times[i]  = now - start;                 // ns since start
                energy[i] = e;
                i++;
            } while (i < MAX_SAMPLES && now - target < 0);
        } catch (IOException e) {
            System.err.println("read energy_uj: " + e);  // stop on failed read
        }

        if (i == MAX_SAMPLES) {
            System.err.println("warning: buffer full, stopped early");
        }

        System.out.printf("%d samples in %.3f s%n", i, (now - start) / 1e9);

        try {
            writeCsv("energy_java.csv", times, energy, i);
            reader.close();
        } catch (IOException e) {
            System.err.println("write csv: " + e);
            System.exit(1);
        }
    }
}
