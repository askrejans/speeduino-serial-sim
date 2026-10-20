/**
 * @file main_native.cpp
 * @brief Desktop simulator: the same model and protocol as the firmware,
 *        on a TCP port or a pseudo-terminal.
 *
 *   .pio/build/native/program                    # TCP 127.0.0.1:5000
 *   .pio/build/native/program --pty              # prints /dev/ttys…
 *   .pio/build/native/program --speed 10 --laps 3 --pace 0.95
 */
#if defined(NATIVE_SIM) && !defined(ARDUINO) && !defined(UNIT_TEST)

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "EngineSimulator.h"
#include "NativeAdapters.h"
#include "SpeeduinoProtocol.h"

static volatile bool stop = false;
static void onSignal(int) { stop = true; }

static void usage() {
    fprintf(stderr,
            "speeduino-serial-sim " FIRMWARE_VERSION " (native)\n"
            "  --bind ADDR      TCP bind address (default 127.0.0.1)\n"
            "  --port N         TCP port (default 5000)\n"
            "  --pty            serve on a pseudo-terminal instead of TCP\n"
            "  --speed X        run X times faster than real time (default 1)\n"
            "  --pace P         hot-lap pace, 0.5 … 1.05 of the grip envelope (default 1.0)\n"
            "  --laps N         hot laps per run (default 5)\n"
            "  --ambient C      ambient temperature (default 18)\n"
            "  --coolant C      coolant at key-on (default 55)\n"
            "  --no-launch      pull away without launch control\n"
            "  --no-noise       noiseless sensors\n"
            "  --quiet          no status line\n");
}

int main(int argc, char** argv) {
    const char* bind = "127.0.0.1";
    uint16_t port = 5000;
    bool pty = false, quiet = false;
    double speed = 1.0;
    SimSettings settings;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        auto next = [&](void) -> const char* {
            if (i + 1 >= argc) { usage(); exit(2); }
            return argv[++i];
        };
        if (!strcmp(a, "--bind")) bind = next();
        else if (!strcmp(a, "--port")) port = (uint16_t)atoi(next());
        else if (!strcmp(a, "--pty")) pty = true;
        else if (!strcmp(a, "--speed")) speed = atof(next());
        else if (!strcmp(a, "--pace")) settings.pace = (float)atof(next());
        else if (!strcmp(a, "--laps")) settings.hotLaps = (uint8_t)atoi(next());
        else if (!strcmp(a, "--ambient")) settings.ambientC = (float)atof(next());
        else if (!strcmp(a, "--coolant")) settings.startCoolantC = (float)atof(next());
        else if (!strcmp(a, "--no-launch")) settings.launchControl = false;
        else if (!strcmp(a, "--no-noise")) settings.sensorNoise = false;
        else if (!strcmp(a, "--quiet")) quiet = true;
        else { usage(); return strcmp(a, "--help") && strcmp(a, "-h") ? 2 : 0; }
    }
    if (speed <= 0) speed = 1;
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    signal(SIGPIPE, SIG_IGN);

    NativeTime clock(speed);
    NativeRandom rng;
    TcpSerial* tcp = nullptr;
    PtySerial* term = nullptr;
    ISerialInterface* line;
    if (pty) {
        term = new PtySerial();
        if (!term->ok()) { fprintf(stderr, "cannot open a pseudo-terminal\n"); return 1; }
        printf("serial device: %s\n", term->path());
        line = term;
    } else {
        tcp = new TcpSerial(bind, port);
        if (!tcp->listening()) return 1;
        printf("listening on tcp://%s:%u\n", bind, port);
        line = tcp;
    }
    fflush(stdout);

    EngineSimulator sim(&clock, &rng);
    sim.settings() = settings;
    sim.begin();
    SpeeduinoProtocol protocol(line, &sim, &clock);
    protocol.begin();

    uint32_t lastPrint = 0;
    while (!stop) {
        sim.update();
        bool busy = protocol.poll();
        uint32_t now = clock.millis();
        if (!quiet && now - lastPrint >= 1000) {
            lastPrint = now;
            const EngineStatus& s = sim.getStatus();
            const Telemetry& t = sim.telemetry();
            fprintf(stderr,
                    "\r%-9s lap %2u %6.1fs best %6.1fs | %5.1f km/h g%u %5u rpm TPS %5.1f%% MAP %3u CLT %3d°C AFR %4.1f PW %4.2fms   ",
                    phaseName(sim.phase()), t.lap, t.lapTimeS, t.bestLapS, t.speedKmh, t.gear, s.rpm(),
                    s.tpsPercent(), s.mapKpa(), s.coolantC(), s.o2 / 10.0, s.pulseWidthUs() / 1000.0);
        }
        if (!busy) clock.delayMicroseconds((uint32_t)(500 * speed));
    }
    fprintf(stderr, "\n");
    delete tcp;
    delete term;
    return 0;
}

#endif
