#include <X11/Xlib.h>
#include <alsa/asoundlib.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#define BATT_PERCENT "/sys/class/power_supply/BAT0/capacity"
#define BATT_STATUS  "/sys/class/power_supply/BAT0/status"

#define GREY  '\01'
#define WHITE '\02'
#define RED   '\03'
#define BLUE  '\04'

static unsigned short int done;

static const char *battery(void)
{
    static char buf[11], col = '\02';
    static int percent = 0;
    static int c = 0;
    char st, s = ' ';
    FILE *fp;

    if (--c <= 0) {
        if ((fp = fopen(BATT_PERCENT, "r"))) {
            fscanf(fp, "%i", &percent);
            fclose(fp);
        }

        /* Determine the color of the percent value. */
        if (percent >= 70) {
            col = '\05';
        } else if (percent <= 10) {
            col = '\03';
        }
        /* Do this only every 60th call. */
        c = 60;
    }

    if ((fp = fopen(BATT_STATUS, "r"))) {
        st = fgetc(fp);
        fclose(fp);

        /* Translate the first char of the status into +- or ? */
        if ('D' == st) {
            s = '-';
        } else if ('C' == st) {
            s = '+';
        } else if ('F' == st) {
            s = '\0';
        }
    }

    if (s) {
        snprintf(buf, sizeof(buf), "%c\uF241%c%c%i%%", col, s, WHITE, percent);
    } else {
        snprintf(buf, sizeof(buf), "%c\uF241%c%i%%", col, WHITE, percent);
    }
    return buf;
}

static const char *cpuusage()
{
    static char buf[10] = {0};
    static long double work_save, total_save;
    long double a[8], work, total;
    float loadavg;
    FILE *fp;
    static int c = 0;

    if (--c <= 0 && (fp = fopen("/proc/stat", "r"))) {
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7]);
        fclose(fp);

        /* summarize all but idle a[3] and waiting a[4] */
        work    = a[0] + a[1] + a[2] + a[5] + a[6] + a[7];
        total   = work + a[3] + a[4];
        loadavg = (work - work_save) / (total - total_save);

        snprintf(buf, sizeof(buf), "\uF110%.1f%%", loadavg * 100.);

        work_save  = work;
        total_save = total;
        /* Calculate usage for a 3 seconds delay. */
        c = 3;
    }

    return buf;
}

static const char *date_time(void)
{
    static char buf[13];
    time_t now = time(0);

    strftime(buf, sizeof(buf), "%d.%m. %R", localtime(&now));

    return buf;
}

static void die(const char *errstr, ...)
{
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}

static const char *loadavg(void)
{
    static char buf[10];
    double avgs[1];

    if (getloadavg(avgs, 1) < 0) {
        buf[0] = '\0';
    } else {
        snprintf(buf, sizeof(buf), "\uF0AE%.2f", avgs[0]);
    }

    return buf;
}

static void terminate(const int unused)
{
    done = 1;
}

static const char *volume(void)
{
    static char buf[10];
    long max = 0, min = 0, vol = 0;
    int playing = 0, volume;

    snd_mixer_t *handle;
    snd_mixer_elem_t *mas_mixer;
    snd_mixer_selem_id_t *mute_info;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    /* Get mute info. */
    snd_mixer_selem_id_malloc(&mute_info);
    snd_mixer_selem_id_set_name(mute_info, "Master");
    mas_mixer = snd_mixer_find_selem(handle, mute_info);
    if (mas_mixer) {
        snd_mixer_selem_id_t *vol_info;
        snd_mixer_elem_t *pcm_mixer;

        snd_mixer_selem_get_playback_switch(mas_mixer, SND_MIXER_SCHN_MONO, &playing);
        snd_mixer_selem_id_malloc(&vol_info);
        snd_mixer_selem_id_set_name(vol_info, "Master");
        pcm_mixer = snd_mixer_find_selem(handle, vol_info);
        snd_mixer_selem_get_playback_volume_range((snd_mixer_elem_t *)pcm_mixer, &min, &max);
        snd_mixer_selem_get_playback_volume((snd_mixer_elem_t *)pcm_mixer, SND_MIXER_SCHN_MONO, &vol);

        volume = (int)vol * 100 / (int)max;

        snd_mixer_selem_id_free(vol_info);

        if (playing) {
            snprintf(buf, sizeof(buf), "%c\uF028%d%%", WHITE, volume);
        } else {
            snprintf(buf, sizeof(buf), "%c\uF6A9%c%d%%", RED, WHITE, volume);
        }
    } else {
        buf[0] = '-';
        buf[1] = '\0';
    }

    if (mute_info) {
        snd_mixer_selem_id_free(mute_info);
    }
    if (handle) {
        snd_mixer_close(handle);
    }

    return buf;
}

int main(void)
{
    int len = 200;
    char *status;

    if (signal(SIGTERM, terminate) == SIG_ERR) {
        die("Can't install SIGHUP handler");
    }

    if ((status = malloc(sizeof(char) * len)) == NULL) {
        die("Cannot allocate memory.\n");
    }

#ifndef NOX
    Display *display;
    Window root;
    if ((display = XOpenDisplay(NULL)) == NULL ) {
        die("Cannot open display!\n");
    }
    root = DefaultRootWindow(display);
#endif

    while (!done) {
        snprintf(status, len,
                "%c%s %s %s %c%s %c%s",
                WHITE,	cpuusage(),
                	loadavg(),
                	battery(),
                WHITE,	volume(),
                BLUE,	date_time());

#ifndef NOX
        XStoreName(display, root, status);
        XSync(display, 0);
#else
        fprintf(stdout, "%s\n", status);
#endif
        sleep(1);
    }

#ifndef NOX
    free(status);
    XCloseDisplay(display);
#endif

    return 0;
}
