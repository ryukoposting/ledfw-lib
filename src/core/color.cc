#include "prelude.hh"
#include "color.hh"

#define fcolor float

using namespace color;

static fcolor hue2rgb(fcolor p, fcolor q, fcolor t);

rgb::rgb(uint8_t r, uint8_t g, uint8_t b):
    red(r),
    green(g),
    blue(b)
{}

hsv::hsv(uint8_t h, uint8_t s, uint8_t v):
    hue(h),
    saturation(s),
    value(v)
{}

hsl::hsl(uint8_t h, uint8_t s, uint8_t l):
    hue(h),
    saturation(s),
    luminance(l)
{}

void hsv::to_rgb(rgb &result, curve curve)
{

    if (saturation == 0) {
        result.blue = value;
        result.green = value;
        result.blue = value;
        return;
    } else if (value == 0) {
        result.blue = 0;
        result.green = 0;
        result.blue = 0;
        return;
    }

    fcolor r, g, b;

    fcolor h = static_cast<fcolor>(hue) / 255.0;
    fcolor s = static_cast<fcolor>(saturation) / 255.0;
    fcolor v = static_cast<fcolor>(value) / 255.0;

    if (curve == curve::ws2812) {
        if (s > 0.004) {
            s = 255.0 - ((s - 255.0) * (s - 255.0) / 255.0);
        }
    }

    auto i = static_cast<int>(h * 6);
    auto f = h * 6.0 - i;
    auto p = v * (1.0 - s);
    auto q = v * (1.0 - f * s);
    auto t = v * (1.0 - (1.0 - f) * s);

    switch (i % 6)
    {
    case 0: r = v , g = t , b = p;
        break;
    case 1: r = q , g = v , b = p;
        break;
    case 2: r = p , g = v , b = t;
        break;
    case 3: r = p , g = q , b = v;
        break;
    case 4: r = t , g = p , b = v;
        break;
    case 5: r = v , g = p , b = q;
        break;
    }

    result.red = static_cast<uint8_t>(r * 255.0);
    result.green = static_cast<uint8_t>(g * 255.0);
    result.blue = static_cast<uint8_t>(b * 255.0);
}

void hsl::to_rgb(rgb &result, curve curve)
{
    fcolor r, g, b;
    fcolor h = static_cast<fcolor>(hue) / 255.0;
    fcolor s = static_cast<fcolor>(saturation) / 255.0;
    fcolor l = static_cast<fcolor>(luminance) / 255.0;

    if (s == 0)
    {
        r = g = b = l; // achromatic
    }
    else
    {
        auto q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        auto p = 2 * l - q;
        r = hue2rgb(p, q, h + 1 / 3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1 / 3.0);
    }

    result.red = static_cast<uint8_t>(r * 255);
    result.green = static_cast<uint8_t>(g * 255);
    result.blue = static_cast<uint8_t>(b * 255);
}

static fcolor hue2rgb(fcolor p, fcolor q, fcolor t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1 / 6.0) return p + (q - p) * 6 * t;
    if (t < 1 / 2.0) return q;
    if (t < 2 / 3.0) return p + (q - p) * (2 / 3.0 - t) * 6;
    return p;
}
