/*
 *  Copyright (C) 2009-2011 Texas Instruments, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sosal/sosal.h>
#include <dvp/dvp.h>

// On ANDROID GB or FROYO and on Linux (like Ubuntu)
#if defined(V4L2_SUPPORT)
#include <v4l2_api.h>

typedef struct _dvp_display_t {
    v4l2_api_t *m_v4l2;
    uint32_t width;
    uint32_t height;
    thread_t m_renderer;
    DVP_BOOL m_running;
    DVP_Perf_t m_perf;
    event_t  m_gate;
} dvp_display_t;

#define _DVP_DISPLAY_T
#include <dvp/dvp_display.h>
#include <dvp/dvp_debug.h>

#if defined(BLAZE)  || defined(SDP)
#define INITIAL_OFFSET_X (0)
#define INITIAL_OFFSET_Y (0)
#else
#define INITIAL_OFFSET_X (480)
#define INITIAL_OFFSET_Y (77) // put it below the systray and title bar on some phones
#endif

DVP_BOOL DVP_Display_Alloc(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
    {
        void *buffer = NULL;
        uint32_t index = 0;
        uint32_t length = 0;
        buffer = v4l2_acquire(dvpd->m_v4l2, &index);
        if (buffer)
        {
            length = v4l2_length(dvpd->m_v4l2, index);
            pImage->pBuffer[0] = buffer;
            pImage->pData[0] = buffer;
            if (pImage->planes == 1)
                pImage->y_stride = length / pImage->bufHeight;
            else if (pImage->color == FOURCC_NV12)
            {
                pImage->y_stride = length / (pImage->bufHeight + pImage->bufHeight/2);
                pImage->pBuffer[1] = &pImage->pBuffer[0][(pImage->y_stride*pImage->bufHeight)];
                pImage->pData[1] = pImage->pBuffer[1];
            }
            // it is important to set this to the length returned from V4L2, not a calculated value.
            pImage->numBytes = length;

#if (defined(ANDROID) || defined(LINUX)) && defined(DVP_USE_TILER)
            // on Android and Linux, V4L2 display buffers are allocated out of the tiler.
            pImage->memType = DVP_MTYPE_DISPLAY_2DTILED;
            pImage->skipCacheOpFlush = DVP_TRUE; // uncached
            pImage->skipCacheOpInval = DVP_TRUE; // uncached
#endif
            DVP_PrintImage(DVP_ZONE_VIDEO, pImage);
            return DVP_TRUE;
        }
        else
            return DVP_FALSE;
    }
    else
        return DVP_FALSE;
}

DVP_BOOL DVP_Display_Free(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
    {
        void *buffer = pImage->pBuffer[0];
        uint32_t index = v4l2_search_buffer(dvpd->m_v4l2, buffer);
        if (v4l2_release(dvpd->m_v4l2, buffer, index) == false_e)
            return DVP_FALSE;
        else
        {
            memset(pImage, 0, sizeof(DVP_Image_t));
            pImage->memType = DVP_MTYPE_DEFAULT;
            return DVP_TRUE;
        }
    }
    return DVP_FALSE;
}

void sysfs_put(const char *key, const char *value)
{
    size_t len = strlen(value);
    int fd = open(key, O_RDWR);
    if (fd)
    {
        DVP_PRINT(DVP_ZONE_VIDEO, "%s => %s\n", value, key);
        write(fd, value, len);
        close(fd);
    }
}

void sysfs_get(const char *key, char *value, size_t len)
{
    int fd = open(key, O_RDWR);
    if (fd)
    {
        read(fd, value, len);
        close(fd);
        DVP_PRINT(DVP_ZONE_VIDEO, "%s => \"%s\"\n", key, value);
    }
}

void set_rgb_color(uint8_t r, uint8_t g, uint8_t b)
{
    char rv[4];
    char gv[4];
    char bv[4];
    snprintf(rv, sizeof(rv), "%03u", r);
    snprintf(gv, sizeof(gv), "%03u", g);
    snprintf(bv, sizeof(bv), "%03u", b);
#if defined(BLAZE)
    sysfs_put("/sys/class/leds/blue/brightness",  rv);
    sysfs_put("/sys/class/leds/red/brightness",   gv);
    sysfs_put("/sys/class/leds/green/brightness", bv);
#endif
}

void enable_secondary_display()
{
#if defined(ANDROID) && (defined(BLAZE) || defined(SDP))
    char value[20];
    memset(value, 0, sizeof(value));
#if defined(FROYO)
    sysfs_put("/sys/devices/platform/omapdss/overlay2/manager", "2lcd");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/zorder", "3");
    sysfs_put("/sys/devices/platform/omapdss/overlay1/zorder", "2");
    sysfs_get("/sys/devices/platform/omapdss/overlay2/zorder", value, sizeof(value));
#elif defined(GINGERBREAD)
    sysfs_put("/sys/devices/platform/omapdss/display1/enabled", "1");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/manager", "2lcd");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/zorder", "3");
    sysfs_put("/sys/devices/platform/omapdss/overlay1/zorder", "2");
    sysfs_put("/sys/devices/platform/omapdss/overlay0/zorder", "3");
#endif
#endif
}

void disable_secondary_display()
{
#if defined(ANDROID) && (defined(BLAZE) || defined(SDP))
    char value[20];
    memset(value, 0, sizeof(value));
#if defined(FROYO)
    sysfs_put("/sys/devices/platform/omapdss/overlay2/manager", "lcd");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/zorder", "0");
    sysfs_put("/sys/devices/platform/omapdss/overlay1/zorder", "2");
    sysfs_put("/sys/devices/platform/omapdss/overlay0/zorder", "3");
    sysfs_get("/sys/devices/platform/omapdss/overlay2/zorder", value, sizeof(value));
#elif defined(GINGERBREAD)
    sysfs_put("/sys/devices/platform/omapdss/display1/enabled", "0");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/manager", "lcd");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/zorder", "1");
    sysfs_put("/sys/devices/platform/omapdss/overlay1/zorder", "2");
    sysfs_put("/sys/devices/platform/omapdss/overlay0/zorder", "3");
#endif
#endif
}

void enable_primary_display()
{
#if !(defined(BLAZE)  || defined(SDP)) && defined(ANDROID)
    sysfs_put("/sys/devices/platform/omapdss/overlay2/manager", "2lcd");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/rotation", "0");
    sysfs_put("/sys/devices/platform/omapdss/overlay0/zorder", "2");
    sysfs_put("/sys/devices/platform/omapdss/overlay1/zorder", "1");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/zorder", "3");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/global_alpha", "100");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/enabled", "1");
    sysfs_put("/sys/devices/platform/omapdss/manager2/trans_key_enable", "0");
    sysfs_put("/sys/devices/platform/omapdss/manager2/alpha_blending_enabled", "1");
    sysfs_put("/sys/devices/platform/omapdss/manager2/trans_key_value", "0");
    sysfs_put("/sys/devices/platform/omapdss/manager2/trans_key_type", "video-source");
#endif
}

void disable_primary_display()
{
#if !(defined(BLAZE) || defined(SDP)) && defined(ANDROID)
    sysfs_put("/sys/devices/platform/omapdss/overlay2/manager", "2lcd");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/rotation", "0");
    sysfs_put("/sys/devices/platform/omapdss/overlay0/zorder", "3");
    sysfs_put("/sys/devices/platform/omapdss/overlay1/zorder", "1");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/zorder", "1");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/global_alpha", "255");
    sysfs_put("/sys/devices/platform/omapdss/overlay2/enabled", "0");
    sysfs_put("/sys/devices/platform/omapdss/manager2/trans_key_enable", "0");
    sysfs_put("/sys/devices/platform/omapdss/manager2/alpha_blending_enabled", "0");
    sysfs_put("/sys/devices/platform/omapdss/manager2/trans_key_value", "0");
    sysfs_put("/sys/devices/platform/omapdss/manager2/trans_key_type", "gfx-destination");
#endif
}

void DVP_Display_Render(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd)
    {
        uint32_t index = 0;

        DVP_PerformanceStart(&dvpd->m_perf);

        DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Rendering DVP_Image_t *%p Buffer %p\n", pImage, pImage->pBuffer[0]);
        // get the index of the buffer
        index = v4l2_search_buffer(dvpd->m_v4l2, pImage->pBuffer[0]);
        if (index != V4L2_INVALID_INDEX)
        {
            //The crop is breaking the Tismo graph since the display is bigger than the image
            //It is only necessary when the x_start and y_start becomes non-zero (no cases yet)
            //v4l2_crop(dvpd->m_v4l2, pImage->x_start, pImage->y_start, pImage->width, pImage->height);
            v4l2_queue(dvpd->m_v4l2, index, V4L2_BUF_TYPE_VIDEO_OUTPUT);
            v4l2_start(dvpd->m_v4l2, V4L2_BUF_TYPE_VIDEO_OUTPUT);
            event_set(&dvpd->m_gate); // unblock the dequeue thread

            DVP_PerformanceStop(&dvpd->m_perf);
            DVP_PerformancePrint(&dvpd->m_perf, "V4L2");
        }
        else
        {
            DVP_PRINT(DVP_ZONE_VIDEO, "ERROR: V4L2 Image is not allocated from display!\n");
        }
    }
}

thread_ret_t DVP_Display_Thread(void *arg)
{
    dvp_display_t *dvpd = (dvp_display_t *)arg;
    thread_ret_t ret = 0;
    DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Dequeue Thread Launched! (Waiting)\n");
    event_wait(&dvpd->m_gate, EVENT_FOREVER);
    DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Dequeue Thread Running!\n");
    while (dvpd->m_running == DVP_TRUE)
    {
        void *buffer;
        uint32_t index;
        bool_e dequeued = true_e;
        do {
            if (v4l2_wait(dvpd->m_v4l2) > 0)
            {
                dequeued = v4l2_dequeue(dvpd->m_v4l2, &index, V4L2_BUF_TYPE_VIDEO_OUTPUT);
                if (dequeued)
                {
                    // the actual buffer dequeued
                    buffer = v4l2_search_index(dvpd->m_v4l2, index);
                    DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Dequeued Buffer %p [%u]\n", buffer, index);
                    break;
                }
                else
                {
                    //DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Dequeue did not work!\n");
                    // there's just nothing to dequeue yet, sleep a while until
                    // there is something to remove...
                    // this should really be propotional to some frame rate parameter
                    thread_msleep(33);
                }
            }
            else
            {
                DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Wait failure!\n");
                dequeued = false_e;
            }
        } while (dequeued == false_e && dvpd->m_running == DVP_TRUE);
    }
    DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Display Thread Exitting\n");
    thread_exit(ret);
}

void DVP_Display_Destroy(dvp_display_t **dvpd)
{
    if (dvpd && *dvpd)
    {
        dvp_display_t *tmp = *dvpd; // convienence pointer

        tmp->m_running = DVP_FALSE;
        event_set(&tmp->m_gate);
        v4l2_stop(tmp->m_v4l2, V4L2_BUF_TYPE_VIDEO_OUTPUT); // this dequeues everything implicitly
        DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Closing Renderer Thread\n");
        thread_join(tmp->m_renderer); // wait for the thread to finish
        DVP_PRINT(DVP_ZONE_VIDEO, "V4L2 Renderer Closed!\n");
        v4l2_free(tmp->m_v4l2); // unmap the memory
        v4l2_close(&tmp->m_v4l2); // close the driver
        event_deinit(&tmp->m_gate);
        free(tmp); // free the data structre
        *dvpd = NULL; // set the user's pointer to null.

        disable_secondary_display();
        disable_primary_display();
    }
}

dvp_display_t *DVP_Display_Create(uint32_t image_width,   uint32_t image_height,
                                  uint32_t buffer_width,   uint32_t buffer_height,
                                  uint32_t display_width, uint32_t display_height,
                                  uint32_t scaled_width,  uint32_t scaled_height,
                                  uint32_t crop_top,  uint32_t crop_left,
                                  fourcc_t color,
                                  int32_t rotation,
                                  uint32_t numBuffers)
{
    DVP_BOOL unwind = DVP_FALSE;
    dvp_display_t *dvpd = (dvp_display_t *)calloc(1,sizeof(dvp_display_t));
    if (dvpd)
    {
        DVP_PRINT(DVP_ZONE_VIDEO, "Image: %ux%u Display: %ux%u Scaled:%ux%u\n",
                  image_width, image_height,
                  display_width, display_height,
                  scaled_width, scaled_height);
        enable_secondary_display();
        enable_primary_display();
        set_rgb_color(0,0,0); // turn off the LED so that we don't get the light in the camera

        event_init(&dvpd->m_gate, false_e); // a one-shot event.
        dvpd->m_v4l2 = v4l2_open(V4L2_DISPLAY_DEVICE, V4L2_CAP_STREAMING, true_e);
        dvpd->width = image_width;
        dvpd->height = image_height;
        dvpd->m_perf.minTime = 0xFFFFFFFF;
        if (v4l2_allocate(dvpd->m_v4l2, buffer_width, buffer_height, numBuffers, color, V4L2_BUF_TYPE_VIDEO_OUTPUT) == false_e)
            unwind = DVP_TRUE;
        else
        {
            uint32_t overlay_width = 0;
            uint32_t overlay_height = 0;
            uint32_t offset_x = INITIAL_OFFSET_X;
            uint32_t offset_y = INITIAL_OFFSET_Y;

            // retrieve the information about the overlay
            v4l2_metrics(dvpd->m_v4l2, &overlay_width, &overlay_height);

            DVP_PRINT(DVP_ZONE_VIDEO, "OVERLAY: %ux%u %ux%u\n", overlay_width, overlay_height, display_width, display_height);

            if (offset_x > (display_width-scaled_width))
                offset_x = (display_width-scaled_width);
            if (offset_y > (display_height-scaled_height))
                offset_y = (display_height-scaled_height);

#if (defined(BLAZE) || defined(SDP)) && defined(ANDROID)
            v4l2_mirror(dvpd->m_v4l2, true_e);
            v4l2_crop(dvpd->m_v4l2, crop_left, crop_top, image_width, image_height);
            v4l2_position(dvpd->m_v4l2, offset_x, offset_y, scaled_width, scaled_height);
#endif

#if !(defined(BLAZE) || defined(SDP)) && defined(ANDROID)
            // no mirror, it's not supported.
            v4l2_position(dvpd->m_v4l2, offset_x, offset_y, scaled_width, scaled_height);
            //if (color == FOURCC_NV12)
            //    v4l2_crop(dvpd->m_v4l2, 0, 32, image_width, image_height);
            v4l2_crop(dvpd->m_v4l2, crop_left, crop_top, image_width, image_height);
            if (color != FOURCC_NV12)
                v4l2_transparency(dvpd->m_v4l2, V4L2_FBUF_FLAG_CHROMAKEY, 0);
#endif
            // set the rotation of the overlay.
            v4l2_rotate(dvpd->m_v4l2, rotation);

            // create the renderer
            dvpd->m_running = DVP_TRUE;
            dvpd->m_renderer = thread_create(DVP_Display_Thread, (void *)dvpd);

            // insert a green/gray screen at the start
            {
                DVP_Image_t tmp;
                uint32_t y, i;

                DVP_Image_Init(&tmp, buffer_width, buffer_height, color);
                tmp.width = image_width;
                tmp.height = image_height;
                DVP_Display_Alloc(dvpd, &tmp);
                if (color == FOURCC_UYVY) {
                    for (y = 0; y < tmp.height; y++) {
                        i = (y * tmp.y_stride);
                        memset(&tmp.pData[0][i], 0x80, tmp.width*tmp.x_stride);
                    }
                }
                else if (color == FOURCC_NV12)
                {
                    for (y = 0; y < tmp.height; y++) {
                        i = (y * tmp.y_stride);
                        memset(&tmp.pData[0][i], 0x80, tmp.width*tmp.x_stride);
                    }
                    for (y = 0; y < tmp.height; y++) {
                        i = (y/2 * tmp.y_stride);
                        memset(&tmp.pData[1][i], 0x10, tmp.width/2*tmp.x_stride*2);
                    }
                }
                else if (color == FOURCC_RGB || color == FOURCC_BGR)
                {
                    for (y = 0; y < tmp.height; y++) {
                        i = (y * tmp.y_stride);
                        memset(&tmp.pData[0][i], 0x80, tmp.width*tmp.x_stride);
                    }
                }
                DVP_Display_Render(dvpd, &tmp); // this will do a memcpy
                DVP_Display_Free(dvpd, &tmp);
            }
#if !(defined(BLAZE)  || defined(SDP))
            // send the alpha value again!
            sysfs_put("/sys/devices/platform/omapdss/overlay2/global_alpha", "100");
#endif
        }
    }
    if (unwind)
        DVP_Display_Destroy(&dvpd);
    return dvpd;
}

DVP_BOOL DVP_Display_Drop(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
        return DVP_TRUE;
    else
        return DVP_FALSE;
}

#elif defined(__QNX__)

#include <qnxscreen_api.h>

typedef struct _dvp_display_t {
    struct qnxscreen_api *m_qnxscreen;
    uint32_t width;
    uint32_t height;
    DVP_BOOL m_running;
    DVP_Perf_t m_perf;
} dvp_display_t;

#define _DVP_DISPLAY_T
#include <dvp/dvp_display.h>
#include <dvp/dvp_mem.h>
#include <dvp/dvp_debug.h>

#define INITIAL_OFFSET_X (0)
#define INITIAL_OFFSET_Y (0)

DVP_BOOL DVP_Display_Alloc(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    void *buffer = NULL;
    uint32_t index = 0;
    uint32_t length = 0;

    if (!dvpd || !pImage)
        return DVP_FALSE;

    buffer = qnxscreen_acquire(dvpd->m_qnxscreen, &index);
    if (!buffer)
        return DVP_FALSE;

    length = qnxscreen_length(dvpd->m_qnxscreen, index);
    pImage->pBuffer[0] = buffer;
    pImage->pData[0] = buffer;
    switch (pImage->color) {
    case FOURCC_NV12:
        pImage->y_stride = length / (pImage->bufHeight + pImage->bufHeight/2);
        pImage->pBuffer[1] = &pImage->pBuffer[0][(pImage->y_stride * pImage->bufHeight)];
        pImage->pData[1] = pImage->pBuffer[1];
        break;
    case FOURCC_UYVY:
    default:
        pImage->y_stride = length / pImage->bufHeight; // we know it's strided.
        break;
    }
    pImage->memType = DVP_MTYPE_DISPLAY_2DTILED;
    pImage->numBytes = length;

    DVP_PrintImage(DVP_ZONE_VIDEO, pImage);
    return DVP_TRUE;
}

DVP_BOOL DVP_Display_Free(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
        void *buffer;
        uint32_t index;

    if (!dvpd || !pImage)
        return DVP_FALSE;

        buffer = pImage->pBuffer[0];
        index = qnxscreen_search_buffer(dvpd->m_qnxscreen, buffer);
    if (qnxscreen_release(dvpd->m_qnxscreen, buffer, index) == false_e)
        return DVP_FALSE;

    memset(pImage, 0, sizeof(DVP_Image_t));
    pImage->memType = DVP_MTYPE_DEFAULT;
    return DVP_TRUE;
}

void DVP_Display_Render(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    uint32_t index = 0;

    if (!dvpd || !pImage)
        return;

    DVP_PerformanceStart(&dvpd->m_perf);
    DVP_PRINT(DVP_ZONE_VIDEO, "QNX Screen Rendering DVP_Image_t *%p Buffer %p\n", pImage, pImage->pBuffer[0]);
    // get the index of the buffer
    index = qnxscreen_search_buffer(dvpd->m_qnxscreen, pImage->pBuffer[0]);
    if (index == QNXSCREEN_INVALID_INDEX) {
        DVP_PRINT(DVP_ZONE_VIDEO, "ERROR: V4L2 Image is not allocated from display!\n");
        return;
    }
    qnxscreen_post(dvpd->m_qnxscreen, index);
    DVP_PerformanceStop(&dvpd->m_perf);
    DVP_PerformancePrint(&dvpd->m_perf, "QNXSCREEN");
}

void DVP_Display_Destroy(dvp_display_t **dvpd)
{
    if (!dvpd || !(*dvpd))
        return;

    qnxscreen_free((*dvpd)->m_qnxscreen); // unmap the memory
    qnxscreen_close(&(*dvpd)->m_qnxscreen); // close the driver
    free(*dvpd); // free the data structre
    *dvpd = NULL; // set the user's pointer to null.
}

dvp_display_t *DVP_Display_Create(uint32_t image_width, uint32_t image_height,
                                  uint32_t buffer_width, uint32_t buffer_height,
                                  uint32_t display_width, uint32_t display_height,
                                  uint32_t scaled_width, uint32_t scaled_height,
                                  uint32_t crop_top,  uint32_t crop_left,
                                  fourcc_t color, int32_t rotation, uint32_t numBuffers)
{
    dvp_display_t *dvpd = (dvp_display_t *)calloc(1,sizeof(dvp_display_t));
    uint32_t overlay_width = 0;
    uint32_t overlay_height = 0;
    // @TODO: figure out a better way to handle this for rotation
    uint32_t offset_x = 15;
    uint32_t offset_y = 600-(image_height+15);

    if (!dvpd)
        return NULL;

    DVP_PRINT(DVP_ZONE_VIDEO, "Image: %ux%u Display: %ux%u Scaled:%ux%u\n",
        image_width, image_height,
        display_width, display_height,
        scaled_width, scaled_height);

    dvpd->m_qnxscreen = qnxscreen_open();
    dvpd->width = image_width;
    dvpd->height = image_height;
    DVP_Perf_Clear(&dvpd->m_perf);

    if (qnxscreen_allocate(dvpd->m_qnxscreen, buffer_width, buffer_height, numBuffers, color) == false_e)
        goto unwind;

    // retrieve the information about the overlay
    qnxscreen_metrics(dvpd->m_qnxscreen, &overlay_width, &overlay_height);

    DVP_PRINT(DVP_ZONE_VIDEO, "OVERLAY: %ux%u %ux%u\n", overlay_width, overlay_height, display_width, display_height);

    if (offset_x > (display_width-scaled_width))
        offset_x = (display_width-scaled_width);
    if (offset_y > (display_height-scaled_height))
        offset_y = (display_height-scaled_height);

    qnxscreen_mirror(dvpd->m_qnxscreen, true_e);
    qnxscreen_transparency(dvpd->m_qnxscreen, 100);
    qnxscreen_position(dvpd->m_qnxscreen, offset_x, offset_y, scaled_width, scaled_height);

    // set the rotation of the overlay.
    qnxscreen_rotate(dvpd->m_qnxscreen, rotation);

    // insert a green/gray screen at the start
    {
        DVP_Image_t tmp;
        uint32_t y, i;

        DVP_Image_Init(&tmp, buffer_width, buffer_width, color);
        tmp.width = image_width;
        tmp.height = image_height;
        DVP_Display_Alloc(dvpd, &tmp);
        if (color == FOURCC_UYVY) {
            for (y = 0; y < tmp.height; y++) {
                i = (y * tmp.y_stride);
                memset(&tmp.pData[0][i], 0x80, tmp.width * 2);
            }
        } else if (color == FOURCC_NV12) {
            for (y = 0; y < tmp.height; y++) {
                i = (y * tmp.y_stride);
                memset(&tmp.pData[0][i], 0x80,
                    tmp.width * tmp.x_stride);
            }
            for (y = 0; y < tmp.height; y++) {
                i = (y/2 * tmp.y_stride);
                memset(&tmp.pData[1][i], 0x10,
                    tmp.width / 2 * tmp.x_stride * 2);
            }
        }
        DVP_Display_Render(dvpd, &tmp); // this will do a memcpy
        DVP_Display_Free(dvpd, &tmp);
    }

    return dvpd;
unwind:
    DVP_Display_Destroy(&dvpd);
    return NULL;
}

DVP_BOOL DVP_Display_Drop(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
        return DVP_TRUE;
    else
        return DVP_FALSE;
}

#elif defined(GTK_SUPPORT)

#include <gtkwindow.h>
#include <dvp/dvp_debug.h>

typedef struct _dvp_diplay_t {
    GtkWindow_t *w;
    uint32_t width;
    uint32_t height;
    thread_t m_renderer;
    bool_e   m_running;
} dvp_display_t;

DVP_BOOL DVP_Display_Alloc(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd)
    {
        uint32_t index;
        void *buffer = gtkwindow_acquire(dvpd->w, &index);
        if (buffer)
        {
            uint32_t length = gtkwindow_length(dvpd->w, index);
            pImage->pBuffer[0] = buffer;
            pImage->pData[0] = buffer;
            if (pImage->planes == 1)
                pImage->y_stride = length / pImage->bufHeight;
            else if (pImage->color == FOURCC_NV12)
            {
                pImage->y_stride = length / (pImage->bufHeight + pImage->bufHeight/2);
                pImage->pBuffer[1] = &pImage->pBuffer[0][(pImage->y_stride*pImage->bufHeight)];
                pImage->pData[1] = pImage->pBuffer[1];
            }
            // it is important to set this to the length returned from V4L2, not a calculated value.
            pImage->numBytes = length;

#if defined(LINUX) && defined(DVP_USE_TILER)
            // on Android and Linux, V4L2 display buffers are allocated out of the tiler.
            pImage->memType = DVP_MTYPE_DISPLAY_2DTILED;
#endif
            DVP_PrintImage(DVP_ZONE_VIDEO, pImage);
            return DVP_TRUE;
        }
    }
    return DVP_FALSE;
}

DVP_BOOL DVP_Display_Free(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd)
    {
        void *buffer = pImage->pBuffer[0];
        uint32_t index = gtkwindow_search_index(dvpd->w, buffer);
        if (gtkwindow_release(dvpd->w, buffer, index) == false_e)
            return DVP_FALSE;
        else
        {
            memset(pImage, 0, sizeof(DVP_Image_t));
            pImage->memType = DVP_MTYPE_DEFAULT;
            return DVP_TRUE;
        }
    }
    return DVP_FALSE;
}


thread_ret_t DVP_Display_Thread(void *arg)
{
    dvp_display_t *dvpd = (dvp_display_t *)arg;
    while (dvpd->m_running)
    {
        uint32_t index;
        gtkwindow_dequeue(dvpd->w, &index);
    }
    thread_exit(0);
}


void DVP_Display_Render(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd)
    {
        uint32_t index = gtkwindow_search_index(dvpd->w, pImage->pData[0]);
        gtkwindow_queue(dvpd->w, index);
    }
}

void DVP_Display_Destroy(dvp_display_t **pdvpd)
{
    if (pdvpd)
    {
        dvp_display_t *dvpd = *pdvpd;
        if (dvpd)
        {
            dvpd->m_running = false_e;
            gtkwindow_free(dvpd->w);
            gtkwindow_close(dvpd->w);
            thread_join(dvpd->m_renderer);
            free(dvpd);
        }
        *pdvpd = NULL;
    }
}

dvp_display_t *DVP_Display_Create(uint32_t image_width, uint32_t image_height,
                                  uint32_t buffer_width, uint32_t buffer_height,
                                  uint32_t display_width, uint32_t display_height,
                                  uint32_t scaled_width, uint32_t scaled_height,
                                  uint32_t crop_top,  uint32_t crop_left,
                                  fourcc_t color, int32_t rotation, uint32_t numBuffers)
{
    DVP_BOOL unwind = DVP_FALSE;
    dvp_display_t *dvpd = (dvp_display_t *)calloc(1,sizeof(dvp_display_t));
    if (dvpd)
    {
        DVP_PRINT(DVP_ZONE_VIDEO, "DISPLAY: Requesting Color Space %08x\n", color);
        dvpd->width = buffer_width;
        dvpd->height = buffer_height;
        dvpd->w = gtkwindow_open("DVP Display");
        if (dvpd->w && gtkwindow_allocate(dvpd->w, buffer_width, buffer_height, numBuffers, color))
        {
            DVP_PRINT(DVP_ZONE_VIDEO, "DISPLAY: Allocated %u images\n", numBuffers);
            dvpd->m_running = true_e;
            dvpd->m_renderer = thread_create(DVP_Display_Thread, dvpd);
        }
        else
        {
            gtkwindow_close(dvpd->w);
            free(dvpd);
            dvpd = NULL;
        }
    }
    return dvpd;
}

DVP_BOOL DVP_Display_Drop(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
        return DVP_TRUE;
    else
        return DVP_FALSE;
}

#elif defined(ICS) || defined(JELLYBEAN)

#if defined(SURFACE_FLINGER)

#include <surfaceflinger.h>

typedef struct _dvp_display_t {
    surfaceflinger_t *sfd;
} dvp_display_t;

#define _DVP_DISPLAY_T
#include <sosal/pyuv.h>
#include <dvp/dvp_display.h>
#include <dvp/dvp_debug.h>
#if defined(ANDROID)
#include <hwc/hal_public.h>
#endif

#define INITIAL_OFFSET_X (100)
#define INITIAL_OFFSET_Y (100)

void DVP_Display_Destroy(dvp_display_t **dvpd)
{
    if (dvpd && *dvpd)
    {
        if ((*dvpd)->sfd)
        {
            surfaceflinger_free((*dvpd)->sfd);
            surfaceflinger_destroy(&(*dvpd)->sfd);
        }
        free((*dvpd));
        dvpd = NULL;
    }
}

dvp_display_t *DVP_Display_Create(uint32_t image_width,
                                  uint32_t image_height,
                                  uint32_t buffer_width,
                                  uint32_t buffer_height,
                                  uint32_t display_width __attribute__((unused)),
                                  uint32_t display_height __attribute__((unused)),
                                  uint32_t scaled_width __attribute__((unused)),
                                  uint32_t scaled_height __attribute__((unused)),
                                  uint32_t crop_top __attribute__((unused)),
                                  uint32_t crop_left __attribute__((unused)),
                                  fourcc_t color,
                                  int32_t rotation __attribute__((unused)),
                                  uint32_t numBuffers)
{
    DVP_PRINT(DVP_ZONE_VIDEO, "Creating surfaceflinger Display!\n");
    if (color == FOURCC_NV12)
    {
        dvp_display_t *dvpd = (dvp_display_t *)calloc(1, sizeof(dvp_display_t));
        if (dvpd)
        {
            dvpd->sfd = surfaceflinger_create(buffer_width, buffer_height);
            if ( surfaceflinger_allocate(dvpd->sfd, buffer_width, buffer_height, numBuffers, color) )
            {
                DVP_PRINT(DVP_ZONE_VIDEO, "Allocated %u buffers\n", numBuffers);
            }

            surfaceflinger_set_crop(dvpd->sfd, 0, 0, image_width, image_height);

            uint32_t offset_x = INITIAL_OFFSET_X;
            uint32_t offset_y = INITIAL_OFFSET_Y;

            surfaceflinger_set_position(dvpd->sfd, offset_x, offset_y);
        }
        return dvpd;
    }
    else
        return NULL;
}

void DVP_Display_Render(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && dvpd->sfd)
    {
        if (pImage->reserved)
        {
            void *ptr;
            surfaceflinger_enqueue(dvpd->sfd, pImage->reserved);
            surfaceflinger_dequeue(dvpd->sfd, &ptr);
            // @TODO did pImage->reserved and ptr ==?
        }
    }
}

DVP_BOOL DVP_Display_Alloc(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    DVP_BOOL allocated = DVP_FALSE;
    bool_e result = false_e;
    if (dvpd && dvpd->sfd)
    {
        int32_t stride;
        uint8_t *ptrs[3] = {NULL, NULL, NULL};
        result = surfaceflinger_acquire(dvpd->sfd, &(pImage->reserved), ptrs, &stride);
        if (result)
        {
            switch (pImage->color)
            {
                case FOURCC_NV12:
                    pImage->memType = DVP_MTYPE_DISPLAY_2DTILED;
                    pImage->y_stride = stride;
                    pImage->pBuffer[0] = ptrs[0];
                    pImage->pBuffer[1] = ptrs[1];
                    pImage->pData[0] = pImage->pBuffer[0];
                    pImage->pData[1] = pImage->pBuffer[1];
                    break;
            }
            DVP_PRINT(DVP_ZONE_VIDEO,"Display buffer stride is %u\n", pImage->y_stride);
            allocated = DVP_TRUE;
        }
    }
    return allocated;
}

DVP_BOOL DVP_Display_Free(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && dvpd->sfd)
    {
        if (pImage->reserved)
        {
            surfaceflinger_release(dvpd->sfd, pImage->reserved);
        }
    }
    return DVP_TRUE;
}

DVP_BOOL DVP_Display_Drop(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
        return DVP_TRUE;
    else
        return DVP_FALSE;
}

#else

#include <gfx_display.h>

typedef struct _dvp_display_t {
    gfx_display_t *gfxd;
} dvp_display_t;

#define _DVP_DISPLAY_T
#include <sosal/pyuv.h>
#include <dvp/dvp_display.h>
#include <dvp/dvp_debug.h>
#if defined(ANDROID)
#include <hwc/hal_public.h>
#endif

void DVP_Display_Destroy(dvp_display_t **dvpd)
{
    if (dvpd && *dvpd)
    {
        if ((*dvpd)->gfxd)
        {
            gfx_display_free((*dvpd)->gfxd);
            gfx_display_destroy(&(*dvpd)->gfxd);
        }
        free((*dvpd));
        dvpd = NULL;
    }
}

dvp_display_t *DVP_Display_Create(uint32_t image_width __attribute__((unused)),
                                  uint32_t image_height __attribute__((unused)),
                                  uint32_t buffer_width,
                                  uint32_t buffer_height,
                                  uint32_t display_width __attribute__((unused)),
                                  uint32_t display_height __attribute__((unused)),
                                  uint32_t scaled_width __attribute__((unused)),
                                  uint32_t scaled_height __attribute__((unused)),
                                  uint32_t crop_top __attribute__((unused)),
                                  uint32_t crop_left __attribute__((unused)),
                                  fourcc_t color,
                                  int32_t rotation __attribute__((unused)),
                                  uint32_t numBuffers)
{
    DVP_PRINT(DVP_ZONE_VIDEO, "Creating GFX Display!\n");
    if (color == FOURCC_NV12)
    {
        dvp_display_t *dvpd = (dvp_display_t *)calloc(1, sizeof(dvp_display_t));
        if (dvpd)
        {
            char filename[MAX_PATH];
            PYUV_GetFilename(filename, "/sdcard/", "display", buffer_width, buffer_height, 30, color);
            dvpd->gfxd = gfx_display_create(filename);
            if (gfx_display_allocate(dvpd->gfxd, numBuffers, buffer_width, buffer_height, color) == true_e)
            {
                DVP_PRINT(DVP_ZONE_VIDEO, "Allocated %u buffers\n", numBuffers);
            }
        }
        return dvpd;
    }
    else
        return NULL;
}

void DVP_Display_Render(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && dvpd->gfxd)
    {
        if (pImage->reserved)
        {
            void *ptr;
            gfx_display_queue(dvpd->gfxd, pImage->reserved);
            gfx_display_dequeue(dvpd->gfxd, &ptr);
            // @TODO did pImage->reserved and ptr ==?
        }
    }
}

DVP_BOOL DVP_Display_Alloc(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    DVP_BOOL allocated = DVP_FALSE;
    if (dvpd && dvpd->gfxd)
    {
        image_t *img = gfx_display_acquire(dvpd->gfxd, &(pImage->reserved));
        if (img)
        {
            switch (pImage->color)
            {
                case FOURCC_NV12:
                    pImage->memType = DVP_MTYPE_DISPLAY_2DTILED;
                    pImage->y_stride = img->plane[0].ystride;
                    pImage->pBuffer[0] = img->plane[0].ptr;
                    pImage->pBuffer[1] = img->plane[1].ptr;
                    pImage->pData[0] = pImage->pBuffer[0];
                    pImage->pData[1] = pImage->pBuffer[1];
                    break;
            }
            DVP_PRINT(DVP_ZONE_VIDEO,"Display buffer stride is %u\n", pImage->y_stride);
            allocated = DVP_TRUE;
        }
    }
    return allocated;
}

DVP_BOOL DVP_Display_Free(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && dvpd->gfxd)
    {
        if (pImage->reserved)
        {
            gfx_display_release(dvpd->gfxd, NULL, pImage->reserved);
        }
    }
    return DVP_TRUE;
}

DVP_BOOL DVP_Display_Drop(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
        return DVP_TRUE;
    else
        return DVP_FALSE;
}
#endif

#else

#if !defined(WIN32)
#warning "Building DVP Display in FILE output mode!"
#else
#pragma message("***** Building DVP Display in FILE output mode!")
#endif

typedef struct _dvp_display_t {
    FILE *out;
} dvp_display_t;

#define _DVP_DISPLAY_T
#include <sosal/pyuv.h>
#include <dvp/dvp_display.h>
#include <dvp/dvp_debug.h>


dvp_display_t *DVP_Display_Create(uint32_t image_width, uint32_t image_height,
                                  uint32_t buffer_width, uint32_t buffer_height,
                                  uint32_t display_width, uint32_t display_height,
                                  uint32_t scaled_width, uint32_t scaled_height,
                                  uint32_t crop_top,  uint32_t crop_left,
                                  fourcc_t color, int32_t rotation, uint32_t numBuffers)
{
    DVP_PRINT(DVP_ZONE_VIDEO, "Creating File Based DVP Display!\n");
    if (color == FOURCC_UYVY || color == FOURCC_NV12 || color == FOURCC_YUY2)
    {
        dvp_display_t *dvpd = (dvp_display_t *)calloc(1, sizeof(dvp_display_t));
        if (dvpd)
        {
            char display_name[255];
            PYUV_GetFilename(display_name, "", "display", image_width, image_height, 30, color);
            //sprintf(display_name, "display_%ux%u_%s.yuv", image_width, image_height, (color==FOURCC_UYVY?"UYVY":"NV12"));
            dvpd->out = fopen(display_name, "wb+");
            DVP_PRINT(DVP_ZONE_VIDEO, "Opened file %s\n", display_name);
        }
        return dvpd;
    }
    else
        return NULL;
}

void DVP_Display_Destroy(dvp_display_t **dvpd)
{
    if (dvpd && *dvpd)
    {
        if ((*dvpd)->out)
            fclose((*dvpd)->out);
        free((*dvpd));
        *dvpd = NULL;
    }
}

void DVP_Display_Render(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    uint32_t y, j, n = 0, len = 0;
    if (dvpd == NULL || dvpd->out == NULL)
        return;
    if (pImage->color == FOURCC_UYVY || pImage->color == FOURCC_YUY2)
    {
        for (y = 0; y < pImage->height; y++)    // loop for each line
        {
            len = (pImage->x_stride * pImage->width);
            j = (y * pImage->y_stride);
            n += (uint32_t)fwrite(&pImage->pData[0][j], 1, len, dvpd->out);
        }
        fflush(dvpd->out);
    }
    else if (pImage->color == FOURCC_NV12)
    {
        for (y = 0; y < pImage->height ; y++)
        {
            len = (pImage->x_stride * pImage->width);
            j = (y * pImage->y_stride);
            n+= (DVP_U32)fwrite(&pImage->pData[0][j], 1, len, dvpd->out);
        }
        for (y = 0; y < pImage->height/2; y++)
        {
            len = (pImage->x_stride * pImage->width); // half-width but double pixels
            j = (y * pImage->y_stride);
            n+= (DVP_U32)fwrite(&pImage->pData[1][j], 1, len, dvpd->out);
        }
        fflush(dvpd->out);
    }
}

DVP_BOOL DVP_Display_Alloc(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    uint32_t p = 0;
    if (pImage->color == FOURCC_UYVY || pImage->color == FOURCC_YUY2)
    {
        uint32_t planeSize = pImage->bufWidth * pImage->bufHeight * pImage->x_stride;
        pImage->pBuffer[0] = calloc(1, planeSize);
        pImage->pData[0] = pImage->pBuffer[0];
        pImage->numBytes = planeSize;
    }
    else if (pImage->color == FOURCC_NV12)
    {
        uint32_t numBytes = (pImage->bufWidth * pImage->bufHeight * 3)/2;
        pImage->pBuffer[0] = calloc(1, numBytes);
        pImage->pBuffer[1] = &pImage->pBuffer[0][pImage->height * pImage->width];
        pImage->pData[0] = pImage->pBuffer[0];
        pImage->pData[1] = pImage->pBuffer[1];
    }
    pImage->y_stride = pImage->width * pImage->x_stride;
    pImage->memType = DVP_MTYPE_MPUCACHED_VIRTUAL;
    if (pImage->pBuffer[0])
        return DVP_TRUE;
    else
        return DVP_FALSE;
}

DVP_BOOL DVP_Display_Free(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    free(pImage->pBuffer[0]);
    pImage->pBuffer[0] = NULL;
    pImage->pBuffer[1] = NULL;
    pImage->pData[0] = NULL;
    pImage->pData[1] = NULL;
    pImage->numBytes = 0;
    pImage->y_stride = 0;
    pImage->memType = DVP_MTYPE_DEFAULT;
    return DVP_TRUE;
}

DVP_BOOL DVP_Display_Drop(dvp_display_t *dvpd, DVP_Image_t *pImage)
{
    if (dvpd && pImage)
        return DVP_TRUE;
    else
        return DVP_FALSE;
}

#endif

