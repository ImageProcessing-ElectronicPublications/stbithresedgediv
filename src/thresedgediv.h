/**

EdgeDiv ("EdgePlus" + "BlurDiv") threshold v1.0
By zvezdochiot <mykaralw@yandex.ru>
This is free and unencumbered software released into the public domain.

QUICK START

    #include ...
    #include ...
    #define THRESHOLD_EDGEDIV_IMPLEMENTATION
    #include "thresedgediv.h"
    ...
    unsigned int width = 0, height = 0;
    unsigned int widthb = 0, heightb = 0;
    unsigned char components = 1,  componentsb = 1;
    float coef = 0.75f, delta = 0.0f;
    unsigned char bound_lower = 0, bound_upper = 255;
    int info = 1;

    unsigned char* image = stbi_load("foo.png", &width, &height, &components, 0);
    unsigned char* blur = stbi_load("foo_blur.png", &widthb, &heightb, &componentsb, 0);

    if ((width == widthb) && (height == heightb) && (components == componentsb))
    {
        image_threshold_edgediv(width, height, components, coef, delta, bound_lower, bound_upper, info, image, blur);
        stbi_write_png("foo.thresgrad.png", width, height, components, image, 0);
    }

VERSION HISTORY

1.0  2024-12-26  "init"    Initial header release.
1.1  2024-01-24  "bimodal"    Correct BiModal Value.

**/

#ifndef THRESHOLD_EDGEDIV_H
#define THRESHOLD_EDGEDIV_H
#ifdef __cplusplus
    extern "C" {
#endif

float image_filter_edgediv(unsigned int width, unsigned int height, unsigned char components, float coefep, float coefbd, unsigned char* image, unsigned char* blur);
float image_threshold_bimodal(unsigned int width, unsigned int height, unsigned char components, float delta, unsigned char bound_lower, unsigned char bound_upper, unsigned char* image);
void image_threshold_edgediv(unsigned int width, unsigned int height, unsigned char components, float coef, float delta, unsigned char bound_lower, unsigned char bound_upper, int info, unsigned char* image, unsigned char* blur);

#ifdef __cplusplus
    }
#endif
#endif  /* THRESHOLD_EDGEDIV_H */

#ifdef THRESHOLD_EDGEDIV_IMPLEMENTATION
#include <stdlib.h>
#include <math.h>

float image_filter_edgediv(unsigned int width, unsigned int height, unsigned char components, float coefep, float coefbd, unsigned char* image, unsigned char* blur)
{
    float mse = 0.0f;
    if ((image != NULL) && (blur != NULL))
    {
        for (unsigned char c = 0; c < components; c++)
        {
            size_t i = c;
            for (unsigned int y = 0; y < height; y++)
            {
                float msel = 0.0f;
                for (unsigned int x = 0; x < width; x++)
                {
                    float s = image[i];
                    float b = blur[i];
                    float retval = s;

                    if (coefep != 0.0f)
                    {
                        /* EdgePlus */
                        /* edge = I / blur (shift = -0.5) {0.0 .. >1.0}, mean value = 0.5 */
                        float edge = (retval + 1.0f) / (b + 1.0f) - 0.5f;
                        /* edgeplus = I * edge, mean value = 0.5 * mean(I) */
                        float edgeplus = retval * edge;
                        /* return k * edgeplus + (1 - k) * I */
                        retval = coefep * edgeplus + (1.0f - coefep) * retval;
                    }

                    if (coefbd != 0.0f)
                    {
                        /* BlurDiv */
                        /* edge = blur / I (shift = -0.5) {0.0 .. >1.0}, mean value = 0.5 */
                        float edgeinv = (b + 1.0f) / (retval + 1.0f) - 0.5f;
                        /* edgenorm = edge * k + max * (1 - k), mean value = {0.5 .. 1.0} * mean(I) */
                        float edgenorm = coefbd * edgeinv + (1.0f - coefbd);
                        /* return I / edgenorm */
                        edgenorm = (edgenorm > 0.0f) ? (1.0f / edgenorm) : 1.0f;
                        retval = s * edgenorm;
                    }

                    /* trim value {0..255} */
                    retval += 0.5f;
                    retval = (retval < 0.0f) ? 0.0f : ((retval < 255.0f) ? retval : 255.0f);

                    float delta = retval - s;
                    msel += (delta * delta);

                    image[i] = (unsigned char) retval;
                    i += components;
                }
                mse += msel;
            }
        }
        mse /= width;
        mse /= height;
        mse /= components;
        mse = sqrtf(mse);
        mse /= 255.0f;
    }

    return mse;
}

