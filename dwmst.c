#include <alsa/asoundlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define GREY  '\01'
#define WHITE '\02'
#define RED   '\03'
#define BLUE  '\04'

static const char *uptime()
{
    static char buf[10];
    struct sysinfo info;
    float h;

    sysinfo(&info);
    h = info.uptime / 3600.;
    snprintf(buf, 10, "%c%.1fh", h > 8 ? RED : WHITE, h);

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

char battery_status(void)
{
    FILE *bs;
    char st;

    if ((bs = fopen("/sys/class/power_supply/BAT0/status", "r")) == NULL) {
        return 'U';
    }

    st = fgetc(bs);
    fclose(bs);

    return tolower(st);
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
                "%s %c♡%s %c±%c%c %c♫%c%d%% %s",
                uptime(),
                WHITE, loadavg(),
                GREY, WHITE, battery_status(),
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
