/*
 * Copyright (C) 2016 The CyanogenMod Project
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "lights"
#define LOG_NDEBUG 0
#include <log/log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)<(b)?(b):(a))
#endif

#define	PWM_LUT_MAX_SIZE		63
#define PM_PWM_LUT_LOOP			0x01
#define PM_PWM_LUT_RAMP_UP		0x02
#define PM_PWM_LUT_REVERSE		0x04
#define PM_PWM_LUT_PAUSE_HI_EN		0x08
#define PM_PWM_LUT_PAUSE_LO_EN		0x10
#define PM_PWM_LUT_NO_TABLE		0x20
#define PM_PWM_LUT_USE_RAW_VALUE	0x40

#define LCD_FILE "/sys/class/leds/lcd-backlight/brightness"

#define KEYBOARD_PWM_1_BRIGHTNESS_FILE "/sys/class/leds/kpdbl-pwm-1/brightness"
#define KEYBOARD_PWM_2_BRIGHTNESS_FILE "/sys/class/leds/kpdbl-pwm-2/brightness"
#define KEYBOARD_PWM_3_BRIGHTNESS_FILE "/sys/class/leds/kpdbl-pwm-3/brightness"
#define KEYBOARD_PWM_4_BRIGHTNESS_FILE "/sys/class/leds/kpdbl-pwm-4/brightness"

#define RED_BRIGHTNESS_FILE "/sys/class/leds/led:rgb_red/brightness"
#define RED_DUTY_PCTS_FILE "/sys/class/leds/led:rgb_red/duty_pcts"
#define RED_START_IDX_FILE "/sys/class/leds/led:rgb_red/start_idx"
#define RED_PAUSE_LO_FILE "/sys/class/leds/led:rgb_red/pause_lo"
#define RED_PAUSE_HI_FILE "/sys/class/leds/led:rgb_red/pause_hi"
#define RED_RAMP_STEP_MS_FILE "/sys/class/leds/led:rgb_red/ramp_step_ms"
#define RED_LUT_FLAGS_FILE "/sys/class/leds/led:rgb_red/lut_flags"
#define RED_BLINK_FILE "/sys/class/leds/led:rgb_red/blink"

#define GREEN_BRIGHTNESS_FILE "/sys/class/leds/led:rgb_green/brightness"
#define GREEN_DUTY_PCTS_FILE "/sys/class/leds/led:rgb_green/duty_pcts"
#define GREEN_START_IDX_FILE "/sys/class/leds/led:rgb_green/start_idx"
#define GREEN_PAUSE_LO_FILE "/sys/class/leds/led:rgb_green/pause_lo"
#define GREEN_PAUSE_HI_FILE "/sys/class/leds/led:rgb_green/pause_hi"
#define GREEN_RAMP_STEP_MS_FILE "/sys/class/leds/led:rgb_green/ramp_step_ms"
#define GREEN_LUT_FLAGS_FILE "/sys/class/leds/led:rgb_green/lut_flags"
#define GREEN_BLINK_FILE "/sys/class/leds/led:rgb_green/blink"

#define BLUE_BRIGHTNESS_FILE "/sys/class/leds/led:rgb_blue/brightness"
#define BLUE_DUTY_PCTS_FILE "/sys/class/leds/led:rgb_blue/duty_pcts"
#define BLUE_START_IDX_FILE "/sys/class/leds/led:rgb_blue/start_idx"
#define BLUE_PAUSE_LO_FILE "/sys/class/leds/led:rgb_blue/pause_lo"
#define BLUE_PAUSE_HI_FILE "/sys/class/leds/led:rgb_blue/pause_hi"
#define BLUE_RAMP_STEP_MS_FILE "/sys/class/leds/led:rgb_blue/ramp_step_ms"
#define BLUE_LUT_FLAGS_FILE "/sys/class/leds/led:rgb_blue/lut_flags"
#define BLUE_BLINK_FILE "/sys/class/leds/led:rgb_blue/blink"

#define LED_DUTY_STEPS       50
#define LED_RAMP_MS          500

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_battery;
static struct light_state_t g_notification;
static struct light_state_t g_attention;

static int read_int(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buffer[20];
        int amt = read(fd, buffer, sizeof(buffer)-1);
        close(fd);
        if (amt <= 0) return -1;
        buffer[amt] = '\0';
        return atoi(buffer);
    } else {
        ALOGE("read_int failed to open %s\n", path);
        return -errno;
    }
}

static int write_int(const char *path, int value)
{
    int fd = open(path, O_WRONLY);

    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        ALOGE("write_int failed to open %s\n", path);
        return -errno;
    }
}

static int write_str(const char *path, char *value)
{
    int fd = open(path, O_WRONLY);

    if (fd >= 0) {
        char buffer[1024];
        int bytes = snprintf(buffer, sizeof(buffer), "%s\n", value);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        ALOGE("write_str failed to open %s\n", path);
        return -errno;
    }
}

static int is_lit(const struct light_state_t *state)
{
    return state->color & 0x00ffffff;
}

static int rgb_to_brightness(const struct light_state_t *state)
{
    // frameworks sends brightness as rgb grayscale
    return state->color & 0x000000ff;
}

static int set_speaker_light_locked(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int onMS, offMS;
    unsigned int colorRGB;
    int red, green, blue, brightness;

    if(!dev) {
        return -1;
    }

    switch (state->flashMode) {
        case LIGHT_FLASH_TIMED:
            onMS = state->flashOnMS;
            offMS = state->flashOffMS;
            break;
        case LIGHT_FLASH_NONE:
        default:
            onMS = 0;
            offMS = 0;
            break;
    }

    colorRGB = state->color;

    ALOGV("set_speaker_light_locked mode %d, colorRGB=%08X, onMS=%d, offMS=%d\n",
            state->flashMode, colorRGB, onMS, offMS);

    brightness = colorRGB >> 24;
    red = (colorRGB >> 16) & 0xFF;
    green = (colorRGB >> 8) & 0xFF;
    blue = colorRGB & 0xFF;
    
    if (brightness)
    {
    	red = red * brightness / 0xFF;
    	green = green * brightness / 0xFF;
    	blue = blue * brightness / 0xFF;
    }

    if (onMS > 0 && offMS > 0) {
        char dutystr[PWM_LUT_MAX_SIZE * 4 + 1];
        char* p;
        int i;

        if (red) {
            p = dutystr + sprintf(dutystr, "0");
            for (i = 1; i < LED_DUTY_STEPS; ++i) {
                p += sprintf(p, ",%d", (min((100 * i * LED_RAMP_MS / LED_DUTY_STEPS) / LED_RAMP_MS, 100)) * red / 0xFF);
            }
            p += sprintf(p, "\n");

            write_int(RED_BRIGHTNESS_FILE, 0xFF);
            write_int(RED_RAMP_STEP_MS_FILE, LED_RAMP_MS / LED_DUTY_STEPS);
            write_str(RED_START_IDX_FILE, 0);
            write_str(RED_DUTY_PCTS_FILE, dutystr);
            write_int(RED_PAUSE_LO_FILE, (offMS > 2 * LED_RAMP_MS) ? (offMS - 2 * LED_RAMP_MS) : 0);
            write_int(RED_PAUSE_HI_FILE, onMS);
            write_int(RED_LUT_FLAGS_FILE, PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_REVERSE | PM_PWM_LUT_PAUSE_HI_EN | PM_PWM_LUT_PAUSE_LO_EN);
            write_int(RED_BLINK_FILE, 1);
        } else {
            write_int(RED_BRIGHTNESS_FILE, 0);
            write_int(RED_BLINK_FILE, 0);
        }

        if (green) {
            p = dutystr + sprintf(dutystr, "0");
            for (i = 1; i < LED_DUTY_STEPS; ++i) {
                p += sprintf(p, ",%d", (min((100 * i * LED_RAMP_MS / LED_DUTY_STEPS) / LED_RAMP_MS, 100)) * green / 0xFF);
            }
            p += sprintf(p, "\n");

            write_int(GREEN_BRIGHTNESS_FILE, 0xFF);
            write_int(GREEN_RAMP_STEP_MS_FILE, LED_RAMP_MS / LED_DUTY_STEPS);
            write_str(GREEN_START_IDX_FILE, 0);
            write_str(GREEN_DUTY_PCTS_FILE, dutystr);
            write_int(GREEN_PAUSE_LO_FILE, (offMS > 2 * LED_RAMP_MS) ? (offMS - 2 * LED_RAMP_MS) : 0);
            write_int(GREEN_PAUSE_HI_FILE, onMS);
            write_int(GREEN_LUT_FLAGS_FILE, PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_REVERSE | PM_PWM_LUT_PAUSE_HI_EN | PM_PWM_LUT_PAUSE_LO_EN);
            write_int(GREEN_BLINK_FILE, 1);
        } else {
            write_int(GREEN_BRIGHTNESS_FILE, 0);
            write_int(GREEN_BLINK_FILE, 0);
        }

        if (blue) {
            p = dutystr + sprintf(dutystr, "0");
            for (i = 1; i < LED_DUTY_STEPS; ++i) {
                p += sprintf(p, ",%d", (min((100 * i * LED_RAMP_MS / LED_DUTY_STEPS) / LED_RAMP_MS, 100)) * blue / 0xFF);
            }
            p += sprintf(p, "\n");

            write_int(BLUE_BRIGHTNESS_FILE, 0xFF);
            write_int(BLUE_RAMP_STEP_MS_FILE, LED_RAMP_MS / LED_DUTY_STEPS);
            write_str(BLUE_START_IDX_FILE, 0);
            write_str(BLUE_DUTY_PCTS_FILE, dutystr);
            write_int(BLUE_PAUSE_LO_FILE, (offMS > 2 * LED_RAMP_MS) ? (offMS - 2 * LED_RAMP_MS) : 0);
            write_int(BLUE_PAUSE_HI_FILE, onMS);
            write_int(BLUE_LUT_FLAGS_FILE, PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_REVERSE | PM_PWM_LUT_PAUSE_HI_EN | PM_PWM_LUT_PAUSE_LO_EN);
            write_int(BLUE_BLINK_FILE, 1);
        } else {
            write_int(BLUE_BRIGHTNESS_FILE, 0);
            write_int(BLUE_BLINK_FILE, 0);
        }
    } else {
        write_int(RED_BLINK_FILE, 0);
        write_int(GREEN_BLINK_FILE, 0);
        write_int(BLUE_BLINK_FILE, 0);
        write_int(RED_BRIGHTNESS_FILE, red);
        write_int(GREEN_BRIGHTNESS_FILE, green);
        write_int(BLUE_BRIGHTNESS_FILE, blue);
    }

    return 0;
}

static void handle_speaker_light_locked(struct light_device_t *dev)
{
    if (is_lit(&g_attention)) {
        set_speaker_light_locked(dev, &g_attention);
    } else if (is_lit(&g_notification)) {
        set_speaker_light_locked(dev, &g_notification);
    } else {
        set_speaker_light_locked(dev, &g_battery);
    }
}

static int set_light_backlight(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    
    if (!dev)
        return -ENODEV;

    int current_brightness = read_int(LCD_FILE);

    if (current_brightness < 0)
        current_brightness = 0;

    pthread_mutex_lock(&g_lock);

    for (int i = 1; i <= LED_DUTY_STEPS; ++i) {
        int value = current_brightness + (brightness - current_brightness) * i / LED_DUTY_STEPS;
        err = write_int(LCD_FILE, value);
        usleep(LED_RAMP_MS / LED_DUTY_STEPS * 1000);
    }

    pthread_mutex_unlock(&g_lock);

    return err;
}

static int set_light_keyboard(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);

    if (!dev)
        return -ENODEV;

    int current_brightness_pwm_1 = read_int(KEYBOARD_PWM_1_BRIGHTNESS_FILE);
    int current_brightness_pwm_2 = read_int(KEYBOARD_PWM_2_BRIGHTNESS_FILE);
    int current_brightness_pwm_3 = read_int(KEYBOARD_PWM_3_BRIGHTNESS_FILE);
    int current_brightness_pwm_4 = read_int(KEYBOARD_PWM_4_BRIGHTNESS_FILE);

    if (current_brightness_pwm_1 < 0)
        current_brightness_pwm_1 = 0;
    if (current_brightness_pwm_2 < 0)
        current_brightness_pwm_2 = 0;
    if (current_brightness_pwm_3 < 0)
        current_brightness_pwm_3 = 0;
    if (current_brightness_pwm_4 < 0)
        current_brightness_pwm_4 = 0;

    pthread_mutex_lock(&g_lock);

    for (int i = 1; i <= LED_DUTY_STEPS; ++i) {
        int value_pwm_1 = current_brightness_pwm_1 + (brightness - current_brightness_pwm_1) * i / LED_DUTY_STEPS;
        int value_pwm_2 = current_brightness_pwm_2 + (brightness - current_brightness_pwm_2) * i / LED_DUTY_STEPS;
        int value_pwm_3 = current_brightness_pwm_3 + (brightness - current_brightness_pwm_3) * i / LED_DUTY_STEPS;
        int value_pwm_4 = current_brightness_pwm_4 + (brightness - current_brightness_pwm_4) * i / LED_DUTY_STEPS;
        err |= write_int(KEYBOARD_PWM_1_BRIGHTNESS_FILE, value_pwm_1);
        err |= write_int(KEYBOARD_PWM_2_BRIGHTNESS_FILE, value_pwm_2);
        err |= write_int(KEYBOARD_PWM_3_BRIGHTNESS_FILE, value_pwm_3);
        err |= write_int(KEYBOARD_PWM_4_BRIGHTNESS_FILE, value_pwm_4);
        usleep(LED_RAMP_MS / LED_DUTY_STEPS * 1000);
    }

    pthread_mutex_unlock(&g_lock);

    return err;
}

static int set_light_battery(struct light_device_t *dev,
        const struct light_state_t *state)
{
    pthread_mutex_lock(&g_lock);

    g_battery = *state;
    handle_speaker_light_locked(dev);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int set_light_notifications(struct light_device_t *dev,
        const struct light_state_t *state)
{
    pthread_mutex_lock(&g_lock);

    g_notification = *state;
    handle_speaker_light_locked(dev);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int set_light_attention(struct light_device_t *dev,
        const struct light_state_t *state)
{
    pthread_mutex_lock(&g_lock);

    g_attention = *state;
    handle_speaker_light_locked(dev);

    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int close_lights(struct hw_device_t *dev)
{
    if (dev)
        free(dev);

    return 0;
}

static int open_lights(const struct hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
    int (*set_light)(struct light_device_t *dev,
            const struct light_state_t *state);

    if (!strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (!strcmp(LIGHT_ID_KEYBOARD, name))
        set_light = set_light_keyboard;
    else if (!strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (!strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (!strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    struct light_device_t *dev = calloc(1, sizeof(struct light_device_t));

    if (!dev)
        return -ENOMEM;

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;

    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = 1,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "WSeries Lights HAL",
    .author = "The LineageOS Project",
    .methods = &lights_module_methods,
};