float image_threshold_bimodal(unsigned int width, unsigned int height, unsigned char components, float delta, unsigned char bound_lower, unsigned char bound_upper, unsigned char* image)
{
    float bwm = 0.0f;
    unsigned int histsize = 256;
    uint64_t histogram[histsize];

    unsigned int threshold = 128, Tn;
    uint64_t im, iw, ib;
    double Tw, Tb;
    int Tmin = 255, Tmax = 0, Ti;
    float part = 0.5f + (delta / 256.0f);
    part = (part < 0.0f) ? 0.0f : ((part < 1.0f) ? part : 1.0f);

    if (image != NULL)
    {
        size_t count_black = 0;
        for (unsigned char c = 0; c < components; c++)
        {
            size_t i = c;
            for (unsigned int k = 0; k < histsize; k++)
            {
                histogram[k] = 0;
            }

            for (unsigned int y = 0; y < height; y++)
            {
                for (unsigned int x = 0; x < width; x++)
                {
                    unsigned int s = image[i];
                    s = (s < bound_lower) ? bound_lower : (s < bound_upper) ? s : bound_upper;
                    histogram[s]++;
                    i += components;
                }
            }

            Ti = 0;
            while (Ti < Tmin)
            {
                Tmin = (histogram[Ti] > 0) ? Ti : Tmin;
                Ti++;
            }
            Ti = 255;
            while (Ti > Tmax)
            {
                Tmax = (histogram[Ti] > 0) ? Ti : Tmax;
                Ti--;
            }
            Tmax++;

            threshold = (unsigned int) (part * Tmax + (1.0 - part) * Tmin + 0.5);
            Tn = threshold + 1;
            while (threshold != Tn)
            {
                Tn = threshold;
                Tb = Tw = 0.0;
                ib = iw = 0;
                for (unsigned int k = 0; k < histsize; k++)
                {
                    im = histogram[k];
                    if (k < threshold)
                    {
                        Tb += (double) (im * k);
                        ib += im;
                    }
                    else
                    {
                        Tw += (double) (im * k);
                        iw += im;
                    }
                }
                Tb = (ib > 0) ? (Tb / ib) : Tmin;
                Tw = (iw > 0) ? (Tw / iw) : Tmax;
                threshold = (unsigned int) (part * Tw + (1.0f - part) * Tb + 0.5f);
            }

            i = c;
            for (unsigned int y = 0; y < height; y++)
            {
                for (unsigned int x = 0; x < width; x++)
                {
                    float s = image[i];
                    unsigned char retval = 255;
                    if (s < threshold)
                    {
                        retval = 0;
                        count_black++;
                    }
                    image[i] = retval;
                    i += components;
                }
            }
        }
        bwm = (double) count_black / (width * height * components);
    }

    return bwm;
}

void image_threshold_edgediv(unsigned int width, unsigned int height, unsigned char components, float coef, float delta, unsigned char bound_lower, unsigned char bound_upper, int info, unsigned char* image, unsigned char* blur)
{
    if (bound_upper < bound_lower)
    {
        unsigned char bound = bound_lower;
        bound_lower = bound_upper;
        bound_upper = bound;
    }

    float mse = image_filter_edgediv(width, height, components, coef, coef, image, blur);
    float bwm = image_threshold_bimodal(width, height, components, delta, bound_lower, bound_upper, image);
    if (info > 0)
    {
        fprintf(stderr, "INFO: mse %f\n", mse);
        fprintf(stderr, "INFO: BW metric %f\n", bwm);
    }
}

#endif  /* THRESHOLD_EDGEDIV_IMPLEMENTATION */
