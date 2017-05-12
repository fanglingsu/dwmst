#include <alsa/asoundlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define BATT_NOW    "/sys/class/power_supply/BAT0/energy_now"
#define BATT_FULL   "/sys/class/power_supply/BAT0/energy_full"
#define BATT_STATUS "/sys/class/power_supply/BAT0/status"

#define GREY  '\01'
#define WHITE '\02'
#define RED   '\03'
#define BLUE  '\04'

static const char *cpuusage()
{
    static char buf[7] = {0};
    static long double a[4];
    long double b[4];
    float loadavg;
    FILE *fp;

	if ((fp = fopen("/proc/stat","r")) == NULL) {
		return buf;
	}
	fscanf(fp,"%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);

	loadavg = ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2]))
            / ((b[0] + b[1] + b[2] + b[3]) - (a[0] + a[1] + a[2] + a[3]));

	snprintf(buf, sizeof(buf), "%.1f%%", loadavg * 100.);

    a[0] = b[0];
    a[1] = b[1];
    a[2] = b[2];
    a[3] = b[3];

    return buf;
}

static const char *uptime()
{
    static char buf[10];
    static int c = 0;

    /* Get uptime only all x seconds. */
    if (--c <= 0) {
        float h;
        struct sysinfo info;

        sysinfo(&info);
        h = info.uptime / 3600.;
        /* Round to quoarters or an hour. */
        snprintf(buf, 10, "%c%.1fh", h > 8 ? RED : WHITE, h);
        /* Do uptime check only all x runs. */
        c = 60;
    }

    return buf;
}

static int volume(void)
{
    long max = 0, min = 0, vol = 0;
    int mute = 0, volume;

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
    if (!mas_mixer) {
        return 0;
    }
    snd_mixer_selem_get_playback_switch(mas_mixer, SND_MIXER_SCHN_MONO, &mute);

    if (mute) {
        snd_mixer_selem_id_t *vol_info;
        snd_mixer_elem_t *pcm_mixer;
        snd_mixer_selem_id_malloc(&vol_info);
        snd_mixer_selem_id_set_name(vol_info, "Master");
        pcm_mixer = snd_mixer_find_selem(handle, vol_info);
        snd_mixer_selem_get_playback_volume_range((snd_mixer_elem_t *)pcm_mixer, &min, &max);
        snd_mixer_selem_get_playback_volume((snd_mixer_elem_t *)pcm_mixer, SND_MIXER_SCHN_MONO, &vol);

        volume = (int)vol * 100 / (int)max;

        snd_mixer_selem_id_free(vol_info);
    } else {
        volume = 0;
    }

    if (mute_info) {
        snd_mixer_selem_id_free(mute_info);
    }
    if (handle) {
        snd_mixer_close(handle);
    }
    return volume;
}

static const char *loadavg(void)
{
    static char buf[10];
    double avgs[1];

    if (getloadavg(avgs, 1) < 0) {
        buf[0] = '\0';
    } else {
        snprintf(buf, 10, "%c%.2f", WHITE, avgs[0]);
    }

    return buf;
}

static const char *date_time(void)
{
    static char buf[14], date[13];
    time_t now = time(0);

    strftime(date, 13, "%d.%m. %R", localtime(&now));
    snprintf(buf, 14, "%c%s", BLUE, date);

    return buf;
}

static const char *battery(void)
{
    static char buf[6], col = '\02';
    static float percent;
    static int c = 0;
    char st = 'U', s = '?';
    FILE *fp;

    if (--c <= 0) {
        int enow = 0, efull = 0;

        if ((fp = fopen(BATT_NOW, "r"))) {
            fscanf(fp, "%d", &enow);
            fclose(fp);
        }
        if ((fp = fopen(BATT_FULL, "r"))) {
            fscanf(fp, "%d", &efull);
            fclose(fp);
        }

        percent = 100 * ((float)enow / efull);
        /* Determine the color of the percent value. */
        if (percent >= 70.) {
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
            s = '=';
        }
    }

    snprintf(buf, sizeof(buf), "%c%c%.0f%%", s, col, percent);
    return buf;
}

int main(void)
{
    int len = 200;
    Display *display;
    Window root;

    char *status;

    if ((display = XOpenDisplay(NULL)) == NULL ) {
        fprintf(stderr, "Cannot open display!\n");
        return 1;
    }
    root = DefaultRootWindow(display);

    if ((status = malloc(len)) == NULL) {
        fprintf(stderr, "Cannot allocate memory.\n");
        return 1;
    }

    while (1) {
        snprintf(status, len,
                "%s %cC%c%s %c♡%s %c±%c%s %c♫%c%d%% %s",
                uptime(),
                GREY, WHITE, cpuusage(),
                WHITE, loadavg(),
                GREY, WHITE, battery(),
                GREY, WHITE, volume(),
                date_time());

        XStoreName(display, root, status);
        XSync(display, 0);

        sleep(1);
    }

    /* Never reached code. */
    free(status);
    XCloseDisplay(display);

    return 0;
}
